// test_math_arc.c
//
// Tests for the inverse trigonometric functions of richc/math/math.h, asin, acos, atan and atan2,
// in both widths: the special values of C17 Annex F bit for bit, the symmetries, a few values for
// sense, and golden results that pin the exact bits each function gives, so that any change, or
// any target that computes differently, shows up.

#include "richc/math/math.h"

#include "richc/test.h"

// The nearest floats and doubles to pi/4, pi/2, 3pi/4 and pi, as bits.
#define PI_4_F32   0x3f490fdbu
#define PI_2_F32   0x3fc90fdbu
#define PI_3_4_F32 0x4016cbe4u
#define PI_F32     0x40490fdbu

#define PI_4_F64   0x3fe921fb54442d18ull
#define PI_2_F64   0x3ff921fb54442d18ull
#define PI_3_4_F64 0x4002d97c7f3321d2ull
#define PI_F64     0x400921fb54442d18ull

#define SIGN_F32 0x80000000u
#define SIGN_F64 0x8000000000000000ull

// An input and the bits of the result for it, recorded once from our own implementation.
typedef struct golden_f32 {
    float x;
    uint32_t bits;
} golden_f32;

typedef struct golden_f64 {
    double x;
    uint64_t bits;
} golden_f64;

typedef struct golden2_f32 {
    float y;
    float x;
    uint32_t bits;
} golden2_f32;

typedef struct golden2_f64 {
    double y;
    double x;
    uint64_t bits;
} golden2_f64;

// Golden inputs cover every branch: for asin and acos, tiny, the kernel up to 1/2, either side of
// 1/2, the square root side up to 1, with its root split and whole either side of 0.975, and
// negatives.
static const golden_f32 asin_golden_f32[] = {
    {0x1p-149f, 0x00000001u},
    {0x1.4484cp-100f, 0x0da24260u},
    {0x1.4f8b58p-17f, 0x3727c5acu},
    {0x1.0624dep-10f, 0x3a831270u},
    {0x1.99999ap-4f, 0x3dcd2494u},
    {0x1p-2f, 0x3e815f4eu},
    {0x1.333334p-2f, 0x3e9c00adu},
    {0x1.ccccccp-2f, 0x3eeefbddu},
    {0x1.fffffep-2f, 0x3f060a91u},
    {0x1p-1f, 0x3f060a92u},
    {0x1.000002p-1f, 0x3f060a93u},
    {0x1.333334p-1f, 0x3f24bc7eu},
    {0x1.666666p-1f, 0x3f468073u},
    {0x1.99999ap-1f, 0x3f6d6339u},
    {0x1.ccccccp-1f, 0x3f8f549bu},
    {0x1.e66666p-1f, 0x3fa06a08u},
    {0x1.fae148p-1f, 0x3fb6f1e4u},
    {0x1.fff2e4p-1f, 0x3fc74067u},
    {0x1.fffffep-1f, 0x3fc9048au},
    {-0x1.99999ap-3f, 0xbe4e30c6u},
    {-0x1.19999ap-1f, 0xbf1515d3u},
    {-0x1.8p-1f, 0xbf591a99u},
    {-0x1.ff7ceep-1f, 0xbfc35650u},
    {-0x1.fffffep-1f, 0xbfc9048au},
};

static const golden_f64 asin_golden_f64[] = {
    {0x0.000000000001p-1022, 0x0000000000000010ull},
    {0x1.56e1fc2f8f359p-997, 0x01a56e1fc2f8f359ull},
    {0x1.12e0be826d695p-30, 0x3e112e0be826d695ull},
    {0x1p-27, 0x3e40000000000000ull},
    {0x1.0624dd2f1a9fcp-10, 0x3f50624e00c1cb16ull},
    {0x1.999999999999ap-4, 0x3fb9a49276037884ull},
    {0x1.999999999999ap-3, 0x3fc9c618aa4ae23dull},
    {0x1p-2, 0x3fd02be9ce0b87cdull},
    {0x1.3333333333333p-2, 0x3fd380159e14f6ffull},
    {0x1.999999999999ap-2, 0x3fda564ac0e73a34ull},
    {0x1.ccccccccccccdp-2, 0x3fdddf7bba8753cdull},
    {0x1.ffffffffffffep-2, 0x3fe0c152382d7364ull},
    {0x1p-1, 0x3fe0c152382d7366ull},
    {0x1.0000000000001p-1, 0x3fe0c152382d7367ull},
    {0x1.199999999999ap-1, 0x3fe2a2ba538032f0ull},
    {0x1.3333333333333p-1, 0x3fe4978fa3269ee1ull},
    {0x1.6666666666666p-1, 0x3fe8d00e692afd95ull},
    {0x1.8p-1, 0x3feb235315c680dcull},
    {0x1.999999999999ap-1, 0x3fedac670561bb50ull},
    {0x1.bb645a1cac083p-1, 0x3ff0c11cf23983e3ull},
    {0x1.ccccccccccccdp-1, 0x3ff1ea93705fa172ull},
    {0x1.e666666666666p-1, 0x3ff40d41159f3409ull},
    {0x1.f333333333333p-1, 0x3ff58c2b5ce0c3e5ull},
    {0x1.f333333333334p-1, 0x3ff58c2b5ce0c3e7ull},
    {0x1.fae147ae147aep-1, 0x3ff6de3c6f33d51dull},
    {0x1.ff7ced916872bp-1, 0x3ff86ac9ad18f803ull},
    {0x1.ffffde7210be9p-1, 0x3ff91c306b2c13adull},
    {0x1.fffffffffffffp-1, 0x3ff921fb50442d18ull},
    {-0x1.999999999999ap-4, 0xbfb9a49276037884ull},
    {-0x1.6666666666666p-2, 0xbfd6e271e909bbe4ull},
    {-0x1p-1, 0xbfe0c152382d7366ull},
    {-0x1.4cccccccccccdp-1, 0xbfe6a4881ddc9b84ull},
    {-0x1.ccccccccccccdp-1, 0xbff1ea93705fa172ull},
    {-0x1.f333333333333p-1, 0xbff58c2b5ce0c3e5ull},
    {-0x1.fffeb074a771dp-1, 0xbff90fa9f3695aa5ull},
    {-0x1.fffffffffffffp-1, 0xbff921fb50442d18ull},
};

