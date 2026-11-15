# TODO

Open issues left behind while porting from the old richc. Remove an entry once
it is done.

## Needs a libc replacement

Library code cannot call libc (see `CODING_STANDARDS.md`), so these old richc
features were left out until richc has its own implementation.

- **Number parsing** to replace `strtod` / `strtol`, which nothing has needed
  yet.
- **Public ASCII helpers** in `richc/str.h` to replace `<ctype.h>`. For now
  `str.c` has a private lower-case function, which is all the case-insensitive
  compares need.

## Still to port

- **The per-type array headers**: `richc/array/u8.h` to `u64.h`, `i8.h` to
  `i64.h`, `f32.h`, `f64.h`, `bool.h`, `str.h` and `mstr.h`, each instantiating
  `array.template.h` once. The old ones are in `~/dev/richc/src/core/include/richc/array/`,
  with tests in `test_arrays.c`.

## Decisions to make

- **`rc_str_as_cstr` reads the byte after the view** to see whether it is
  already null-terminated. That is fine for literals and `rc_mstr`, but for a view
  ending at the edge of readable memory it is undefined behaviour and could
  crash. Options:
  - Keep it as it is.
  - Always copy.
  - Add `rc_mstr_as_cstr` for the no-copy case, and have `rc_str_as_cstr` always
    copy.
