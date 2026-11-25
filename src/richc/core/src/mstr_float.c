// mstr_float.c
//
// rc_mstr_append_f64 and _f32: floating point as decimal text, the same text C's "%.*g" and "%.*f"
// give, with no libc.
//
// A double is m x 2^e exactly, so its decimal expansion is finite, and we work it out exactly with
// a little bignum: the integer part by dividing by 10^9 again and again, the fraction by
// multiplying by 10 and taking the bits that cross the binary point. Rounding sees the exact value,
// so a tie rounds to even, as printf does. It is slow next to Ryu, but small and plainly right,
// which will do until number formatting turns up somewhere hot.

#include "richc/mstr.h"

#include "richc/math/math.h"

#include "float_big.h"

enum {
    // The most digits we ever keep: 309 before the point, 255 after it, one to round on and one
    // for a carry
    RC_FLOAT_MAX_DIGITS = 600,

    // The most digits a double has before the point
    RC_FLOAT_MAX_INT_DIGITS = 324,

    // The longest text we build: every digit, a sign, a point and an exponent
    RC_FLOAT_MAX_TEXT = RC_FLOAT_MAX_DIGITS + 16,

    // The precision used when none is given
    RC_FLOAT_DEFAULT_PRECISION = 6
};

// A value's leading decimal digits (each 0 to 9) and where the point goes: the value is
// 0.d1 d2 d3 ... x 10^exp10.
typedef struct rc_float_digits {
    uint8_t digits[RC_FLOAT_MAX_DIGITS];
    int32_t num;
    int32_t exp10;
} rc_float_digits;

// The text as it is built.
typedef struct rc_float_text {
    char chars[RC_FLOAT_MAX_TEXT];
    uint32_t len;
} rc_float_text;

// ---- the bignum ----

// value << shift, where the result fits.
static rc_float_big rc_float_big_from_shifted(uint64_t value, uint32_t shift)
{
    rc_float_big big = {0};
    uint32_t word = shift / 32;
    uint32_t bit = shift % 32;

    // value spans up to three words once shifted
    big.words[word] = (uint32_t)(value << bit);
    if (bit == 0) {
        big.words[word + 1] = (uint32_t)(value >> 32);
    }
    else {
        big.words[word + 1] = (uint32_t)(value >> (32 - bit));
        big.words[word + 2] = (uint32_t)(value >> (64 - bit));
    }

    big.num = word + 3;
    rc_float_big_trim(&big);
    return big;
}


// The two words of big from word up, as one, with zeros past its top.
static uint64_t rc_float_big_pair(const rc_float_big *big, uint32_t word)
{
    uint64_t low = word < big->num ? big->words[word] : 0;
    uint64_t high = word + 1 < big->num ? big->words[word + 1] : 0;

    return low | high << 32;
}


// Clear every bit of big from bits up.
static void rc_float_big_truncate(rc_float_big *big, uint32_t bits)
{
    uint32_t word = bits / 32;

    if (word < big->num) {
        big->words[word] &= (uint32_t)((1ull << (bits % 32)) - 1);
        big->num = word + 1;
        rc_float_big_trim(big);
    }
}


// Take the four bits of big from bits up, which are all it has there, leaving the bits below.
static uint8_t rc_float_big_take_digit(rc_float_big *big, uint32_t bits)
{
    uint64_t pair = rc_float_big_pair(big, bits / 32);

    rc_float_big_truncate(big, bits);
    return (uint8_t)((pair >> (bits % 32)) & 15u);
}


// The next decimal digit of a fraction held as big / 2^bits, leaving the rest of the fraction.
static uint8_t rc_float_big_next_digit(rc_float_big *big, uint32_t bits)
{
    // big < 2^bits, so big x 10 < 2^(bits + 4): the digit is the four bits from bits up
    rc_float_big_multiply_add(big, 10, 0);
    return rc_float_big_take_digit(big, bits);
}

// ---- digits ----