static const golden_f32 acos_golden_f32[] = {
    {0x1p-149f, 0x3fc90fdbu},
    {0x1.4484cp-100f, 0x3fc90fdbu},
    {0x1.4f8b58p-17f, 0x3fc90f87u},
    {0x1.0624dep-10f, 0x3fc8ef16u},
    {0x1.99999ap-4f, 0x3fbc3d91u},
    {0x1p-2f, 0x3fa8b807u},
    {0x1.333334p-2f, 0x3fa20fafu},
    {0x1.ccccccp-2f, 0x3f8d50e3u},
    {0x1.fffffep-2f, 0x3f860a92u},
    {0x1p-1f, 0x3f860a92u},
    {0x1.000002p-1f, 0x3f860a91u},
    {0x1.333334p-1f, 0x3f6d6338u},
    {0x1.666666p-1f, 0x3f4b9f42u},
    {0x1.99999ap-1f, 0x3f24bc7du},
    {0x1.ccccccp-1f, 0x3ee6ecfeu},
    {0x1.e66666p-1f, 0x3ea29749u},
    {0x1.fae148p-1f, 0x3e10efb5u},
    {0x1.fff2e4p-1f, 0x3c67b9d5u},
    {0x1.fffffep-1f, 0x39b504f3u},
    {-0x1.99999ap-3f, 0x3fe2d5f3u},
    {-0x1.19999ap-1f, 0x4009cd62u},
    {-0x1.8p-1f, 0x401ace93u},
    {-0x1.ff7ceep-1f, 0x40463315u},
    {-0x1.fffffep-1f, 0x40490a32u},
};

static const golden_f64 acos_golden_f64[] = {
    {0x0.000000000001p-1022, 0x3ff921fb54442d18ull},
    {0x1.56e1fc2f8f359p-997, 0x3ff921fb54442d18ull},
    {0x1.12e0be826d695p-30, 0x3ff921fb53ff74e9ull},
    {0x1p-27, 0x3ff921fb52442d18ull},
    {0x1.0624dd2f1a9fcp-10, 0x3ff91de2c0c3fca6ull},
    {0x1.999999999999ap-4, 0x3ff787b22ce3f590ull},
    {0x1.999999999999ap-3, 0x3ff5e9383efad0d1ull},
    {0x1p-2, 0x3ff51700e0c14b25ull},
    {0x1.3333333333333p-2, 0x3ff441f5ecbeef59ull},
    {0x1.999999999999ap-2, 0x3ff28c68a40a5e8bull},
    {0x1.ccccccccccccdp-2, 0x3ff1aa1c65a25825ull},
    {0x1.ffffffffffffep-2, 0x3ff0c152382d7366ull},
    {0x1p-1, 0x3ff0c152382d7366ull},
    {0x1.0000000000001p-1, 0x3ff0c152382d7365ull},
    {0x1.199999999999ap-1, 0x3fefa13c55082740ull},
    {0x1.3333333333333p-1, 0x3fedac670561bb50ull},
    {0x1.6666666666666p-1, 0x3fe973e83f5d5c9bull},
    {0x1.8p-1, 0x3fe720a392c1d955ull},
    {0x1.999999999999ap-1, 0x3fe4978fa3269ee0ull},
    {0x1.bb645a1cac083p-1, 0x3fe0c1bcc415526aull},
    {0x1.ccccccccccccdp-1, 0x3fdcdd9f8f922e98ull},
    {0x1.e666666666666p-1, 0x3fd452e8fa93e43cull},
    {0x1.f333333333333p-1, 0x3fccae7fbb1b499aull},
    {0x1.f333333333334p-1, 0x3fccae7fbb1b4988ull},
    {0x1.fae147ae147aep-1, 0x3fc21df72882bfd8ull},
    {0x1.ff7ced916872bp-1, 0x3fa6e634e566a29eull},
    {0x1.ffffde7210be9p-1, 0x3f572ba46065af16ull},
    {0x1.fffffffffffffp-1, 0x3e50000000000000ull},
    {-0x1.999999999999ap-4, 0x3ffabc447ba464a1ull},
    {-0x1.6666666666666p-2, 0x3ffeda97ce869c11ull},
    {-0x1p-1, 0x4000c152382d7366ull},
    {-0x1.4cccccccccccdp-1, 0x40023a1fb1993d6dull},
    {-0x1.ccccccccccccdp-1, 0x400586476251e745ull},
    {-0x1.f333333333333p-1, 0x400757135892787full},
    {-0x1.fffeb074a771dp-1, 0x400918d2a3d6c3dfull},
    {-0x1.fffffffffffffp-1, 0x400921fb52442d18ull},
};

