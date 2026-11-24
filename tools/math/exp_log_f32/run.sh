#!/usr/bin/env bash
#
# run.sh
#
# Regenerates the tables, constants, polynomials and golden values behind the f32 exp, log, log2
# and pow, and reruns their checks. Binaries and outputs go in out/.
#
#   ./run.sh tables                      the constants, both tables and the three polynomials,
#                                        fitted by Sollya (gen_exp_log_f32.py); the tables and
#                                        coefficients are compared with exp_f32_table.c and
#                                        log_f32_table.c
#   ./run.sh golden                      the golden rows (golden.c), compared with
#                                        test_math_exp_log_f32.c; their errors against glibc's
#                                        long double go to out/golden_errors.txt
#   ./run.sh stress [samples [function]] exp, log and log2 on every input and pow on seven
#                                        distributions, against glibc's long double (stress.c);
#                                        100 million pow samples per distribution by default
#   ./run.sh bench                       throughput and latency against glibc (bench.c)

set -eo pipefail
cd "$(dirname "$0")"
source ../common.sh

case "$1" in
tables)
    mkdir -p "$out"
    python3 gen_exp_log_f32.py | tee "$out/tables.txt"
    compare table "$out/tables.txt" "$core/src/math/exp_f32_table.c" rc_exp2_f32_table rc_exp2_f32_coefficients
    compare table "$out/tables.txt" "$core/src/math/log_f32_table.c" rc_log_f32_table rc_log_f32_coefficients \
        rc_log2_f32_coefficients
    ;;
golden)
    build golden golden.c -lm
    "$out/golden" 2> "$out/golden_errors.txt" | tee "$out/golden.txt"
    echo "largest error: $(awk '{print $NF, $0}' "$out/golden_errors.txt" | sort -g | tail -1 | cut -d' ' -f2-)"
    compare golden "$out/golden.txt" "$core/test/test_math_exp_log_f32.c"
    ;;
stress)
    build stress stress.c -lm -lpthread
    "$out/stress" ${2:+"$2"} ${3:+"$3"}
    ;;
bench)
    build bench bench.c -lm
    "$out/bench"
    ;;
*)
    echo "usage: ./run.sh tables|golden|stress [samples [function]]|bench" >&2
    exit 2
    ;;
esac
