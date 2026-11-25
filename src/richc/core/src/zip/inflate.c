// inflate.c
//
// DEFLATE decoding (RFC 1951), and the zlib wrapper around it (RFC 1950).
//
// A block's code lengths become a canonical Huffman code, as in Mark Adler's puff.c: the number of
// codes of each length, and the symbols in order of length, then value. A table indexed by the
// next RC_INFLATE_FAST_BITS bits of input gives the symbol and length of every code that short,
// so most symbols take one lookup; a longer code is found a bit at a time, the canonical way.
// Where puff keeps a 32 KiB window, the output is an array that only grows, so a back-reference
// copies from earlier in the output itself.
//
// Bits are buffered 64 at a time, so that a whole symbol, with its extra bits and distance, comes
// from one refill. Past the end of the input the buffer fills with zeros, counted so that the
// decoder can tell, after each read that matters, whether it used any: that is
// RC_ZIP_ERROR_TRUNCATED. So decoding always ends, however the input does.

#include "richc/zip/inflate.h"

enum {
    RC_INFLATE_MAX_BITS = 15,         // the longest code
    RC_INFLATE_MAX_LITERALS = 286,    // literal and length codes a dynamic block may have
    RC_INFLATE_MAX_DISTANCES = 30,    // distance codes a block may have
    RC_INFLATE_FIXED_DISTANCES = 32,  // the fixed code's distances, two never used
    RC_INFLATE_FIXED_LITERALS = 288,  // the fixed code's literals and lengths, two never used
    RC_INFLATE_CODE_LENGTHS = 19,     // the alphabet the code lengths are sent in
    RC_INFLATE_LITERALS = 256,        // symbols below this are bytes
    RC_INFLATE_END_OF_BLOCK = 256,
    RC_INFLATE_FIRST_LENGTH = 257,    // the symbol of the first length
    RC_INFLATE_LENGTH_CODES = 29,
    RC_INFLATE_FAST_BITS = 10,        // codes this long or shorter are decoded with one lookup
    RC_INFLATE_FAST_SIZE = 1 << RC_INFLATE_FAST_BITS,
    // The bits of a fast entry's symbol, under its length. The highest symbol, past every alphabet,
    // stands for bits that are no code, and its entry uses as many bits as the longest code
    RC_INFLATE_SYMBOL_BITS = 9,
    RC_INFLATE_BAD_SYMBOL = (1 << RC_INFLATE_SYMBOL_BITS) - 1,
    RC_INFLATE_BAD_ENTRY = RC_INFLATE_MAX_BITS << RC_INFLATE_SYMBOL_BITS | RC_INFLATE_BAD_SYMBOL,
    RC_INFLATE_REFILLED = 56,         // the fewest bits a refill leaves in the buffer
    RC_INFLATE_SYMBOL_NEED = 48,      // the most bits a symbol takes: 15 + 5 extra, distance 15 + 13
    RC_INFLATE_MIN_CAPACITY = 256,    // the output's capacity when the caller gives less
    RC_INFLATE_ADLER_MODULUS = 65521,
    RC_INFLATE_ADLER_RUN = 5552       // the most bytes whose weighted sum stays under 2^32
};

// The input as a stream of bits, least significant first.
typedef struct rc_inflate_bits {
    rc_view_bytes input;
    uint64_t buffer;   // bits taken but not yet used, the next lowest
    uint32_t next;     // the next byte to take bits from, at most input.num
    uint32_t count;    // how many bits buffer holds
    uint32_t zeros;    // how many bits were zeros from past the end of the input, the last taken
} rc_inflate_bits;

// A canonical Huffman code: how many codes there are of each length (counts[0] counts the unused
// symbols), and the symbols in order of length, then value. fast is indexed by the next
// RC_INFLATE_FAST_BITS bits of input, and holds the length and symbol of the code they start
// with, length << RC_INFLATE_SYMBOL_BITS | symbol, or 0 if that code is longer, or no code.
typedef struct rc_inflate_huffman {
    uint32_t longest;  // the longest code's length
    uint16_t counts[RC_INFLATE_MAX_BITS + 1];
    uint16_t symbols[RC_INFLATE_FIXED_LITERALS];
    uint16_t fast[RC_INFLATE_FAST_SIZE];
} rc_inflate_huffman;

// The two codes a compressed block uses.
typedef struct rc_inflate_codes {
    rc_inflate_huffman literals;   // literals, lengths and the end of the block
    rc_inflate_huffman distances;
} rc_inflate_codes;

// The codes a stream is decoded with: too big for the stack, which is small on wasm, so they are
// kept in scratch, and each block builds its own over the last block's.
typedef struct rc_inflate_tables {
    rc_inflate_codes codes;           // the block's literal and distance codes
    rc_inflate_huffman code_lengths;  // the code a dynamic block's code lengths are sent in
} rc_inflate_tables;