// For atan, either side of 2^-27, below the table, each of its binades and edges, beyond 16, and
// either side of 2^54; for f32, either side of tan(pi/8), tan(3pi/8) and 2^26.
static const golden_f32 atan_golden_f32[] = {
    {0x1p-149f, 0x00000001u},
    {0x1.4484cp-100f, 0x0da24260u},
    {0x1.4f8b58p-17f, 0x3727c5acu},
    {0x1.47ae14p-7f, 0x3c23d5a4u},
    {0x1.99999ap-4f, 0x3dcc1f14u},
    {0x1.333334p-2f, 0x3e9539d4u},
    {0x1.a82798p-2f, 0x3ec90fdau},
    {0x1.a8279ap-2f, 0x3ec90fdbu},
    {0x1p-1f, 0x3eed6338u},
    {0x1.8p-1f, 0x3f24bc7du},
    {0x1.fffffep-1f, 0x3f490fdau},
    {0x1p0f, 0x3f490fdbu},
    {0x1.000002p0f, 0x3f490fdcu},
    {0x1.4p0f, 0x3f6563e3u},
    {0x1p1f, 0x3f8db70du},
    {0x1.3504f2p1f, 0x3f96cbe4u},
    {0x1.3504f4p1f, 0x3f96cbe4u},
    {0x1.a66666p1f, 0x3fa36660u},
    {0x1.4p3f, 0x3fbc4de9u},
    {0x1.9p6f, 0x3fc7c82fu},
    {0x1.86ap16f, 0x3fc90f87u},
    {0x1.5af1d8p66f, 0x3fc90fdbu},
    {0x1.fffffep127f, 0x3fc90fdbu},
    {-0x1.99999ap-2f, 0xbec2d1bcu},
    {-0x1.a8279ap-2f, 0xbec90fdbu},
    {-0x1.cp2f, 0xbfb6e62cu},
    {-0x1.2a05f2p33f, 0xbfc90fdbu},
    {0x1.fffffep25f, 0x3fc90fdbu},
    {0x1p26f, 0x3fc90fdbu},
    {-0x1p26f, 0xbfc90fdbu},
};

static const golden_f64 atan_golden_f64[] = {
    {0x0.000000000001p-1022, 0x0000000000000010ull},
    {0x1.56e1fc2f8f359p-997, 0x01a56e1fc2f8f359ull},
    {0x1.12e0be826d695p-30, 0x3e112e0be826d695ull},
    {0x1.fffffffffffffp-28, 0x3e3fffffffffffffull},
    {0x1p-27, 0x3e40000000000000ull},
    {0x1.47ae147ae147bp-7, 0x3f847ab48b1efb5dull},
    {0x1.fffffffffffffp-5, 0x3faff55bb72cfde9ull},
    {0x1p-4, 0x3faff55bb72cfdeaull},
    {0x1.1eb851eb851ecp-4, 0x3fb1e40c8b780fdeull},
    {0x1.999999999999ap-4, 0x3fb983e282e2cc4dull},
    {0x1.999999999999ap-3, 0x3fc94441f8f7260cull},
    {0x1.3333333333333p-2, 0x3fd2a73a661eaf06ull},
    {0x1.ccccccccccccdp-2, 0x3fdb1009e9ee79bcull},
    {0x1.3333333333333p-1, 0x3fe14b1dd5f90ce1ull},
    {0x1.b333333333333p-1, 0x3fe68b3722c4afb4ull},
    {0x1p0, 0x3fe921fb54442d18ull},
    {0x1.4cccccccccccdp0, 0x3fed488143ad8c44ull},
    {0x1.4p1, 0x3ff30b6d796a4da8ull},
    {0x1.d99999999999ap1, 0x3ff4e8c94dbf54e5ull},
    {0x1.8p2, 0x3ff67d8863bc99bdull},
    {0x1.6p3, 0x3ff7aea38c1acbd1ull},
    {0x1.fffffffffffffp3, 0x3ff82250768ac529ull},
    {0x1p4, 0x3ff82250768ac529ull},
    {0x1.18p4, 0x3ff8382dca698943ull},
    {0x1.9p6, 0x3ff8f905eb2def22ull},
    {0x1.81c8p13, 0x3ff921a663cc691dull},
    {0x1.2a05f2p33, 0x3ff921fb543d4de0ull},
    {0x1.fffffffffffffp53, 0x3ff921fb54442d18ull},
    {0x1p54, 0x3ff921fb54442d18ull},
    {0x1.5af1d78b58c4p66, 0x3ff921fb54442d18ull},
    {0x1.7e43c8800759cp996, 0x3ff921fb54442d18ull},
    {-0x1p-2, 0xbfcf5b75f92c80ddull},
    {-0x1.4p2, 0xbff5f97315254857ull},
    {-0x1.e848p19, 0xbff921fa47d4b30dull},
    {-0x1.93e5939a08ceap99, 0xbff921fb54442d18ull},
};

