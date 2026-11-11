# Coding standards

Draft. Derived from the conventions of the existing richc (`~/dev/richc`) plus the
new requirements of this project: C17, libc wrapped behind richc, and wasm32
(with no WASI or emscripten) as a first-class target. Items still to be decided are collected under
[Open questions](#open-questions) at the end.

## Contents

- [Scope and layout](#scope-and-layout)
- [Language standard](#language-standard)
- [libc usage](#libc-usage)
- [Floating point](#floating-point)
- [Platform layer](#platform-layer)
- [Portability rules for wasm32](#portability-rules-for-wasm32)
- [Build and tooling](#build-and-tooling)
- [Ground rules](#ground-rules)
- [Error handling and assertions](#error-handling-and-assertions)
- [Memory model](#memory-model)
- [Naming](#naming)
- [Formatting](#formatting)
- [Comments](#comments)
- [Headers and includes](#headers-and-includes)
- [Templates](#templates)
- [Testing](#testing)
- [Documentation](#documentation)
- [Repository conventions](#repository-conventions)
- [Open questions](#open-questions)

## Scope and layout

One repository holds three layers. Each depends only on the layers above it in
this table.

| Directory | Library | Role |
|-----------|---------|------|
| `src/richc/core/` | `richc` | Data structures, algorithms, math, strings, formatting, the platform layer. No external dependencies. |
| `src/richc/app/` | `richc_app` | Windowing, input, gfx, image, font, UI. Built on core. |
| `src/lowpoly/` | `lowpoly` (executable) | The mesh editor itself. Built on core and app. |

Each richc layer mirrors the existing richc structure: `include/richc/...` for
public headers, `src/` for implementation, `test/` for tests. All headers are
included as `#include "richc/..."`, and both layers share the `richc/` namespace.

The core layer aims to stay source compatible with the existing richc where that
does not conflict with the rules below. Where it must diverge (for example
anything that relied on libc), the divergence is deliberate and documented.

## Language standard

C17, which clang, gcc and MSVC all implement in full. CMake asks for it with
`CMAKE_C_STANDARD 17`: `-std=c17` on clang and gcc, `/std:c17` on MSVC (plus
`/Zc:preprocessor`). C23 waits until MSVC supports it fully; the `RC_` shims
below are where its keywords will go when we move.

Minimum compilers: clang 21, gcc 15, MSVC 19.40 (Visual Studio 17.10).

### C11 and C17 features in use

| Feature | Notes |
|---------|-------|
| `_Static_assert` | Always through `RC_STATIC_ASSERT(cond)`, which supplies the message C17 requires. `static_assert` itself lives in `<assert.h>`, which wasm32 does not have. |
| `_Noreturn` | Spelled `RC_NORETURN`. |
| `_Generic` | The unsuffixed dispatchers (see [Naming](#naming)). |
| `alignas`, `alignof` | From `<stdalign.h>`. |
| `bool`, `true`, `false` | From `<stdbool.h>`. |
| Designated initialisers, compound literals, anonymous structs and unions, flexible array members, `restrict` | |

### Not allowed (C23 only)

| Feature | Use instead |
|---------|-------------|
| `static_assert` with no message | `RC_STATIC_ASSERT(cond)` |
| `[[noreturn]]` | `RC_NORETURN` |
| `[[fallthrough]]` | A `// fall through` comment, which gcc's `-Wimplicit-fallthrough` recognises |
| `[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated]]` | Nothing yet. When one is needed, add an `RC_` shim to `richc/macros.h` |
| `typeof` | Pass the type to the macro as an argument |
| Binary literals `0b1010` | Hex |
| Digit separators `1'000'000` | Write the constant out, or build it from a shift (`64u << 20`) |
| `#elifdef`, `#elifndef` | `#elif defined(X)` |
| `__VA_OPT__` | Variadic macros whose variable part is never empty |
| Unnamed parameters in function definitions | Name it and write `(void)param;` |
| `nullptr` | `NULL` |
| `constexpr` objects | `enum` for integer constants, `#define` otherwise |
| Enums with a fixed underlying type (`enum : uint8_t`) | A plain enum, stored in structs as an explicit-width integer (see [Memory model](#memory-model)) |
| `bool`, `alignas`, `alignof`, `static_assert` as keywords | The headers above, and `RC_STATIC_ASSERT` |
| `auto` type inference | Spell the type |
| `#embed` | Generate a C array at build time |
| `_BitInt` | Fixed-width `<stdint.h>` types |
| `unreachable()` from `<stddef.h>` | `RC_UNREACHABLE()` |
| `<stdckdint.h>`, `<stdbit.h>` | `rc_*_overflows_*`, `rc_clz_*`, `rc_ctz_*`, `rc_popcount_*` in `richc/ops.h` |
| Labels before declarations or at the end of a block | Put a statement or `;` after the label |
| Empty initialiser `= {}` | `= {0}` |

### Never, regardless of compiler support

- Variable-length arrays (MSVC lacks them, and the wasm stack is small).
- `long` and `unsigned long` (32-bit on Windows and wasm32, 64-bit on Linux and
  macOS). Use fixed-width types.
- `long double` (80-bit on x86 Linux, 64-bit on MSVC, 128-bit soft-float on
  wasm32).
- Plain `char` for anything but text: its signedness varies by target.
- K&R declarations, implicit `int`, empty parameter lists in declarations
  (write `(void)`: in C17, `()` means "unspecified", not "none").

## libc usage

Library code (core, app, lowpoly) never includes a libc header or calls libc
directly. richc wraps what it needs (`rc_sqrt`, `rc_sin`, `rc_memcpy`, ...),
and libc appears only inside the few richc modules that implement those
wrappers, plus native-only code (platform backends and tests). wasm32 has no
libc at all, so a libc include in library code fails the wasm build.

Library code may include only the compiler's own headers: `<float.h>`,
`<limits.h>`, `<stdalign.h>`, `<stdarg.h>`, `<stdbool.h>`, `<stddef.h>`,
`<stdint.h>`.

### Scalar math

Every math function gives bit-identical results on every target. richc
therefore implements all of them itself and uses its own versions everywhere,
native included; libm is never called (except as a reference in tests). See
[Floating point](#floating-point) for the rules that make the results
identical.

| Group | Functions | Implementation |
|-------|-----------|----------------|
| Instruction-backed | `rc_sqrt`, `rc_abs`, `rc_floor`, `rc_ceil`, `rc_trunc`, `rc_round_even`, `rc_copysign` | `static inline` in the header. One instruction on every target (`sqrtss`, `roundss`, `f32.sqrt`, `f32.floor`, ...). IEEE 754 requires these to be exact or correctly rounded, so hardware results already agree everywhere. |
| Simple | `rc_min`, `rc_max`, `rc_fmod`, `rc_round`, ... | `static inline` C, or a `.c` |
| Transcendental | `rc_sin`, `rc_cos`, `rc_tan`, `rc_asin`, `rc_acos`, `rc_atan`, `rc_atan2`, `rc_exp`, `rc_log`, `rc_log2`, `rc_pow`, `rc_cbrt`, ... | Our own C, **one source file per function and precision** (`math/sin_f32.c`, `math/exp_f64.c`), so a program that never calls `rc_exp_f64` never links its tables. |

- Each function exists in `_f32` and `_f64` forms. The unsuffixed name is a
  `_Generic` dispatcher:

  ```c
  #define rc_sqrt(x) _Generic((x), float: rc_sqrt_f32, double: rc_sqrt_f64)(x)
  ```

  An argument that is neither `float` nor `double` (an integer) is a compile
  error. A double literal silently selects the f64 path (`rc_sqrt(x * 2.0)`),
  which `-Wdouble-promotion` catches on gcc and clang.
- The instruction-backed group uses `__builtin_*` on gcc and clang. MSVC has
  no `__builtin_*`; its path uses SSE intrinsics from `<immintrin.h>`
  (`_mm_sqrt_ss`, `_mm_round_ss`, ...) and bit operations for abs and copysign.
  These only compile to one instruction with the right flags (see
  [Build and tooling](#build-and-tooling)): `-fno-math-errno` (without it gcc
  adds a compare and a libm call to every sqrt), and SSE4.1 on x86-64 for
  floor/ceil/trunc (`roundss`; without it they become libm calls).
- Transcendental implementations are ported from MIT-licensed sources (Arm
  optimized-routines, CORE-MATH, musl), never from glibc (LGPL). Each file's
  header comment names its source and licence. Where the original uses FMA or
  target intrinsics, we take its portable path.
- They are written branch-free where practical, so loops over them can
  vectorise (`-msimd128` on wasm). There is no hardware sin/cos on any target,
  so intrinsics do not help here.
- Measured on node 22: an in-module f32 sin costs 3.7 ns per call, against
  12 ns for importing JS `Math.sin`.

### Memory and strings

`rc_memcpy`, `rc_memmove`, `rc_memset` and `rc_memcmp` are
`static inline` wrappers over `__builtin_mem*` (on MSVC, the `<string.h>`
functions, which it treats as intrinsics). On wasm32, copy, move and set become
`memory.copy` / `memory.fill`. Compilers can still emit calls to `memcpy`,
`memmove`, `memset` and `memcmp` themselves (struct copies, `-O0`), so the
wasm runtime defines those four symbols.

### Everything else

`rc_arena` replaces `malloc`. `RC_ASSERT` / `RC_PANIC` replace `assert` /
`abort`. `rc_mstr_append_*` and core number formatting and parsing replace
`printf` / `strtod`. ASCII helpers in `richc/str.h` replace `<ctype.h>`. The
platform layer handles files and time.

### The wasm runtime

- The wasm build is `--target=wasm32 -ffreestanding -nostdlib`, with no
  sysroot, so no libc header is reachable. The builtins behind the
  instruction-backed functions still compile to instructions under
  `-ffreestanding`.
- The runtime itself is only the four mem symbols above. Math needs nothing
  from the host.
- The link does not use `--allow-undefined`. Only symbols declared with an
  explicit import name (see [Platform layer](#platform-layer)) may stay
  unresolved, so a stray `printf` is a link error rather than an import that
  fails when the module is loaded.
- When a host import takes or returns a float, declare it with `double`: V8
  calls a double-signature import bound to a JS function about twice as fast
  as a float one (12 vs 22 ns per call measured for `Math.sin`, node 22).

## Floating point

Floating-point results are bit-identical on every target. Geometry computed in
the browser and natively, or saved on one and loaded on the other, must agree
exactly.

- **Only IEEE basic operations are trusted to agree across targets:** `+ - * /`,
  sqrt, conversions, and the rounding functions. Every other function is our
  own (see [Scalar math](#scalar-math)).
- **No FMA contraction.** Build with `-ffp-contract=off`. clang otherwise
  fuses `a * b + c` into one fused multiply-add (one rounding instead of two) on
  ARM64, Apple Silicon included, and on x86-64-v3. wasm has no FMA at all, so
  results would differ. There is no `rc_fma`.
- **No fast-math**, on any compiler: never `-ffast-math`,
  `-funsafe-math-optimizations` or `-ffinite-math-only`; on MSVC always
  `/fp:precise`, never `/fp:fast` or `/fp:contract`.
- **No x87.** `richc/macros.h` has `RC_STATIC_ASSERT(FLT_EVAL_METHOD == 0)`, so
  32-bit x86 builds without SSE2 fail to compile.
- **Never change the rounding mode, or enable flush-to-zero** or
  denormals-are-zero.
- **Never depend on NaN payloads or NaN sign bits.** wasm leaves them
  unspecified.
- **Never convert an out-of-range float to an integer.** It is undefined in C,
  and the targets really do differ (x86 gives `INT_MIN`, wasm saturates). Range
  check first, or use a richc conversion helper with defined saturation.
- **Math tests check accuracy and identity separately.** Accuracy: f32
  functions are tested exhaustively (all 2^32 inputs) against libm in double
  precision, f64 functions on large random and edge-case samples, each against
  a stated maximum ulp error. Identity: exact bit patterns for a set of inputs,
  recorded once as golden values, checked on every target.

## Platform layer

All contact with the outside world goes through one small platform interface in
core. Nothing outside the platform backends knows which target it is running on.

- The interface is a set of private headers of plain functions, one per
  capability, in core's `src/` (`heap.h` for page reservation and commit;
  later monotonic time, debug output and so on). It should stay as small as
  possible: one function per primitive capability, with richc building
  everything else on top.
- Each header is implemented once per target, in a file of the same name under
  `src/platform/<target>/`: `posix`, `win32`, `wasm` (so `heap.h` is
  implemented by `src/platform/posix/heap.c` and its siblings). The build
  compiles one target's directory. No vtables and no runtime dispatch, the
  same rule the gfx backends follow.
- Target `#if`s appear only in the platform backends, `richc/macros.h` and
  the (native-only) test framework.
- The native backends call the OS API (Win32, or POSIX on Linux and macOS). No
  OS header ever appears in a public richc header.
- The wasm backend declares host functions as imports and implements them in a
  matching JavaScript file kept beside it:

  ```c
  __attribute__((import_module("richc"), import_name("time_now_ns")))
  uint64_t rc_wasm_time_now_ns_(void);
  ```

  Every import is declared in the wasm backend `.c` only, never in a header.
  The import module name is `richc` for core and app, `lowpoly` for the app's
  own imports.
- Entry points exported to the host are marked with
  `__attribute__((export_name("...")))`. Nothing is exported implicitly; link
  with `--no-entry` and without `--export-all`.

## Portability rules for wasm32

- **Pointers and `size_t` are 32-bit.** Never assume 64-bit pointers. Sizes,
  counts and offsets are `uint32_t`, which also matches the richc index
  convention.
- **Little-endian everywhere.** We may assume it, but say so with a
  `RC_STATIC_ASSERT` in `richc/macros.h` rather than silently.
- **No unaligned access through casts.** It works on wasm and x86 but is
  undefined in C. Reinterpret bytes with `rc_memcpy` (it compiles to a single
  load).
- **Function pointers must match exactly.** Calling through a pointer of a
  different signature "works" on native but traps on wasm (`call_indirect`
  checks the type). Never cast between function pointer types.
- **The stack is small** (64 KiB by default under wasm-ld). Keep large buffers
  out of local variables: a scratch arena is the place for anything over about
  4 KiB.
- **Nothing blocks.** In the browser the host drives the frame loop. App code is
  callback-driven (init, event, update, render) and never spins waiting for an
  event, a file or a timer.
- **No threads on wasm.** Code must work single-threaded. Threading
  primitives exist only for native backends (see Open questions).
- **No global constructors or destructors**, no `atexit`. Static data is
  zero-initialised or constant-initialised.
- **Linear memory only grows.** Memory handed back to the platform on wasm is
  reused by later reservations, never returned to the host.

## Build and tooling

- CMake 3.25 or later, and Ninja. One build directory per compiler and target; never mix
  compilers in one build tree.
- In VSCode, the CMake Tools variants (`.vscode/cmake-variants.yaml`) are the
  four usual build types plus Wasm, which applies `cmake/wasm32.cmake`. Each
  variant builds in its own `build/<variant>` directory.
- wasm builds use a toolchain file (`cmake/wasm32.cmake`): clang with
  `--target=wasm32 -ffreestanding -nostdlib -msimd128`, linked by `wasm-ld`
  with `--no-entry`. This needs LLVM's lld installed (on Ubuntu: `lld-21`).
  `-msimd128` lets clang vectorise loops (a loop of `rc_sqrt` becomes
  `f32x4.sqrt`); every current browser supports wasm SIMD.
- Floating point, every gcc/clang build including wasm: `-ffp-contract=off
  -fno-math-errno`. Native x86-64 also uses `-march=x86-64-v2` (for SSE4.1).
  MSVC: `/fp:precise`. See [Scalar math](#scalar-math) and
  [Floating point](#floating-point).
- MSVC also needs `/Zc:preprocessor` (CMake adds `/std:c17`).
- The wasm build gathers the web page in `<build>/site`: `lowpoly.wasm` plus
  `src/lowpoly/web/`. To try it locally, serve that directory over http (for
  example `python3 -m http.server -d build-wasm/site`); browsers will not load
  wasm from `file://`.
- Every push to `main` runs `.github/workflows/deploy.yml`: a native debug
  build (which runs the tests), then a wasm release build, deployed to GitHub
  Pages.
- Dead-code stripping: gcc/clang native use `-ffunction-sections
  -fdata-sections` with `--gc-sections` (Linux) or `-dead_strip` (macOS);
  MSVC `/Gy /Gw` with `/OPT:REF`. wasm-ld strips unused functions by default.
  One source file per function means a static library only pulls in what is
  called, whatever the linker.
- Warnings: clang/gcc `-Wall -Wextra -Wpedantic`, plus `-Wconversion`,
  `-Wshadow` and `-Wdouble-promotion`; MSVC `/W4`. The tree builds
  warning-free on all three compilers.

## Ground rules

- **Write exhaustive tests** for new code, including the awkward edge cases:
  empty containers, a single element, duplicates, maximum sizes, zero or NULL
  inputs where they are allowed.
- **Run the full test suite after every change**, and build the wasm target.
  Nothing is done until the tests are green and the wasm build links.
- **Always return a value (or a small struct) over an out-parameter.** Use an
  out-parameter only when the alternative is genuinely worse: a caller-owned
  buffer the callee fills, or a large object that should not be copied.
- **Hoist closed-set dispatch out of hot loops.** When a per-element loop
  branches on a small fixed set (pixel format, filter type, backend), switch
  once outside the loop and dispatch to a small named helper per case. The
  validated-impossible `default` gets `RC_UNREACHABLE()`.
- **Data is just data.** Structs are plain bytes: no hidden ownership, no
  back-pointers, no function-pointer tables standing in for objects. Operations
  take their context (the arena, the pool) as parameters.
- **Zero is a valid value.** A zero-initialised container, handle or descriptor
  is ready to use, and zero fields in a descriptor mean "the default".

## Error handling and assertions

- **Assert internal preconditions.** If a function relies on a precondition it
  does not handle and report (non-NULL pointer, non-zero size, in-range index),
  guard it with `RC_ASSERT`. It traps in debug builds and costs nothing in
  release.
- **`RC_PANIC` for unrecoverable failures** that must stop the program even in
  release, such as an arena running out of reservation.
- **`RC_UNREACHABLE()` for paths that cannot occur** given preconditions already
  enforced. It traps in debug and is an optimiser hint in release.
- **Untrusted input returns an error, never traps.** Anything read from a file,
  the network or the host (OBJ text, PNG bytes, a chunk length) is validated,
  and malformed data produces an error result. Watch for overflow on untrusted
  sizes: compute in a wider type, or use the `rc_*_overflows_*` checks, and
  reject before anything wraps.
- **Error results are for conditions a caller genuinely handles.** They are an
  enum (first enumerator `..._OK` / zero) or a small result struct.
- On wasm a trap is the `unreachable` instruction, which the host sees as a
  `RuntimeError`. The host prints the reason passed through the platform layer
  before the trap, where one is available.

## Memory model

- **Arenas are the only allocator.** Allocating functions take `rc_arena *arena`
  as their last parameter. A scratch arena passed by value comes after it, as
  the very last parameter.
- **Never pass the same arena as both `arena` and by-value `scratch`** to one
  call: the two cursors overwrite each other. A function taking both asserts
  `!rc_arena_is_same(arena, &scratch)`. In tests, make a second arena.
- **Arena allocations never return NULL.** Never check them.
- **Choose the container by capability:**
  - fixed count, mutable elements: **span**;
  - the holder grows or manages it: **array**;
  - read-only access: **view**.
  A struct field says what its consumers do: a view for results they only read,
  a span where they keep mutating.
- **A span's backing comes from an array.** Build an `rc_array_<T>`, size it with
  `_resize` (which returns the span), then initialise each slot in place through
  the span accessors.
- **Element access always goes through the typed accessors**
  (`rc_{view,span,array}_<name>_{get,set,at}`), never `.data[i]`. They
  bounds-check in debug builds.
- **Prefer indices to pointers**, including as return values: a lookup returns a
  `uint32_t` index, or `RC_INDEX_NONE` for "not found". Indices survive growth
  and relocation, are half the size of a 64-bit pointer, and serialise trivially.
- **A pointer to a whole allocation is stable; a pointer to an element is not.**
  Re-fetch by index after any push to the same array.
- **A membership set is an `rc_bitset`** when iteration cost allows. A sparse set
  scanned every sweep is better as a list.
- **Struct members never have enum types.** Store an explicit-width integer
  (`uint8_t`, or `rc_view_u8` for a row of them) with a comment naming the enum,
  and cast back at the reading accessor or switch head. This keeps the layout
  exact and `-Wswitch` honest.

## Naming

- **snake_case everywhere.** No CamelCase.
- **Prefixes:** `rc_` on every public richc type and function, `RC_` on every
  public macro. lowpoly is an app, not a reusable library, so it has no
  prefix: its functions are named after their module (`editor_run` in
  `editor.c`).
- **A trailing `_` marks an internal item exposed in a public header** (macros,
  struct members, functions not part of the API). It is not used for things
  private to a `.c` file: `static` already hides those.
- **Template control macros** (`RC_ARRAY_TYPE`, `RC_ARRAY_NAME`) take no trailing
  `_`. The name-building macros a template generates do, and are `#undef`d at
  the end of the header.
- **Template types are container-first:** `rc_array_u32`, `rc_view_vec3f`,
  `rc_pool_face`. Functions follow the same order: `rc_array_u32_push()`.
- **Functions are named by role:**
  - `rc_<type>_make(...)`, `rc_<type>_make_<thing>(...)`: constructor returning
    `rc_<type>` by value.
  - `rc_<type>_from_<other>(...)`: construct from a value of a different type.
  - `rc_<type>_as_<other>(...)`: reinterpret without allocating; the source
    stays usable.
  - `rc_<type>_to_<other>(...)`: convert by moving; the source is reset so only
    the result owns the resource.
  - `rc_<type>_is_<predicate>(...)`: returns `bool`.
- **Scalar type suffixes:** `u8`..`u64`, `i8`..`i64`, `f32`, `f64`. Inline
  math helpers always carry one (`rc_min_i32`, `rc_sqrt_f32`), which also
  avoids the Windows `min`/`max`/`near`/`far` macros. The unsuffixed name
  (`rc_sqrt`, `rc_min`) is a `_Generic` dispatcher over the suffixed forms.
- **The first enumerator is the zero/default.** Tagged-union discriminant enums
  are named `..._type`, not `..._kind`.
- **Public structs are tagged with their typedef name**
  (`typedef struct rc_foo {...} rc_foo;`) so they can be forward-declared.

## Formatting

- 4-space indentation, no tabs. Opening brace of a function on its own line;
  other braces on the same line. No cuddled `else` (i.e. Stroustrup style).
- **Don't omit braces** around `if`/`else` blocks.
- **Comments wrap at 100 columns**, not 80. Lines of code aim for the same
  margin, but it is not enforced strictly: a line that reads better slightly
  long stays long.
- **Compound literals:** a space between the closing `)` of the type and the
  `{`; no space after the `{` or before the `}` (`(rc_str) {.data = p, .len = n}`,
  `{0}`, never `{ 0 }`).

  ```c
  return (rc_str) {
      .data = s.data,
      .len = count
  };
  ```

- **Prefer designated initialisers** for structs and compound literals.
- **Split a compound literal one entry per line** when the entries are parallel
  and it reads more clearly (a per-component vector result). Keep trivial ones
  on one line.

  ```c
  return (rc_vec2i) {
      a.x + b.x,
      a.y + b.y
  };
  ```

- **Build a composite value in one place:** gather the parts into locals, then
  construct the whole value with one designated initialiser at the return.
  Never declare a partly filled struct and patch fields along the way.
- **Zero-initialise with `{0}`** where a valid empty value exists.
- **One variable per declaration line.**
- **A blank line between the variable declarations at the top of a block and
  the first statement.**
- **Long functions are split into blocks** separated by a blank line, each with
  a comment directly above saying what it does, for visual relief.
- **Two blank lines** between functions in a .c file.

## Comments

- `//` style throughout, including file banners. (The existing richc headers
  still use `/* */` banners in places; new code we write now does not.)
- File banners look like:
  ```c
  // arena.h
  //
  // The bump allocator that owns every allocation in richc.
  //
  // An arena is a chain of chunks, each a run of 64 KiB pages taken from one
  // (etc)
  
  ```
- ASCII only in code and documentation: `x` not the multiplication sign, `->`
  not an arrow, `>=` not the glyph, `2^32` not a superscript, plain hyphens,
  straight quotes.
- Every file starts with a short comment saying what it is for. It does not
  repeat the API.
- Every function declaration in a header has a short (one or two line) comment
  saying what it does.
- A comment is always preceded by a blank line (except at the start of a
  block). Short trailing comments on struct members are allowed if the member
  meaning/purpose is not obvious, but avoid them.
- Comment the why and the how, not the what. A comment that restates its line
  is deleted, not shortened.
- No single word orphaned on the last line of a comment. Move the whole final
  sentence down to the next line instead.
- Keep them short: one line per declaration by default. More only for genuinely
  non-obvious invariants (gating conditions, ownership).
- British English, a touch light-hearted rather than dry, first person plural
  where not passive ("we hand these around by index").
- Comment only what the code does, never what it deliberately does not do
  because of a change elsewhere. That belongs in the journal.

## Headers and includes

- **Include guards, never `#pragma once`.** The guard is the path under
  `include/richc/`, upper-cased, with `/` and `.` turned into `_`:
  `RC_<PATH>_H_`. So `richc/arena.h` -> `RC_ARENA_H_`, `richc/math/vec2i.h` ->
  `RC_MATH_VEC2I_H_`, `richc/app/app.h` -> `RC_APP_APP_H_`. lowpoly headers
  use their path under `src/lowpoly/` with `LOWPOLY_` in front:
  `editor.h` -> `LOWPOLY_EDITOR_H_`.
- **Template headers have no include guard** (they are included once per type).
  Once-only sections inside them use the normal guard naming.
- **Include order:** in a `.c`, its own header first, alone, then a blank line.
  Then system headers `<...>`, then project headers `"richc/..."`, each group in
  alphabetical order. A header follows the same order minus the own-header line.
- **No superfluous includes:** do not include what another included header
  already provides.
- **Forward-declare instead of including** when only a pointer to a type is
  needed: `typedef struct rc_foo rc_foo;` (a repeated typedef is valid since
  C11).
- **Every header is self-sufficient**, proved by being the first include of its
  own `.c`.
- **CMake source lists are alphabetical**, sources before headers, and entries
  in subdirectories after the files at the same level.

## Templates

Generic containers and algorithms are preprocessor templates. They do not live in
their own `template/` directory, but are named as `thing.template.h`. The caller defines
control macros and includes the header, which generates type-specific code and
`#undef`s everything so it can be included again.

```c
#define RC_ARRAY_TYPE uint32_t
#define RC_ARRAY_NAME u32
#include "richc/array.template.h"   // rc_view_u32, rc_span_u32, rc_array_u32
```

Each element type is instantiated once, in a header-guarded convenience header
(`richc/array/u32.h`, `richc/math/array/vec3f.h`), so every includer shares the
same generated type. Instantiate `array.template.h` directly only for a one-off
type private to a single `.c`.

## Testing

- **richc:** every module has a `test_<module>.c` in its layer's `test/`
  directory, built into one test executable per layer with a single
  `RC_TEST_MAIN` entry point.
- **lowpoly:** there is no separate test executable. Tests sit at the bottom
  of the source file they test, under `#ifdef LOWPOLY_TESTS`, so they can reach
  `static` functions, and `lowpoly --test [filter]` runs them.
- Tests run automatically as a post-build step, and a failure fails the build.
  `RICHC_BUILD_TESTS` and `LOWPOLY_BUILD_TESTS` (both on by default natively)
  build and run them; `RICHC_BUILD_APP` (on by default) builds the app layer
  as well as core, and lowpoly needs it.
- Tests are native only and never compiled for wasm. The test framework uses
  `setjmp` / `longjmp` to abandon a failing test (and still deinit its
  fixture), and `<stdio.h>` to report.
- Bundled fixtures live in `test/data/` and are staged next to the test binary.
- Purely graphical code that cannot be asserted on is exercised through example
  programs instead.

## Documentation

- `README.md` and `docs/`: plain markdown (headings, tables, fenced code),
  well organised and exhaustive.
- Document only what exists, when it exists. Planned features stay out of the
  reference docs (this file and `PLAN.md` excepted).
- Engineering decisions and mistakes go in `JOURNAL.md`, as in the TypeScript
  lowpoly.

## Repository conventions

- Commits are in the author's name only, with no co-author or tool attribution.
- Commit messages are terse and human: no trailing full stop, a few related
  items per commit is fine.
- `CLAUDE.md` files are local only: never committed, excluded by the bare
  `CLAUDE.md` line in `.gitignore`, which is itself not committed.

## Open questions

1. **wasm coverage.** Unit tests are native only, so the wasm runtime, the
   wasm platform backend and, most importantly, the claim that math gives
   identical bits on wasm are never checked there. Option: a small check
   module without the test framework (no `setjmp`) that runs under node and
   compares the golden math values, plus a few smoke checks.
2. **Float formatting and parsing.** Shortest round-trip output (Ryu-style) or
   fixed-precision only? Needed for OBJ/glTF export and the measurement box.
   (Library code cannot call `snprintf`, so this is ours either way.)
3. **Threads.** Keep the existing `thread/` module and scheduler as native-only,
   or leave them out of the new core until there is a need?
4. **Geometry precision.** The TypeScript editor uses doubles throughout. richc
   math is `float` only. Does the lowpoly document store `f32` or `f64`
   positions? Does core need `vec3d`-style types?
5. **`-Werror`.** On for all builds, or CI only?
6. **Windows on ARM.** The MSVC path of the instruction-backed math uses SSE
   intrinsics, which are x64 only. ARM64 Windows would need a NEON path. In
   scope?
7. **MSVC verification.** There is no MSVC on this machine. The C17
   shims above (`_Noreturn`, `_Static_assert`) and the MSVC math
   path and flags are written from documentation, not tested. A Windows CI
   build from the first commit would confirm both.
