#!/usr/bin/env bash
#
# run.sh
#
# Regenerates the fits, constants and golden values behind sin, cos and tan, and reruns their
# checks. Binaries and outputs go in out/.
#
#   ./run.sh fits               the four kernel polynomials, by Sollya (fit_trig.sollya), and tan's
#                               two rationals, by Remez and Sollya (fit_tan.py)
#   ./run.sh tables             the kernel tables of trig.h rebuilt from the fits (tables.py), and
#                               compared with it
#   ./run.sh constants          2/pi's limbs and windows and the pieces of pi/2
#                               (trig_constants.py), with the tables compared with trig_reduce.c
#   ./run.sh hardest            the closest floats and doubles to multiples of pi/2 (hardest.py),
#                               with every binade's in out/binades24.txt and out/binades53.txt
#   ./run.sh golden             the golden rows, checked by mpmath (golden.py), compared with
#                               test_math_trig.c
#   ./run.sh stress [samples]   sin, cos and tan just below pi/4, then a quadrant on, where they
#                               take the other kernel, against libquadmath (stress.c); 400 million
#                               samples each by default (cos 200 million), 100 million a quadrant on
#   ./run.sh bench              ns per call against glibc (bench.c)

set -eo pipefail
cd "$(dirname "$0")"
source ../common.sh

case "$1" in
fits)
    sollya fit_trig.sollya
    python3 fit_tan.py
    ;;
tables)
    python3 tables.py "$core/src/math/trig.h"
    ;;
constants)
    mkdir -p "$out"
    python3 trig_constants.py | tee "$out/constants.txt"
    compare table "$out/constants.txt" "$core/src/math/trig_reduce.c" rc_trig_two_over_pi rc_trig_two_over_pi_f32
    ;;
hardest)
    python3 hardest.py 24 "$out"
    python3 hardest.py 53 "$out"
    ;;
golden)
    build dump dump.c
    python3 golden.py "$out/dump" | tee "$out/golden.txt"
    compare golden "$out/golden.txt" "$core/test/test_math_trig.c"
    ;;
stress)
    build stress stress.c -lquadmath -lm -lpthread
    "$out/stress" sin 0.5 0.7854 "${2:-400000000}"
    "$out/stress" cos 0.5 0.7854 "${2:-200000000}"
    "$out/stress" tan 0.6 0.7854 "${2:-400000000}"
    "$out/stress" sin 0.7854 1.0708 "${2:-100000000}"
    "$out/stress" cos 0.7854 1.0708 "${2:-100000000}"
    "$out/stress" tan 0.7854 0.9708 "${2:-100000000}"
    ;;
bench)
    build bench bench.c -lm
    "$out/bench"
    ;;
*)
    echo "usage: ./run.sh fits|tables|constants|hardest|golden|stress [samples]|bench" >&2
    exit 2
    ;;
esac
