// str_parse_float.c
//
// Parsing floats from strings, correctly rounded: to nearest, ties to even, straight to the
// type's precision, so a float is never rounded twice through a double.
//
// Every path ends in the same rounding. It takes the value as a 64-bit head whose top bit is set,
// and a sticky flag saying whether anything nonzero lies below it: the value is
// (head + a fraction) x 2^exponent, the fraction nonzero exactly when sticky is set. That is all
// rounding needs, to the normal precision or into the subnormals.
//
// Decimal text is d x 10^e, d an integer of up to 800 significant digits (past those, a final 1
// stands in for the rest, which is enough: a double needs at most 767). Three ways in:
//
// - Clinger's fast path: when d and 10^|e| are both exact in the type, one multiply or divide by
//   an exact power of ten is correctly rounded. That covers most numbers people write.
// - e >= 0: d x 10^e is an integer, worked out exactly in the bignum, whose top 64 bits are the
//   head.
// - e < 0: d / 10^-e, divided exactly in the bignum to 65 bits plus a remainder.
//
// Values too big for the type, or below half its smallest subnormal, are decided from the number
// of digits and e before any of that. Hex text needs no bignum: its first 16 significant digits
// are the head, and the rest only feed sticky.

#include "richc/str.h"

#include "richc/math/math.h"

#include "float_big.h"
#include "str_parse.h"

enum {
    // The most significant decimal digits we keep; a final 1 stands in for any past them
    RC_PARSE_MAX_DIGITS = 800,

    // Exponents beyond this are as good as infinite, and cannot overflow the sums made with them
    RC_PARSE_MAX_EXPONENT = 1 << 20
};

// A binary floating-point format to round to.
typedef struct rc_parse_format {
    uint32_t precision;        // the significand's bits, the hidden one included
    int32_t min_exponent;      // the smallest normal exponent, which is also the subnormals'
    int32_t max_exponent;      // the largest exponent, which is also the bias
    int32_t overflow_digits;   // number of digits + e above which a decimal overflows
    int32_t underflow_digits;  // number of digits + e at or below which a decimal is zero
    uint32_t fast_digits;      // the most digits Clinger's fast path takes
    int32_t fast_exponent;     // the largest |e| it takes
    uint64_t infinity;         // the bits of infinity
    uint64_t quiet_nan;        // the bits of the default quiet NaN
    uint64_t sign;             // the sign bit
    bool single;               // a float, whose fast path works in float
} rc_parse_format;

static const rc_parse_format rc_parse_f64_format = {
    .precision = 53,
    .min_exponent = -1022,
    .max_exponent = 1023,
    .overflow_digits = 309,
    .underflow_digits = -324,
    .fast_digits = 15,
    .fast_exponent = 22,
    .infinity = 0x7ff0000000000000ull,
    .quiet_nan = 0x7ff8000000000000ull,
    .sign = 0x8000000000000000ull,
    .single = false
};

static const rc_parse_format rc_parse_f32_format = {
    .precision = 24,
    .min_exponent = -126,
    .max_exponent = 127,
    .overflow_digits = 39,
    .underflow_digits = -46,
    .fast_digits = 7,
    .fast_exponent = 10,
    .infinity = 0x7f800000u,
    .quiet_nan = 0x7fc00000u,
    .sign = 0x80000000u,
    .single = true
};

// The powers of ten that doubles and floats hold exactly
static const double rc_parse_f64_powers[23] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};

static const float rc_parse_f32_powers[11] = {
    1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f
};

// A float as its bits, whichever type, with the text after it and an rc_parse_error.
typedef struct rc_parse_bits {
    uint64_t bits;
    rc_str rest;
    uint8_t error;
} rc_parse_bits;

// A value ready to round: (head + a fraction) x 2^exponent, head's top bit set, the fraction
// nonzero exactly when sticky.
typedef struct rc_parse_head {
    uint64_t head;
    int64_t exponent;
    bool sticky;
} rc_parse_head;

// The bits of a finite value, and whether it overflowed.
typedef struct rc_parse_rounded {
    uint64_t bits;
    bool overflow;
} rc_parse_rounded;