// For atan2, every octant with y either sign, the table and the small ratio with and without its
// correction, either side of 2^-30 (for f32, either side of tan(pi/8) and of |u| = 2^-15), and
// pairs that need scaling or whose ratio is subnormal or underflows.
static const golden2_f32 atan2_golden_f32[] = {
    {0x1p0f, 0x1p1f, 0x3eed6338u},
    {0x1p1f, 0x1p0f, 0x3f8db70du},
    {0x1p1f, -0x1p0f, 0x40023454u},
    {0x1p0f, -0x1p1f, 0x402b6374u},
    {-0x1p0f, 0x1p1f, 0xbeed6338u},
    {-0x1p1f, -0x1p0f, 0xc0023454u},
    {0x1p0f, 0x1p0f, 0x3f490fdbu},
    {0x1p0f, -0x1p0f, 0x4016cbe4u},
    {0x1.333334p-1f, 0x1.99999ap-1f, 0x3f24bc7du},
    {0x1.8p1f, 0x1.cp2f, 0x3ecf4dfau},
    {0x1.cp2f, -0x1.8p1f, 0x3ffce359u},
    {0x1.4f8b58p-17f, 0x1.8p1f, 0x365fb23bu},
    {0x1.8p1f, 0x1.4f8b58p-17f, 0x3fc90fbfu},
    {0x1.4484cp-100f, 0x1.93e594p99f, 0x00000000u},
    {0x1p-149f, 0x1p0f, 0x00000001u},
    {0x1p-149f, 0x1p-148f, 0x3eed6338u},
    {0x1.2ced32p126f, 0x1.c363ccp127f, 0x3ea4bc7du},
    {-0x1.2ced32p126f, -0x1.b38fb8p-127f, 0xbfc90fdbu},
    {0x1.edd2f2p6f, -0x1.472916p9f, 0x403d2079u},
    {-0x1.0624dep-10f, 0x1.f4p9f, 0xb58637bdu},
    {0x1p-126f, 0x1.8p-120f, 0x3c2aa916u},
    {0x1.a82798p-2f, 0x1p0f, 0x3ec90fdau},
    {0x1.a8279ap-2f, -0x1p0f, 0x402feddfu},
    {-0x1.8p1f, 0x1.3504f4p1f, 0xbf64a6feu},
    {0x1p-15f, 0x1p0f, 0x38000000u},
    {0x1.fffffep-16f, -0x1p0f, 0x40490f5bu},
    {-0x1.0c6f7ap-20f, -0x1p1f, 0xc0490fd9u},
    {-0x1p1f, 0x1.0c6f7ap-20f, 0xbfc90fd6u},
};

static const golden2_f64 atan2_golden_f64[] = {
    {0x1p0, 0x1p1, 0x3fddac670561bb4full},
    {0x1p1, 0x1p0, 0x3ff1b6e192ebbe44ull},
    {0x1p1, -0x1p0, 0x4000468a8ace4df6ull},
    {0x1p0, -0x1p1, 0x40056c6e7397f5aeull},
    {-0x1p0, 0x1p1, 0xbfddac670561bb4full},
    {-0x1p1, -0x1p0, 0xc000468a8ace4df6ull},
    {0x1p0, 0x1p0, 0x3fe921fb54442d18ull},
    {0x1p0, -0x1p0, 0x4002d97c7f3321d2ull},
    {0x1.3333333333333p-1, 0x1.999999999999ap-1, 0x3fe4978fa3269ee1ull},
    {0x1.8p1, 0x1.cp2, 0x3fd9e9bf3d20dc71ull},
    {0x1.cp2, -0x1.8p1, 0x3fff9c6b238c6434ull},
    {0x1.47ae147ae147bp-7, 0x1p0, 0x3f847ab48b1efb5dull},
    {0x1.4f8b588e368f1p-17, 0x1.8p1, 0x3ecbf647612ec4b8ull},
    {0x1.8p1, 0x1.4f8b588e368f1p-17, 0x3ff921f7d57b40f2ull},
    {-0x1.4f8b588e368f1p-17, -0x1.8p1, 0xc00921f994dfb705ull},
    {0x1.19799812dea11p-40, 0x1p0, 0x3d719799812dea11ull},
    {0x1p0, -0x1.19799812dea11p-40, 0x3ff921fb54443eb0ull},
    {0x1p-4, 0x1p0, 0x3faff55bb72cfdeaull},
    {0x1p0, 0x1.0000000000001p4, 0x3faff55bb72cfde8ull},
    {0x0.012688b70e62bp-1022, 0x0.03739a252b281p-1022, 0x3fd4978fa3269ee1ull},
    {0x0.0000000000001p-1022, 0x0.00000000007e8p-1022, 0x3f4030919f057214ull},
    {0x1.7e43c8800759cp996, 0x1.7e43c8800759cp997, 0x3fddac670561bb4full},
    {-0x1.e42d130773b76p1023, 0x1.1ccf385ebc8ap1023, 0xbff0a00a3bce369full},
    {0x1.56e1fc2f8f359p-997, 0x1.7e43c8800759cp996, 0x0000000000000000ull},
    {0x0.0000000000001p-1022, 0x1p0, 0x0000000000000001ull},
    {0x0.012688b70e62bp-1022, 0x1p1, 0x0000093445b87316ull},
    {0x1.edd2f1a9fbe77p6, -0x1.472916872b021p9, 0x4007a40f18fca979ull},
    {-0x1.0624dd2f1a9fcp-10, 0x1.f4p9, 0xbeb0c6f7a0b5e767ull},
    {0x1p-30, 0x1p0, 0x3e10000000000000ull},
    {0x1.fffffffffffffp-31, 0x1p0, 0x3e0fffffffffffffull},
    {-0x1p-31, -0x1p0, 0xc00921fb54342d18ull},
    {-0x1.8p1, -0x1.79ca10c924223p-67, 0xbff921fb54442d18ull},
    {-0x1.3333333333333p-1, -0x1.999999999999ap-1, 0xc003fc176b7a8560ull},
    {-0x1.cp2, 0x1.8p1, 0xbff2a78b84fbf5fcull},
};


// The infinities and a quiet NaN, built from their bits.
static float infinity_f32(void)
{
    return rc_bitcast_u32(0x7f800000u);
}


static double infinity_f64(void)
{
    return rc_bitcast_u64(0x7ff0000000000000ull);
}


static float nan_f32(void)
{
    return rc_bitcast_u32(0x7fc00000u);
}


