// zip/inflate.h
//
// DEFLATE decompression: raw DEFLATE (RFC 1951), or DEFLATE in a zlib wrapper (RFC 1950), the form
// PNG's IDAT chunks use. There is no compressor.
//
// rc_zip_inflate decodes raw DEFLATE. rc_zip_inflate_zlib checks a zlib stream's two-byte header,
// decodes the DEFLATE inside, and checks the Adler-32 of the output against the trailer.
//
// Neither format records how big the output will be, so the output grows as it is decoded. A
// caller who knows the size passes it as minimum_capacity, and the output is made that big up
// front: PNG knows it exactly, height x (1 + width x bytes per pixel). Pass 0 when it is unknown.
//
// The output comes from arena, and is the only thing taken from it, so it is always the arena's
// last allocation and grows in place where it can. The Huffman codes, about 8 KiB, too big for the
// stack, are kept in scratch, which must be a different arena.
//
// Compressed data is untrusted, so anything malformed is an error, never a trap: the output is
// then the invalid {0}, and arena is given back everything the output took. An output that would
// reach 4 GiB, more than an array holds, is RC_ZIP_ERROR_TOO_LARGE; running out of memory before
// then is the arena's panic, as for any allocation.

#ifndef RC_ZIP_INFLATE_H_
#define RC_ZIP_INFLATE_H_

#include "richc/bytes.h"

typedef enum rc_zip_error {
    RC_ZIP_ERROR_NONE = 0,
    RC_ZIP_ERROR_BAD_DATA,    // malformed DEFLATE: a bad block type, Huffman code, length or distance
    RC_ZIP_ERROR_TRUNCATED,   // the input ended part way through
    RC_ZIP_ERROR_BAD_HEADER,  // zlib: not DEFLATE, a bad header check, or a preset dictionary
    RC_ZIP_ERROR_CHECKSUM,    // zlib: the Adler-32 does not match the output
    RC_ZIP_ERROR_TOO_LARGE    // the output would reach 4 GiB
} rc_zip_error;

typedef struct rc_zip_inflate_result {
    rc_array_bytes data;  // {0} on any error
    uint8_t error;        // rc_zip_error
} rc_zip_inflate_result;

// Decompress raw DEFLATE.
rc_zip_inflate_result rc_zip_inflate(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch);

// Decompress DEFLATE in a zlib wrapper, checking its header and checksum.
rc_zip_inflate_result rc_zip_inflate_zlib(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena, rc_arena scratch);

#endif // RC_ZIP_INFLATE_H_
