# Breaking changes

How the new richc core differs from the old richc (`~/dev/richc`, the version
Baron uses as `extern/richc`), as a guide for porting code to it. Only the core
library is covered: richc_app is all new.

Add an entry here whenever a change to core breaks code written for the old
richc.

## Contents

- [Check these first](#check-these-first)
- [Building](#building)
- [Conventions](#conventions)
- [arena.h](#arenah)
- [macros.h, ops.h and mem.h](#macrosh-opsh-and-memh)
- [str.h](#strh)
- [mstr.h](#mstrh)
- [bytes.h](#bytesh)
- [file.h](#fileh)
- [print.h](#printh)
- [random.h and time.h](#randomh-and-timeh)
- [hash.h](#hashh)
- [The array template](#the-array-template)
- [bitset.h](#bitseth)
- [Pools](#pools)
- [Genpools and handles](#genpools-and-handles)
- [Algorithms](#algorithms)
- [The hash trie](#the-hash-trie)
- [Math](#math)
- [test.h](#testh)
- [thread/](#thread)
- [zip/inflate.h](#zipinflateh)
- [Not ported yet](#not-ported-yet)
- [Porting Baron](#porting-baron)

## Check these first

These changes still compile, but behave differently, so the compiler will not
find them.

- **`rc_str_substr`'s third argument is an end index, not a count.** The old
  `rc_str_substr(s, start, n)` is now `rc_str_substr(s, start, start + n)`.
- **The old `get_tail(x, n)` dropped the first `n` elements.** It becomes
  `drop_first(x, n)`. The new `last(x, n)` keeps the last `n`, which is
  different.
- **`rc_array_<name>_append` returns the index of the first appended
  element**, not the new count.
- **Arenas:**
  - `rc_arena_make(size)` takes a chunk size, committed at once, not a cap. An
    arena grows until the global heap runs out, so a size meant as a memory
    budget no longer limits anything.
  - Fresh arena memory is not zeroed: chunks are recycled from released
    arenas. Use the `_zero` allocations where code relied on zeroed pages.
  - Using an arena after releasing it reads or writes some other arena's
    memory, where the old one faulted.
  - `rc_arena_reset` keeps all of the arena's chunks, where the old one gave
    pages back to the OS.
- **Hash values changed:**
  - `rc_hash_bytes` and `rc_hash_str` are now Murmur3 (32-bit), not FNV-1a.
  - `rc_hash_combine` is now a splitmix64 step, not Boost's formula.
  - Anything that stored hashes, or depended on hash order, sees new values.
- **The hash trie:**
  - `add` no longer replaces an existing key's value; `set` does.
  - Iteration order is different (it was always unspecified).
  - Lookups compare the stored hash before calling `EQUAL`, so keys that
    compare equal must hash alike.
  - Blocks hold 4 nodes, not 16, so `pool_make(n)` and `pool_reserve(n)`
    reserve a quarter of the slots they did.
- **Files:**
  - `rc_file_exists` is false for a directory; it used to report anything
    there.
  - A path over 1023 bytes used to be cut short, so a different file was
    opened; it is now `RC_FILE_ERROR_BAD_NAME`.
  - On Windows, paths are UTF-8; they used to be in the ANSI code page.
  - Loading a directory is `RC_FILE_ERROR_NOT_FOUND`; on Linux it used to be
    `RC_FILE_ERROR_IO`.
- **Tests:**
  - `RC_CHECK` on a `uint64_t` (and `size_t` on 64-bit targets) compares
    unsigned.
  - A `float` left operand compares as `float`, so `RC_CHECK(f, ==, 0.1)` now
    passes where it used to fail.
  - An invalid `rc_str` (`{0}`) no longer equals `RC_STR("")`.
  - An arena made outside a test is invalid once the first test has run.
- **Math:** `component_min` and `component_max` on the float vectors, and so
  the boxes, compare with `<` rather than calling `fminf` and `fmaxf`. A NaN
  in the second argument now comes through, where it used to lose to the
  other.

## Building

- **richc lives in `lowpolyc/src/richc`**, with no `project()` or language
  settings of its own. Add it with `add_subdirectory`, and set C17 in the
  including project. lowpolyc needs CMake 3.25.
- **Options:**
  - `RICHC_CORE_ONLY` is gone. Set `RICHC_BUILD_APP` to `OFF` instead: it
    defaults to `ON`, and the app needs X11 and EGL on Linux.
  - `RICHC_RUN_TESTS` is gone. `RICHC_BUILD_TESTS` builds richc's tests and
    runs them after building; set it to `OFF` to skip both. It defaults to `ON`
    natively, and for wasm32 when node is found to run them.
  - `RICHC_WASM` selects the wasm platform layer. Leave it unset natively.
  - `RICHC_WASM_THREADS`, off by default, makes a wasm32 build with threads,
    on shared memory. The host must create that memory and start the workers
    (`createMemory` and `startThreads` in `threads.js`) before the module
    runs anything.
- **Libraries:**
  - `rc_test_run` and the `RC_CHECK` functions moved out of `richc` into
    `richc_test`, which test builds must link.
  - `richc` no longer links libm, nor defines `_DEFAULT_SOURCE`. Code that
    calls libm links `m` itself. It still links the threads library on Linux
    and macOS, as `Threads::Threads`.
- **Targets:** `macros.h` rejects targets with x87 float evaluation
  (`FLT_EVAL_METHOD != 0`) and big-endian targets.

## Conventions

- **`deinit` is now `release`** for everything that frees one object:
  `rc_arena_release`, `rc_mstr_release`, `rc_array_<name>_release`,
  `rc_bitset_release` (new), and the pools' `release`. `init` and `deinit`
  only name global setup and teardown (`rc_arena_init`, `rc_arena_deinit`).
- **Template headers moved:**

  | Old | New |
  |---|---|
  | `richc/template/array.h` | `richc/array.template.h` |
  | `richc/template/pool.h` | `richc/pool.template.h` |
  | `richc/template/genpool.h` | `richc/genpool.template.h` |
  | `richc/genpool_handle.h` | `richc/handle.template.h` |
  | `richc/template/hash_trie.h` | `richc/hash_trie.template.h` |
  | `richc/template/algorithm/<name>.h` | `richc/algorithm/<name>.template.h` |

- **Container templates' `NAME` macros are a suffix**, never the full type
  name: the type is always `rc_pool_<NAME>`, `rc_genpool_<NAME>` or
  `rc_hash_trie_<NAME>`. The array template already worked this way. An
  algorithm's `NAME` is still its function's full name.
- **Slicing has no `get_` prefix:** `first`, `last`, `drop_first`,
  `drop_last`, and `substr` / `subview` / `subspan(x, start, end)`.
- **`make` allocates; `from_ptr` wraps memory you already have.**
  `rc_str_make`, `rc_view_<name>_make` and the old `rc_span_<name>_make(data,
  num)` are now `from_ptr`.
- **Library code is libc-free.** `mem.h` replaces `memcpy` and friends. The old
  array template included `<string.h>`; the new one does not, so code that got
  `memcpy` through it must include `<string.h>` itself.
- **`rc_arena_alloc_type` and `rc_arena_alloc_zero_type` are gone.** Write
  `(T *)rc_arena_alloc(arena, (uint32_t)sizeof(T) * n)`.

## arena.h

The model changed. An old arena was its own virtual memory reservation (256 MB
by default, a hard cap), with pages committed as needed. A new arena is a chain
of chunks, each a whole number of 64 KiB pages. The chunks come from one global
page heap (64 GiB reserved natively, 4 GiB on wasm). A chunk grows in place
into the pages after it when they are the top of the heap or free. A released
arena's pages go back to the top of the heap if they reach it, or else to a
global pool of free runs, merged with their neighbours, that later chunks are
cut from. Natively, free pages past the first 16 MiB of them go back to the OS
(on wasm, linear memory cannot shrink). That global state has a lock of its
own, so arenas on different threads can be used at once, but an arena itself,
like an old one, belongs to one thread at a time.

| Old | New | Notes |
|---|---|---|
| `rc_arena_deinit(&a)` | `rc_arena_release(&a)` | `rc_arena_deinit(void)` now tears down the global heap, invalidating every arena |
| `rc_arena_make(reserve)` | `rc_arena_make(chunk_size)` | the minimum chunk size, committed at once; no cap |
| `rc_arena_make` returning `{0}` on failure | never fails | out of memory calls the OOM handler, then panics |
| `rc_arena_make_default()` | same | now 64 KiB chunks with no limit, not a 256 MB reserve |
| `RC_ARENA_DEFAULT_RESERVE` | none | `RC_ARENA_DEFAULT_CHUNK` (64 KiB) is the nearest |
| `a.base`, `a.committed`, `a.reserved` | none | fields are now `first`, `chunk`, `top`, `end` |
| `a.top` as bytes used | none | `top` is an offset in the current chunk |
| `mark = a.top; rc_arena_free_to(&a, mark)` | `rc_arena_mark m = rc_arena_save(&a); rc_arena_restore(&a, m)` | marks work across chunks |
| `rc_arena_free` asserting the pointer is in the arena | returns `false` if it is not in the current chunk | |
| `RC_MAX_ALIGN` = `alignof(max_align_t)` | 16 | |

- **Nothing reports bytes used or remaining.** Code that stops before an arena
  fills needs another approach: count its own usage, or install a handler with
  `rc_arena_set_oom_handler`. The handler is global and gets the failed size;
  if it returns, richc panics, so it must exit or `longjmp`.
- **A single allocation can be just under 4 GiB** (0xFFFF pages less 16 bytes).
- **New:** `rc_arena_init`, `rc_arena_alloc_aligned` (alignment up to 64 KiB),
  `rc_arena_is_same`, `rc_arena_set_oom_handler`, and the `rc_arena_mark` and
  `rc_arena_oom_fn` types.
- **New:** `rc_arena_ts` in `richc/arena_ts.h`, one arena that threads share:
  an `rc_arena` behind a mutex, with the same calls bar save and restore, and
  `rc_arena_ts_lock`/`rc_arena_ts_unlock` to work on the arena underneath.
- **Functions that allocate take the arena last**, followed by any scratch
  arena by value, and assert that the two are not the same arena.
- **On wasm**, richc must be the only code that grows linear memory.

## macros.h, ops.h and mem.h

- **macros.h:**
  - `RC_CONCAT`, `RC_STRINGIFY`, `RC_INDEX_NONE`, `RC_PANIC` and
    `RC_UNREACHABLE` are unchanged.
  - `RC_ASSERT`'s condition is no longer evaluated in release builds, so it
    costs nothing there. A condition that does work, such as
    `RC_ASSERT(rc_heap_commit(...))`, must move out of the assert.
  - `RC_PANIC` on MSVC uses `__fastfail` rather than `__debugbreak`, so it
    cannot be continued past. clang-cl now takes the clang path.
  - New: `RC_STATIC_ASSERT` and `RC_NORETURN`. It now includes `<float.h>`,
    `<stdalign.h>`, `<stdbool.h>` and `<stddef.h>`.
- **`ops.h` is now `math/math.h`,** which also holds the scalar float
  functions (see [Math](#math)). Include `richc/math/math.h` instead.
- **ops.h's functions:**
  - Unchanged: `rc_min_*` and `rc_max_*` (`i32`, `i64`, `f32`, `f64`),
    `rc_sgn_*`, the `rc_*_overflows_*` checks, `rc_clz_*`, `rc_ctz_*`,
    `rc_popcount_u32` and `rc_bitcast_f32/f64`.
  - `rc_gcd_i32` and `rc_gcd_i64` work on the magnitudes in unsigned, so
    `INT32_MIN` with `-1` no longer traps. They assert the answer fits, which
    rules out `INT32_MIN` with `0` (or itself), where the old ones gave a
    negative result.
  - `rc_deg_to_rad` picks `rc_deg_to_rad_f32` or the new `rc_deg_to_rad_f64`
    by its argument's type, so a `double` argument now gives a `double`.
  - New: `rc_popcount_u64` and `rc_sub_overflows_u64`, so each family covers
    both widths.
  - New: `rc_min`, `rc_max`, `rc_sgn`, `rc_gcd`, `rc_add_overflows`,
    `rc_sub_overflows`, `rc_mul_overflows`, `rc_clz`, `rc_ctz`, `rc_popcount`
    and `rc_bitcast`, which pick the suffixed form by the first argument's
    type.
  - New: `rc_bitcast_u32` and `rc_bitcast_u64`, which go the other way, from
    bits to a float or double.
  - It no longer includes `<stdlib.h>`.
- **mem.h** is new:
  - `rc_memcpy`, `rc_memmove`, `rc_memset` and `rc_memcmp`, with `uint32_t`
    sizes.
  - `rc_memset` takes a `uint8_t`, and only `rc_memcmp` returns anything.

## str.h

| Old | New | Notes |
|---|---|---|
| `rc_str_substr(s, start, count)` | `rc_str_substr(s, start, end)` | **silent**: write `start + count` |
| `rc_str_left(s, n)` | `rc_str_first(s, n)` | |
| `rc_str_right(s, n)` | `rc_str_last(s, n)` | |
| `rc_str_skip(s, n)` | `rc_str_drop_first(s, n)` | |
| `rc_str_make(data, len)` | `rc_str_from_ptr(data, len)` | |
| `rc_str_compare*` returning `int` | returning `int32_t` | only the sign means anything, as before |

- **The case-insensitive compares fold ASCII `A` to `Z` only**, where the old
  ones used the locale's `tolower`. They're the same in the C locale.
- **New:** `rc_str_drop_last(s, n)`.
- **New: number parsing**, where code called `strtol`, `strtoul`, `strtod` or
  `strtof`. `rc_str_parse_<type>(s, base)` reads an `i8` to `i64`, `u8` to
  `u64`, `f32` or `f64` from the start of `s`, and returns an
  `rc_parsed_<type>`: the value, the text after it, and an `rc_parse_error`.
  `rc_str_as_<type>` takes all of `s` or fails. They follow C++'s
  `std::from_chars` rather than C:
  - No leading whitespace is skipped, and a `+` sign is not read.
  - Base 0 reads `0x` and `0b` prefixes, but a leading `0` is never octal.
  - Overflow is `RC_PARSE_ERROR_RANGE` with the value 0, not a clamped value
    and `ERANGE`. Underflow to zero or a subnormal is no error.
  - Floats are correctly rounded, as glibc's are, so the same text gives the
    same bits everywhere. Hex floats are base 16, or base 0 behind `0x`.
- **New: UTF-8**, a codepoint at a time, by byte index.
  `rc_str_utf8_next(s, cursor)` and `rc_str_utf8_prev(s, cursor)` return an
  `rc_str_utf8_char`: the codepoint stepped over and where the cursor lands.
  `rc_str_utf8_floor`, `_ceil` and `_is_boundary` move a cursor onto a
  character boundary, and `rc_str_utf8_is_valid` and `_count` take the whole
  string. Malformed text never traps: each maximal subpart is one step, as
  `RC_CODEPOINT_NONE`, so a cursor in the middle of a character is never mended.
- **Unchanged:** everything else, including the `find`, `split`, prefix and
  suffix functions and `rc_str_as_cstr`.

## mstr.h

- **`rc_mstr_deinit` is now `rc_mstr_release`.**
- **Float formatting is richc's own**, with the same API: `rc_mstr_append_f32`,
  `rc_mstr_append_f64` and `rc_float_format`. The text is what `snprintf`
  gives with `%.*g` or `%.*f`, except that NaN is always `nan`, never `-nan`.
- **Lengths that would overflow `uint32_t` panic**, where they used to wrap.
- **New:** `rc_mstr_append_utf8(s, codepoint, arena)`, which writes U+FFFD for
  a surrogate, anything past U+10FFFF, or `RC_CODEPOINT_NONE`.
- **Unchanged:** the other appends (integers, hex, chars), `make`, `from_str`,
  `from_cstr`, `reset`, `reserve` and `replace`.

## bytes.h

- **Back with the same names:** `rc_view_bytes`, `rc_span_bytes`,
  `rc_array_bytes`, `rc_view_bytes_as_str`, `rc_span_bytes_as_str` and
  `rc_array_bytes_to_mstr`.
- **The containers follow the new array template** (see
  [The array template](#the-array-template)): `make_copy(view, ...)` is now
  `from_view`, `deinit` is now `release`, and so on.

## file.h

- **Same functions and arguments:** `rc_file_size`, `rc_file_exists`,
  `rc_file_load_text`, `rc_file_load_binary`, `rc_file_save_text`,
  `rc_file_save_binary` and `rc_file_delete`.
- **The result structs' `error` is a `uint8_t`**, holding an `rc_file_error`,
  following the rule that struct members never have enum types. Comparisons,
  switches and assignments to an `rc_file_error` still work.
- **`RC_FILE_OK` is now `RC_FILE_ERROR_NONE`**, still zero, so every value
  shares the enum's stem.
- **New error `RC_FILE_ERROR_BAD_NAME`**, for a path holding a null byte or too
  long for the platform. A `switch` that lists every error needs a case for it.
- **Behaviour changes** are under [Check these first](#check-these-first).
- **On wasm**, files live in the page's memory, in the store `richc/fs.js`
  keeps: the page fills it and takes saved files from it.

## print.h

- **New:** `rc_print` and `rc_print_err` write an `rc_str` to standard output
  and standard error, as it is, with no formatting and no line break added.
  Library code that called `printf` or `fputs` builds its text with the
  `rc_mstr_append_*` functions and prints that.
- **On wasm**, the page needs the imports from `richc/print.js`, which sends
  each finished line to the console.

## random.h and time.h

- **Unchanged:** `rc_random_make`, `rc_random_next` and `rc_time_now_ns`, with
  the same splitmix32 draws for every seed. `{0}` is a generator seeded with 0.
- **On wasm**, the page needs the import from `richc/time.js`. The clock is the
  page's `performance.now()`, which browsers coarsen to tens of microseconds or
  more.

## hash.h

- **Every hash is still 32-bit**, with the same names and signatures.
- **New values:**
  - `rc_hash_bytes` and `rc_hash_str` are now Murmur3 (32-bit, seed 0). The
    empty string hashes to 0.
  - `rc_hash_combine(seed, hash)` is now a splitmix64 step on the pair. It
    mixes every input bit into every output bit, which Boost's formula did
    not.
- **Unchanged values:** the integer, float and pointer hashes.
- **Removed:** `rc_hash_vec2i`, `rc_hash_vec3i`, `rc_hash_vec2f`,
  `rc_hash_vec3f` and `rc_hash_vec4f`, until the vector types return. Hash
  the components and combine them.

## The array template

Include `richc/array.template.h`. Its control macros are unchanged
(`RC_ARRAY_TYPE`, `RC_ARRAY_NAME`, `RC_ARRAY_DECLARE_ONLY`,
`RC_ARRAY_IMPL_ONLY`), as are the types and their fields.

| Old | New | Notes |
|---|---|---|
| `rc_array_<n>_make_copy(view, min_cap, arena)` | `rc_array_<n>_from_view(view, min_cap, arena)` | `make_copy` now takes `const rc_array_<n> *` |
| `rc_array_<n>_deinit(array, arena)` | `rc_array_<n>_release(array, arena)` | |
| `rc_array_<n>_append` returning the new count | returning the first new element's index | **silent** |
| `rc_view_<n>_make(data, num)` | `rc_view_<n>_from_ptr(data, num)` | views have no `make` now |
| `rc_span_<n>_make(data, num)` | `rc_span_<n>_from_ptr(data, num)` | `make(n, arena)` now allocates, uninitialised |
| `_get_subview(x, start, end)` | `_subview(x, start, end)` | |
| `_get_subspan(x, start, end)` | `_subspan(x, start, end)` | |
| `_get_head(x, n)` | `_first(x, n)` | |
| `_get_tail(x, n)` | `_drop_first(x, n)` | **not** `_last` |

- **New:**
  - `rc_span_<n>_make_zero(n, arena)`
  - `_last(x, n)` and `_drop_last(x, n)` for views and spans
  - `rc_array_<n>_last_at(array)`
- **Unchanged:** everything else, including growth (`max(2 * cap, need, 8)`),
  `push_n_zero`, `resize_zero`, `reverse` and `rotate`.

**The per-type headers are back at the same paths**, `richc/array/<name>.h`,
for `u8` to `u64`, `i8` to `i64`, `f32`, `f64`, `str`, `mstr` and `bitset`.
`richc/array/bitset.h` still makes arrays of bitsets (`rc_array_bitset`), not a
bitset's words.

- **`richc/array/bool.h` is gone.** `bool` is a macro for `_Bool` in C17, so
  the old header made `rc_array__Bool`, not `rc_array_bool`. Use a bitset, or
  an array of `u8`.

## bitset.h

| Old | New |
|---|---|
| `rc_bitset_get_first_set(bs)` | `rc_bitset_first_set(bs)` |
| `rc_bitset_get_next_set(bs, pos)` | `rc_bitset_next_set(bs, pos)` |
| `rc_bitset_reset(bs)` | `rc_bitset_clear_all(bs)` |
| `rc_bitset_num_set_bits(bs)` | `rc_bitset_count_set(bs)` |

- **New:** `rc_bitset_release(bs, arena)`.
- **Growth that would overflow panics**, where it used to wrap.
- **Unchanged:** everything else, including the arena arguments.

## Pools

| Old | New | Notes |
|---|---|---|
| `RC_POOL_NAME` the full type name | the suffix | the type is `rc_pool_<NAME>` |
| `alloc(pool, arena)`, returning a zeroed slot's index | `add(pool, value, arena)` | stores the value |
| `free(pool, index)` | `remove(pool, index)` | |
| `deinit(pool, arena)` | `release(pool, arena)` | |
| `pool.items.num` | `num_slots(pool)` | the fields are now internal (`slots_`, `first_free_`) |
| `template/algorithm/pool_foreach.h` | `algorithm/pool_foreach.template.h` | new `RC_POOL_FOREACH_CONST` for a read-only walk |

`make`, `reserve`, `reset`, `get`, `set`, `at`, `at_const` and `free_bitset`
are unchanged apart from the type name.

- **New: `RC_POOL_DECLARE_ONLY` and `RC_POOL_IMPL_ONLY`**, the array
  template's two passes, for an element type holding a pool of itself. The
  genpool and the hash trie have them too.

## Genpools and handles

- **Each genpool has its own handle type**, made by `handle.template.h`
  (`RC_HANDLE_TYPE` takes the full type name). The shared `rc_genpool_handle`
  is gone, so handles from different pools no longer mix.
  - A genpool makes `rc_handle_<NAME>` itself unless `RC_GENPOOL_HANDLE` names
    one made earlier.
  - `RC_GENPOOL_NAME` is a suffix, like the other templates' `NAME` macros.
- **The handle encoding changed.** `index_` is the raw index and `gen_ == 0`
  is none, where the old `index_` was the index plus one. `{0}` is still none.
  Saved handles cannot be read back.
- **Generations:** add and remove each add one, so a slot is live exactly when
  its generation is odd. Generations wrap after 2^31 lifetimes of a slot.

| Old | New | Notes |
|---|---|---|
| `rc_genpool_handle_is_null(h)` | `<handle>_is_none(h)` | |
| `rc_genpool_handle_equal(a, b)` | `<handle>_is_equal(a, b)` | |
| `rc_genpool_handle_make/index/gen` | none | |
| `alloc(pool, arena)` | `add(pool, value, arena)` | returns a handle |
| `free(pool, h)` | `remove(pool, h)` | |
| `deinit` | `release` | |
| `at` / `at_const` returning `NULL` for a stale handle | assert | check `is_valid` first |
| `handle_at(pool, index)` on a free slot | asserts | it must be live |
| `free_bitset` | none | loop to `num_slots`, using `is_live` |
| `template/algorithm/genpool_foreach.h` | `algorithm/genpool_foreach.template.h` | no scratch arena; new `RC_GENPOOL_FOREACH_CONST` for a read-only walk |

- **New: `RC_GENPOOL_DECLARE_ONLY` and `RC_GENPOOL_IMPL_ONLY`**, for an
  element type holding a genpool of itself. The declaring pass makes the
  handle type too.

## Algorithms

`sort`, `lower_bound`, `upper_bound`, `min_element`, `max_element`,
`bitset_foreach`, `pool_foreach`, `genpool_foreach` and `hash_trie_foreach`
are all in `richc/algorithm/`. Their control macros, function names and
arguments are unchanged, apart from the hash trie foreach's (see
[The hash trie](#the-hash-trie)) and the genpool foreach's scratch arena (see
[Genpools and handles](#genpools-and-handles)).

- **New: `hash_index_foreach`**, which calls `RC_HASH_INDEX_FOREACH_FUNC`
  with the index of every entry of a hash index, in the table's order.

- **New: projections.** `sort`, the two bounds and the two element searches
  take `KEY(e)`, or `KEY(ctx, e)` with a context, which turns an element into
  the key that is compared, and `KEY_TYPE`, the key's type; one needs the
  other. `CMP` then compares keys. Without `KEY`, the element is its own key,
  as before.
  - The bounds search for a key, not an element.
  - `min_element` and `max_element` project each element once.
  - `sort` keeps the key of whichever side of a comparison stays put.
- **New: `RC_SORT_CACHED`.** The sort projects each element once, into
  scratch memory, and takes a scratch arena by value as its last argument:
  `NAME(span, scratch)` or `NAME(span, ctx, scratch)`.
- **The same macros everywhere.** Every one of the five takes `TYPE`, `NAME`,
  `VIEW` (`SPAN` for `sort`), `CTX`, `KEY`, `KEY_TYPE` and `CMP`, with the same
  meanings, and the arguments are always the container, then the context, then
  anything else.

## The hash trie

| Old | New | Notes |
|---|---|---|
| `RC_TRIE_KEY_TYPE`, `_VALUE_TYPE`, `_HASH`, `_EQUAL` | `RC_HASH_TRIE_KEY_TYPE`, `_VALUE_TYPE`, `_HASH`, `_EQUAL` | |
| `RC_TRIE_NAME` the full type name (`rc_trie_symbol`) | `RC_HASH_TRIE_NAME` the suffix (`symbol`) | the type is `rc_hash_trie_<NAME>` |
| `RC_TRIE_HASH` giving a `uint64_t` | `RC_HASH_TRIE_HASH` giving a `uint32_t` | the `rc_hash_*` functions were already 32-bit |
| 16 nodes a block | 4 by default | `RC_HASH_TRIE_WIDTH`: 2, 4, 8 or 16 |
| `bool add(&trie, pool, key, [value,] arena)`: true if new; replaces | `uint32_t add(...)`: the node index; never replaces | |
| none | `uint32_t set(&trie, pool, key, value, arena)` | adds or replaces |
| `delete(trie, pool, key)` | `remove(trie, pool, key)` | |
| `find` (maps only) | `find` (sets too) | |
| `find_ptr(trie, pool, key)` | `find`, then `value_at` | |
| `key_at(pool, index)` | `key_get(pool, index)` | |
| `pool_deinit(pool, arena)` | `pool_release(pool, arena)` | |
| the pool's own `alloc`, `free`, `get`, ... | none | the pool is a wrapper; only `pool_make`, `pool_reserve`, `pool_reset` and `pool_release` |
| none | `release(&trie, pool)` | gives a trie's blocks back to the pool and sets it to `{0}` |
| node index `block * 16 + slot` | `block * WIDTH + slot` | |

- **New: `RC_HASH_TRIE_DECLARE_ONLY` and `RC_HASH_TRIE_IMPL_ONLY`**, for a
  key or value type holding a trie of itself.
- **Each slot stores its key's hash.** Lookups compare it before calling
  `EQUAL`, so `EQUAL` must never match keys with different hashes.
- **Remove, release and the foreach don't recurse**, so long chains of
  colliding keys are safe.
- **A width of 4 uses about 2 slots per key**, against about 5 at 16. To keep
  the old shape, set `RC_HASH_TRIE_WIDTH` to 16.
- **The foreach's control macros** are renamed the same way:
  `RC_HASH_TRIE_FOREACH_TRIE`, `_FUNC`, `_CTX`, `_CONST` and `_NAME`.
  - `_TRIE` is still the full type name.
  - The callback's shape is unchanged.
  - The visit order is different.

Before:

```c
#define RC_TRIE_KEY_TYPE rc_str
#define RC_TRIE_VALUE_TYPE symbol
#define RC_TRIE_NAME rc_trie_symbol
#define RC_TRIE_HASH(k) rc_hash_str(k)
#define RC_TRIE_EQUAL(a, b) rc_str_is_equal((a), (b))
#include "richc/template/hash_trie.h"

#define RC_TRIE_FOREACH_TRIE rc_trie_symbol
#define RC_TRIE_FOREACH_CTX symbol_flatten
#define RC_TRIE_FOREACH_CONST
#define RC_TRIE_FOREACH_FUNC(c, p, i) flatten_symbol(c, p, i)
#define RC_TRIE_FOREACH_NAME rc_trie_symbol_foreach_flatten
#include "richc/template/algorithm/hash_trie_foreach.h"
```

After:

```c
#define RC_HASH_TRIE_KEY_TYPE rc_str
#define RC_HASH_TRIE_VALUE_TYPE symbol
#define RC_HASH_TRIE_NAME symbol
#define RC_HASH_TRIE_HASH(k) rc_hash_str(k)
#define RC_HASH_TRIE_EQUAL(a, b) rc_str_is_equal((a), (b))
#include "richc/hash_trie.template.h"

#define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_symbol
#define RC_HASH_TRIE_FOREACH_CTX symbol_flatten
#define RC_HASH_TRIE_FOREACH_CONST
#define RC_HASH_TRIE_FOREACH_FUNC(c, p, i) flatten_symbol(c, p, i)
#define RC_HASH_TRIE_FOREACH_NAME rc_hash_trie_symbol_foreach_flatten
#include "richc/algorithm/hash_trie_foreach.template.h"
```

## Math

The headers keep their names under `richc/math/`, and their arrays under
`richc/math/array/`. Every function not listed below is unchanged.

- **The headers no longer include `<math.h>`.** Code that got `sqrtf` or
  `floorf` through them must include it itself, or use `richc/math/math.h`.
- **New in `richc/math/math.h`:** the float functions that are one
  instruction on every target: `rc_sqrt`, `rc_abs`, `rc_floor`, `rc_ceil`,
  `rc_trunc`, `rc_round_even`, `rc_copysign` and `rc_is_finite`, each in `_f32`
  and `_f64` forms, with the unsuffixed name picking one by type.
- **New in `richc/math/math.h`:** richc's own transcendentals, `rc_sin`,
  `rc_cos`, `rc_tan`, `rc_asin`, `rc_acos`, `rc_atan`, `rc_atan2`, `rc_exp`,
  `rc_log`, `rc_log2`, `rc_pow` and `rc_cbrt`, in `_f32` and `_f64` forms.
  They are richc's own, so they give the same bits on every target; they can
  differ from the platform's libm in the last place, so results that used
  `sinf` and friends may change by an ulp.
- **More functions are out of line** (same names and behaviour, now in `.c`
  files): the matrix inverses, `rc_mat33f_mul`, `rc_mat44f_mul`,
  `rc_mat34f_make_lookat` and `rc_mat44f_make_ortho`.
- **`rc_quadratic_roots.num_roots` is a `uint32_t`,** not an `int`.

## test.h

- **Unchanged:** `RC_TEST`, `RC_TEST_STEP`, the group macros, `RC_CHECK`,
  `RC_CHECK_TRUE`, `RC_CHECK_FALSE`, `rc_test_run` and `RC_TEST_MAIN`.
- **The runner sets up and tears down the arena heap around every test**, so
  an arena made before `rc_test_run`, or in a global, is invalid after the
  first test. Make arenas in the test or its group's `INIT`.
- **The tests live in `richc_test`** (see [Building](#building)), and build
  natively and for wasm32, where node runs them through
  `tools/run_wasm_tests.mjs`. The module exports `test_main` in place of `main`.
- **No `setjmp` in tests**, which wasm has none of. Code that must leave without
  returning, such as an out-of-memory handler, calls `rc_test_bail`, which
  leaves the `rc_test_try` it runs within.
- **The out-of-memory handler is cleared after every test**, so a test that
  sets one cannot leave it set for the next.
- **The runner prints through `rc_print`,** unbuffered, not `printf`. A test
  that prints with `printf` must flush `stdout` for its lines to come out in
  order.
- **`RC_CHECK` changes:**
  - `uint64_t` operands go through a new unsigned check.
  - `rc_test_check_float` is split into `rc_test_check_f32` and
    `rc_test_check_f64`, and a `float` left operand compares as `float`.
  - String checks never trap on an invalid `rc_str`; it only equals another
    invalid one.
  - The vector and rational cases are unchanged. `test.h` no longer brings in
    `<math.h>`.
- **`RC_TEST_STEP_SKIP`** needs the group to define both `INIT` and `DEINIT`.
- **A skipped test is marked by `.skip = true`** in its `rc_test`, not by a
  `NULL` function.

## thread/

### atomic.h and spinlock.h

The types and their load, store, exchange and `fetch_` operations are
unchanged, apart from compare-exchange.

| Old | New | Notes |
|---|---|---|
| `rc_atomic_t_compare_exchange_strong(a, &expected, desired, success, failure)`, returning `bool` | `rc_atomic_t_compare_exchange(a, expected, desired, order)`, returning the value it held | the swap happened if that equals `expected`; a failed swap is ordered as `order` allows a load (acquire for `ACQ_REL`, relaxed for `RELEASE`) |
| `rc_atomic_t_compare_exchange_weak` | `rc_atomic_t_compare_exchange` | wasm, x64 and ARM64 with LSE have only a strong compare-exchange; ARM without LSE pays an inner retry loop, which a retry loop of its own hardly notices |
| `rc_atomic_flag`, `_test_and_set`, `_clear` | `rc_atomic_bool`, `_exchange(f, true, order)`, `_store(f, false, order)` | |
| none | `rc_atomic_t_make(value)` | for initialising an atomic to something other than zero |
| `rc_spinlock_trylock` | `rc_spinlock_try_lock` | |
| none | `rc_spin_pause` | the CPU's spin-wait hint (`pause`, `yield`), and nothing on wasm |

- **A load ordered `RELEASE` or `ACQ_REL`, or a store ordered `ACQUIRE` or
  `ACQ_REL`,** asserts in debug builds. C11 leaves both undefined.
- **The `rc_memory_order` values are now the `__atomic` builtins' own**
  (`RELAXED` is 0, `ACQUIRE` is 2, ...), so code that stored one as a number
  must store the enum instead.
- **Each atomic holds an unsigned integer of its width,** in a private `raw_`
  member, in place of a `value_` of its own type. Code that reached into the
  struct must go through the functions.
- **On MSVC, a load is a plain load** with a barrier after it, not an
  `_InterlockedOr`, so loading no longer writes to the atomic, and a `const`
  one in read-only memory can be loaded.
- **A waiting `rc_spinlock_lock` spins reading the lock,** and tries to take
  it again only when it reads free, with `rc_spin_pause` in between. Unlocking
  a lock that is not held asserts in debug builds.

### The locks

`rc_mutex`, `rc_cond`, `rc_rwlock` and `rc_semaphore` are now plain structs of
one or two 32-bit words over a futex, the same code on every target, with no
OS object behind them. A zeroed one is ready to use, and nothing needs freeing.

| Old | New | Notes |
|---|---|---|
| `rc_mutex_init`, `rc_cond_init`, `rc_rwlock_init` | none | zero the struct (`= {0}`) |
| `rc_mutex_deinit`, `rc_cond_deinit`, `rc_rwlock_deinit`, `rc_semaphore_deinit` | none | |
| `rc_semaphore_init(&s, count)` | `s = rc_semaphore_make(count)` | a zeroed one holds 0 |
| `rc_mutex_trylock` | `rc_mutex_try_lock` | |
| `rc_rwlock_read_trylock`, `rc_rwlock_write_trylock` | `rc_rwlock_try_read_lock`, `rc_rwlock_try_write_lock` | |
| `rc_mutex_recursive` and its functions | none | a thread must not lock a mutex it holds |
| `rc_once_run(once, fn)`, in `thread.h`, with `fn` taking nothing | `rc_once_call(once, fn, context)`, in `once.h` | `fn` takes `context` |

- **`rc_rwlock` puts writers first:** a reader does not join while a writer
  waits, so a thread must not take a read lock it already holds, which could
  wait forever behind a writer that waits on it. The old lock was the OS's
  own, and glibc's puts readers first.
- **Freeing a lock:** a semaphore may be freed as soon as a wait on it
  returns, and a mutex as soon as it is unlocked for the last time. An
  `rc_rwlock` may be freed only once every thread that used it has finished
  unlocking it, as an unlock goes on touching it briefly to wake waiters.
- **Change what an `rc_cond`'s waiters wait for with the mutex held,** as
  with pthreads, or a signal can miss a waiter that had not yet begun to
  wait. A signal or broadcast with nobody waiting makes no syscall.
- **On single-threaded wasm, a wait with no timeout that nothing could end
  panics,** and a wait with a timeout passes the time. The old locks did not
  build for wasm.
- **The rest is unchanged:** `lock`, `unlock`, `rc_cond_wait`,
  `rc_cond_wait_for`, `rc_cond_signal`, `rc_cond_broadcast`,
  `rc_semaphore_wait`, `_try_wait`, `_wait_for` and `_post`, timeouts in
  nanoseconds included.

### thread.h and tls.h

A thread is now a small handle by value, naming a slot in a static table richc
keeps, so nothing the caller owns has to stay put while the thread runs. A TLS
key is likewise a slot in a thread-local array, with no OS key behind it.

| Old | New | Notes |
|---|---|---|
| `rc_thread t; rc_thread_create(&t, fn, arg)`, returning `bool` | `rc_thread t = rc_thread_make(fn, context)` | check it started with `rc_thread_is_valid(t)` |
| `rc_thread_join(&t)` | `rc_thread_join(&t)` | also zeroes `t` |
| `rc_thread_detach` | none | join every thread |
| `rc_thread_func` | none | write `void (*)(void *)` |
| `rc_tls_create()` | `rc_tls_make()` | |
| `rc_tls_destroy(&key)` | `rc_tls_release(&key)` | also zeroes `key` |

- **Every thread asks for a 256 KiB stack** (`RC_THREAD_STACK_SIZE`) on
  every target, where the OS defaults ranged from 512 KiB to 8 MiB. Linux and
  Windows take a guard page, and glibc its thread-local storage, out of it.
- **At most `RC_THREAD_MAX` (256) threads** can be started and not yet joined,
  and **at most `RC_TLS_MAX` (64) TLS keys** can exist at once; making one
  more key traps.
- **`RC_THREAD_LOCAL` is `_Thread_local`** on clang and gcc, and still
  `__declspec(thread)` on MSVC.
- **`thread.h` and `tls.h` no longer include `<windows.h>` or
  `<pthread.h>`.**
- **`rc_thread_current_id` is the address of a thread-local,** not the OS's
  thread id, so it means nothing outside the process.
- **On wasm without threads, `rc_thread_make` always fails,** and
  `rc_thread_hardware_concurrency` is 1. With threads, at most 16 threads run
  at once, on a pool of workers, and each has 4 KiB of thread-local storage.
- **Unchanged:** `rc_thread_yield`, `rc_thread_sleep_ns`,
  `rc_thread_hardware_concurrency`, `rc_tls_get` and `rc_tls_set`.

## zip/inflate.h

`rc_zip_inflate` and `rc_zip_inflate_zlib` decode the same streams, and take a
scratch arena after the output's.

| Old | New | Notes |
|---|---|---|
| `rc_zip_inflate(compressed, minimum_capacity, arena)` | `rc_zip_inflate(compressed, minimum_capacity, arena, scratch)` | the Huffman tables, about 8 KiB, too big for the wasm stack; likewise `rc_zip_inflate_zlib` |
| `RC_ZIP_OK` | `RC_ZIP_ERROR_NONE` | still zero, so the enum and its values share a stem |
| `rc_zip_inflate_result.error` an `rc_zip_error` | a `uint8_t` | holding an `rc_zip_error`, as struct members never have enum types |
| none | `RC_ZIP_ERROR_TOO_LARGE` | output that would reach 4 GiB, which used to trap in the array |

- **A fixed block using distance code 30 or 31** is `RC_ZIP_ERROR_BAD_DATA`,
  as in zlib. The old decoder read on past it, and usually reported
  `RC_ZIP_ERROR_TRUNCATED`.
- **On an error, `arena` is rewound** to where it was before the call. The
  old decoder freed only the output's last buffer, so any it had grown out of
  stayed allocated.

## Not ported yet

| Old | What it was |
|---|---|
| `richc/rect_pack.h` | a rectangle packer |
| `template/future.h`, `richc/thread/scheduler.h` | futures and a task scheduler |

## Porting Baron

What Baron uses that changed, roughly in order of effort:

- **Arena budgets.** Baron makes arenas with a size meant as a cap, and reads
  `top` and `reserved` to see how full they are. None of that works now.
  - The cap: `rc_arena_make` in `scopes.c` and `assemble.c`.
  - The checks: `arena_is_low` and `memory_is_low` in `assemble.c`, the
    `reserved - top` check in `scopes.c`, and the `top < reserved / 2` loop
    in `assemble.c`.
  - `permanent_start = permanent.top` in `baron.c` and `baron.h`.
- **`sort.h` and `lower_bound.h` in `symdump.c` and `expression.c`:** only
  the include paths change, to `richc/algorithm/sort.template.h` and
  `richc/algorithm/lower_bound.template.h`.
- **`rc_str_substr`:** 11 calls in `assemble.c`, `expression.c` and `lexer.c`.
  Each already passes a difference (`x - start`), so it becomes
  `rc_str_substr(s, start, x)`.
- **The hash tries in `scopes.h` and their foreach walks in `scopes.c`:**
  - Use the new macros and names.
  - Check the 4096-block `pool_make` reserves against the narrower blocks.
  - Rerun the tests, since walk order changes.
- **Files** (17 calls in `main.c`, `output.c`, `assemble.c` and
  `source_files.c`): add a case for `RC_FILE_ERROR_BAD_NAME` wherever errors
  are listed, and check any code that relied on `rc_file_exists` seeing a
  directory.
- **Renames:**
  - `rc_arena_deinit` to `rc_arena_release` (about 200 calls)
  - the bitset `get_first_set`, `get_next_set` and `reset` (about 60)
  - `make_copy(view, ...)` to `from_view` (13)
  - `rc_str_left`, `right`, `skip` and `make` (about 13)
  - the view and span `get_head`, `get_tail` and `get_subview` (6)
  - `rc_view_u8_make` (1)
  - the template include paths
- **CMake:**
  - replace `RICHC_CORE_ONLY` and `RICHC_RUN_TESTS`
  - link `richc_test` for test builds
  - link `m` for the maths in `expression.c`, `value.c` and `assemble.c`