static double nan_f64(void)
{
    return rc_bitcast_u64(0x7ff8000000000000ull);
}


// True if x is a NaN, of any payload or sign.
static bool is_nan_f32(float x)
{
    return !rc_is_finite_f32(x) && x != x;
}


static bool is_nan_f64(double x)
{
    return !rc_is_finite_f64(x) && x != x;
}


// How many the spreads below hold: six mantissas for each finite exponent, then infinity.
enum {
    SPREAD_COUNT_F32 = 255 * 6 + 1,
    SPREAD_COUNT_F64 = 2047 * 6 + 1
};


// The ith of a spread of floats or doubles across every binade, infinity included: each exponent
// with a few mantissas.
static float spread_f32(uint32_t i)
{
    static const uint32_t mantissas[] = {0x000000u, 0x000001u, 0x2aaaabu, 0x400000u, 0x5a827au, 0x7fffffu};
    uint32_t exponent = i / RC_COUNT(mantissas);

    return rc_bitcast_u32((exponent << 23) | mantissas[i % RC_COUNT(mantissas)]);
}


static double spread_f64(uint32_t i)
{
    static const uint64_t mantissas[] = {0x0ull, 0x1ull, 0x5555555555555ull, 0x8000000000000ull, 0xb504f333f9de6ull, 0xfffffffffffffull};
    uint64_t exponent = i / RC_COUNT(mantissas);

    return rc_bitcast_u64((exponent << 52) | mantissas[i % RC_COUNT(mantissas)]);
}


// ---- asin ----

RC_TEST(math_asin, special_values)
{
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.0f)), ==, 0u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0.0f)), ==, SIGN_F32);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(1.0f)), ==, PI_2_F32);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-1.0f)), ==, PI_2_F32 | SIGN_F32);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.0)), ==, 0u);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0.0)), ==, SIGN_F64);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(1.0)), ==, PI_2_F64);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-1.0)), ==, PI_2_F64 | SIGN_F64);

    // Beyond [-1, 1], however little, and NaN
    RC_CHECK_TRUE(is_nan_f32(rc_asin_f32(0x1.000002p0f)));
    RC_CHECK_TRUE(is_nan_f32(rc_asin_f32(-2.0f)));
    RC_CHECK_TRUE(is_nan_f32(rc_asin_f32(infinity_f32())));
    RC_CHECK_TRUE(is_nan_f32(rc_asin_f32(-infinity_f32())));
    RC_CHECK_TRUE(is_nan_f32(rc_asin_f32(nan_f32())));
    RC_CHECK_TRUE(is_nan_f64(rc_asin_f64(0x1.0000000000001p0)));
    RC_CHECK_TRUE(is_nan_f64(rc_asin_f64(-1e300)));
    RC_CHECK_TRUE(is_nan_f64(rc_asin_f64(infinity_f64())));
    RC_CHECK_TRUE(is_nan_f64(rc_asin_f64(-infinity_f64())));
    RC_CHECK_TRUE(is_nan_f64(rc_asin_f64(nan_f64())));
}


RC_TEST(math_asin, odd)
{
    for (uint32_t i = 0; i < SPREAD_COUNT_F32; i++) {
        float x = spread_f32(i);

        RC_CHECK(rc_bitcast_f32(rc_asin_f32(-x)), ==, rc_bitcast_f32(rc_asin_f32(x)) ^ SIGN_F32);
    }

    for (uint32_t i = 0; i < SPREAD_COUNT_F64; i++) {
        double x = spread_f64(i);

        RC_CHECK(rc_bitcast_f64(rc_asin_f64(-x)), ==, rc_bitcast_f64(rc_asin_f64(x)) ^ SIGN_F64);
    }
}


RC_TEST(math_asin, values)
{
    RC_CHECK(rc_asin_f32(0.5f), ~=, 0.5235988f);
    RC_CHECK(rc_asin_f32(-0.8660254f), ~=, -1.0471976f);
    RC_CHECK(rc_asin_f64(0.5), ~=, 0.5235987755982989);
    RC_CHECK(rc_asin_f64(0.7071067811865476), ~=, 0.7853981633974483);

    // Small enough that the cube term is lost in the rounding
    RC_CHECK(rc_asin_f32(1e-5f), ==, 1e-5f);
    RC_CHECK(rc_asin_f64(1e-9), ==, 1e-9);
}


RC_TEST(math_asin, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(asin_golden_f32); i++) {
        RC_CHECK(rc_bitcast_f32(rc_asin_f32(asin_golden_f32[i].x)), ==, asin_golden_f32[i].bits);
    }

    for (uint32_t i = 0; i < RC_COUNT(asin_golden_f64); i++) {
        RC_CHECK(rc_bitcast_f64(rc_asin_f64(asin_golden_f64[i].x)), ==, asin_golden_f64[i].bits);
    }
}

// ---- acos ----

RC_TEST(math_acos, special_values)
{
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(1.0f)), ==, 0u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-1.0f)), ==, PI_F32);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.0f)), ==, PI_2_F32);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0.0f)), ==, PI_2_F32);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(1.0)), ==, 0u);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-1.0)), ==, PI_F64);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.0)), ==, PI_2_F64);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0.0)), ==, PI_2_F64);

    // Beyond [-1, 1], however little, and NaN
    RC_CHECK_TRUE(is_nan_f32(rc_acos_f32(0x1.000002p0f)));
    RC_CHECK_TRUE(is_nan_f32(rc_acos_f32(-0x1.000002p0f)));
    RC_CHECK_TRUE(is_nan_f32(rc_acos_f32(infinity_f32())));
    RC_CHECK_TRUE(is_nan_f32(rc_acos_f32(-infinity_f32())));
    RC_CHECK_TRUE(is_nan_f32(rc_acos_f32(nan_f32())));
    RC_CHECK_TRUE(is_nan_f64(rc_acos_f64(0x1.0000000000001p0)));
    RC_CHECK_TRUE(is_nan_f64(rc_acos_f64(-0x1.0000000000001p0)));
    RC_CHECK_TRUE(is_nan_f64(rc_acos_f64(infinity_f64())));
    RC_CHECK_TRUE(is_nan_f64(rc_acos_f64(-infinity_f64())));
    RC_CHECK_TRUE(is_nan_f64(rc_acos_f64(nan_f64())));
}