// An exponent read from text, and the text after it. found is false if there was none.
typedef struct rc_parse_exponent {
    int64_t value;
    rc_str rest;
    bool found;
} rc_parse_exponent;

// The digits of a number's text, before the point and after it, its exponent as written, and the
// text after it all.
typedef struct rc_parse_text {
    rc_str integer;
    rc_str fraction;
    int64_t exponent;
    rc_str rest;
    bool found;  // there was at least one digit
} rc_parse_text;

// A decimal value d x 10^e, from its text's significant digits: num of them, from first, counting
// the digits before and after the point as one run.
typedef struct rc_parse_decimal {
    rc_parse_text text;
    uint32_t first;
    uint32_t num;
    int64_t e;
} rc_parse_decimal;

// The first 16 significant digits of a hex number, and whether any digit past them is nonzero.
typedef struct rc_parse_hex_digits {
    uint64_t value;
    uint32_t dropped;  // the digits past the kept ones
    bool sticky;
} rc_parse_hex_digits;

// ---- rounding ----

// head shifted right by drop (1 to 64), rounded to nearest with ties to even, sticky counting as
// more below it.
static uint64_t rc_parse_round_shift(uint64_t head, uint32_t drop, bool sticky)
{
    uint64_t kept = drop == 64 ? 0 : head >> drop;
    uint64_t half = 1ull << (drop - 1);
    bool has_half = (head & half) != 0;
    bool more = (head & (half - 1)) != 0 || sticky;
    bool round_up = has_half && (more || (kept & 1) != 0);

    return kept + (round_up ? 1u : 0u);
}


// value rounded to format, its top bit's exponent top no more than the largest. The kept bits
// carry the hidden one into the exponent field, and a carry out of them, or up from the
// subnormals, lands there too. Below half the smallest subnormal, the value is zero.
static rc_parse_rounded rc_parse_round_finite(rc_parse_head value, int64_t top, rc_parse_format format)
{
    bool normal = top >= format.min_exponent;
    int64_t drop = 64 - (int64_t)format.precision + (normal ? 0 : format.min_exponent - top);
    uint64_t field = normal ? (uint64_t)(top + format.max_exponent - 1) : 0;
    uint64_t kept = drop > 64 ? 0 : rc_parse_round_shift(value.head, (uint32_t)drop, value.sticky);
    uint64_t bits = drop > 64 ? 0 : (field << (format.precision - 1)) + kept;

    return (rc_parse_rounded) {.bits = bits, .overflow = bits >= format.infinity};
}


// value rounded to format: its bits, and whether it overflowed.
static rc_parse_rounded rc_parse_round(rc_parse_head value, rc_parse_format format)
{
    int64_t top = value.exponent + 63;

    if (top > format.max_exponent) {
        return (rc_parse_rounded) {.overflow = true};
    }
    return rc_parse_round_finite(value, top, format);
}


// The result of a finite, nonzero value with its sign: RC_PARSE_ERROR_RANGE if it overflowed.
static rc_parse_bits rc_parse_finish(rc_parse_head value, bool negative, rc_str rest, rc_parse_format format)
{
    rc_parse_rounded rounded = rc_parse_round(value, format);

    if (rounded.overflow) {
        return (rc_parse_bits) {.rest = rest, .error = RC_PARSE_ERROR_RANGE};
    }
    return (rc_parse_bits) {.bits = rounded.bits | (negative ? format.sign : 0), .rest = rest};
}


// The result of zero with its sign.
static rc_parse_bits rc_parse_zero(bool negative, rc_str rest, rc_parse_format format)
{
    return (rc_parse_bits) {.bits = negative ? format.sign : 0, .rest = rest};
}

// ---- the bignum ----

// The number of bits in big, 0 for zero.
static uint32_t rc_parse_big_bits(const rc_float_big *big)
{
    if (big->num == 0) {
        return 0;
    }
    return big->num * 32 - rc_clz_u32(big->words[big->num - 1]);
}


