// mstr_float.c
//
// rc_mstr_append_f64 and _f32: floating point as decimal text, the same text C's "%.*g" and "%.*f"
// give, with no libc.
//
// A double is m x 2^e exactly, so its decimal expansion is finite, and we work it out exactly with a
// little bignum: the integer part by dividing by 10^9 again and again, the fraction by multiplying
// by 10 and taking the bits that cross the binary point. Rounding sees the exact value, so a tie
// rounds to even, as printf does. It is slow next to Ryu, but small and plainly right, which will
// do until number formatting turns up somewhere hot.

#include "richc/mstr.h"

#include "richc/ops.h"

enum {
    // Enough 32-bit words for the biggest integer part (1024 bits) and the longest fraction (1074
    // bits) with room to multiply it by 10
    RC_FLOAT_BIG_WORDS = 36,

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

// An unsigned integer of up to RC_FLOAT_BIG_WORDS words, least significant first. num counts the
// words in use, so zero has none.
typedef struct rc_float_big {
    uint32_t words[RC_FLOAT_BIG_WORDS];
    uint32_t num;
} rc_float_big;

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

// Drop zero words from the top.
static void rc_float_big_trim(rc_float_big *big)
{
    while (big->num > 0 && big->words[big->num - 1] == 0) {
        big->num--;
    }
}


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


// Divide by divisor in place, returning the remainder.
static uint32_t rc_float_big_divide(rc_float_big *big, uint32_t divisor)
{
    uint64_t remainder = 0;

    for (uint32_t i = big->num; i-- > 0;) {
        uint64_t part = (remainder << 32) | big->words[i];

        big->words[i] = (uint32_t)(part / divisor);
        remainder = part % divisor;
    }
    rc_float_big_trim(big);
    return (uint32_t)remainder;
}


// Multiply by factor in place.
static void rc_float_big_multiply(rc_float_big *big, uint32_t factor)
{
    uint64_t carry = 0;

    for (uint32_t i = 0; i < big->num; i++) {
        uint64_t part = (uint64_t)big->words[i] * factor + carry;

        big->words[i] = (uint32_t)part;
        carry = part >> 32;
    }
    if (carry != 0) {
        RC_ASSERT(big->num < RC_FLOAT_BIG_WORDS);
        big->words[big->num++] = (uint32_t)carry;
    }
}


// The next decimal digit of a fraction held as big / 2^bits, leaving the rest of the fraction.
static uint8_t rc_float_big_next_digit(rc_float_big *big, uint32_t bits)
{
    uint32_t word = bits / 32;
    uint32_t bit = bits % 32;
    uint64_t pair;

    // big < 2^bits, so big x 10 < 2^(bits + 4): the digit is the four bits from bits up
    rc_float_big_multiply(big, 10);
    pair = word < big->num ? big->words[word] : 0;
    if (word + 1 < big->num) {
        pair |= (uint64_t)big->words[word + 1] << 32;
    }
    if (word < big->num) {
        big->words[word] &= (uint32_t)((1ull << bit) - 1);
        big->num = word + 1;
        rc_float_big_trim(big);
    }
    return (uint8_t)((pair >> bit) & 15u);
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


// The digits of a positive, finite value, rounded half to even. With fixed, places digits after the
// point; otherwise places significant digits. A carry out of the top (9.99 to 10.0) moves exp10,
// adding a digit when fixed and keeping the count when not.
static rc_float_digits rc_float_round_digits(double value, bool fixed, int32_t places)
{
    rc_float_digits out = {0};
    uint8_t integer_digits[RC_FLOAT_MAX_INT_DIGITS];
    uint64_t bits = rc_bitcast_f64(value);
    uint32_t biased = (uint32_t)(bits >> 52) & 0x7ffu;
    uint64_t mantissa = bits & ((1ull << 52) - 1);
    int32_t exponent = -1074;
    rc_float_big integer = {0};
    rc_float_big fraction = {0};
    uint32_t fraction_bits = 0;
    int32_t num_integer;
    int32_t want;
    int32_t taken = 0;
    bool sticky = false;
    bool up;

    // value is mantissa x 2^exponent; subnormals have no hidden bit
    if (biased != 0) {
        mantissa |= 1ull << 52;
        exponent = (int32_t)biased - 1075;
    }

    // Split it at the binary point
    if (exponent >= 0) {
        integer = rc_float_big_from_shifted(mantissa, (uint32_t)exponent);
    }
    else {
        fraction_bits = (uint32_t)-exponent;
        if (fraction_bits < 64) {
            integer = rc_float_big_from_shifted(mantissa >> fraction_bits, 0);
            fraction = rc_float_big_from_shifted(mantissa & ((1ull << fraction_bits) - 1), 0);
        }
        else {
            fraction = rc_float_big_from_shifted(mantissa, 0);
        }
    }

    // The integer part's digits come first; the point sits after them
    num_integer = rc_float_integer_digits(integer, integer_digits);
    out.exp10 = num_integer;
    want = fixed ? num_integer + places : places;
    while (taken < num_integer && out.num <= want) {
        out.digits[out.num++] = integer_digits[RC_FLOAT_MAX_INT_DIGITS - num_integer + taken];
        taken++;
    }
    while (taken < num_integer) {
        sticky |= integer_digits[RC_FLOAT_MAX_INT_DIGITS - num_integer + taken] != 0;
        taken++;
    }

    // Significant digits start at the first that is not zero, which a fraction alone may put
    // hundreds of places down
    if (!fixed && num_integer == 0) {
        uint8_t digit = rc_float_big_next_digit(&fraction, fraction_bits);

        while (digit == 0) {
            out.exp10--;
            digit = rc_float_big_next_digit(&fraction, fraction_bits);
        }
        out.digits[out.num++] = digit;
    }

    // The fraction's digits up to one past the last we keep, then whether anything follows
    while (out.num <= want) {
        out.digits[out.num++] = rc_float_big_next_digit(&fraction, fraction_bits);
    }
    sticky |= fraction.num != 0;

    // Round on the digit past the last, with ties going to an even last digit
    up = out.digits[want] > 5
        || (out.digits[want] == 5 && (sticky || (want > 0 && (out.digits[want - 1] & 1u))));
    out.num = want;
    if (up) {
        int32_t i = want - 1;

        while (i >= 0 && out.digits[i] == 9) {
            out.digits[i] = 0;
            i--;
        }
        if (i >= 0) {
            out.digits[i]++;
        }
        else {
            // All nines: 1 then zeros, one place further up
            for (int32_t j = want; j > 0; j--) {
                out.digits[j] = 0;
            }
            out.digits[0] = 1;
            out.exp10++;
            if (fixed) {
                out.num = want + 1;
            }
        }
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
static void rc_float_text_digits(rc_float_text *text, const rc_float_digits *digits, int32_t start,
    int32_t end)
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


// A positive, finite value in fixed point, with places digits after the point.
static void rc_float_text_fixed(rc_float_text *text, double value, int32_t places)
{
    rc_float_digits digits = {0};

    // Zero has no digits to find, so it is all zeros
    if (value != 0.0) {
        digits = rc_float_round_digits(value, true, places);
    }
    if (digits.exp10 <= 0) {
        rc_float_text_char(text, '0');
    }
    else {
        rc_float_text_digits(text, &digits, 0, digits.exp10);
    }
    if (places > 0) {
        int32_t start = digits.exp10 > 0 ? digits.exp10 : 0;

        rc_float_text_char(text, '.');
        rc_float_text_digits(text, &digits, start, start + places);
    }
}


// A positive, finite value as "%g" writes it, with precision significant digits.
static void rc_float_text_shortest(rc_float_text *text, double value, int32_t precision)
{
    rc_float_digits digits;
    int32_t exponent;
    uint32_t point;

    if (value == 0.0) {
        rc_float_text_char(text, '0');
        return;
    }
    digits = rc_float_round_digits(value, false, precision);
    exponent = digits.exp10 - 1;

    // Exponent form for the very big and the very small, as C chooses it
    if (exponent < -4 || exponent >= precision) {
        uint32_t magnitude = (uint32_t)(exponent < 0 ? -exponent : exponent);

        rc_float_text_digits(text, &digits, 0, 1);
        point = text->len;
        rc_float_text_char(text, '.');
        rc_float_text_digits(text, &digits, 1, precision);
        rc_float_text_strip_zeros(text, point);
        rc_float_text_char(text, 'e');
        rc_float_text_char(text, exponent < 0 ? '-' : '+');
        if (magnitude >= 100) {
            rc_float_text_char(text, (char)('0' + magnitude / 100));
        }
        rc_float_text_char(text, (char)('0' + magnitude / 10 % 10));
        rc_float_text_char(text, (char)('0' + magnitude % 10));
        return;
    }

    // Otherwise fixed point, keeping only the significant digits
    if (exponent >= 0) {
        rc_float_text_digits(text, &digits, 0, exponent + 1);
        point = text->len;
        rc_float_text_char(text, '.');
        rc_float_text_digits(text, &digits, exponent + 1, precision);
    }
    else {
        rc_float_text_char(text, '0');
        point = text->len;
        rc_float_text_char(text, '.');
        for (int32_t i = exponent + 1; i < 0; i++) {
            rc_float_text_char(text, '0');
        }
        rc_float_text_digits(text, &digits, 0, precision);
    }
    rc_float_text_strip_zeros(text, point);
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