// The integer part's digits, most significant first, at the end of buf; returns how many.
static int32_t rc_float_integer_digits(rc_float_big integer, uint8_t buf[RC_FLOAT_MAX_INT_DIGITS])
{
    int32_t start = RC_FLOAT_MAX_INT_DIGITS;

    // Nine digits per division, then drop the zeros the last one padded with
    while (integer.num > 0) {
        uint32_t chunk = rc_float_big_divide(&integer, 1000000000u);

        for (uint32_t i = 0; i < 9; i++) {
            buf[--start] = (uint8_t)(chunk % 10);
            chunk /= 10;
        }
    }
    while (start < RC_FLOAT_MAX_INT_DIGITS && buf[start] == 0) {
        start++;
    }
    return RC_FLOAT_MAX_INT_DIGITS - start;
}


// A positive, finite value split at its binary point: integer + fraction / 2^fraction_bits.
typedef struct rc_float_split {
    rc_float_big integer;
    rc_float_big fraction;
    uint32_t fraction_bits;
} rc_float_split;


// Split mantissa / 2^fraction_bits at its binary point.
static rc_float_split rc_float_split_fraction(uint64_t mantissa, uint32_t fraction_bits)
{
    if (fraction_bits >= 64) {
        return (rc_float_split) {
            .fraction = rc_float_big_from_shifted(mantissa, 0),
            .fraction_bits = fraction_bits
        };
    }

    return (rc_float_split) {
        .integer = rc_float_big_from_shifted(mantissa >> fraction_bits, 0),
        .fraction = rc_float_big_from_shifted(mantissa & ((1ull << fraction_bits) - 1), 0),
        .fraction_bits = fraction_bits
    };
}


// Split a positive, finite value, which is mantissa x 2^exponent, at its binary point. Subnormals
// have no hidden bit.
static rc_float_split rc_float_split_value(double value)
{
    uint64_t bits = rc_bitcast_f64(value);
    uint32_t biased = (uint32_t)(bits >> 52) & 0x7ffu;
    uint64_t stored = bits & ((1ull << 52) - 1);
    uint64_t mantissa = biased != 0 ? stored | 1ull << 52 : stored;
    int32_t exponent = biased != 0 ? (int32_t)biased - 1075 : -1074;

    if (exponent >= 0) {
        return (rc_float_split) {.integer = rc_float_big_from_shifted(mantissa, (uint32_t)exponent)};
    }

    return rc_float_split_fraction(mantissa, (uint32_t)-exponent);
}


// Add the integer part's num digits, at the end of integer_digits, to out, up to one past the last
// we keep (want). True if any of the digits left over is not zero.
static bool rc_float_take_integer_digits(rc_float_digits *out, const uint8_t *integer_digits, int32_t num, int32_t want)
{
    int32_t taken = 0;
    bool sticky = false;

    while (taken < num && out->num <= want) {
        out->digits[out->num++] = integer_digits[RC_FLOAT_MAX_INT_DIGITS - num + taken];
        taken++;
    }

    while (taken < num) {
        sticky |= integer_digits[RC_FLOAT_MAX_INT_DIGITS - num + taken] != 0;
        taken++;
    }

    return sticky;
}


// Skip a fraction's leading zeros, moving the point down past each, and add its first digit that
// is not zero.
static void rc_float_take_first_significant(rc_float_digits *out, rc_float_big *fraction, uint32_t fraction_bits)
{
    uint8_t digit = rc_float_big_next_digit(fraction, fraction_bits);

    while (digit == 0) {
        out->exp10--;
        digit = rc_float_big_next_digit(fraction, fraction_bits);
    }

    out->digits[out->num++] = digit;
}


// True if the digits round up on the one at want, past the last we keep: past halfway, or halfway
// with anything after it, or a tie that an odd last digit sends to even.
static bool rc_float_rounds_up(const rc_float_digits *digits, int32_t want, bool sticky)
{
    uint8_t next = digits->digits[want];

    if (next != 5) {
        return next > 5;
    }

    if (sticky) {
        return true;
    }

    return want > 0 && (digits->digits[want - 1] & 1u);
}


