// test.h
//
// richc's unit-test framework. Native only: it is never built for wasm.
//
// Tests register themselves by dropping a descriptor pointer into a dedicated
// linker section, which rc_test_run walks, so there is no list to maintain.
//
//   RC_TEST(group, name) { ... }                a plain test
//   RC_TEST_SKIP(group, name) { ... }           registered but reported as skipped
//   RC_TEST_GROUP_DATA(group) { ... };          the per-group fixture struct
//   RC_TEST_GROUP_INIT(group, fix) { ... }      runs before each step of the group
//   RC_TEST_GROUP_DEINIT(group, fix) { ... }    runs after each step, failed ones too
//   RC_TEST_STEP(group, name, fix) { ... }      a test that receives the fixture
//   RC_TEST_STEP_SKIP(group, name, fix) { ... } a fixtured test, not run
//
//   RC_CHECK(a, op, b)   a op b, with op one of == != < > <= >= (and ~= for
//                        floats, a fixed-epsilon compare); a's type picks the check
//   RC_CHECK_TRUE(a), RC_CHECK_FALSE(a)
//
// A failing check prints file:line and the actual value, then abandons the test.
//
// Each test runs between rc_arena_init and rc_arena_deinit, so it starts with an
// empty page heap and an empty chunk pool, whatever ran before it.

#ifndef RC_TEST_H_
#define RC_TEST_H_

#include <stdbool.h>
#include <stdint.h>

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
#elif defined(__linux__)
#  define RC_TEST_SECTION_(s) __attribute__((used, section(s)))

// The ELF linker synthesises __start_rc_test / __stop_rc_test for us
#  define RC_TEST_SECTION_NAME_ "rc_test"
#elif defined(__APPLE__)
#  define RC_TEST_SECTION_(s) __attribute__((used, section(s)))

// ld64 synthesises section$start$__DATA$rc_test / section$end$__DATA$rc_test
#  define RC_TEST_SECTION_NAME_ "__DATA,rc_test"
#else
#  error "richc test: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
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

#define RC_TEST_GROUP_DATA(group) \
    struct rc_test_group_data_##group

#define RC_TEST_GROUP_INIT(group, fixture) \
    static void rc_test_init_##group(struct rc_test_group_data_##group *fixture); \
    static void rc_test_init_##group##_wrapper(void *fixture) \
    { \
        rc_test_init_##group((struct rc_test_group_data_##group *)fixture); \
    } \
    static void rc_test_init_##group(struct rc_test_group_data_##group *fixture)

#define RC_TEST_GROUP_DEINIT(group, fixture) \
    static void rc_test_deinit_##group(struct rc_test_group_data_##group *fixture); \
    static void rc_test_deinit_##group##_wrapper(void *fixture) \
    { \
        rc_test_deinit_##group((struct rc_test_group_data_##group *)fixture); \
    } \
    static void rc_test_deinit_##group(struct rc_test_group_data_##group *fixture)

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

#define RC_CHECK(a, op, b)  RC_CHECK_IMPL_(a, op, b, __FILE__, __LINE__)
#define RC_CHECK_TRUE(a)    RC_CHECK_IMPL_(!!(a), ==, true, __FILE__, __LINE__)
#define RC_CHECK_FALSE(a)   RC_CHECK_IMPL_(!(a), ==, true, __FILE__, __LINE__)

// The fixed-width entries also cover plain int / unsigned literals. No long or
// char entries: on LP64 int64_t is long, which would make _Generic ambiguous.
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
        uint64_t: rc_test_check_int, \
        float: rc_test_check_float, \
        double: rc_test_check_float \
    )(a, #op, b, #a " " #op " " #b, file, line)

// Check functions behind RC_CHECK, one per operand type.
void rc_test_check_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line);
void rc_test_check_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line);
void rc_test_check_float(double actual, const char *op, double expected, const char *expr, const char *file, int line);

// ---- running ----

// Run every test whose group name starts with filter ("" runs all). Returns the failure count.
int rc_test_run(const char *filter);

// A main() that runs the tests named by the first argument, exiting 1 on any failure.
#define RC_TEST_MAIN() \
    int main(int argc, char **argv) \
    { \
        return rc_test_run(argc > 1 ? argv[1] : "") != 0; \
    }

#endif // RC_TEST_H_