// What one symbol of the code-length alphabet stands for: repeat copies of value.
typedef struct rc_inflate_run {
    uint32_t repeat;
    uint8_t value;
    uint8_t error;  // rc_zip_error
} rc_inflate_run;

// Where a compressed block's decoding has got to after a symbol.
typedef struct rc_inflate_step {
    uint8_t error;  // rc_zip_error
    bool end;       // the symbol ended the block
} rc_inflate_step;

// The lengths and distances of back-references: a base, and how many extra bits to add to it
// (RFC 1951 3.2.5)
static const uint16_t rc_inflate_length_base[RC_INFLATE_LENGTH_CODES] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static const uint8_t rc_inflate_length_extra[RC_INFLATE_LENGTH_CODES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static const uint16_t rc_inflate_distance_base[RC_INFLATE_MAX_DISTANCES] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

static const uint8_t rc_inflate_distance_extra[RC_INFLATE_MAX_DISTANCES] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

// ---- bits ----

// The 64-bit little-endian number in the first 8 bytes of bytes, which compilers make one load.
static uint64_t rc_inflate_read_u64(rc_view_bytes bytes)
{
    uint64_t value = 0;

    for (uint32_t i = 0; i < 8; i++) {
        value |= (uint64_t)rc_view_bytes_get(bytes, i) << (8 * i);
    }
    return value;
}


// bits topped up a byte at a time, near the end of the input, with zeros past it.
static rc_inflate_bits rc_inflate_refill_slow(rc_inflate_bits bits)
{
    while (bits.count <= RC_INFLATE_REFILLED) {
        bool has_byte = bits.next < bits.input.num;
        uint64_t byte = has_byte ? rc_view_bytes_get(bits.input, bits.next) : 0;

        bits.buffer |= byte << bits.count;
        bits.count += 8;
        bits.next += has_byte ? 1 : 0;
        bits.zeros += has_byte ? 0 : 8;
    }
    return bits;
}


// bits topped up to at least RC_INFLATE_REFILLED bits. While the input has 8 bytes left, they are
// read at once, and only the whole bytes that fit are counted as taken: the bits above count are
// then the next bytes of input, which the next refill puts in the same place. The refills take
// and return bits by value, so that the buffer can stay in registers in the loops that call them.
static rc_inflate_bits rc_inflate_refill(rc_inflate_bits bits)
{
    uint32_t taken = (63 - bits.count) / 8;

    if (bits.input.num - bits.next < 8) {
        return rc_inflate_refill_slow(bits);
    }

    bits.buffer |= rc_inflate_read_u64(rc_view_bytes_drop_first(bits.input, bits.next)) << bits.count;
    bits.next += taken;
    bits.count += taken * 8;
    return bits;
}


// Make the buffer hold at least n bits (n at most RC_INFLATE_REFILLED).
static void rc_inflate_fill(rc_inflate_bits *bits, uint32_t n)
{
    if (bits->count < n) {
        *bits = rc_inflate_refill(*bits);
    }
}


// Drop n bits from the buffer, which holds them.
static void rc_inflate_drop(rc_inflate_bits *bits, uint32_t n)
{
    bits->buffer >>= n;
    bits->count -= n;
}


// The next n bits (0 to 16) from a buffer that holds them, the first lowest: DEFLATE's integers
// come least significant first.
static uint32_t rc_inflate_take_held(rc_inflate_bits *bits, uint32_t n)
{
    uint32_t value = (uint32_t)(bits->buffer & ((UINT64_C(1) << n) - 1));

    rc_inflate_drop(bits, n);
    return value;
}


// The next n bits (0 to 16).
static uint32_t rc_inflate_take(rc_inflate_bits *bits, uint32_t n)
{
    rc_inflate_fill(bits, n);
    return rc_inflate_take_held(bits, n);
}


// True if the bits used so far ran past the end of the input. The zeros from past it are the last
// bits in the buffer, so some have been used if there are more of them than bits left.
static bool rc_inflate_overran(const rc_inflate_bits *bits)
{
    return bits->zeros > bits->count;
}


// The 16-bit little-endian number at index of bytes.
static uint32_t rc_inflate_read_u16(rc_view_bytes bytes, uint32_t index)
{
    return rc_view_bytes_get(bytes, index) | (uint32_t)rc_view_bytes_get(bytes, index + 1) << 8;
}


// The 32-bit big-endian number at index of bytes.
static uint32_t rc_inflate_read_u32_be(rc_view_bytes bytes, uint32_t index)
{
    return (uint32_t)rc_view_bytes_get(bytes, index) << 24 | (uint32_t)rc_view_bytes_get(bytes, index + 1) << 16 |
        (uint32_t)rc_view_bytes_get(bytes, index + 2) << 8 | rc_view_bytes_get(bytes, index + 3);
}

// ---- Huffman codes ----

// code, length bits long, with its bits in reverse order: a code is sent most significant bit
// first, into a stream that is read least significant first.
static uint32_t rc_inflate_reverse(uint32_t code, uint32_t length)
{
    uint32_t reversed = 0;

    for (uint32_t i = 0; i < length; i++) {
        reversed = reversed << 1 | (code >> i & 1);
    }
    return reversed;
}


// Point the fast entries that start with bits, a code length bits long, at symbol: every entry
// whose low length bits are the code, reversed, whatever the bits above them.
static void rc_inflate_fast_fill(rc_inflate_huffman *code, uint32_t bits, uint32_t length, uint32_t symbol)
{
    uint16_t entry = (uint16_t)(length << RC_INFLATE_SYMBOL_BITS | symbol);

    for (uint32_t index = rc_inflate_reverse(bits, length); index < RC_INFLATE_FAST_SIZE; index += 1u << length) {
        code->fast[index] = entry;
    }
}


// Fill code's fast table from its counts and symbols. Each length's codes are consecutive numbers,
// from first, which follows on from the end of the shorter lengths' codes with a bit more.
static void rc_inflate_fast_make(rc_inflate_huffman *code)
{
    uint32_t first = 0;
    uint32_t index = 0;

    for (uint32_t length = 1; length <= RC_INFLATE_FAST_BITS; length++) {
        uint32_t count = code->counts[length];

        for (uint32_t i = 0; i < count; i++) {
            rc_inflate_fast_fill(code, first + i, length, code->symbols[index + i]);
        }

        index += count;
        first = (first + count) << 1;
    }
}


// Make code the canonical Huffman code with the given length for each symbol, 0 for a symbol not
// used.
static void rc_inflate_huffman_make(rc_inflate_huffman *code, rc_view_bytes lengths)
{
    uint16_t offsets[RC_INFLATE_MAX_BITS + 1] = {0};

    rc_memset(code->counts, 0, sizeof code->counts);
    rc_memset(code->fast, 0, sizeof code->fast);
    for (uint32_t symbol = 0; symbol < lengths.num; symbol++) {
        code->counts[rc_view_bytes_get(lengths, symbol)]++;
    }

    code->longest = RC_INFLATE_MAX_BITS;
    while (code->longest > 0 && code->counts[code->longest] == 0) {
        code->longest--;
    }

    // Each length's symbols start after all the shorter lengths' symbols
    for (uint32_t length = 1; length < RC_INFLATE_MAX_BITS; length++) {
        offsets[length + 1] = (uint16_t)(offsets[length] + code->counts[length]);
    }

    for (uint32_t symbol = 0; symbol < lengths.num; symbol++) {
        uint8_t length = rc_view_bytes_get(lengths, symbol);

        if (length != 0) {
            code->symbols[offsets[length]] = (uint16_t)symbol;
            offsets[length]++;
        }
    }
    rc_inflate_fast_make(code);
}


// The codes code leaves unused: 0 if it uses every one, which is complete, and below 0 if it
// asks for more than there are. Each bit of length doubles the codes there are, and each code of
// that length takes one of them.
static int32_t rc_inflate_space_left(const rc_inflate_huffman *code)
{
    int32_t left = 1;

    for (uint32_t length = 1; length <= RC_INFLATE_MAX_BITS && left >= 0; length++) {
        left = left * 2 - code->counts[length];
    }
    return left;
}


// True if code can decode num symbols' worth of lengths. A complete code can. An incomplete one
// only can as a single one-bit code, every length 0 or 1, as puff allows, so a block can have a
// single distance code.
static bool rc_inflate_is_usable(const rc_inflate_huffman *code, uint32_t num)
{
    int32_t left = rc_inflate_space_left(code);

    if (left <= 0) {
        return left == 0;
    }
    return num == (uint32_t)code->counts[0] + code->counts[1];
}


// The fast entry for a code too long for the fast table, found a bit at a time from next, the
// next RC_INFLATE_MAX_BITS bits of input, or RC_INFLATE_BAD_ENTRY if they are no code. The codes
// of each length are consecutive numbers, from first, which follows on from the end of the
// shorter lengths' codes with a bit more.
static uint32_t rc_inflate_slow_entry(const rc_inflate_huffman *code, uint32_t next)
{
    uint32_t value = 0;
    uint32_t first = 0;
    uint32_t index = 0;

    for (uint32_t length = 1; length <= code->longest; length++) {
        uint32_t count = code->counts[length];

        value |= next >> (length - 1) & 1;
        if (value < first + count) {
            return length << RC_INFLATE_SYMBOL_BITS | code->symbols[index + value - first];
        }

        index += count;
        first = (first + count) << 1;
        value <<= 1;
    }
    return RC_INFLATE_BAD_ENTRY;
}


// The next symbol in code, from a buffer holding at least RC_INFLATE_MAX_BITS bits, or
// RC_INFLATE_BAD_SYMBOL if the bits are no code.
static uint32_t rc_inflate_decode_held(rc_inflate_bits *bits, const rc_inflate_huffman *code)
{
    uint32_t next = (uint32_t)bits->buffer;
    uint32_t entry = code->fast[next & (RC_INFLATE_FAST_SIZE - 1)];
    uint32_t found = entry != 0 ? entry : rc_inflate_slow_entry(code, next);

    rc_inflate_drop(bits, found >> RC_INFLATE_SYMBOL_BITS);
    return found & RC_INFLATE_BAD_SYMBOL;
}


// The next symbol in code, or RC_INFLATE_BAD_SYMBOL if the bits are no code.
static uint32_t rc_inflate_decode(rc_inflate_bits *bits, const rc_inflate_huffman *code)
{
    rc_inflate_fill(bits, RC_INFLATE_MAX_BITS);
    return rc_inflate_decode_held(bits, code);
}

// ---- compressed blocks ----

// Append a literal byte to out.
static uint8_t rc_inflate_literal(rc_array_bytes *out, uint32_t symbol, rc_arena *arena)
{
    if (out->num == UINT32_MAX) {
        return RC_ZIP_ERROR_TOO_LARGE;
    }

    rc_array_bytes_push(out, (uint8_t)symbol, arena);
    return RC_ZIP_ERROR_NONE;
}


// Copy length bytes of bytes from from to to, a byte at a time and in order, so that where the two
// overlap, the copy repeats what it has just written.
static void rc_inflate_repeat(rc_span_bytes bytes, uint32_t from, uint32_t to, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {
        rc_span_bytes_set(bytes, to + i, rc_span_bytes_get(bytes, from + i));
    }
}


// Append length bytes copied from distance back in out. The two may overlap, distance under
// length repeating what the copy has just written.
static uint8_t rc_inflate_copy_back(rc_array_bytes *out, uint32_t length, uint32_t distance, rc_arena *arena)
{
    uint32_t start = out->num;
    uint32_t from = distance <= start ? start - distance : 0;

    if (distance > start) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    if (length > UINT32_MAX - start) {
        return RC_ZIP_ERROR_TOO_LARGE;
    }

    rc_array_bytes_push_n(out, length, arena);
    rc_inflate_repeat(out->span, from, start, length);
    return RC_ZIP_ERROR_NONE;
}


// The distance a distance code stands for: its base, plus the extra bits that follow the code.
static uint32_t rc_inflate_distance(rc_inflate_bits *bits, uint32_t code)
{
    return rc_inflate_distance_base[code] + rc_inflate_take_held(bits, rc_inflate_distance_extra[code]);
}


// A back-reference of length bytes: read its distance, then copy.
static uint8_t rc_inflate_copy(rc_inflate_bits *bits, const rc_inflate_huffman *distances, uint32_t length, rc_array_bytes *out,
                               rc_arena *arena)
{
    uint32_t code = rc_inflate_decode_held(bits, distances);

    if (rc_inflate_overran(bits)) {
        return RC_ZIP_ERROR_TRUNCATED;
    }

    // A bad code is RC_INFLATE_BAD_SYMBOL, which is past the distances too
    if (code >= RC_INFLATE_MAX_DISTANCES) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    return rc_inflate_copy_back(out, length, rc_inflate_distance(bits, code), arena);
}


// The length a length code stands for: its base, plus the extra bits that follow the code.
static uint32_t rc_inflate_length_of(rc_inflate_bits *bits, uint32_t code)
{
    return rc_inflate_length_base[code] + rc_inflate_take_held(bits, rc_inflate_length_extra[code]);
}


// The back-reference that the length symbol starts.
static uint8_t rc_inflate_length(rc_inflate_bits *bits, const rc_inflate_huffman *distances, uint32_t symbol, rc_array_bytes *out,
                                 rc_arena *arena)
{
    uint32_t code = symbol - RC_INFLATE_FIRST_LENGTH;

    // Symbols 286 and 287 are in the fixed code, but stand for no length
    if (code >= RC_INFLATE_LENGTH_CODES) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    return rc_inflate_copy(bits, distances, rc_inflate_length_of(bits, code), out, arena);
}


// The next literal or length symbol in code, after filling the buffer with all the bits the
// symbol can need: the longest code, a length's extra bits, and the longest distance code and its
// extra bits. Refilling once a symbol, rather than for each part, keeps the loop small.
static uint32_t rc_inflate_decode_symbol(rc_inflate_bits *bits, const rc_inflate_huffman *code)
{
    rc_inflate_fill(bits, RC_INFLATE_SYMBOL_NEED);
    return rc_inflate_decode_held(bits, code);
}


// Decode one symbol of a compressed block into out: a literal, the end of the block, or a length
// and its distance.
static rc_inflate_step rc_inflate_next_symbol(rc_inflate_bits *bits, const rc_inflate_codes *codes, rc_array_bytes *out, rc_arena *arena)
{
    uint32_t symbol = rc_inflate_decode_symbol(bits, &codes->literals);

    if (rc_inflate_overran(bits)) {
        return (rc_inflate_step) {.error = RC_ZIP_ERROR_TRUNCATED};
    }

    if (symbol == RC_INFLATE_BAD_SYMBOL) {
        return (rc_inflate_step) {.error = RC_ZIP_ERROR_BAD_DATA};
    }

    if (symbol == RC_INFLATE_END_OF_BLOCK) {
        return (rc_inflate_step) {.end = true};
    }

    if (symbol < RC_INFLATE_LITERALS) {
        return (rc_inflate_step) {.error = rc_inflate_literal(out, symbol, arena)};
    }
    return (rc_inflate_step) {.error = rc_inflate_length(bits, &codes->distances, symbol, out, arena)};
}


// Decode a compressed block's symbols into out, up to the end of the block. The loop works on
// copies of bits and out, written back after it: a byte written to the output could, for all the
// compiler knows, be one of their fields, so through the pointers it would reload them after each.
static uint8_t rc_inflate_symbols(rc_inflate_bits *bits, const rc_inflate_codes *codes, rc_array_bytes *out, rc_arena *arena)
{
    rc_inflate_step step = {0};
    rc_inflate_bits local_bits = *bits;
    rc_array_bytes local_out = *out;

    while (!step.end && step.error == RC_ZIP_ERROR_NONE) {
        step = rc_inflate_next_symbol(&local_bits, codes, &local_out, arena);
    }

    *bits = local_bits;
    *out = local_out;
    return step.error;
}

// ---- fixed codes ----

// The fixed codes (RFC 1951 3.2.6): literals 0 to 143 have 8 bits, 144 to 255 have 9, the end of
// the block and lengths 257 to 279 have 7, and the rest 8. Every distance has 5, the two unused
// ones included, so that the code is complete and a stream using one is caught at 5 bits, as a
// bad distance, rather than read on for a longer code that is not there.
static void rc_inflate_fixed_codes(rc_inflate_codes *codes)
{
    uint8_t literal_lengths[RC_INFLATE_FIXED_LITERALS] = {0};
    uint8_t distance_lengths[RC_INFLATE_FIXED_DISTANCES] = {0};

    for (uint32_t symbol = 0; symbol < RC_INFLATE_FIXED_LITERALS; symbol++) {
        literal_lengths[symbol] = symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
    }

    rc_memset(distance_lengths, 5, sizeof distance_lengths);
    rc_inflate_huffman_make(&codes->literals, rc_view_bytes_from_ptr(literal_lengths, RC_INFLATE_FIXED_LITERALS));
    rc_inflate_huffman_make(&codes->distances, rc_view_bytes_from_ptr(distance_lengths, RC_INFLATE_FIXED_DISTANCES));
}


// A block compressed with the fixed codes.
static uint8_t rc_inflate_fixed_block(rc_inflate_bits *bits, rc_inflate_tables *tables, rc_array_bytes *out, rc_arena *arena)
{
    rc_inflate_fixed_codes(&tables->codes);
    return rc_inflate_symbols(bits, &tables->codes, out, arena);
}

// ---- dynamic codes ----

// The next run of code lengths, read with the code-length code: a length on its own (0 to 15), or
// 16, which repeats the length before 3 to 6 times, or 17 and 18, which give 3 to 10 or 11 to 138
// zeros. index is how many lengths are already in lengths.
static rc_inflate_run rc_inflate_next_run(rc_inflate_bits *bits, const rc_inflate_huffman *code, rc_span_bytes lengths, uint32_t index)
{
    uint32_t symbol = rc_inflate_decode(bits, code);

    if (rc_inflate_overran(bits)) {
        return (rc_inflate_run) {.error = RC_ZIP_ERROR_TRUNCATED};
    }

    if (symbol == RC_INFLATE_BAD_SYMBOL) {
        return (rc_inflate_run) {.error = RC_ZIP_ERROR_BAD_DATA};
    }

    if (symbol < 16) {
        return (rc_inflate_run) {.repeat = 1, .value = (uint8_t)symbol};
    }

    // 16 needs a length before it to repeat
    if (symbol == 16 && index == 0) {
        return (rc_inflate_run) {.error = RC_ZIP_ERROR_BAD_DATA};
    }

    if (symbol == 16) {
        return (rc_inflate_run) {.repeat = 3 + rc_inflate_take(bits, 2), .value = rc_span_bytes_get(lengths, index - 1)};
    }

    if (symbol == 17) {
        return (rc_inflate_run) {.repeat = 3 + rc_inflate_take(bits, 3)};
    }
    return (rc_inflate_run) {.repeat = 11 + rc_inflate_take(bits, 7)};
}


// Read lengths.num code lengths, the literal code's and then the distance code's as one run, with
// the code-length code.
static uint8_t rc_inflate_read_lengths(rc_inflate_bits *bits, const rc_inflate_huffman *code, rc_span_bytes lengths)
{
    uint32_t index = 0;

    while (index < lengths.num) {
        rc_inflate_run run = rc_inflate_next_run(bits, code, lengths, index);

        if (run.error != RC_ZIP_ERROR_NONE) {
            return run.error;
        }

        // A run may not spill past the lengths
        if (run.repeat > lengths.num - index) {
            return RC_ZIP_ERROR_BAD_DATA;
        }

        for (uint32_t i = 0; i < run.repeat; i++) {
            rc_span_bytes_set(lengths, index + i, run.value);
        }
        index += run.repeat;
    }
    return rc_inflate_overran(bits) ? RC_ZIP_ERROR_TRUNCATED : RC_ZIP_ERROR_NONE;
}


// Make codes from the lengths read: an error if either cannot decode.
static uint8_t rc_inflate_dynamic_build(rc_inflate_codes *codes, rc_view_bytes literal_lengths, rc_view_bytes distance_lengths)
{
    rc_inflate_huffman_make(&codes->literals, literal_lengths);
    rc_inflate_huffman_make(&codes->distances, distance_lengths);
    if (!rc_inflate_is_usable(&codes->literals, literal_lengths.num)) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    if (!rc_inflate_is_usable(&codes->distances, distance_lengths.num)) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    return RC_ZIP_ERROR_NONE;
}


// Read num_literals and num_distances code lengths with the code-length code, and build the
// block's codes from them.
static uint8_t rc_inflate_dynamic_lengths(rc_inflate_bits *bits, rc_inflate_tables *tables, uint32_t num_literals, uint32_t num_distances)
{
    uint8_t lengths[RC_INFLATE_MAX_LITERALS + RC_INFLATE_MAX_DISTANCES] = {0};
    rc_span_bytes all = rc_span_bytes_from_ptr(lengths, num_literals + num_distances);
    uint8_t error = rc_inflate_read_lengths(bits, &tables->code_lengths, all);
    rc_view_bytes literal_lengths = rc_span_bytes_first(all, num_literals).view;
    rc_view_bytes distance_lengths = rc_span_bytes_drop_first(all, num_literals).view;

    if (error != RC_ZIP_ERROR_NONE) {
        return error;
    }

    // A block with no end-of-block code could never end
    if (lengths[RC_INFLATE_END_OF_BLOCK] == 0) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    return rc_inflate_dynamic_build(&tables->codes, literal_lengths, distance_lengths);
}


// Make code the code-length code, from num 3-bit lengths sent in the order RFC 1951 3.2.7 gives,
// which puts the lengths most likely to be zero last so they can be left off.
static void rc_inflate_code_length_code(rc_inflate_bits *bits, rc_inflate_huffman *code, uint32_t num)
{
    static const uint8_t order[RC_INFLATE_CODE_LENGTHS] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t lengths[RC_INFLATE_CODE_LENGTHS] = {0};

    for (uint32_t i = 0; i < num; i++) {
        lengths[order[i]] = (uint8_t)rc_inflate_take(bits, 3);
    }
    rc_inflate_huffman_make(code, rc_view_bytes_from_ptr(lengths, RC_INFLATE_CODE_LENGTHS));
}


// A dynamic block's codes, after the counts in its header: the code-length code, which must be
// complete, then the code lengths sent with it.
static uint8_t rc_inflate_dynamic_codes_of(rc_inflate_bits *bits, rc_inflate_tables *tables, uint32_t num_literals, uint32_t num_distances,
                                           uint32_t num_code_lengths)
{
    rc_inflate_code_length_code(bits, &tables->code_lengths, num_code_lengths);
    if (rc_inflate_overran(bits)) {
        return RC_ZIP_ERROR_TRUNCATED;
    }

    if (rc_inflate_space_left(&tables->code_lengths) != 0) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    return rc_inflate_dynamic_lengths(bits, tables, num_literals, num_distances);
}


// A dynamic block's codes, read from its header (RFC 1951 3.2.7): how many literal, distance and
// code-length codes it has, then the codes.
static uint8_t rc_inflate_dynamic_codes(rc_inflate_bits *bits, rc_inflate_tables *tables)
{
    uint32_t num_literals = rc_inflate_take(bits, 5) + 257;
    uint32_t num_distances = rc_inflate_take(bits, 5) + 1;
    uint32_t num_code_lengths = rc_inflate_take(bits, 4) + 4;

    if (rc_inflate_overran(bits)) {
        return RC_ZIP_ERROR_TRUNCATED;
    }

    if (num_literals > RC_INFLATE_MAX_LITERALS) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    if (num_distances > RC_INFLATE_MAX_DISTANCES) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    return rc_inflate_dynamic_codes_of(bits, tables, num_literals, num_distances, num_code_lengths);
}


// A block compressed with codes of its own.
static uint8_t rc_inflate_dynamic_block(rc_inflate_bits *bits, rc_inflate_tables *tables, rc_array_bytes *out, rc_arena *arena)
{
    uint8_t error = rc_inflate_dynamic_codes(bits, tables);

    if (error != RC_ZIP_ERROR_NONE) {
        return error;
    }
    return rc_inflate_symbols(bits, &tables->codes, out, arena);
}

// ---- stored blocks ----

// The index of the next whole byte of input: the bits in the buffer that came from the input are
// the last of the bytes taken, and any part of a byte among them is dropped.
static uint32_t rc_inflate_next_byte(const rc_inflate_bits *bits)
{
    return bits->next - (bits->count - bits->zeros) / 8;
}


// A stored block: from the next whole byte, a 16-bit length, its complement as a check, then that
// many bytes as they are. The bits buffered are dropped, and reading starts again after them.
static uint8_t rc_inflate_stored_block(rc_inflate_bits *bits, rc_array_bytes *out, rc_arena *arena)
{
    uint32_t start = rc_inflate_next_byte(bits);
    bool has_header = bits->input.num - start >= 4;
    uint32_t len = has_header ? rc_inflate_read_u16(bits->input, start) : 0;
    uint32_t check = has_header ? rc_inflate_read_u16(bits->input, start + 2) : 0;
    bool has_data = has_header && bits->input.num - start - 4 >= len;

    if (!has_header) {
        return RC_ZIP_ERROR_TRUNCATED;
    }

    if ((len ^ 0xffffu) != check) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    if (!has_data) {
        return RC_ZIP_ERROR_TRUNCATED;
    }

    if (len > UINT32_MAX - out->num) {
        return RC_ZIP_ERROR_TOO_LARGE;
    }

    rc_array_bytes_append(out, rc_view_bytes_subview(bits->input, start + 4, start + 4 + len), arena);
    *bits = (rc_inflate_bits) {.input = bits->input, .next = start + 4 + len};
    return RC_ZIP_ERROR_NONE;
}

// ---- streams ----

// One block, after its final-block bit: its type, then the block.
static uint8_t rc_inflate_block(rc_inflate_bits *bits, rc_inflate_tables *tables, rc_array_bytes *out, rc_arena *arena)
{
    uint32_t type = rc_inflate_take(bits, 2);

    if (rc_inflate_overran(bits)) {
        return RC_ZIP_ERROR_TRUNCATED;
    }

    switch (type) {
    case 0:
        return rc_inflate_stored_block(bits, out, arena);
    case 1:
        return rc_inflate_fixed_block(bits, tables, out, arena);
    case 2:
        return rc_inflate_dynamic_block(bits, tables, out, arena);
    default:
        return RC_ZIP_ERROR_BAD_DATA;
    }
}


// A raw DEFLATE stream, decoded into out a block at a time, to the block marked final.
static uint8_t rc_inflate_stream(rc_view_bytes compressed, rc_inflate_tables *tables, rc_array_bytes *out, rc_arena *arena)
{
    rc_inflate_bits bits = {.input = compressed};
    bool final = false;
    uint8_t error = RC_ZIP_ERROR_NONE;

    while (!final && error == RC_ZIP_ERROR_NONE) {
        final = rc_inflate_take(&bits, 1) != 0;
        error = rc_inflate_block(&bits, tables, out, arena);
    }
    return error;
}


// The two sums of an Adler-32 (RFC 1950): of the bytes, plus 1, and of the first sum after each
// byte, both modulo 65521.
typedef struct rc_inflate_adler {
    uint32_t a;
    uint32_t b;
} rc_inflate_adler;


// sums, carried on over a run of at most RC_INFLATE_ADLER_RUN bytes. a gains each byte, and b
// gains a after each byte, so b gains a once for every byte, and each byte once for every byte
// from it to the end of the run: two plain sums, which compilers can vectorise.
static rc_inflate_adler rc_inflate_adler_run(rc_inflate_adler sums, rc_view_bytes run)
{
    uint32_t total = 0;
    uint32_t weighted = 0;

    for (uint32_t i = 0; i < run.num; i++) {
        uint32_t byte = rc_view_bytes_get(run, i);

        total += byte;
        weighted += (run.num - i) * byte;
    }

    return (rc_inflate_adler) {
        .a = (sums.a + total) % RC_INFLATE_ADLER_MODULUS,
        .b = (uint32_t)((sums.b + (uint64_t)run.num * sums.a + weighted) % RC_INFLATE_ADLER_MODULUS)
    };
}


// The Adler-32 of data, a run at a time.
static uint32_t rc_inflate_adler32(rc_view_bytes data)
{
    rc_inflate_adler sums = {.a = 1};
    rc_view_bytes rest = data;

    while (rest.num > 0) {
        uint32_t run = rest.num < RC_INFLATE_ADLER_RUN ? rest.num : RC_INFLATE_ADLER_RUN;

        sums = rc_inflate_adler_run(sums, rc_view_bytes_first(rest, run));
        rest = rc_view_bytes_drop_first(rest, run);
    }
    return sums.b << 16 | sums.a;
}


// A raw DEFLATE stream decoded into a new array. The codes come from scratch first, before the
// output, so that if either needs a fresh chunk, the output's is the newer, which can grow in
// place. On an error, everything the output took from arena is given back, any space it grew out
// of along with it.
static rc_zip_inflate_result rc_inflate_raw(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch)
{
    rc_inflate_tables *tables = rc_arena_alloc(&scratch, (uint32_t)sizeof *tables);
    uint32_t capacity = minimum_capacity > RC_INFLATE_MIN_CAPACITY ? minimum_capacity : RC_INFLATE_MIN_CAPACITY;
    rc_arena_mark mark = rc_arena_save(arena);
    rc_array_bytes out = rc_array_bytes_make(capacity, arena);
    uint8_t error = rc_inflate_stream(compressed, tables, &out, arena);

    if (error != RC_ZIP_ERROR_NONE) {
        rc_arena_restore(arena, mark);
        return (rc_zip_inflate_result) {.error = error};
    }
    return (rc_zip_inflate_result) {.data = out};
}


rc_zip_inflate_result rc_zip_inflate(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch)
{
    RC_ASSERT(!rc_arena_is_same(arena, &scratch));
    return rc_inflate_raw(compressed, minimum_capacity, arena, scratch);
}

// ---- zlib ----

// True if a zlib stream's first two bytes are a header we take: the method is DEFLATE, the 16 bits
// are a multiple of 31, as their check, and no preset dictionary is wanted.
static bool rc_inflate_is_zlib_header(uint32_t method, uint32_t flags)
{
    if ((method & 0x0fu) != 8) {
        return false;
    }

    if ((method << 8 | flags) % 31 != 0) {
        return false;
    }
    return (flags & 0x20u) == 0;
}


// A zlib stream's DEFLATE, between the header and the Adler-32, whose checksum must match.
static rc_zip_inflate_result rc_inflate_zlib_body(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch)
{
    rc_arena_mark mark = rc_arena_save(arena);
    rc_view_bytes body = rc_view_bytes_subview(compressed, 2, compressed.num - 4);
    rc_zip_inflate_result result = rc_inflate_raw(body, minimum_capacity, arena, scratch);
    uint32_t expected = rc_inflate_read_u32_be(compressed, compressed.num - 4);

    if (result.error != RC_ZIP_ERROR_NONE) {
        return result;
    }

    if (rc_inflate_adler32(result.data.view) != expected) {
        rc_arena_restore(arena, mark);
        return (rc_zip_inflate_result) {.error = RC_ZIP_ERROR_CHECKSUM};
    }
    return result;
}


// A zlib stream at least big enough for its header and checksum.
static rc_zip_inflate_result rc_inflate_zlib(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch)
{
    uint32_t method = rc_view_bytes_get(compressed, 0);
    uint32_t flags = rc_view_bytes_get(compressed, 1);

    if (!rc_inflate_is_zlib_header(method, flags)) {
        return (rc_zip_inflate_result) {.error = RC_ZIP_ERROR_BAD_HEADER};
    }
    return rc_inflate_zlib_body(compressed, minimum_capacity, arena, scratch);
}


rc_zip_inflate_result rc_zip_inflate_zlib(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch)
{
    RC_ASSERT(!rc_arena_is_same(arena, &scratch));

    // Two header bytes and a 4-byte checksum, at the least
    if (compressed.num < 6) {
        return (rc_zip_inflate_result) {.error = RC_ZIP_ERROR_BAD_HEADER};
    }
    return rc_inflate_zlib(compressed, minimum_capacity, arena, scratch);
}