// All nines rounded up: 1 then zeros, one place further up.
static void rc_float_carry_out(rc_float_digits *digits, int32_t want, bool fixed)
{
    for (int32_t j = want; j > 0; j--) {
        digits->digits[j] = 0;
    }

    digits->digits[0] = 1;
    digits->exp10++;
    if (fixed) {
        digits->num = want + 1;
    }
}


// Add one to the last of the want digits, carrying up through any nines.
static void rc_float_round_up(rc_float_digits *digits, int32_t want, bool fixed)
{
    int32_t i = want - 1;

    while (i >= 0 && digits->digits[i] == 9) {
        digits->digits[i] = 0;
        i--;
    }

    if (i < 0) {
        rc_float_carry_out(digits, want, fixed);
        return;
    }

    digits->digits[i]++;
}


// The digits of a positive, finite value, rounded half to even. With fixed, places digits after the
// point; otherwise places significant digits. A carry out of the top (9.99 to 10.0) moves exp10,
// adding a digit when fixed and keeping the count when not.
static rc_float_digits rc_float_round_digits(double value, bool fixed, int32_t places)
{
    rc_float_split split = rc_float_split_value(value);
    uint8_t integer_digits[RC_FLOAT_MAX_INT_DIGITS] = {0};
    int32_t num_integer = rc_float_integer_digits(split.integer, integer_digits);
    int32_t want = fixed ? num_integer + places : places;
    rc_float_digits out = {.exp10 = num_integer};
    bool sticky = rc_float_take_integer_digits(&out, integer_digits, num_integer, want);

    // Significant digits start at the first that is not zero, which a fraction alone may put
    // hundreds of places down
    if (!fixed && num_integer == 0) {
        rc_float_take_first_significant(&out, &split.fraction, split.fraction_bits);
    }

    // The fraction's digits up to one past the last we keep, then whether anything follows
    while (out.num <= want) {
        out.digits[out.num++] = rc_float_big_next_digit(&split.fraction, split.fraction_bits);
    }

    sticky |= split.fraction.num != 0;

    // Round on the digit past the last
    out.num = want;
    if (rc_float_rounds_up(&out, want, sticky)) {
        rc_float_round_up(&out, want, fixed);
    }

    return out;
}

// ---- text ----

// Add one character.
static void rc_float_text_char(rc_float_text *text, char c)
{
    RC_ASSERT(text->len < RC_FLOAT_MAX_TEXT);
    text->chars[text->len++] = c;
}


// Add digits [start, end) of digits, with zeros past the end of what is there.
static void rc_float_text_digits(rc_float_text *text, const rc_float_digits *digits, int32_t start, int32_t end)
{
    for (int32_t i = start; i < end; i++) {
        rc_float_text_char(text, (char)('0' + (i < digits->num ? digits->digits[i] : 0)));
    }
}


// Remove zeros from the end of a fraction, and its point if nothing is left after it.
static void rc_float_text_strip_zeros(rc_float_text *text, uint32_t point)
{
    while (text->len > point + 1 && text->chars[text->len - 1] == '0') {
        text->len--;
    }

    if (text->len == point + 1) {
        text->len--;
    }
}


// Add the point, zeros zeros, then digits [start, end) of digits, and strip the zeros at the end.
static void rc_float_text_fraction(rc_float_text *text, const rc_float_digits *digits, int32_t zeros, int32_t start, int32_t end)
{
    uint32_t point = text->len;

    rc_float_text_char(text, '.');
    for (int32_t i = 0; i < zeros; i++) {
        rc_float_text_char(text, '0');
    }

    rc_float_text_digits(text, digits, start, end);
    rc_float_text_strip_zeros(text, point);
}