// The 64 bits of big from bit up, with zeros past its top.
static uint64_t rc_parse_big_bits_at(const rc_float_big *big, uint32_t bit)
{
    uint32_t word = bit / 32;
    uint32_t shift = bit % 32;
    uint64_t low = word < big->num ? big->words[word] : 0;
    uint64_t middle = word + 1 < big->num ? big->words[word + 1] : 0;
    uint64_t high = word + 2 < big->num ? big->words[word + 2] : 0;
    uint64_t pair = low | middle << 32;

    return shift == 0 ? pair : pair >> shift | high << (64 - shift);
}


// True if any bit of big below bit is set.
static bool rc_parse_big_any_below(const rc_float_big *big, uint32_t bit)
{
    uint32_t word = bit / 32;
    uint32_t partial = word < big->num ? big->words[word] & (uint32_t)((1ull << (bit % 32)) - 1) : 0;

    for (uint32_t i = 0; i < word && i < big->num; i++) {
        if (big->words[i] != 0) {
            return true;
        }
    }
    return partial != 0;
}


// big << shift.
static rc_float_big rc_parse_big_shifted(const rc_float_big *big, uint32_t shift)
{
    rc_float_big shifted = {0};
    uint32_t words = shift / 32;
    uint32_t bits = shift % 32;

    RC_ASSERT(big->num + words < RC_FLOAT_BIG_WORDS);
    for (uint32_t i = 0; i < big->num; i++) {
        uint64_t part = (uint64_t)big->words[i] << bits;

        shifted.words[i + words] |= (uint32_t)part;
        shifted.words[i + words + 1] = (uint32_t)(part >> 32);
    }
    shifted.num = big->num + words + 1;
    rc_float_big_trim(&shifted);
    return shifted;
}


// Halve big in place, dropping the bit that falls off.
static void rc_parse_big_halve(rc_float_big *big)
{
    for (uint32_t i = 0; i < big->num; i++) {
        uint32_t carry = i + 1 < big->num ? big->words[i + 1] << 31 : 0;

        big->words[i] = big->words[i] >> 1 | carry;
    }
    rc_float_big_trim(big);
}


// True if a >= b.
static bool rc_parse_big_at_least(const rc_float_big *a, const rc_float_big *b)
{
    if (a->num != b->num) {
        return a->num > b->num;
    }

    for (uint32_t i = a->num; i-- > 0;) {
        if (a->words[i] != b->words[i]) {
            return a->words[i] > b->words[i];
        }
    }
    return true;
}


// a -= b, where a >= b.
static void rc_parse_big_subtract(rc_float_big *a, const rc_float_big *b)
{
    uint64_t borrow = 0;

    for (uint32_t i = 0; i < a->num; i++) {
        uint64_t take = (i < b->num ? b->words[i] : 0) + borrow;

        borrow = take > a->words[i] ? 1 : 0;
        a->words[i] = (uint32_t)(a->words[i] - take);
    }
    rc_float_big_trim(a);
}


