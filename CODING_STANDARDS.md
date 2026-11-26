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
| `src/richc/shader/` | `richc_shader` | The WGSL translator, shader packages and the shader tool. Built on core. |
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
| `<stdckdint.h>`, `<stdbit.h>` | `rc_*_overflows_*`, `rc_clz_*`, `rc_ctz_*`, `rc_popcount_*` in `richc/math/math.h` |
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
| Instruction-backed | `rc_sqrt`, `rc_abs`, `rc_floor`, `rc_ceil`, `rc_trunc`, `rc_round_even`, `rc_copysign` | `static inline` in `richc/math/math.h`. One instruction on every target (`sqrtss`, `roundss`, `f32.sqrt`, `f32.floor`, ...). IEEE 754 requires these to be exact or correctly rounded, so hardware results already agree everywhere. |
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
- Transcendental implementations are our own, written from a specification of
  their results, accuracy and constraints (`tools/math/spec.md`), never ported
  from another libm. Coefficients come from our own minimax fits (Sollya's
  `fpminimax`) and constants and tables from arbitrary-precision arithmetic
  (mpmath), by the tools in `tools/math/`, which also regenerate the golden
  test values and check speed and that every target gives the same bits. Each
  file's banner says how it works: the reduction, the approximation and its
  fitted error, and where the error budget goes. Extra precision comes from
  error-free transformations (two-sums, Dekker products), never FMA.
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
`printf` / `strtod`, and `rc_print` / `rc_print_err` write the result to the
standard streams. ASCII helpers in `richc/str.h` replace `<ctype.h>`. The
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
  recorded once as golden values, checked on every target. The identity tests
  run with the other tests; the accuracy ones take minutes, so they live in
  `richc_math_accuracy`, which is built natively but run by hand after changing
  a function.

## Platform layer

All contact with the outside world goes through one small platform interface in
core. Nothing outside the platform backends knows which target it is running on.

- The interface is a set of private headers of plain functions, one per
  capability, in core's `src/platform/` (`platform/heap.h` for page
  reservation and commit, `platform/fs.h` for files, and so on), included as
  `"platform/heap.h"`. It should stay as small as possible: one function per
  primitive capability, with richc building everything else on top.
- Each header is implemented once per target, in a file of the same name under
  `src/platform/<target>/`: `posix`, `win32`, `wasm` (so `platform/heap.h` is
  implemented by `src/platform/posix/heap.c` and its siblings). The build
  compiles one target's directory. No vtables and no runtime dispatch, the
  same rule the gfx backends follow.
- Where Linux and macOS differ, the header is implemented in `linux` and
  `macos` instead of `posix`. So far that is only `platform/futex.h`, the one
  call the locks in `thread/` are built on.
- The app layer follows the same pattern, but names its targets by windowing
  system, since macOS is POSIX but not X11: `src/platform/x11/app.c` and
  `src/platform/wasm/app.c` both implement `rc_app_run` from `app.h`. The X11
  backend links libX11 (on Ubuntu, the headers are in `libx11-dev`).
- Target `#if`s appear only in the platform backends, `richc/macros.h`, the
  compiler's atomics in `richc/thread/` and the test framework.
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
- **The stack is small** (64 KiB by default under wasm-ld, and 256 KiB for
  every thread richc starts, on every target). Keep large buffers out of local
  variables: a scratch arena is the place for anything over about 4 KiB.
- **Nothing blocks.** In the browser the host drives the frame loop. App code is
  callback-driven (init, event, update, render) and never spins waiting for an
  event, a file or a timer.
