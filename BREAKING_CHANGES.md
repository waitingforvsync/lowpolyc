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
- [hash.h](#hashh)
- [The array template](#the-array-template)
- [bitset.h](#bitseth)
- [Pools](#pools)
- [Genpools and handles](#genpools-and-handles)
- [The hash trie](#the-hash-trie)
- [test.h](#testh)
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
- **Tests:**
  - `RC_CHECK` on a `uint64_t` (and `size_t` on 64-bit targets) compares
    unsigned.
  - A `float` left operand compares as `float`, so `RC_CHECK(f, ==, 0.1)` now
    passes where it used to fail.
  - An invalid `rc_str` (`{0}`) no longer equals `RC_STR("")`.
  - An arena made outside a test is invalid once the first test has run.

## Building

- **richc lives in `lowpolyc/src/richc`**, with no `project()` or language
  settings of its own. Add it with `add_subdirectory`, and set C17 in the
  including project. lowpolyc needs CMake 3.25.
- **Options:**
  - `RICHC_CORE_ONLY` is gone. Set `RICHC_BUILD_APP` to `OFF` instead: it
    defaults to `ON`, and the app needs X11 and EGL on Linux.
  - `RICHC_RUN_TESTS` is gone. `RICHC_BUILD_TESTS` (default `ON` natively)
    builds richc's tests and runs them after building; set it to `OFF` to skip
    both.
  - `RICHC_WASM` selects the wasm platform layer. Leave it unset natively.
- **Libraries:**
  - `rc_test_run` and the `RC_CHECK` functions moved out of `richc` into
    `richc_test`, which test builds must link.
  - `richc` no longer links libm or the threads library, nor defines
    `_DEFAULT_SOURCE`. Code that calls libm links `m` itself.
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
  | `richc/template/algorithm/hash_trie_foreach.h` | `richc/hash_trie_foreach.template.h` |

- **Template `NAME` macros are a suffix**, never the full type name: the type
  is always `rc_pool_<NAME>`, `rc_genpool_<NAME>` or `rc_hash_trie_<NAME>`.
  The array template already worked this way.
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
page heap (64 GiB reserved natively, 4 GiB on wasm), and a released arena's
chunks go to a global pool for reuse. That global state is not thread-safe,
even between different arenas.

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
- **Functions that allocate take the arena last**, followed by any scratch
  arena by value, and assert that the two are not the same arena.
- **On wasm**, richc must be the only code that grows linear memory.

## macros.h, ops.h and mem.h

- **macros.h:**
  - `RC_CONCAT`, `RC_STRINGIFY`, `RC_INDEX_NONE`, `RC_ASSERT`, `RC_PANIC` and
    `RC_UNREACHABLE` are unchanged.
  - `RC_PANIC` on MSVC uses `__fastfail` rather than `__debugbreak`, so it
    cannot be continued past. clang-cl now takes the clang path.
  - New: `RC_STATIC_ASSERT` and `RC_NORETURN`. It now includes `<float.h>`,
    `<stdalign.h>`, `<stdbool.h>` and `<stddef.h>`.
- **ops.h:**
  - Removed, for now:
    - `rc_min_*` and `rc_max_*` (`i32`, `i64`, `f32`, `f64`)
    - `rc_sgn_i32` and `rc_sgn_i64`
    - `rc_gcd_i32` and `rc_gcd_i64`
    - the `rc_*_overflows_*` checks
    - `rc_deg_to_rad`
  - `rc_clz_*`, `rc_ctz_*`, `rc_popcount_u32` and `rc_bitcast_f32/f64` are
    unchanged.
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
- **Unchanged:** everything else, including the `find`, `split`, prefix and
  suffix functions and `rc_str_as_cstr`.

## mstr.h

- **`rc_mstr_deinit` is now `rc_mstr_release`.**
- **Float formatting is gone for now:** `rc_mstr_append_f32`,
  `rc_mstr_append_f64` and `rc_float_format`. The old versions used
  `snprintf`, and richc needs its own formatter first (see `TODO.md`).
- **Lengths that would overflow `uint32_t` panic**, where they used to wrap.
- **Unchanged:** the other appends (integers, hex, chars), `make`, `from_str`,
  `from_cstr`, `reset`, `reserve` and `replace`.

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

**The per-type headers are not ported yet** (`richc/array/u8.h` and the rest).
Until they are, write the instantiation yourself, in a header with an include
guard:

```c
#include "richc/str.h"

#define RC_ARRAY_TYPE rc_str
#define RC_ARRAY_NAME str
#include "richc/array.template.h"
```

They will return at the same paths, so the include lines can then go back.
Note that the old `richc/array/bitset.h` made arrays of bitsets
(`rc_array_bitset`), not a bitset's words.

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
| `template/algorithm/pool_foreach.h` | none | loop to `num_slots`, skipping the slots set in `free_bitset` |

`make`, `reserve`, `reset`, `get`, `set`, `at`, `at_const` and `free_bitset`
are unchanged apart from the type name.

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
| `template/algorithm/genpool_foreach.h` | none | the same loop, with no scratch arena |

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
#include "richc/hash_trie_foreach.template.h"
```

## test.h

- **Unchanged:** `RC_TEST`, `RC_TEST_STEP`, the group macros, `RC_CHECK`,
  `RC_CHECK_TRUE`, `RC_CHECK_FALSE`, `rc_test_run` and `RC_TEST_MAIN`.
- **The runner sets up and tears down the arena heap around every test**, so
  an arena made before `rc_test_run`, or in a global, is invalid after the
  first test. Make arenas in the test or its group's `INIT`.
- **The tests live in `richc_test`** (see [Building](#building)), and only
  build natively.
- **`RC_CHECK` changes:**
  - `uint64_t` operands go through a new unsigned check.
  - `rc_test_check_float` is split into `rc_test_check_f32` and
    `rc_test_check_f64`, and a `float` left operand compares as `float`.
  - String checks never trap on an invalid `rc_str`; it only equals another
    invalid one.
  - The vector and rational cases are gone, with those types. `test.h` no
    longer brings in `<math.h>`.
- **`RC_TEST_STEP_SKIP`** needs the group to define both `INIT` and `DEINIT`.
- **A skipped test is marked by `.skip = true`** in its `rc_test`, not by a
  `NULL` function.

## Not ported yet

| Old | What it was |
|---|---|
| `richc/array/*.h` | per-type arrays: `u8` to `u64`, `i8` to `i64`, `f32`, `f64`, `bool`, `str`, `mstr`, `bitset` |
| `richc/bytes.h` | `uint8_t` arrays (`rc_view_bytes` etc.), with conversions to and from strings |
| `richc/file.h` | load and save whole files |
| `richc/random.h` | `rc_random`, a PRNG |
| `richc/time.h` | a monotonic nanosecond clock |
| `richc/rect_pack.h` | a rectangle packer |
| `template/algorithm/sort.h` | introsort on a span |
| `template/algorithm/lower_bound.h`, `upper_bound.h` | binary search on a view |
| `template/algorithm/min_element.h`, `max_element.h` | index of the smallest or largest element |
| `template/algorithm/bitset_foreach.h` | visit the set bits (or loop with `first_set` and `next_set`) |
| `template/future.h`, `richc/thread/` | futures, threads, locks, atomics and a task scheduler |
| `richc/zip/inflate.h` | DEFLATE and zlib decompression |
| `richc/math/` | vectors, matrices, quaternions, rationals, polynomial solvers, and their arrays |
| float formatting and number parsing | `rc_mstr_append_f32/f64`, `rc_float_format` |

To use an old algorithm template before it is ported, copy it and update its
calls: `get_head` to `first`, `get_tail` to `drop_first`, and the bitset
renames.

## Porting Baron

What Baron uses that changed, roughly in order of effort:

- **Arena budgets.** Baron makes arenas with a size meant as a cap, and reads
  `top` and `reserved` to see how full they are. None of that works now.
  - The cap: `rc_arena_make` in `scopes.c` and `assemble.c`.
  - The checks: `arena_is_low` and `memory_is_low` in `assemble.c`, the
    `reserved - top` check in `scopes.c`, and the `top < reserved / 2` loop
    in `assemble.c`.
  - `permanent_start = permanent.top` in `baron.c` and `baron.h`.
- **Missing modules:**
  - the array headers for `u8`, `u32`, `i32`, `i64`, `str` and `bitset`
  - `bytes.h` and `file.h`
  - `random.h`, for `RND`
  - `sort.h` (3 uses) and `lower_bound.h` (1)
  - `rc_mstr_append_f64` with `rc_float_format` (2 uses in `value.c`), which
    waits on float formatting
- **`rc_str_substr`:** 11 calls in `assemble.c`, `expression.c` and `lexer.c`.
  Each already passes a difference (`x - start`), so it becomes
  `rc_str_substr(s, start, x)`.
- **The hash tries in `scopes.h` and their foreach walks in `scopes.c`:**
  - Use the new macros and names.
  - Check the 4096-block `pool_make` reserves against the narrower blocks.
  - Rerun the tests, since walk order changes.
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
