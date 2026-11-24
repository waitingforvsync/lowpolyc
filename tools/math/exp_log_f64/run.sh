#!/usr/bin/env bash
#
# run.sh
#
# Regenerates the fits, tables, constants and golden values behind the f64 exp, log, log2 and pow,
# and reruns their checks. Binaries and outputs go in out/.
#
#   ./run.sh fits               the four polynomials, by Sollya (fit_exp_log_f64.sollya)
#   ./run.sh tables             the log, log2 and exp tables and the constants (gen_tables.py);
#                               the tables go to out/*.inc and are compared with log_f64_table.c,
#                               log2_f64_table.c and exp_f64_kernel.c
#   ./run.sh check              that the log reductions are exact (check_reduce.c) and so are the
#                               fast two-sums after them (check_fast_sum.py)
#   ./run.sh golden             the golden rows (golden.c), compared with test_math_exp_log_f64.c
#   ./run.sh stress [samples]   the targeted accuracy runs against libquadmath (stress.c), with
#                               30 to 100 million samples each by default, on THREADS threads
#                               (4 by default; each thread has its own seed, so the count
#                               changes which samples run)
#   ./run.sh bench              ns per call against glibc (bench.c)

set -eo pipefail
cd "$(dirname "$0")"
source ../common.sh

# The targeted runs and their default sample counts.
stress_runs=(
    log_near1:100000000 log2_near1:100000000 log2_far:100000000 exp_hi:50000000 exp_sub:50000000 exp_small:30000000
    pow_edge:100000000 pow_near1:60000000 pow_hi:50000000 pow_mid:50000000 pow_xsub:30000000
    pow_neg:30000000 pow_bits:30000000
)

case "$1" in
fits)
    sollya fit_exp_log_f64.sollya
    ;;
tables)
    mkdir -p "$out"
    python3 gen_tables.py "$out" | tee "$out/tables.txt"
    compare table "$out/log_table.inc" "$core/src/math/log_f64_table.c" rc_log_f64_inverse rc_log_f64_table
    compare table "$out/log2_table.inc" "$core/src/math/log2_f64_table.c" rc_log2_f64_inverse rc_log2_f64_table
    compare table "$out/exp_table.inc" "$core/src/math/exp_f64_kernel.c" rc_exp_f64_table
    ;;
check)
    build check_reduce check_reduce.c -lquadmath
    "$out/check_reduce"
    python3 check_fast_sum.py "$core/src/math/log_f64_table.c" "$core/src/math/log2_f64_table.c"
    ;;
golden)
    build golden golden.c -lquadmath -lm
    "$out/golden" | tee "$out/golden.txt"
    compare golden "$out/golden.txt" "$core/test/test_math_exp_log_f64.c"
    ;;
stress)
    build stress stress.c -lquadmath -lm -lpthread
    for run in "${stress_runs[@]}"; do
        "$out/stress" "${run%%:*}" "${2:-${run##*:}}" "${THREADS:-4}"
    done
    ;;
bench)
    build bench bench.c -lm
    "$out/bench"
    ;;
*)
    echo "usage: ./run.sh fits|tables|check|golden|stress [samples]|bench" >&2
    exit 2
    ;;
esac
