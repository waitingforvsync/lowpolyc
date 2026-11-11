// macros.h
//
// The base of everything: compiler shims, target assumptions, token helpers and
// the assertion macros.

#ifndef RC_MACROS_H_
#define RC_MACROS_H_

#include <float.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ---- compile-time checks ----

// A compile-time RC_ASSERT. C17 needs a message, so the condition is its own.
// (static_assert itself lives in <assert.h>, which wasm32 does not have.)
#define RC_STATIC_ASSERT(cond) _Static_assert(cond, #cond)

// ---- target assumptions ----

// No x87: every float operation rounds to its own type, on every target.
// FLT_EVAL_METHOD comes from <float.h>.
RC_STATIC_ASSERT(FLT_EVAL_METHOD == 0);

// Little-endian everywhere; we say so here rather than assume it silently.
// __BYTE_ORDER__ is predefined by clang and gcc; MSVC has no equivalent, but
// every MSVC target is little-endian.
#if defined(__BYTE_ORDER__)
RC_STATIC_ASSERT(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
#endif

// ---- token helpers ----

// Paste two tokens together, expanding macros first.
#define RC_CONCAT_(a, b) a##b
#define RC_CONCAT(a, b)  RC_CONCAT_(a, b)

// Turn a token into a string literal, expanding macros first.
#define RC_STRINGIFY_(x) #x
#define RC_STRINGIFY(x)  RC_STRINGIFY_(x)

// ---- sentinels ----

// The "not found" / "invalid" index.
#define RC_INDEX_NONE ((uint32_t)-1)

// ---- attributes ----

// The function never returns. The C11 keyword, named to match the other shims.
#define RC_NORETURN _Noreturn

// ---- traps ----

// RC_ASSERT stops in the debugger where it can; RC_PANIC always terminates.
#if defined(_MSC_VER) && !defined(__clang__)
#  include <intrin.h>
#  define RC_DEBUG_BREAK_() __debugbreak()
#  define RC_TRAP_()        __fastfail(7)
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
#  define RC_DEBUG_BREAK_() __builtin_debugtrap()
#  define RC_TRAP_()        __builtin_trap()
#else
#  define RC_DEBUG_BREAK_() __builtin_trap()
#  define RC_TRAP_()        __builtin_trap()
#endif

// ---- assertions ----

// Internal precondition: traps in debug, evaluated and discarded in release.
// An expression, so it can sit on the left of a comma: *(RC_ASSERT(i < n), p + i)
#ifndef NDEBUG
#  define RC_ASSERT(cond) ((cond) ? (void)0 : (RC_DEBUG_BREAK_(), (void)0))
#else
#  define RC_ASSERT(cond) ((void)(cond))
#endif

// Unrecoverable failure: terminates in debug and release alike.
#define RC_PANIC(cond) ((cond) ? (void)0 : (RC_TRAP_(), (void)0))

// A path that cannot be taken: traps in debug, an optimiser hint in release.
#if defined(_MSC_VER) && !defined(__clang__)
#  define RC_UNREACHABLE_HINT_() __assume(0)
#else
#  define RC_UNREACHABLE_HINT_() __builtin_unreachable()
#endif

#ifndef NDEBUG
#  define RC_UNREACHABLE() (RC_DEBUG_BREAK_(), RC_UNREACHABLE_HINT_())
#else
#  define RC_UNREACHABLE() RC_UNREACHABLE_HINT_()
#endif

#endif // RC_MACROS_H_
