// math/atan_table.c
//
// The arctangents atan_f64 and atan2_f64 reduce against: atan(c) to twice double precision for the
// middle c of each quarter of every binade from 1/16 to 16, so any x there lies within 1/8 of a
// binade of its c. Each entry is the double nearest atan(c) and the double nearest what that
// leaves, worked out with mpmath at 300 bits. 32 entries, 512 bytes.

#include "math/arc.h"

const rc_arc_sum rc_arc_atan_table[RC_ARC_ATAN_TABLE_COUNT] = {
    // [2^-4, 2^-3)
    {0x1.1f86dbf082d59p-4, -0x1.095dc7732ef81p-59},  // c = 0x1.2p-4
    {0x1.5f2324fd2d7b2p-4, 0x1.8a8da4401318ep-58},   // c = 0x1.6p-4
    {0x1.9e94153cfdcf1p-4, 0x1.a332e1d69c47ep-58},   // c = 0x1.ap-4
    {0x1.ddd21701eba6ep-4, 0x1.94effcd76fe58p-58},   // c = 0x1.ep-4

    // [2^-3, 2^-2)
    {0x1.1e1fafb043727p-3, -0x1.b485914dacf8cp-59},  // c = 0x1.2p-3
    {0x1.5c9811e3ec26ap-3, -0x1.054ab2c010f3dp-58},  // c = 0x1.6p-3
    {0x1.9a6a8e96c8626p-3, 0x1.cf601e7b4348ep-59},   // c = 0x1.ap-3
    {0x1.d77d5df205736p-3, 0x1.c648d1534597ep-57},   // c = 0x1.ep-3

    // [2^-2, 2^-1)
    {0x1.18bf5a30bf178p-2, 0x1.30ca4748b1bf9p-57},   // c = 0x1.2p-2
    {0x1.530ad9951cd4ap-2, -0x1.2566480884082p-57},  // c = 0x1.6p-2
    {0x1.8b24d394a1b25p-2, 0x1.b6d0ba3748fa8p-56},   // c = 0x1.ap-2
    {0x1.c0db4c94ec9fp-2, -0x1.cc1ce70934c34p-56},   // c = 0x1.ep-2

    // [2^-1, 2^0)
    {0x1.0657e94db30dp-1, -0x1.d5b495f6349e6p-56},   // c = 0x1.2p-1
    {0x1.345f01cce37bbp-1, 0x1.1021137c71102p-55},   // c = 0x1.6p-1
    {0x1.5d58987169b18p-1, 0x1.0028e4bc5e7cap-57},   // c = 0x1.ap-1
    {0x1.819d0b7158a4dp-1, -0x1.bf76229d3b917p-56},  // c = 0x1.ep-1

    // [2^0, 2^1)
    {0x1.b034f38649c88p-1, -0x1.be88d6936f833p-55},  // c = 0x1.2p0
    {0x1.e24dd44c855d1p-1, 0x1.f7ac612ab33d8p-55},   // c = 0x1.6p0
    {0x1.04e67277a01d7p0, 0x1.7115496c13eb6p-57},    // c = 0x1.ap0
    {0x1.14b1dd5f90ce1p0, -0x1.212d570a63fa2p-56},   // c = 0x1.ep0

    // [2^1, 2^2)
    {0x1.270ef55a53a25p0, -0x1.a66b1af5f84fbp-54},   // c = 0x1.2p1
    {0x1.38d6a6ce13353p0, -0x1.12c77e8a80f5cp-55},   // c = 0x1.6p1
    {0x1.45b54837351ap0, 0x1.9e4a72eedacc4p-56},     // c = 0x1.ap1
    {0x1.4f68dea672617p0, 0x1.934f9f2b0020ep-54},    // c = 0x1.ep1

    // [2^2, 2^3)
    {0x1.5a25052114e6p0, 0x1.8c2d0c89de218p-56},     // c = 0x1.2p2
    {0x1.6414d44094c7cp0, -0x1.c5f60a65c7397p-54},   // c = 0x1.6p2
    {0x1.6b0bae830c07p0, -0x1.7d1ab82ffb70bp-54},    // c = 0x1.ap2
    {0x1.7030cf9403197p0, -0x1.cbe1896221608p-56},   // c = 0x1.ep2

    // [2^3, 2^4)
    {0x1.75cbad2a40bd5p0, 0x1.20bc8af35c4d5p-54},    // c = 0x1.2p3
    {0x1.7aea38c1acbd1p0, 0x1.881d48ae6de92p-54},    // c = 0x1.6p3
    {0x1.7e7862aa0157cp0, -0x1.58c9f564b028cp-54},   // c = 0x1.ap3
    {0x1.811518cde39a6p0, 0x1.511fe80fbb23p-57},     // c = 0x1.ep3
};
