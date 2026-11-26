// test.h
//
// richc's unit-test framework, natively and on wasm. wasm has no main, so a runner such as
// tools/run_wasm_tests.mjs calls the module's test_main export instead.
//
// Tests register themselves by dropping a descriptor pointer into a dedicated linker section, which
// rc_test_run walks, so there is no list to maintain.
//
//   RC_TEST(group, name) { ... }                a plain test
//   RC_TEST_SKIP(group, name) { ... }           registered but reported as skipped
//   RC_TEST_GROUP_DATA(group) { ... };          the per-group fixture struct
//   RC_TEST_GROUP_INIT(group, fix) { ... }      runs before each step of the group
//   RC_TEST_GROUP_DEINIT(group, fix) { ... }    runs after each step, failed ones too
//   RC_TEST_STEP(group, name, fix) { ... }      a test that receives the fixture
//   RC_TEST_STEP_SKIP(group, name, fix) { ... } a fixtured test, not run
//
//   RC_CHECK(a, op, b)   a op b, with op one of == != < > <= >=; a's type picks the check. Floats
//                        and float vectors also take ~=, each component within a fixed epsilon.
//                        rc_str and the vectors take only == and != (and ~=); rc_rational takes all
//                        but ~=
//   RC_CHECK_TRUE(a), RC_CHECK_FALSE(a)
//
//   rc_test_try(fn, context)   call fn(context), false if it left through rc_test_bail
//
// A failing check prints file:line and the actual value, then abandons the test.
//
// Each test runs between rc_arena_init and rc_arena_deinit, so it starts with an empty page heap
// and an empty chunk pool, whatever ran before it.

#ifndef RC_TEST_H_
#define RC_TEST_H_

#include "richc/macros.h"
#include "richc/math/rational.h"
#include "richc/math/vec4f.h"
#include "richc/str.h"

// ---- linker-section placement ----

#if defined(_WIN32)
#  if defined(__clang__) || defined(__GNUC__)
#    define RC_TEST_SECTION_(s) __attribute__((used, section(s)))
#  elif defined(_MSC_VER)
#    define RC_TEST_SECTION_(s) __pragma(section(s)); __declspec(allocate(s))
#  else
#    error "richc test: unsupported compiler on Windows"
#  endif

// The linker sorts sections by name, so items in $b sit between the $a and $c sentinels
#  define RC_TEST_SECTION_NAME_ "rc_test$b"
#elif defined(__linux__) || defined(__wasm__)
#  define RC_TEST_SECTION_(s) __attribute__((used, section(s)))

// The ELF linkers and wasm-ld synthesise __start_rc_test / __stop_rc_test for us
#  define RC_TEST_SECTION_NAME_ "rc_test"
#elif defined(__APPLE__)
#  define RC_TEST_SECTION_(s) __attribute__((used, section(s)))

// ld64 synthesises section$start$__DATA$rc_test / section$end$__DATA$rc_test
#  define RC_TEST_SECTION_NAME_ "__DATA,rc_test"
#else
#  error "richc test: unsupported platform (expected _WIN32, __APPLE__, __linux__ or __wasm__)"
#endif

// ---- descriptor ----

typedef struct rc_test {
    const char *group_name;
    const char *test_name;
    void (*fn)(void);
    void (*step_fn)(void *);
    void (*init_fn)(void *);
    void (*deinit_fn)(void *);
    void *context;
    bool skip;
} rc_test;

// ---- defining tests ----

// A plain test, registered by a pointer to its descriptor in the test section.
#define RC_TEST(group, name) \
    static void rc_test_fn_##group##_##name(void); \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .fn = rc_test_fn_##group##_##name \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(void)

// A skipped test still references its function, so it is never an unused static
#define RC_TEST_SKIP(group, name) \
    static void rc_test_fn_##group##_##name(void); \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .fn = rc_test_fn_##group##_##name, \
        .skip = true \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(void)

// ---- fixtures ----

// The fixture struct a group's steps share, defined by the braces that follow.
#define RC_TEST_GROUP_DATA(group) \
    struct rc_test_group_data_##group

// Set the fixture up before each step of the group.
#define RC_TEST_GROUP_INIT(group, fixture) \
    static void rc_test_init_##group(struct rc_test_group_data_##group *fixture); \
    static void rc_test_init_##group##_wrapper(void *fixture) \
    { \
        rc_test_init_##group((struct rc_test_group_data_##group *)fixture); \
    } \
    static void rc_test_init_##group(struct rc_test_group_data_##group *fixture)

// Tear the fixture down after each step of the group, failed ones too.
#define RC_TEST_GROUP_DEINIT(group, fixture) \
    static void rc_test_deinit_##group(struct rc_test_group_data_##group *fixture); \
    static void rc_test_deinit_##group##_wrapper(void *fixture) \
    { \
        rc_test_deinit_##group((struct rc_test_group_data_##group *)fixture); \
    } \
    static void rc_test_deinit_##group(struct rc_test_group_data_##group *fixture)