- **Code must work single-threaded.** wasm has threads only on a page that is
  cross-origin isolated, so they are a build of their own (see
  [Build and tooling](#build-and-tooling)). The atomics and locks in
  `richc/thread/` work on every target, wasm included; with one thread, a wait
  that nothing could end panics, and a wait with a timeout passes the time.
- **A page's main thread cannot block,** so with threads, its waits spin.
  Only workers sleep in `memory.atomic.wait32`. The file store and anything
  else the page supplies live on the main thread, and calling them from
  another traps.
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
- `RICHC_WASM_THREADS` makes a wasm build with threads: `-matomics
  -mbulk-memory`, and memory that is imported, shared, and at most
  `RICHC_WASM_MAX_MEMORY` (1 GiB by default). Its threads run on a pool of 16
  workers that `threads.js` starts before the module runs, each on a static
  256 KiB stack. A page can run it only when cross-origin isolated, and its
  host must call `startThreads` before anything else in the module, even if
  nothing starts a thread, as that gives the main thread its thread-local
  storage.
- Floating point, every gcc/clang build including wasm: `-ffp-contract=off
  -fno-math-errno`, which clang-cl takes as `/clang:-ffp-contract=off
  /clang:-fno-math-errno`, as it ignores them otherwise. Native x86-64 also
  uses `-march=x86-64-v2` (for SSE4.1), clang-cl included. MSVC:
  `/fp:precise`, but not clang-cl, where it would turn contraction back on.
  See [Scalar math](#scalar-math) and [Floating point](#floating-point).
- MSVC also needs `/Zc:preprocessor` (CMake adds `/std:c17`).
- The wasm build gathers the web page in `<build>/site`: `lowpoly.wasm` plus
  `src/lowpoly/web/`. A build with threads makes `lowpoly-threads.wasm`
  instead, so the two sites can be laid one over the other, and the page loads
  the threaded one only when it is cross-origin isolated. To try it locally,
  `tools/serve_site.sh start` serves `build/wasm/site` and
  `build/wasm-threads/site` together, with the headers that isolate the page;
  browsers will not load wasm from `file://`.
- Every push to `main` runs `.github/workflows/deploy.yml`: a native debug
  build, a wasm release build and a wasm release build with threads, each
  running the tests, then both wasm builds' tests again in headless Chrome.
  It deploys both wasm builds' sites, laid together, to GitHub Pages, which
  cannot send the isolating headers, so there the page always runs the build
  without threads.
- Every push to any branch runs `.github/workflows/ci.yml`, which builds core
  and shader and runs their tests on macOS (Apple Silicon), Windows with MSVC
  (debug and release) and clang-cl, Linux ARM64 with clang (release), Linux
  x64 with gcc (release), and Linux x64 under ThreadSanitizer.
- Dead-code stripping: gcc/clang native use `-ffunction-sections
  -fdata-sections` with `--gc-sections` (Linux) or `-dead_strip` (macOS);
  MSVC `/Gy /Gw` with `/OPT:REF`. wasm-ld strips unused functions by default.
  One source file per function means a static library only pulls in what is
  called, whatever the linker.
- Warnings: clang/gcc `-Wall -Wextra -Wpedantic`, plus `-Wconversion`,
  `-Wshadow` and `-Wdouble-promotion`; MSVC `/W4`. The tree builds
  warning-free on all three compilers. CI makes warnings errors
  (`CMAKE_COMPILE_WARNING_AS_ERROR`); local builds leave them as warnings, so
  work in progress still builds.
- One warning is off: MSVC's C4189, for anything that links `richc_test`.
  `RC_CHECK`'s `_Generic` never evaluates its operand, but MSVC still makes any
  compound literal in it (every `RC_STR`) and warns that it goes unused.

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
  release, where its condition is not evaluated, so the condition must only
  look, never do.
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
  enum, whose zero is no error (`RC_FILE_ERROR_NONE`), or a small result struct.
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
- **A fixed-size span is made directly:** `rc_span_<T>_make(n, arena)` (or
  `_make_zero`), then each slot is initialised in place through the span
  accessors. Use an array only when it needs to grow.
- **Element access always goes through the typed accessors**
  (`rc_{view,span,array}_<name>_{get,set,at}`), never `.data[i]` or `RC_AT`.
  They bounds-check in debug builds. `RC_AT` is not type-checked and exists only
  as a convenience for code outside richc and lowpoly.
- **Always prefer indices to pointers**, in return values, parameters and
  struct members alike. A lookup returns a `uint32_t` index, or `RC_INDEX_NONE`
  for "not found", never a pointer or `NULL`, and the caller reads the element
  through the typed accessors. Indices survive growth and relocation, are half
  the size of a 64-bit pointer, and serialise trivially. The pointer an `_at`
  accessor gives is only for changing that element in place, there and then.
- **A pointer to a whole allocation is stable; a pointer to an element is not.**
  Re-fetch by index after any push to the same array.
- **A membership set is an `rc_bitset`** when iteration cost allows. A sparse set
  scanned every sweep is better as a list.
- **Struct members never have enum types.** Store an explicit-width integer
  (`uint8_t`, or `rc_view_u8` for a row of them) with a comment naming the enum,
  and cast back at the reading accessor or switch head. This keeps the layout
  exact and `-Wswitch` honest.

### Using arenas well

Every arena takes its chunks from one linear page heap, which is all wasm
offers. An allocation grows in place only while it is the last in its arena,
and its chunk only grows into the pages straight after it, when they are free or
the top of the heap. Anything else moves: one copy, cheap next to whatever made
the data, but the space it leaves stays allocated until the arena is reset.

- **Arenas follow lifetimes, not types.** A few arenas, one per lifetime (the
  app, the document, a frame, a load), each holding what dies together.
- **Reset, don't release, for work that repeats.** A reset arena keeps its
  chunks, so once warmed up it needs no new pages and nothing in it moves.
  Released pages stay committed for reuse only up to 16 MiB in all; natively,
  the rest go back to the OS, and come back as page faults when used again.
- **Make long-lived arenas early, and release short-lived ones in the reverse
  order they were made.** The long-lived sit low and stay put; the short-lived
  come and go at the top, where their pages go straight back to the heap and
  the chunk below can grow in place again.
- **Reserve sizes you know**, exactly or as an upper bound: `_make(n, arena)`
  for an array, `minimum_capacity` for inflate. That is the only way a buffer
  never grows at all.
- **Nothing else goes in an arena while an array in it grows.** Temporaries go
  in a by-value scratch arena; build the array first, then allocate what comes
  after it.
- **Don't grow big things in several arenas at once.** Side by side, they block
  each other, and every blocked growth moves.
- **An arena for one big growing buffer can take a large chunk up front**
  (`rc_arena_make(size)`). Growth within its own chunk never moves, whatever
  its neighbours do, though on wasm memory grows to that size at once.
- **Scratch is passed by value.** Its allocations vanish when the callee
  returns, and any chunk it adds stays in the owner's chain for next time.
- **`rc_arena_save` and `rc_arena_restore` handle nested temporary work in one
  arena, and errors.** Restoring gives back everything since the mark, including
  space a growing array moved out of.

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
    `rc_<type>` by value, allocating from the arena if it takes one
    (`rc_array_u32_make(capacity, arena)`, `rc_span_u32_make(n, arena)`).
  - `rc_<type>_from_<other>(...)`: construct from a value of a different type.
    Wrapping memory the value does not own is `from_ptr(data, num)`
    (`rc_view_u32_from_ptr`, `rc_str_from_ptr`).
  - `rc_<type>_as_<other>(...)`: reinterpret without allocating; the source
    stays usable.
  - `rc_<type>_parse_<other>(...)`: read an `<other>` from the start of the
    source, returning it in a small struct with the rest of the source and an
    error (`rc_str_parse_i32`). The matching `as_` reads all of the source.
  - `rc_<type>_to_<other>(...)`: convert by moving; the source is reset so only
    the result owns the resource.
  - `rc_<type>_is_<predicate>(...)`: returns `bool`.
  - `rc_<type>_first(x, n)`, `_last`, `_drop_first`, `_drop_last`, and
    `_substr` / `_subview` / `_subspan(x, start, end)`: a narrower value of the
    same type over the same memory. They never allocate or copy, and clamp
    out-of-range counts and indices. Plain nouns, not `make_`, since they read
    as parts of `x` rather than new objects.
  - `rc_<type>_release(x, ...)`: free what `x` owns and zero it.
- **Scalar type suffixes:** `u8`..`u64`, `i8`..`i64`, `f32`, `f64`. Inline
  math helpers always carry one (`rc_min_i32`, `rc_sqrt_f32`), which also
  avoids the Windows `min`/`max`/`near`/`far` macros. The unsuffixed name
  (`rc_sqrt`, `rc_min`) is a `_Generic` dispatcher over the suffixed forms.
- **An enum and its values share one stem:** `rc_wgsl_space` holds
  `RC_WGSL_SPACE_PRIVATE`, and `rc_file_error` holds `RC_FILE_ERROR_NOT_FOUND`.
  Pick whichever name reads better and make the other match it. If the stem
  already names a struct, lengthen the enum's name and its values with it:
  `rc_wgsl_layout_rule` holds `RC_WGSL_LAYOUT_RULE_STD140`.
- **A sum type's discriminator is a field called `type`**, and its enum is
  the struct's name with `_type`: `rc_wgsl_token` has `type`, an
  `rc_wgsl_token_type`, which holds `RC_WGSL_TOKEN_TYPE_IDENT`. Its zero is
  `NONE`, as for every enum.
- **No sum type's name ends in `_type`**, since its discriminator's enum would
  then end in `_type_type`. Name it for what it holds instead: a WGSL type's
  description is an `rc_wgsl_typeinfo`, whose `type` is an
  `rc_wgsl_typeinfo_type`.
- **Field names say what they hold.** A field is never just `kind`, `a` or
  `b`, and is only `type` when it is a sum type's discriminator: write
  `operand_type`, `texture_class`, `columns`. One field holds one thing; if
  its meaning would change with the case, split it into fields named for each.
- **The first enumerator, 0, is the default or uninitialised value, named
  `NONE`** (`RC_WGSL_TOKEN_TYPE_NONE`, `RC_GFX_LOAD_OP_NONE`), so a zeroed struct
  holds it. Where an API has a default, `NONE` means it. For an error enum,
  `NONE` is no error. Bit flags and named constants in an `enum` are not
  enumerations, and have no `NONE`.
- **Public structs are tagged with their typedef name**
  (`typedef struct rc_foo {...} rc_foo;`) so they can be forward-declared.

## Formatting

- 4-space indentation, no tabs. Opening brace of a function on its own line;
  other braces on the same line. No cuddled `else` (i.e. Stroustrup style).
- **Don't omit braces** around `if`/`else` blocks.
- **Comments wrap at 100 columns**, not 80.
- **Code lines have a hard limit of 140 characters.** A line that fits stays
  whole, signatures and calls included; one that doesn't is split at a natural
  place, such as between arguments.
- **Code reads for itself.** Well-named symbols and small functions should
  make the code understandable without its comments. Comments are brief, sit
  between blocks for visual relief, and say what the names can't.
- **No big conditions.** An `if` condition should not be a complicated boolean
  expression. An early-out condition is split into several `if`s, one each;
  any other becomes a small static function, named for what it tests. Two
  simple terms on one line are fine.

  ```c
  if (type == 0) {
      return 0;
  }

  if (!rc_wgsl_is_float_vector(c, type)) {
      return 0;
  }
  ```

- **A split boolean expression ends each line with its `&&` or `||`**, and the
  next line is indented one level further. Before splitting one, see whether a
  helper makes it short enough not to need it.
- **Every local is given its value where it is declared.** Initialisers run in
  order, so they can do work in turn, as a parser reading tokens does. If an
  early out has to come before a value can be worked out, move what follows
  into a small helper taking what it needs, or use a ternary.
- **A blank line follows an `if` statement**, with its whole else chain, when
  another statement follows it in the same block.
- **A table indexed by an enum names each index:**
  `[RC_WGSL_SPACE_FUNCTION] = RC_STR_INIT("function")`, so every value stays
  beside its key and reordering the enum can't misalign the table. Assert the
  index is in range before reading it:
  `RC_ASSERT((uint32_t)space < RC_COUNT(names));`, with the cast as MSVC makes
  an enum a signed `int`.
- **Count a C array's elements with `RC_COUNT`**, never a hand-written
  `sizeof` division.
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
  on one line. A macro that expands to an initialiser puts each field on a line
  of its own, the braces on lines of their own.

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
  C11). It needs no comment.
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

Algorithms live in `richc/algorithm/`, and their `NAME` is the function's full
name. Their functions take the container first, then the context, then anything
else, such as a scratch arena. Those over a view or span all take the same
control macros, `TYPE`, `NAME`, `VIEW` or `SPAN`, `CTX`, `KEY`, `KEY_TYPE` and
`CMP`, and get the context, projection and comparison from
`algorithm/private/begin.template.h`, so a new one behaves like the rest. The
foreach templates take `FUNC`, `CTX` and `NAME`, and `CONST` where the
callback is handed the container.

## Testing

- **richc:** every module has a `test_<module>.c` in its layer's `test/`
  directory, built into one test executable per layer with a single
  `RC_TEST_MAIN` entry point.
- **lowpoly:** there is no separate test executable. Tests sit at the bottom
  of the source file they test, under `#ifdef LOWPOLY_TESTS`, so they can reach
  `static` functions, and `lowpoly --test [filter]` runs them.
- Tests run automatically as a post-build step, and a failure fails the build.
  `RICHC_BUILD_TESTS` (on by default natively, and for wasm32 when node is
  found) and `LOWPOLY_BUILD_TESTS` (on by default natively) build and run them; `RICHC_BUILD_APP` (on by default) builds the app layer
  as well as core, and lowpoly needs it.
- richc's tests build natively and for wasm32, where node runs them after the
  build through `tools/run_wasm_tests.mjs`, and
  `tools/run_wasm_tests_chrome.mjs` runs them in headless Chrome, where a
  page's main thread may not block and some APIs refuse shared memory. A test that cannot run on wasm is
  skipped there, and one that compares against libc is built natively only.
  lowpoly's tests are native only.
- The test framework is libc-free, and reports through `rc_print`. It leaves a
  failing test (and still deinits its fixture) by `longjmp` natively and a
  JavaScript throw on wasm, so tests never use `setjmp` themselves: code that
  must leave without returning calls `rc_test_bail` within `rc_test_try`.
- Checks run only on the test's own thread, as leaving a test from any other
  would wreck it. A worker thread records what it saw, and the test checks
  that after joining it.
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

1. **Float formatting and parsing.** `rc_mstr_append_f32/f64` give `%g` and
   `%f` with a precision, exactly. Is a shortest round-trip form (Ryu-style)
   wanted as well, for OBJ/glTF export? And parsing (`strtod`) is still to
   come.
2. **Geometry precision.** The TypeScript editor uses doubles throughout. richc
   math is `float` only. Does the lowpoly document store `f32` or `f64`
   positions? Does core need `vec3d`-style types?
3. **Windows on ARM.** The MSVC path of the instruction-backed math uses SSE
   intrinsics, which are x64 only. ARM64 Windows would need a NEON path. In
   scope?
