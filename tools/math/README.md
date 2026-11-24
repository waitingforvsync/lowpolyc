# Math tools

The tools behind richc's 24 transcendental functions (sin, cos, tan, asin, acos, atan, atan2,
exp, log, log2, pow and cbrt, each in `_f32` and `_f64`), in `src/richc/core/src/math/`.

The functions were written from [spec.md](spec.md), which says what each must return and how
accurately, but not how. These tools are how they were built and checked: they fit the
polynomial coefficients, generate the constants and tables, produce the golden values in the
tests, and check accuracy, cross-target identity and speed. Rerun them after changing a function,
or to see where a number in the source came from.

The broad accuracy check is not here: `richc_math_accuracy`
(`src/richc/core/test/math_accuracy.c`) enforces every limit of the spec, over all 2^32 inputs
for the f32 functions of one argument. The stress checks here dig into the regions where each
function is weakest.

## Contents

- [Prerequisites](#prerequisites)
- [Layout](#layout)
- [trig: sin, cos and tan](#trig-sin-cos-and-tan)
- [arc: asin, acos, atan, atan2 and cbrt](#arc-asin-acos-atan-atan2-and-cbrt)
- [exp_log_f32: exp, log, log2 and pow in f32](#exp_log_f32-exp-log-log2-and-pow-in-f32)
- [exp_log_f64: exp, log, log2 and pow in f64](#exp_log_f64-exp-log-log2-and-pow-in-f64)
- [identity: every function on every target](#identity-every-function-on-every-target)

## Prerequisites

- [Sollya](https://www.sollya.org/) for the polynomial fits (`apt install sollya`).
- Python 3 with mpmath for the constants, tables and golden checks (`apt install python3-mpmath`).
- gcc with libquadmath, the reference for the f64 stress checks (part of gcc on x86-64 Linux).
- glibc, whose functions the benchmarks time ours against, and whose `long double` functions
  some checks use as a reference.
- clang with the wasm32 target and `wasm-ld`, and node, for the identity check.

The C tools build with `-march=x86-64-v2`, as the library does natively, so they expect an
x86-64 machine.

## Layout

Each family has a directory with one `run.sh`, whose subcommands regenerate or check one thing
each. Run `./run.sh` with no subcommand for the list. The scripts work from any directory.

| Path | What it is |
|------|------------|
| `trig/`, `arc/`, `exp_log_f32/`, `exp_log_f64/` | Each family's fits (`.sollya`), generators (Python), golden-value generator, stress check and benchmark |
| `identity/` | The check that gcc, clang and wasm32 builds give the same bits for all 24 functions |
| `common.sh` | What the `run.sh` scripts share: the paths, the flags, and `build` |
| `compare.py` | Checks generated tables and golden rows against the tree |
| `spec.md` | The specification the functions were written from |

The C tools need no build of the library. `run.sh` compiles each tool together with the
transcendental sources in `src/richc/core/src/math/`, with the library's native flags
(`-O2 -ffp-contract=off -fno-math-errno -march=x86-64-v2`). Set `CC` to choose the compiler; it
is gcc by default, and the tools that use libquadmath need gcc.

Binaries and outputs go in an `out/` directory beside each `run.sh`, which git ignores.

Where a subcommand regenerates something that is in the tree, it checks it too:

- `tables` compares the generated tables with the arrays in the source, value for value.
- `golden` compares the generated rows with the golden arrays in the tests, value for value, in
  any order.

Either prints a line per array saying it matches, or what differs and exits with an error.
`fits` and `constants` only print their results, since the values are spread through the
source; search for them there.

## trig: sin, cos and tan

| Subcommand | What it does |
|------------|--------------|
| `fits` | Fits the four kernel polynomials of `trig.h` (`fit_trig.sollya`) and tan's two rationals, by Remez exchange then Sollya (`fit_tan.py`), and prints each one's relative error and coefficients |
| `tables` | Rebuilds the kernel tables of `trig.h` from the fits and compares them with it (`tables.py`) |
| `constants` | Prints 2/pi's 32-bit limbs and the 64-bit windows of it for floats, and compares both with `trig_reduce.c`, then prints the pieces of pi/2 for the short reductions in `trig.h` (`trig_constants.py`) |
| `hardest` | Finds the floats and doubles closest to a multiple of pi/2, binade by binade (`hardest.py`); prints the closest, and writes every binade's to `out/binades24.txt` and `out/binades53.txt` |
| `golden` | Runs `dump.c` on the golden inputs, checks each result against mpmath (`golden.py`), and compares the rows with `test_math_trig.c` |
| `stress [samples]` | sin, cos and tan just below pi/4, then a quadrant on, where each takes the other kernel, against libquadmath (`stress.c`); 400 million samples each by default (cos 200 million), 100 million a quadrant on, a few minutes |
| `bench` | ns per call against glibc, for \|x\| up to 0.7, 3.2, 100, 1e5, 1e9 and 1e30 (`bench.c`) |

`hardest` takes about half a minute. Its bounds are the ones `trig.h` quotes: no double is
closer than 2^-60.9 to a multiple of pi/2, and no float closer than 2^-29.2.

`golden` prints each result's error in ulps against mpmath, and the worst for each function: all
are correctly rounded but tan_f64 of the double just below pi/4, which is 0.55 ulp out.

## arc: asin, acos, atan, atan2 and cbrt

| Subcommand | What it does |
|------------|--------------|
| `fits` | Fits the atan kernels of both widths and asin_f32's (`fit_arc.sollya`), and asin_f64's rational kernel (`fit_asin_ratio.py`, about half a minute) |
| `tables` | Generates `rc_arc_atan_table` (`gen_tables.py`) and compares it with `atan_table.c` |
| `constants` | Prints pi/2, pi, pi/4 and 3pi/4 as double-doubles and nearest floats, tan(pi/8) and tan(3pi/8), and 1/3, 1/9, 5/81 and 10/243 (`gen_tables.py`), then searches out the biases of cbrt's first guesses (`cbrt_bias.c`, about a minute) |
| `golden` | Prints the golden rows (`gen_golden.c`) and compares them with `test_math_arc.c` and `test_math_cbrt.c` |
| `check` | Special values, symmetry and exact cubes against glibc, bit for bit (`check.c`) |
| `stress [samples]` | The risky regions of the f64 functions against glibc's `long double` (`stress.c`); 10 million samples per region by default |
| `bench [filter]` | ns per call against glibc (`bench.c`); the filter picks rows by name, such as `f64` or `atan2` |

`check` reports 5 failures, all expected, as glibc's cbrt does not always round correctly: it is
out by more than an ulp for 2, -2, 1e-310 and -1e-310, and gives less than the exact 2^-358 for
2^-1074.

## exp_log_f32: exp, log, log2 and pow in f32

| Subcommand | What it does |
|------------|--------------|
| `tables` | Prints 32 log2(e) and ln 2, the 2^(i/32) table, exp's cubic, the 128-entry log table, and the polynomials of log and log2 (which pow shares), fitting the polynomials with Sollya (`gen_exp_log_f32.py`), and compares the tables and coefficient arrays with `exp_f32_table.c` and `log_f32_table.c` |
| `golden` | Prints the golden rows (`golden.c`) and compares them with `test_math_exp_log_f32.c`; each result's error against glibc's `long double` goes to `out/golden_errors.txt`, and the largest is printed |
| `stress [samples [function]]` | exp, log and log2 on all 2^32 inputs and pow on seven distributions of pairs, against glibc's `long double`, counting results that are not correctly rounded (`stress.c`); 100 million pow pairs per distribution by default. The function, if given, runs alone |
| `bench` | Throughput and latency against glibc (`bench.c`) |

Each exhaustive run in `stress` takes about a minute on 16 threads. The worst errors are 0.5017
ulp for exp, 0.50002 for log and 0.50006 for log2, and pow's worst, 0.5084, comes with x near 1
and the result near the ends of the floats.

## exp_log_f64: exp, log, log2 and pow in f64

| Subcommand | What it does |
|------------|--------------|
| `fits` | Fits the log, pow and exp polynomials of `exp_log_f64.h` (`fit_exp_log_f64.sollya`) and prints each fit's error |
| `tables` | Generates log's table (`rc_log_f64_inverse` and `rc_log_f64_table`), log2's (`rc_log2_f64_inverse` and `rc_log2_f64_table`), the exp table, and the constants (`gen_tables.py`); the tables go to `out/log_table.inc`, `out/log2_table.inc` and `out/exp_table.inc` and are compared with `log_f64_table.c`, `log2_f64_table.c` and `exp_f64_kernel.c` |
| `check` | Checks that the reductions of log and log2 give r = m c - 1 exactly (`check_reduce.c`), and that the fast two-sums after them are exact for every interval of their tables (`check_fast_sum.py`) |
| `golden` | Prints the golden rows (`golden.c`), each with its error against libquadmath, and compares them with `test_math_exp_log_f64.c` |
| `stress [samples]` | Thirteen targeted accuracy runs against libquadmath (`stress.c`): log and log2 near 1, log2 just outside [1/2, 2), exp near overflow, into the subnormals and for small x, and pow at the edges of its table and range, with negative and subnormal x and on random bits; 30 to 100 million samples each by default |
| `bench` | ns per call against glibc (`bench.c`) |

`stress.c` has more tests than `run.sh` runs; run `out/stress` with no arguments for the list.
The runs use 4 threads unless `THREADS` says otherwise. Each thread has its own seed, so the
thread count changes which samples are drawn. With 4, every run's worst error is between 0.502
and 0.511 ulp.

## identity: every function on every target

`./run.sh` builds `driver.c` with all 24 functions three ways: natively with gcc and with clang
(with `native.c`), and for wasm32 with clang, freestanding. It runs all three, the wasm build
under node (`run.mjs`), and each prints a hash of every function's results: over every 17th bit
pattern for the f32 functions of one argument, and 20 million inputs for each of the others,
half ordinary values and half random bits. NaNs are canonicalised before hashing.

It prints the hashes side by side and says whether all three builds agree on all 24, exiting
with an error if not. It takes about half a minute.