RC_TEST(math_acos, values)
{
    RC_CHECK(rc_acos_f32(0.5f), ~=, 1.0471976f);
    RC_CHECK(rc_acos_f32(-0.5f), ~=, 2.0943951f);
    RC_CHECK(rc_acos_f64(0.5), ~=, 1.0471975511965979);
    RC_CHECK(rc_acos_f64(-0.7071067811865476), ~=, 2.356194490192345);

    // acos(-x) is pi - acos(x), to within the rounding
    for (uint32_t i = 0; i <= 20; i++) {
        float x_f32 = (float)i / 20.0f;
        double x_f64 = (double)i / 20.0;

        RC_CHECK(rc_acos_f32(-x_f32) + rc_acos_f32(x_f32), ~=, 3.14159265f);
        RC_CHECK(rc_acos_f64(-x_f64) + rc_acos_f64(x_f64), ~=, 3.141592653589793);
    }
}


RC_TEST(math_acos, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(acos_golden_f32); i++) {
        RC_CHECK(rc_bitcast_f32(rc_acos_f32(acos_golden_f32[i].x)), ==, acos_golden_f32[i].bits);
    }

    for (uint32_t i = 0; i < RC_COUNT(acos_golden_f64); i++) {
        RC_CHECK(rc_bitcast_f64(rc_acos_f64(acos_golden_f64[i].x)), ==, acos_golden_f64[i].bits);
    }
}

// ---- atan ----

RC_TEST(math_atan, special_values)
{
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0.0f)), ==, 0u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-0.0f)), ==, SIGN_F32);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(infinity_f32())), ==, PI_2_F32);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-infinity_f32())), ==, PI_2_F32 | SIGN_F32);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0.0)), ==, 0u);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-0.0)), ==, SIGN_F64);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(infinity_f64())), ==, PI_2_F64);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-infinity_f64())), ==, PI_2_F64 | SIGN_F64);
    RC_CHECK_TRUE(is_nan_f32(rc_atan_f32(nan_f32())));
    RC_CHECK_TRUE(is_nan_f64(rc_atan_f64(nan_f64())));

    // The largest finite numbers are as good as infinite
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1.fffffep127f)), ==, PI_2_F32);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1.fffffffffffffp1023)), ==, PI_2_F64);
}


RC_TEST(math_atan, odd)
{
    for (uint32_t i = 0; i < SPREAD_COUNT_F32; i++) {
        float x = spread_f32(i);

        RC_CHECK(rc_bitcast_f32(rc_atan_f32(-x)), ==, rc_bitcast_f32(rc_atan_f32(x)) ^ SIGN_F32);
    }

    for (uint32_t i = 0; i < SPREAD_COUNT_F64; i++) {
        double x = spread_f64(i);

        RC_CHECK(rc_bitcast_f64(rc_atan_f64(-x)), ==, rc_bitcast_f64(rc_atan_f64(x)) ^ SIGN_F64);
    }
}


RC_TEST(math_atan, values)
{
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(1.0f)), ==, PI_4_F32);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(1.0)), ==, PI_4_F64);
    RC_CHECK(rc_atan_f32(1.7320508f), ~=, 1.0471976f);
    RC_CHECK(rc_atan_f32(-0.57735027f), ~=, -0.5235988f);
    RC_CHECK(rc_atan_f64(1.7320508075688772), ~=, 1.0471975511965979);
    RC_CHECK(rc_atan_f64(1e6), ~=, 1.5707953267948966);

    // Small enough that the cube term is lost in the rounding
    RC_CHECK(rc_atan_f32(1e-5f), ==, 1e-5f);
    RC_CHECK(rc_atan_f64(1e-9), ==, 1e-9);
}


RC_TEST(math_atan, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(atan_golden_f32); i++) {
        RC_CHECK(rc_bitcast_f32(rc_atan_f32(atan_golden_f32[i].x)), ==, atan_golden_f32[i].bits);
    }

    for (uint32_t i = 0; i < RC_COUNT(atan_golden_f64); i++) {
        RC_CHECK(rc_bitcast_f64(rc_atan_f64(atan_golden_f64[i].x)), ==, atan_golden_f64[i].bits);
    }
}

// ---- atan2 ----

// atan2(y, x) gives the bits expected, and atan2(-y, x) the same with the sign flipped.
static void check_atan2_f32(float y, float x, uint32_t expected)
{
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(y, x)), ==, expected);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-y, x)), ==, expected ^ SIGN_F32);
}


static void check_atan2_f64(double y, double x, uint64_t expected)
{
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(y, x)), ==, expected);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-y, x)), ==, expected ^ SIGN_F64);
}