// big x 10^power.
static rc_float_big rc_parse_big_times_power(rc_float_big big, uint32_t power)
{
    static const uint32_t small_powers[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    rc_float_big product = big;
    uint32_t left = power;

    while (left >= 9) {
        rc_float_big_multiply_add(&product, 1000000000u, 0);
        left -= 9;
    }
    rc_float_big_multiply_add(&product, small_powers[left], 0);
    return product;
}

// ---- decimal ----

// How many digits of base start s.
static uint32_t rc_parse_count_digits(rc_str s, uint32_t base)
{
    uint32_t i = 0;

    while (i < s.len && rc_parse_digit_value(s.data[i]) < base) {
        i++;
    }
    return i;
}


// Digit i of the run of a number's digits, before and after the point, as a value.
static uint32_t rc_parse_digit_at(rc_parse_text text, uint32_t i)
{
    char c = i < text.integer.len ? text.integer.data[i] : text.fraction.data[i - text.integer.len];

    return rc_parse_digit_value(c);
}


// The exponent at the start of s, if there is one: letter in either case, an optional sign, and
// decimal digits, saturated to RC_PARSE_MAX_EXPONENT. Without the digits, nothing is read.
static rc_parse_exponent rc_parse_exponent_of(rc_str s, char letter)
{
    bool has_letter = s.len > 0 && (s.data[0] | 0x20) == letter;
    bool has_sign = has_letter && s.len > 1 && (s.data[1] == '+' || s.data[1] == '-');
    uint32_t start = has_sign ? 2u : 1u;
    rc_str digits = rc_str_first(rc_str_drop_first(s, start), rc_parse_count_digits(rc_str_drop_first(s, start), 10));
    int64_t value = 0;

    if (!has_letter || digits.len == 0) {
        return (rc_parse_exponent) {.rest = s};
    }

    for (uint32_t i = 0; i < digits.len; i++) {
        value = rc_min_i64(value * 10 + (digits.data[i] - '0'), RC_PARSE_MAX_EXPONENT);
    }

    return (rc_parse_exponent) {
        .value = has_sign && s.data[1] == '-' ? -value : value,
        .rest = rc_str_drop_first(s, start + digits.len),
        .found = true
    };
}


// The digits of base (10 or 16) at the start of s, before and after an optional point, then an
// exponent with the given letter. found is false if there is no digit either side of the point,
// and then nothing is read.
static rc_parse_text rc_parse_number_text(rc_str s, uint32_t base, char letter)
{
    uint32_t integer_len = rc_parse_count_digits(s, base);
    bool has_point = integer_len < s.len && s.data[integer_len] == '.';
    uint32_t fraction_len = has_point ? rc_parse_count_digits(rc_str_drop_first(s, integer_len + 1), base) : 0;
    rc_str after = rc_str_drop_first(s, integer_len + (has_point ? 1 + fraction_len : 0));
    rc_parse_exponent exponent = rc_parse_exponent_of(after, letter);

    if (integer_len + fraction_len == 0) {
        return (rc_parse_text) {.rest = s};
    }

    return (rc_parse_text) {
        .integer = rc_str_first(s, integer_len),
        .fraction = rc_str_substr(s, integer_len + 1, integer_len + 1 + fraction_len),
        .exponent = exponent.value,
        .rest = exponent.found ? exponent.rest : after,
        .found = true
    };
}


// The significant digits of a decimal's text, and the power of ten they are scaled by.
static rc_parse_decimal rc_parse_decimal_of(rc_parse_text text)
{
    uint32_t total = text.integer.len + text.fraction.len;
    uint32_t first = 0;
    uint32_t end = total;

    while (first < total && rc_parse_digit_at(text, first) == 0) {
        first++;
    }

    while (end > first && rc_parse_digit_at(text, end - 1) == 0) {
        end--;
    }

    return (rc_parse_decimal) {
        .text = text,
        .first = first,
        .num = end - first,
        .e = text.exponent - (int64_t)text.fraction.len + (int64_t)(total - end)
    };
}


// The decimal's digits as an integer, when there are at most 19.
static uint64_t rc_parse_small_digits(rc_parse_decimal decimal)
{
    uint64_t value = 0;

    for (uint32_t i = 0; i < decimal.num; i++) {
        value = value * 10 + rc_parse_digit_at(decimal.text, decimal.first + i);
    }
    return value;
}


// The decimal's digits as a bignum, at most RC_PARSE_MAX_DIGITS of them. If there are more, a final
// 1 stands for the rest, which are not all zero since the last significant digit never is.
static rc_float_big rc_parse_big_digits(rc_parse_decimal decimal)
{
    rc_float_big big = {0};
    uint32_t num = decimal.num < RC_PARSE_MAX_DIGITS ? decimal.num : RC_PARSE_MAX_DIGITS;
    uint32_t i = 0;

    // Nine digits at a time
    while (i < num) {
        uint32_t chunk = 0;
        uint32_t factor = 1;

        for (uint32_t j = 0; j < 9 && i < num; j++) {
            chunk = chunk * 10 + rc_parse_digit_at(decimal.text, decimal.first + i);
            factor *= 10;
            i++;
        }
        rc_float_big_multiply_add(&big, factor, chunk);
    }

    if (decimal.num > RC_PARSE_MAX_DIGITS) {
        rc_float_big_multiply_add(&big, 10, 1);
    }
    return big;
}


// The power of ten the bignum digits are scaled by, allowing for the stand-in 1.
static int64_t rc_parse_big_e(rc_parse_decimal decimal)
{
    if (decimal.num > RC_PARSE_MAX_DIGITS) {
        return decimal.e + (int64_t)decimal.num - (RC_PARSE_MAX_DIGITS + 1);
    }
    return decimal.e;
}


// d x 10^e for e >= 0, exactly: the product's top 64 bits, and whether any below them are set.
static rc_parse_head rc_parse_scale_up(rc_float_big digits, int64_t e)
{
    rc_float_big product = rc_parse_big_times_power(digits, (uint32_t)e);
    uint32_t bits = rc_parse_big_bits(&product);

    if (bits <= 64) {
        return (rc_parse_head) {.head = rc_parse_big_bits_at(&product, 0) << (64 - bits), .exponent = (int64_t)bits - 64};
    }

    return (rc_parse_head) {
        .head = rc_parse_big_bits_at(&product, bits - 64),
        .exponent = (int64_t)bits - 64,
        .sticky = rc_parse_big_any_below(&product, bits - 64)
    };
}


// d / 10^-e for e < 0, exactly. The numerator is shifted so the quotient has 64 or 65 bits: with d
// of m bits and 10^-e of n, d x 2^(64 + n - m) / 10^-e lies between 2^63 and 2^65. Long division
// finds the quotient a bit at a time, and the remainder is sticky.
static rc_parse_head rc_parse_scale_down(rc_float_big digits, int64_t e)
{
    rc_float_big one = {.words = {1}, .num = 1};
    rc_float_big power = rc_parse_big_times_power(one, (uint32_t)-e);
    int64_t shift = 64 + (int64_t)rc_parse_big_bits(&power) - (int64_t)rc_parse_big_bits(&digits);
    rc_float_big numerator = shift > 0 ? rc_parse_big_shifted(&digits, (uint32_t)shift) : digits;
    rc_float_big step = rc_parse_big_shifted(&power, 64 + (uint32_t)(shift < 0 ? -shift : 0));
    bool quotient_top = rc_parse_big_at_least(&numerator, &step);
    uint64_t quotient = 0;

    // Bit 64 of the quotient, then bits 63 down to 0
    if (quotient_top) {
        rc_parse_big_subtract(&numerator, &step);
    }

    for (uint32_t bit = 64; bit-- > 0;) {
        rc_parse_big_halve(&step);
        if (rc_parse_big_at_least(&numerator, &step)) {
            rc_parse_big_subtract(&numerator, &step);
            quotient |= 1ull << bit;
        }
    }

    // Fold a 65th bit down, the bit that falls off into sticky
    if (quotient_top) {
        return (rc_parse_head) {
            .head = quotient >> 1 | 1ull << 63,
            .exponent = -shift + 1,
            .sticky = numerator.num != 0 || (quotient & 1) != 0
        };
    }
    return (rc_parse_head) {.head = quotient, .exponent = -shift, .sticky = numerator.num != 0};
}


// Clinger's fast path for a double: the digits and the power of ten are exact, so one operation
// rounds correctly.
static uint64_t rc_parse_fast_f64(uint64_t digits, int64_t e)
{
    double value = (double)digits;

    if (e < 0) {
        return rc_bitcast_f64(value / rc_parse_f64_powers[-e]);
    }
    return rc_bitcast_f64(value * rc_parse_f64_powers[e]);
}


// The same for a float, worked in float.
static uint64_t rc_parse_fast_f32(uint64_t digits, int64_t e)
{
    float value = (float)digits;

    if (e < 0) {
        return rc_bitcast_f32(value / rc_parse_f32_powers[-e]);
    }
    return rc_bitcast_f32(value * rc_parse_f32_powers[e]);
}


// True if Clinger's fast path takes the decimal in format.
static bool rc_parse_is_fast(rc_parse_decimal decimal, rc_parse_format format)
{
    if (decimal.num > format.fast_digits) {
        return false;
    }
    return decimal.e >= -format.fast_exponent && decimal.e <= format.fast_exponent;
}


// A nonzero decimal in range, exactly, in the bignum.
static rc_parse_bits rc_parse_decimal_exact(rc_parse_decimal decimal, bool negative, rc_parse_format format)
{
    rc_float_big digits = rc_parse_big_digits(decimal);
    int64_t e = rc_parse_big_e(decimal);
    rc_parse_head value = e >= 0 ? rc_parse_scale_up(digits, e) : rc_parse_scale_down(digits, e);

    return rc_parse_finish(value, negative, decimal.text.rest, format);
}


// A nonzero decimal in range, by the fast path or exactly.
static rc_parse_bits rc_parse_decimal_in_range(rc_parse_decimal decimal, bool negative, rc_parse_format format)
{
    uint64_t sign = negative ? format.sign : 0;
    uint64_t small = decimal.num <= 19 ? rc_parse_small_digits(decimal) : 0;

    if (!rc_parse_is_fast(decimal, format)) {
        return rc_parse_decimal_exact(decimal, negative, format);
    }

    if (format.single) {
        return (rc_parse_bits) {.bits = rc_parse_fast_f32(small, decimal.e) | sign, .rest = decimal.text.rest};
    }
    return (rc_parse_bits) {.bits = rc_parse_fast_f64(small, decimal.e) | sign, .rest = decimal.text.rest};
}


// A decimal's value in format, its digits found.
static rc_parse_bits rc_parse_decimal_value(rc_parse_decimal decimal, bool negative, rc_parse_format format)
{
    int64_t magnitude = (int64_t)decimal.num + decimal.e;

    if (decimal.num == 0) {
        return rc_parse_zero(negative, decimal.text.rest, format);
    }

    if (magnitude <= format.underflow_digits) {
        return rc_parse_zero(negative, decimal.text.rest, format);
    }

    if (magnitude > format.overflow_digits) {
        return (rc_parse_bits) {.rest = decimal.text.rest, .error = RC_PARSE_ERROR_RANGE};
    }
    return rc_parse_decimal_in_range(decimal, negative, format);
}

// ---- hex ----

// The first 16 significant digits of a hex number's text, as an integer.
static rc_parse_hex_digits rc_parse_hex_digits_of(rc_parse_text text)
{
    uint32_t total = text.integer.len + text.fraction.len;
    uint32_t first = 0;
    uint64_t value = 0;
    uint32_t kept = 0;
    bool sticky = false;

    while (first < total && rc_parse_digit_at(text, first) == 0) {
        first++;
    }

    for (uint32_t i = first; i < total; i++) {
        uint32_t digit = rc_parse_digit_at(text, i);

        if (kept < 16) {
            value = value << 4 | digit;
            kept++;
        }
        else {
            sticky = sticky || digit != 0;
        }
    }

    return (rc_parse_hex_digits) {.value = value, .dropped = total - first - kept, .sticky = sticky};
}


// A nonzero hex number's value as a head. The kept digits are an integer whose point sits after
// the dropped ones and before the fraction's, so they are scaled by 16^(dropped - fraction).
static rc_parse_head rc_parse_hex_head(rc_parse_hex_digits digits, rc_parse_text text)
{
    uint32_t lead = rc_clz_u64(digits.value);
    int64_t scale = 4 * ((int64_t)digits.dropped - (int64_t)text.fraction.len);

    return (rc_parse_head) {
        .head = digits.value << lead,
        .exponent = text.exponent + scale - (int64_t)lead,
        .sticky = digits.sticky
    };
}


// A hex number's value in format, its digits found.
static rc_parse_bits rc_parse_hex_value(rc_parse_text text, bool negative, rc_parse_format format)
{
    rc_parse_hex_digits digits = rc_parse_hex_digits_of(text);

    if (digits.value == 0) {
        return rc_parse_zero(negative, text.rest, format);
    }
    return rc_parse_finish(rc_parse_hex_head(digits, text), negative, text.rest, format);
}

// ---- infinity and NaN ----

// True if s starts with word, in any case.
static bool rc_parse_starts_with_word(rc_str s, rc_str word)
{
    return rc_str_is_equal_insensitive(rc_str_first(s, word.len), word);
}


// inf, infinity or nan at the start of s, with its sign, or RC_PARSE_ERROR_SYNTAX if none is.
static rc_parse_bits rc_parse_special(rc_str s, bool negative, rc_parse_format format)
{
    uint64_t sign = negative ? format.sign : 0;

    if (rc_parse_starts_with_word(s, RC_STR("infinity"))) {
        return (rc_parse_bits) {.bits = format.infinity | sign, .rest = rc_str_drop_first(s, 8)};
    }

    if (rc_parse_starts_with_word(s, RC_STR("inf"))) {
        return (rc_parse_bits) {.bits = format.infinity | sign, .rest = rc_str_drop_first(s, 3)};
    }

    if (rc_parse_starts_with_word(s, RC_STR("nan"))) {
        return (rc_parse_bits) {.bits = format.quiet_nan | sign, .rest = rc_str_drop_first(s, 3)};
    }
    return (rc_parse_bits) {.error = RC_PARSE_ERROR_SYNTAX};
}

// ---- parsing ----

// True if s starts with 0x, in either case, then a point and a hex digit.
static bool rc_parse_has_hex_point_prefix(rc_str s)
{
    if (s.len < 4 || s.data[0] != '0') {
        return false;
    }

    if ((s.data[1] | 0x20) != 'x' || s.data[2] != '.') {
        return false;
    }
    return rc_parse_digit_value(s.data[3]) < 16;
}


// True if text past any sign is hex: in base 16, or in base 0 behind a 0x prefix.
static bool rc_parse_is_hex(rc_str s, uint32_t base)
{
    if (base != 0) {
        return base == 16;
    }
    return rc_parse_has_prefix(s, 'x', 16) || rc_parse_has_hex_point_prefix(s);
}


// A float from the start of s in base, as format's bits.
static rc_parse_bits rc_parse_float(rc_str s, uint32_t base, rc_parse_format format)
{
    bool negative = rc_parse_is_minus(s);
    rc_str unsigned_text = rc_str_drop_first(s, negative ? 1 : 0);
    bool hex = rc_parse_is_hex(unsigned_text, base);
    rc_str digits = rc_str_drop_first(unsigned_text, base == 0 && hex ? 2 : 0);
    rc_parse_text text = rc_parse_number_text(digits, hex ? 16 : 10, hex ? 'p' : 'e');
    rc_parse_bits special = rc_parse_special(unsigned_text, negative, format);

    RC_ASSERT(base == 0 || base == 10 || base == 16);
    if (text.found && hex) {
        return rc_parse_hex_value(text, negative, format);
    }

    if (text.found) {
        return rc_parse_decimal_value(rc_parse_decimal_of(text), negative, format);
    }

    if (special.error == RC_PARSE_ERROR_NONE) {
        return special;
    }
    return (rc_parse_bits) {.rest = s, .error = RC_PARSE_ERROR_SYNTAX};
}


rc_parsed_f64 rc_str_parse_f64(rc_str s, uint32_t base)
{
    rc_parse_bits parsed = rc_parse_float(s, base, rc_parse_f64_format);

    return (rc_parsed_f64) {.value = rc_bitcast_u64(parsed.bits), .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_f32 rc_str_parse_f32(rc_str s, uint32_t base)
{
    rc_parse_bits parsed = rc_parse_float(s, base, rc_parse_f32_format);

    return (rc_parsed_f32) {.value = rc_bitcast_u32((uint32_t)parsed.bits), .rest = parsed.rest, .error = parsed.error};
}


rc_parsed_f64 rc_str_as_f64(rc_str s, uint32_t base)
{
    rc_parsed_f64 parsed = rc_str_parse_f64(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_f64) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0.0, .rest = parsed.rest, .error = error};
}


rc_parsed_f32 rc_str_as_f32(rc_str s, uint32_t base)
{
    rc_parsed_f32 parsed = rc_str_parse_f32(s, base);
    uint8_t error = rc_parse_whole_error(parsed.error, parsed.rest);

    return (rc_parsed_f32) {.value = error == RC_PARSE_ERROR_NONE ? parsed.value : 0.0f, .rest = parsed.rest, .error = error};
}
