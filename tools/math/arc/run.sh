#!/usr/bin/env bash
#
# run.sh
#
# Regenerates the fits, table, constants and golden values behind asin, acos, atan, atan2 and
# cbrt, and reruns their checks. Binaries and outputs go in out/.
#
#   ./run.sh fits               the kernel polynomials, by Sollya (fit_arc.sollya), and asin_f64's
#                               rational kernel (fit_asin_ratio.py)
#   ./run.sh tables             the atan table (gen_tables.py), compared with atan_table.c
#   ./run.sh constants          pi and its fractions, tan(pi/8) and tan(3pi/8), cbrt's correction
#                               constants (gen_tables.py), and the biases of cbrt's first guesses
#                               (cbrt_bias.c)
#   ./run.sh golden             the golden rows (gen_golden.c), compared with test_math_arc.c and
#                               test_math_cbrt.c
#   ./run.sh check              special values, symmetry and exact cubes against glibc (check.c)
#   ./run.sh stress [samples]   the risky regions of the f64 functions against glibc's long double
#                               (stress.c); 10 million samples per region by default
#   ./run.sh bench [filter]     ns per call against glibc (bench.c)

set -eo pipefail
cd "$(dirname "$0")"
source ../common.sh

case "$1" in
fits)
    sollya fit_arc.sollya
    python3 fit_asin_ratio.py
    ;;
tables)
    mkdir -p "$out"
    python3 gen_tables.py table | tee "$out/atan_table.txt"
    compare table "$out/atan_table.txt" "$core/src/math/atan_table.c" rc_arc_atan_table
    ;;
constants)
    python3 gen_tables.py constants
    mkdir -p "$out"
    "$CC" -O2 -std=c17 cbrt_bias.c -o "$out/cbrt_bias" -lm
    "$out/cbrt_bias"
    ;;
golden)
    build gen_golden gen_golden.c
    "$out/gen_golden" | tee "$out/golden.txt"
    compare golden "$out/golden.txt" "$core/test/test_math_arc.c" "$core/test/test_math_cbrt.c"
    ;;
check)
    build check check.c -lm
    "$out/check"
    ;;
stress)
    build stress stress.c -lm
    "$out/stress" ${2:+"$2"}
    ;;
bench)
    build bench bench.c -lm
    "$out/bench" ${2:+"$2"}
    ;;
*)
    echo "usage: ./run.sh fits|tables|constants|golden|check|stress [samples]|bench [filter]" >&2
    exit 2
    ;;
esac