RC_TEST(math_atan2, zero_y)
{
    // (+-0, -0) is +-pi, (+-0, +0) is +-0
    check_atan2_f32(0.0f, -0.0f, PI_F32);
    check_atan2_f32(0.0f, 0.0f, 0u);
    check_atan2_f64(0.0, -0.0, PI_F64);
    check_atan2_f64(0.0, 0.0, 0u);

    // (+-0, x < 0) is +-pi, (+-0, x > 0) is +-0, however small or large x
    check_atan2_f32(0.0f, -1.0f, PI_F32);
    check_atan2_f32(0.0f, -0x1p-149f, PI_F32);
    check_atan2_f32(0.0f, -infinity_f32(), PI_F32);
    check_atan2_f32(0.0f, 1.0f, 0u);
    check_atan2_f32(0.0f, 0x1p-149f, 0u);
    check_atan2_f32(0.0f, infinity_f32(), 0u);
    check_atan2_f64(0.0, -1.0, PI_F64);
    check_atan2_f64(0.0, -0x1p-1074, PI_F64);
    check_atan2_f64(0.0, -infinity_f64(), PI_F64);
    check_atan2_f64(0.0, 1.0, 0u);
    check_atan2_f64(0.0, 0x1p-1074, 0u);
    check_atan2_f64(0.0, infinity_f64(), 0u);
}


RC_TEST(math_atan2, zero_x)
{
    // (y < 0, +-0) is -pi/2 and (y > 0, +-0) is pi/2, however small or large y
    check_atan2_f32(1.0f, 0.0f, PI_2_F32);
    check_atan2_f32(1.0f, -0.0f, PI_2_F32);
    check_atan2_f32(0x1p-149f, 0.0f, PI_2_F32);
    check_atan2_f32(0x1.fffffep127f, -0.0f, PI_2_F32);
    check_atan2_f64(1.0, 0.0, PI_2_F64);
    check_atan2_f64(1.0, -0.0, PI_2_F64);
    check_atan2_f64(0x1p-1074, 0.0, PI_2_F64);
    check_atan2_f64(0x1.fffffffffffffp1023, -0.0, PI_2_F64);
}


RC_TEST(math_atan2, infinite_x)
{
    // (+-y, -inf) is +-pi and (+-y, +inf) is +-0, for finite y > 0
    check_atan2_f32(1.0f, -infinity_f32(), PI_F32);
    check_atan2_f32(0x1.fffffep127f, -infinity_f32(), PI_F32);
    check_atan2_f32(0x1p-149f, -infinity_f32(), PI_F32);
    check_atan2_f32(1.0f, infinity_f32(), 0u);
    check_atan2_f32(0x1.fffffep127f, infinity_f32(), 0u);
    check_atan2_f64(1.0, -infinity_f64(), PI_F64);
    check_atan2_f64(0x1.fffffffffffffp1023, -infinity_f64(), PI_F64);
    check_atan2_f64(0x1p-1074, -infinity_f64(), PI_F64);
    check_atan2_f64(1.0, infinity_f64(), 0u);
    check_atan2_f64(0x1.fffffffffffffp1023, infinity_f64(), 0u);
}


RC_TEST(math_atan2, infinite_y)
{
    // (+-inf, finite x) is +-pi/2
    check_atan2_f32(infinity_f32(), 1.0f, PI_2_F32);
    check_atan2_f32(infinity_f32(), -1.0f, PI_2_F32);
    check_atan2_f32(infinity_f32(), 0.0f, PI_2_F32);
    check_atan2_f32(infinity_f32(), -0x1.fffffep127f, PI_2_F32);
    check_atan2_f64(infinity_f64(), 1.0, PI_2_F64);
    check_atan2_f64(infinity_f64(), -1.0, PI_2_F64);
    check_atan2_f64(infinity_f64(), -0.0, PI_2_F64);
    check_atan2_f64(infinity_f64(), 0x1.fffffffffffffp1023, PI_2_F64);

    // (+-inf, -inf) is +-3pi/4 and (+-inf, +inf) is +-pi/4
    check_atan2_f32(infinity_f32(), -infinity_f32(), PI_3_4_F32);
    check_atan2_f32(infinity_f32(), infinity_f32(), PI_4_F32);
    check_atan2_f64(infinity_f64(), -infinity_f64(), PI_3_4_F64);
    check_atan2_f64(infinity_f64(), infinity_f64(), PI_4_F64);
}


RC_TEST(math_atan2, nan)
{
    float values_f32[] = {0.0f, -0.0f, 1.0f, -1.0f, infinity_f32(), -infinity_f32(), nan_f32()};
    double values_f64[] = {0.0, -0.0, 1.0, -1.0, infinity_f64(), -infinity_f64(), nan_f64()};

    for (uint32_t i = 0; i < RC_COUNT(values_f32); i++) {
        RC_CHECK_TRUE(is_nan_f32(rc_atan2_f32(nan_f32(), values_f32[i])));
        RC_CHECK_TRUE(is_nan_f32(rc_atan2_f32(values_f32[i], nan_f32())));
        RC_CHECK_TRUE(is_nan_f64(rc_atan2_f64(nan_f64(), values_f64[i])));
        RC_CHECK_TRUE(is_nan_f64(rc_atan2_f64(values_f64[i], nan_f64())));
    }
}


