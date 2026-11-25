// test_zip_inflate.c
//
// Tests for DEFLATE and zlib decompression. The fixtures in data/zip/ were made with zlib, and the
// hand-built streams below were checked against it: zlib decodes the good ones the same, and
// rejects the bad ones for the same reasons.

#include "richc/zip/inflate.h"

#include "richc/arena.h"
#include "richc/file.h"
#include "richc/test.h"

// Most tests run against a fresh default arena, with another for scratch.
RC_TEST_GROUP_DATA(zip_inflate) {
    rc_arena a;
    rc_arena scratch;
};


RC_TEST_GROUP_INIT(zip_inflate, fix)
{
    fix->a = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(zip_inflate, fix)
{
    rc_arena_release(&fix->scratch);
    rc_arena_release(&fix->a);
}


// A fixture's bytes, or an empty view if it cannot be read, which fails the check that follows.
static rc_view_bytes load(const char *path, rc_arena *arena)
{
    rc_file_load_binary_result loaded = rc_file_load_binary(rc_str_from_cstr(path), 0, arena);

    return loaded.error == RC_FILE_ERROR_NONE ? loaded.contents.view : (rc_view_bytes) {0};
}


// True if decompressing compressed, raw or in zlib, gives exactly the expected fixture.
static bool inflates_to(rc_zip_inflate_result result, const char *expected_path, rc_arena *arena)
{
    rc_view_bytes expected = load(expected_path, arena);

    if (result.error != RC_ZIP_ERROR_NONE) {
        return false;
    }
    return rc_str_is_equal(rc_view_bytes_as_str(result.data.view), rc_view_bytes_as_str(expected));
}


// The result of decompressing a hand-built raw stream.
static rc_zip_inflate_result inflate_bytes(const uint8_t *bytes, uint32_t num, rc_arena *arena, rc_arena scratch)
{
    return rc_zip_inflate(rc_view_bytes_from_ptr(bytes, num), 0, arena, scratch);
}

// ---- stored blocks ----

RC_TEST_STEP(zip_inflate, stored_block, fix)
{
    // A final stored block (BFINAL 1, BTYPE 0), then LEN 5, NLEN, and the bytes
    static const uint8_t stream[] = {0x01, 0x05, 0x00, 0xfa, 0xff, 'h', 'e', 'l', 'l', 'o'};
    rc_zip_inflate_result result = inflate_bytes(stream, RC_COUNT(stream), &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_NONE);
    RC_CHECK(rc_view_bytes_as_str(result.data.view), ==, RC_STR("hello"));
}


RC_TEST_STEP(zip_inflate, two_blocks, fix)
{
    // A stored block that is not final, then one that is
    static const uint8_t stream[] = {0x00, 0x02, 0x00, 0xfd, 0xff, 'a', 'b', 0x01, 0x01, 0x00, 0xfe, 0xff, 'c'};
    rc_zip_inflate_result result = inflate_bytes(stream, RC_COUNT(stream), &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_NONE);
    RC_CHECK(rc_view_bytes_as_str(result.data.view), ==, RC_STR("abc"));
}


RC_TEST_STEP(zip_inflate, stored_check_mismatch, fix)
{
    // NLEN is not LEN's complement
    static const uint8_t stream[] = {0x01, 0x05, 0x00, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o'};
    rc_zip_inflate_result result = inflate_bytes(stream, RC_COUNT(stream), &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_BAD_DATA);
    RC_CHECK_FALSE(rc_array_bytes_is_valid(&result.data));
}


RC_TEST_STEP(zip_inflate, stored_truncated, fix)
{
    // Five bytes promised and three given, then a header cut short
    static const uint8_t short_data[] = {0x01, 0x05, 0x00, 0xfa, 0xff, 'h', 'e', 'l'};
    static const uint8_t short_header[] = {0x01, 0x05, 0x00};

    RC_CHECK(inflate_bytes(short_data, RC_COUNT(short_data), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_TRUNCATED);
    RC_CHECK(inflate_bytes(short_header, RC_COUNT(short_header), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_TRUNCATED);
}


RC_TEST_STEP(zip_inflate, empty_input, fix)
{
    rc_zip_inflate_result result = rc_zip_inflate((rc_view_bytes) {0}, 0, &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_TRUNCATED);
}


RC_TEST_STEP(zip_inflate, reserved_block_type, fix)
{
    // BFINAL 1, BTYPE 3
    static const uint8_t stream[] = {0x07};

    RC_CHECK(inflate_bytes(stream, RC_COUNT(stream), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
}

// ---- fixed codes ----

RC_TEST_STEP(zip_inflate, overlapping_copy, fix)
{
    // 'a', then 4 bytes from 1 back, each copying the one just written
    static const uint8_t stream[] = {0x4b, 0x04, 0x01, 0x00};
    rc_zip_inflate_result result = inflate_bytes(stream, RC_COUNT(stream), &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_NONE);
    RC_CHECK(rc_view_bytes_as_str(result.data.view), ==, RC_STR("aaaaa"));
}


RC_TEST_STEP(zip_inflate, bad_fixed_blocks, fix)
{
    // A copy from before the start; length symbol 286, which the fixed code has but means nothing;
    // and distance code 30, likewise
    static const uint8_t far_back[] = {0x03, 0x02, 0x00};
    static const uint8_t symbol_286[] = {0x1b, 0x03, 0x00};
    static const uint8_t distance_30[] = {0x4b, 0x04, 0x3e, 0x00};

    RC_CHECK(inflate_bytes(far_back, RC_COUNT(far_back), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
    RC_CHECK(inflate_bytes(symbol_286, RC_COUNT(symbol_286), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
    RC_CHECK(inflate_bytes(distance_30, RC_COUNT(distance_30), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
}

// ---- dynamic codes ----

RC_TEST_STEP(zip_inflate, bad_dynamic_headers, fix)
{
    // A repeat (16) with no length before it; a code-length code that uses only half its space;
    // and 287 literal codes, one more than there can be
    static const uint8_t repeat_first[] = {0x05, 0x00, 0x02, 0x24};
    static const uint8_t incomplete[] = {0x05, 0x00, 0x02, 0x00, 0x00};
    static const uint8_t too_many[] = {0xf5, 0x00, 0x00, 0x00, 0x00};

    RC_CHECK(inflate_bytes(repeat_first, RC_COUNT(repeat_first), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
    RC_CHECK(inflate_bytes(incomplete, RC_COUNT(incomplete), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
    RC_CHECK(inflate_bytes(too_many, RC_COUNT(too_many), &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_DATA);
}

// ---- fixtures ----

RC_TEST_STEP(zip_inflate, raw_streams, fix)
{
    // Dynamic codes, fixed codes, long overlapping runs, and 100 KB in many blocks
    rc_zip_inflate_result dynamic = rc_zip_inflate(load("data/zip/lorem.deflate", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result fixed = rc_zip_inflate(load("data/zip/lorem.fixed.deflate", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result repeat = rc_zip_inflate(load("data/zip/repeat.deflate", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result large = rc_zip_inflate(load("data/zip/large.deflate", &fix->a), 0, &fix->a, fix->scratch);

    RC_CHECK_TRUE(inflates_to(dynamic, "data/zip/lorem.txt", &fix->a));
    RC_CHECK_TRUE(inflates_to(fixed, "data/zip/lorem.txt", &fix->a));
    RC_CHECK_TRUE(inflates_to(repeat, "data/zip/repeat.txt", &fix->a));
    RC_CHECK_TRUE(inflates_to(large, "data/zip/large.txt", &fix->a));
}


RC_TEST_STEP(zip_inflate, zlib_streams, fix)
{
    // Text, every byte value as a literal, and 70 KB of random bytes in several stored blocks
    rc_zip_inflate_result lorem = rc_zip_inflate_zlib(load("data/zip/lorem.zlib", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result large = rc_zip_inflate_zlib(load("data/zip/large.zlib", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result alphabet = rc_zip_inflate_zlib(load("data/zip/alphabet.zlib", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result random = rc_zip_inflate_zlib(load("data/zip/random.zlib", &fix->a), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result empty = rc_zip_inflate_zlib(load("data/zip/empty.zlib", &fix->a), 0, &fix->a, fix->scratch);

    RC_CHECK_TRUE(inflates_to(lorem, "data/zip/lorem.txt", &fix->a));
    RC_CHECK_TRUE(inflates_to(large, "data/zip/large.txt", &fix->a));
    RC_CHECK_TRUE(inflates_to(alphabet, "data/zip/alphabet.bin", &fix->a));
    RC_CHECK_TRUE(inflates_to(random, "data/zip/random.bin", &fix->a));
    RC_CHECK(empty.error, ==, RC_ZIP_ERROR_NONE);
    RC_CHECK(empty.data.num, ==, 0u);
}


RC_TEST_STEP(zip_inflate, minimum_capacity, fix)
{
    // The exact size up front: the output fills it without growing
    rc_view_bytes expected = load("data/zip/lorem.txt", &fix->a);
    rc_zip_inflate_result result = rc_zip_inflate(load("data/zip/lorem.deflate", &fix->a), expected.num, &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_NONE);
    RC_CHECK(result.data.num, ==, expected.num);
    RC_CHECK(result.data.cap, ==, expected.num);
}

// ---- zlib wrapper ----

RC_TEST_STEP(zip_inflate, zlib_bad_headers, fix)
{
    // Not DEFLATE (method 7, check kept good), a failed header check, and a preset dictionary
    rc_array_bytes method = rc_array_bytes_from_view(load("data/zip/lorem.zlib", &fix->a), 0, &fix->a);
    rc_array_bytes check = rc_array_bytes_from_view(method.view, 0, &fix->a);
    rc_array_bytes dictionary = rc_array_bytes_from_view(method.view, 0, &fix->a);
    static const uint8_t too_short_bytes[] = {0x78, 0x9c, 0x03, 0x00, 0x00};
    rc_view_bytes too_short = rc_view_bytes_from_ptr(too_short_bytes, RC_COUNT(too_short_bytes));

    rc_array_bytes_set(&method, 0, 0x77);
    rc_array_bytes_set(&method, 1, 0x09);
    rc_array_bytes_set(&check, 1, (uint8_t)(rc_array_bytes_get(&check, 1) + 1));
    rc_array_bytes_set(&dictionary, 0, 0x78);
    rc_array_bytes_set(&dictionary, 1, 0xbb);
    RC_CHECK(rc_zip_inflate_zlib(method.view, 0, &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_HEADER);
    RC_CHECK(rc_zip_inflate_zlib(check.view, 0, &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_HEADER);
    RC_CHECK(rc_zip_inflate_zlib(dictionary.view, 0, &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_HEADER);
    RC_CHECK(rc_zip_inflate_zlib(too_short, 0, &fix->a, fix->scratch).error, ==, RC_ZIP_ERROR_BAD_HEADER);
}


// A copy of bytes with every bit of its last byte flipped.
static rc_array_bytes with_last_byte_flipped(rc_view_bytes bytes, rc_arena *arena)
{
    rc_array_bytes copy = rc_array_bytes_from_view(bytes, 0, arena);
    uint32_t last = copy.num - 1;

    rc_array_bytes_set(&copy, last, (uint8_t)(rc_array_bytes_get(&copy, last) ^ 0xff));
    return copy;
}


RC_TEST_STEP(zip_inflate, zlib_bad_checksum, fix)
{
    // The last byte is the end of the Adler-32
    rc_array_bytes damaged = with_last_byte_flipped(load("data/zip/lorem.zlib", &fix->a), &fix->a);
    rc_zip_inflate_result result = rc_zip_inflate_zlib(damaged.view, 0, &fix->a, fix->scratch);

    RC_CHECK(result.error, ==, RC_ZIP_ERROR_CHECKSUM);
    RC_CHECK_FALSE(rc_array_bytes_is_valid(&result.data));
}

// ---- damaged input ----

RC_TEST_STEP(zip_inflate, every_truncation_fails, fix)
{
    // The last block ends in the last byte, so every shorter prefix is incomplete
    rc_view_bytes stream = load("data/zip/lorem.deflate", &fix->a);
    uint32_t succeeded = 0;

    for (uint32_t len = 0; len < stream.num; len++) {
        rc_zip_inflate_result result = rc_zip_inflate(rc_view_bytes_first(stream, len), 0, &fix->a, fix->scratch);

        succeeded += result.error == RC_ZIP_ERROR_NONE ? 1u : 0u;
    }
    RC_CHECK(stream.num, >, 0u);
    RC_CHECK(succeeded, ==, 0u);
}


RC_TEST_STEP(zip_inflate, errors_give_the_arena_back, fix)
{
    // Cut short, or with a bad checksum, after the output has grown well past a chunk, and moved,
    // since the scratch arena's chunk is newer than the output's and stops it growing in place
    rc_view_bytes raw = load("data/zip/large.deflate", &fix->a);
    rc_array_bytes damaged = with_last_byte_flipped(load("data/zip/large.zlib", &fix->a), &fix->a);
    rc_arena_mark before = rc_arena_save(&fix->a);
    rc_zip_inflate_result truncated = rc_zip_inflate(rc_view_bytes_drop_last(raw, 1), 0, &fix->a, fix->scratch);
    rc_zip_inflate_result checksum = rc_zip_inflate_zlib(damaged.view, 0, &fix->a, fix->scratch);
    rc_arena_mark after = rc_arena_save(&fix->a);

    RC_CHECK(truncated.error, ==, RC_ZIP_ERROR_TRUNCATED);
    RC_CHECK(checksum.error, ==, RC_ZIP_ERROR_CHECKSUM);
    RC_CHECK(after.chunk, ==, before.chunk);
    RC_CHECK(after.top, ==, before.top);
}


// True if stream decodes. The output is released, as there may be thousands.
static bool decodes(rc_view_bytes stream, rc_arena *arena, rc_arena scratch)
{
    rc_zip_inflate_result result = rc_zip_inflate(stream, 0, arena, scratch);
    bool succeeded = result.error == RC_ZIP_ERROR_NONE;

    rc_array_bytes_release(&result.data, arena);
    return succeeded;
}


RC_TEST_STEP(zip_inflate, every_flipped_bit_is_safe, fix)
{
    // Whatever a damaged stream holds, decoding it ends in an error or an output, never a trap
    rc_array_bytes stream = rc_array_bytes_from_view(load("data/zip/lorem.deflate", &fix->a), 0, &fix->a);
    uint32_t decoded = 0;

    for (uint32_t bit = 0; bit < stream.num * 8; bit++) {
        uint8_t byte = rc_array_bytes_get(&stream, bit / 8);

        rc_array_bytes_set(&stream, bit / 8, (uint8_t)(byte ^ (1u << (bit % 8))));
        decoded += decodes(stream.view, &fix->a, fix->scratch) ? 1u : 0u;
        rc_array_bytes_set(&stream, bit / 8, byte);
    }

    // Some flips only change a literal; others break the stream
    RC_CHECK(decoded, >, 0u);
    RC_CHECK(decoded, <, stream.num * 8);
}