// Add e, the exponent's sign and at least two of its digits.
static void rc_float_text_exponent(rc_float_text *text, int32_t exponent)
{
    uint32_t magnitude = (uint32_t)(exponent < 0 ? -exponent : exponent);

    rc_float_text_char(text, 'e');
    rc_float_text_char(text, exponent < 0 ? '-' : '+');
    if (magnitude >= 100) {
        rc_float_text_char(text, (char)('0' + magnitude / 100));
    }

    rc_float_text_char(text, (char)('0' + magnitude / 10 % 10));
    rc_float_text_char(text, (char)('0' + magnitude % 10));
}


// A positive, finite value in fixed point, with places digits after the point. Zero has no digits
// to find, so it is all zeros.
static void rc_float_text_fixed(rc_float_text *text, double value, int32_t places)
{
    rc_float_digits digits = value != 0.0 ? rc_float_round_digits(value, true, places) : (rc_float_digits) {0};
    int32_t start = digits.exp10 > 0 ? digits.exp10 : 0;

    if (digits.exp10 <= 0) {
        rc_float_text_char(text, '0');
    }
    else {
        rc_float_text_digits(text, &digits, 0, digits.exp10);
    }

    if (places > 0) {
        rc_float_text_char(text, '.');
        rc_float_text_digits(text, &digits, start, start + places);
    }
}


// A positive, finite, non-zero value as "%g" writes it, with precision significant digits.
static void rc_float_text_significant(rc_float_text *text, double value, int32_t precision)
{
    rc_float_digits digits = rc_float_round_digits(value, false, precision);
    int32_t exponent = digits.exp10 - 1;

    // Exponent form for the very big and the very small, as C chooses it
    if (exponent < -4 || exponent >= precision) {
        rc_float_text_digits(text, &digits, 0, 1);
        rc_float_text_fraction(text, &digits, 0, 1, precision);
        rc_float_text_exponent(text, exponent);
        return;
    }

    // Otherwise fixed point, keeping only the significant digits
    if (exponent >= 0) {
        rc_float_text_digits(text, &digits, 0, exponent + 1);
        rc_float_text_fraction(text, &digits, 0, exponent + 1, precision);
        return;
    }

    rc_float_text_char(text, '0');
    rc_float_text_fraction(text, &digits, -exponent - 1, 0, precision);
}


// A positive, finite value as "%g" writes it, with precision significant digits.
static void rc_float_text_shortest(rc_float_text *text, double value, int32_t precision)
{
    if (value == 0.0) {
        rc_float_text_char(text, '0');
        return;
    }

    rc_float_text_significant(text, value, precision);
}


void rc_mstr_append_f64(rc_mstr *s, double value, rc_float_format fmt, rc_arena *arena)
{
    rc_float_text text = {0};
    uint64_t bits = rc_bitcast_f64(value);
    bool negative = (bits >> 63) != 0;
    double magnitude = negative ? -value : value;
    int32_t precision = fmt.precision != 0 ? fmt.precision : RC_FLOAT_DEFAULT_PRECISION;

    // NaN carries no sign we can rely on, so it never shows one
    if (value != value) {
        rc_mstr_append(s, RC_STR("nan"), arena);
        return;
    }

    if (negative) {
        rc_float_text_char(&text, '-');
    }

    if (magnitude > 1.7976931348623157e308) {
        rc_mstr_append(s, rc_str_from_ptr(text.chars, text.len), arena);
        rc_mstr_append(s, RC_STR("inf"), arena);
        return;
    }

    if (fmt.fixed) {
        rc_float_text_fixed(&text, magnitude, precision);
    }
    else {
        rc_float_text_shortest(&text, magnitude, precision);
    }

    rc_mstr_append(s, rc_str_from_ptr(text.chars, text.len), arena);
}


void rc_mstr_append_f32(rc_mstr *s, float value, rc_float_format fmt, rc_arena *arena)
{
    rc_mstr_append_f64(s, (double)value, fmt, arena);
}