RC_TEST(math_atan2, quadrants)
{
    // The diagonals are exactly the nearest pi/4 and 3pi/4
    check_atan2_f32(1.0f, 1.0f, PI_4_F32);
    check_atan2_f32(1.0f, -1.0f, PI_3_4_F32);
    check_atan2_f32(1e30f, 1e30f, PI_4_F32);
    check_atan2_f64(1.0, 1.0, PI_4_F64);
    check_atan2_f64(1.0, -1.0, PI_3_4_F64);
    check_atan2_f64(1e-300, -1e-300, PI_3_4_F64);

    // A 3-4-5 triangle in each quadrant
    RC_CHECK(rc_atan2_f32(3.0f, 4.0f), ~=, 0.6435011f);
    RC_CHECK(rc_atan2_f32(3.0f, -4.0f), ~=, 2.4980915f);
    RC_CHECK(rc_atan2_f32(-3.0f, -4.0f), ~=, -2.4980915f);
    RC_CHECK(rc_atan2_f32(-3.0f, 4.0f), ~=, -0.6435011f);
    RC_CHECK(rc_atan2_f64(4.0, 3.0), ~=, 0.9272952180016122);
    RC_CHECK(rc_atan2_f64(4.0, -3.0), ~=, 2.214297435588181);
    RC_CHECK(rc_atan2_f64(-4.0, -3.0), ~=, -2.214297435588181);
    RC_CHECK(rc_atan2_f64(-4.0, 3.0), ~=, -0.9272952180016122);

    // Agrees with atan where x > 0
    RC_CHECK(rc_atan2_f32(3.0f, 4.0f), ~=, rc_atan_f32(0.75f));
    RC_CHECK(rc_atan2_f64(3.0, 4.0), ~=, rc_atan_f64(0.75));
}


RC_TEST(math_atan2, steep_and_shallow)
{
    // Shallow: the angle is the slope, whose cube is lost in the rounding
    check_atan2_f32(1e-5f, 1.0f, rc_bitcast_f32(1e-5f));
    check_atan2_f64(1e-9, 1.0, rc_bitcast_f64(1e-9));
    check_atan2_f64(3e-300, 3e10, rc_bitcast_f64(1e-310));

    // Steep: just short of pi/2, or just past it for x < 0
    RC_CHECK(rc_atan2_f32(1e5f, 1.0f), <, rc_bitcast_u32(PI_2_F32));
    RC_CHECK(rc_atan2_f32(1e5f, 1.0f), ~=, 1.5707863f);
    RC_CHECK(rc_atan2_f32(1e5f, -1.0f), ~=, 1.5708063f);
    RC_CHECK(rc_atan2_f64(1e9, 1.0), <, rc_bitcast_u64(PI_2_F64));
    RC_CHECK(rc_atan2_f64(1e9, 1.0), ~=, 1.5707963257948966);
    RC_CHECK(rc_atan2_f64(1e9, -1.0), ~=, 1.5707963277948966);

    // So steep or shallow that the other side vanishes
    check_atan2_f32(1e30f, 1e-30f, PI_2_F32);
    check_atan2_f32(1e-30f, -1e30f, PI_F32);
    check_atan2_f64(1e300, 1e-300, PI_2_F64);
    check_atan2_f64(1e-300, -1e300, PI_F64);
}


RC_TEST(math_atan2, subnormal_ratios)
{
    // A ratio among the subnormals is the result, rounded once: atan adds nothing it can hold
    check_atan2_f32(0x1p-149f, 1.0f, 1u);
    check_atan2_f32(0x1p-138f, 0x1p10f, 2u);
    check_atan2_f32(3e-39f, 3.0f, rc_bitcast_f32(1e-39f));
    check_atan2_f64(0x1p-1074, 1.0, 1u);
    check_atan2_f64(0x1p-1000, 0x1p70, 16u);
    check_atan2_f64(5e-300, 5e10, rc_bitcast_f64(1e-310));

    // Too small even for those, it rounds to zero, or to pi for x < 0
    check_atan2_f32(0x1p-149f, 4.0f, 0u);
    check_atan2_f32(0x1p-149f, -4.0f, PI_F32);
    check_atan2_f64(0x1p-1074, 4.0, 0u);
    check_atan2_f64(0x1p-1074, -4.0, PI_F64);
}


RC_TEST(math_atan2, odd_in_y)
{
    float values_f32[] = {0.0f, 0x1p-149f, 1e-20f, 0.3f, 1.0f, 2.5f, 1e20f, 0x1.fffffep127f, infinity_f32()};
    double values_f64[] = {0.0, 0x1p-1074, 1e-200, 0.3, 1.0, 2.5, 1e200, 0x1.fffffffffffffp1023, infinity_f64()};

    // Every pair, with x of both signs, zeros and infinities included
    for (uint32_t i = 0; i < RC_COUNT(values_f32); i++) {
        for (uint32_t j = 0; j < 2 * RC_COUNT(values_f32); j++) {
            float y_f32 = values_f32[i];
            float x_f32 = j % 2 == 0 ? values_f32[j / 2] : -values_f32[j / 2];
            double y_f64 = values_f64[i];
            double x_f64 = j % 2 == 0 ? values_f64[j / 2] : -values_f64[j / 2];

            RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-y_f32, x_f32)), ==, rc_bitcast_f32(rc_atan2_f32(y_f32, x_f32)) ^ SIGN_F32);
            RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-y_f64, x_f64)), ==, rc_bitcast_f64(rc_atan2_f64(y_f64, x_f64)) ^ SIGN_F64);
        }
    }
}


RC_TEST(math_atan2, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(atan2_golden_f32); i++) {
        RC_CHECK(rc_bitcast_f32(rc_atan2_f32(atan2_golden_f32[i].y, atan2_golden_f32[i].x)), ==, atan2_golden_f32[i].bits);
    }

    for (uint32_t i = 0; i < RC_COUNT(atan2_golden_f64); i++) {
        RC_CHECK(rc_bitcast_f64(rc_atan2_f64(atan2_golden_f64[i].y, atan2_golden_f64[i].x)), ==, atan2_golden_f64[i].bits);
    }
}