// A test that receives its group's fixture.
#define RC_TEST_STEP(group, name, fixture) \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *); \
    static void rc_test_step_##group##_##name##_wrapper(void *fixture) \
    { \
        rc_test_fn_##group##_##name((struct rc_test_group_data_##group *)fixture); \
    } \
    static struct rc_test_group_data_##group rc_test_data_##group##_##name; \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .step_fn = rc_test_step_##group##_##name##_wrapper, \
        .init_fn = rc_test_init_##group##_wrapper, \
        .deinit_fn = rc_test_deinit_##group##_wrapper, \
        .context = &rc_test_data_##group##_##name \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *fixture)

// A fixtured test, registered but reported as skipped.
#define RC_TEST_STEP_SKIP(group, name, fixture) \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *); \
    static void rc_test_step_##group##_##name##_wrapper(void *fixture) \
    { \
        rc_test_fn_##group##_##name((struct rc_test_group_data_##group *)fixture); \
    } \
    static struct rc_test_group_data_##group rc_test_data_##group##_##name; \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .step_fn = rc_test_step_##group##_##name##_wrapper, \
        .init_fn = rc_test_init_##group##_wrapper, \
        .deinit_fn = rc_test_deinit_##group##_wrapper, \
        .context = &rc_test_data_##group##_##name, \
        .skip = true \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *fixture)

// ---- assertions ----

// Check a op b (or that a is true or false), abandoning the test with the actual value if not.
#define RC_CHECK(a, op, b)  RC_CHECK_IMPL_(a, op, b, __FILE__, __LINE__)
#define RC_CHECK_TRUE(a)    RC_CHECK_IMPL_(!!(a), ==, true, __FILE__, __LINE__)
#define RC_CHECK_FALSE(a)   RC_CHECK_IMPL_(!(a), ==, true, __FILE__, __LINE__)

// The fixed-width entries also cover plain int / unsigned literals. No long or char entries: on
// LP64 int64_t is long, which would make _Generic ambiguous.
#define RC_CHECK_IMPL_(a, op, b, file, line) \
    _Generic((a), \
        bool: rc_test_check_bool, \
        int8_t: rc_test_check_int, \
        uint8_t: rc_test_check_int, \
        int16_t: rc_test_check_int, \
        uint16_t: rc_test_check_int, \
        int32_t: rc_test_check_int, \
        uint32_t: rc_test_check_int, \
        int64_t: rc_test_check_int, \
        uint64_t: rc_test_check_u64, \
        float: rc_test_check_f32, \
        double: rc_test_check_f64, \
        rc_str: rc_test_check_str, \
        rc_vec2i: rc_test_check_vec2i, \
        rc_vec3i: rc_test_check_vec3i, \
        rc_vec2f: rc_test_check_vec2f, \
        rc_vec3f: rc_test_check_vec3f, \
        rc_vec4f: rc_test_check_vec4f, \
        rc_rational: rc_test_check_rational \
    )(a, #op, b, #a " " #op " " #b, file, line)

// Check functions behind RC_CHECK, one per operand type.
void rc_test_check_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line);
void rc_test_check_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line);
void rc_test_check_u64(uint64_t actual, const char *op, uint64_t expected, const char *expr, const char *file, int line);
void rc_test_check_f32(float actual, const char *op, float expected, const char *expr, const char *file, int line);
void rc_test_check_f64(double actual, const char *op, double expected, const char *expr, const char *file, int line);
void rc_test_check_str(rc_str actual, const char *op, rc_str expected, const char *expr, const char *file, int line);
void rc_test_check_vec2i(rc_vec2i actual, const char *op, rc_vec2i expected, const char *expr, const char *file, int line);
void rc_test_check_vec3i(rc_vec3i actual, const char *op, rc_vec3i expected, const char *expr, const char *file, int line);
void rc_test_check_vec2f(rc_vec2f actual, const char *op, rc_vec2f expected, const char *expr, const char *file, int line);
void rc_test_check_vec3f(rc_vec3f actual, const char *op, rc_vec3f expected, const char *expr, const char *file, int line);
void rc_test_check_vec4f(rc_vec4f actual, const char *op, rc_vec4f expected, const char *expr, const char *file, int line);
void rc_test_check_rational(rc_rational actual, const char *op, rc_rational expected, const char *expr, const char *file, int line);

// ---- leaving code that must not return ----

// Call fn(context), returning false if it called rc_test_bail: for leaving code that may not
// return, such as an out-of-memory handler. Tries do not nest, and a failing check inside one
// leaves the whole test.
bool rc_test_try(void (*fn)(void *), void *context);

// Leave the rc_test_try this is called within.
RC_NORETURN void rc_test_bail(void);

// ---- running ----

// Run every test whose group name starts with filter ("" runs all). Returns the failure count.
int rc_test_run(const char *filter);

#if defined(__wasm__)
// The filter the runner gave, which it passes on through test.js.
const char *rc_test_wasm_filter(void);

// wasm has no main, so the runner calls this export instead. It returns 1 if any test failed.
#  define RC_TEST_MAIN() \
    __attribute__((export_name("test_main"))) int rc_test_main(void); \
    int rc_test_main(void) \
    { \
        return rc_test_run(rc_test_wasm_filter()) != 0; \
    }
#else
// A main() that runs the tests named by the first argument, exiting 1 on any failure.
#  define RC_TEST_MAIN() \
    int main(int argc, char **argv) \
    { \
        return rc_test_run(argc > 1 ? argv[1] : "") != 0; \
    }
#endif

#endif // RC_TEST_H_
