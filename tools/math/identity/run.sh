#!/usr/bin/env bash
#
# run.sh
#
# The cross-target identity check: builds driver.c with all 24 transcendentals natively with gcc
# and with clang, and for wasm32 with clang, runs all three (the wasm build under node), and checks
# that they print the same 24 hashes. Each run hashes 2^32 / 17 inputs per f32 function and 20
# million per f64 function and per function of two arguments. Binaries and outputs go in out/.
#
#   ./run.sh

set -eo pipefail
cd "$(dirname "$0")"
source ../common.sh

flags=(-O2 -ffp-contract=off -fno-math-errno -std=c17 -I"$core/include" -I"$core/src")
mkdir -p "$out"

# Build
gcc "${flags[@]}" -march=x86-64-v2 driver.c native.c "${math_sources[@]}" -o "$out/native"
clang "${flags[@]}" -march=x86-64-v2 driver.c native.c "${math_sources[@]}" -o "$out/native_clang"
clang --target=wasm32 -ffreestanding -nostdlib -msimd128 "${flags[@]}" -Wl,--no-entry -Wl,--export=hash_f1 \
    -Wl,--export=hash_d1 -Wl,--export=hash_2 driver.c "${math_sources[@]}" "$core/src/platform/wasm/runtime.c" -o "$out/math.wasm"

# Run all three at once, then show the hashes side by side
"$out/native" > "$out/gcc.txt" &
gcc_pid=$!
"$out/native_clang" > "$out/clang.txt" &
clang_pid=$!
node run.mjs "$out/math.wasm" > "$out/wasm.txt" &
wasm_pid=$!
wait "$gcc_pid"
wait "$clang_pid"
wait "$wasm_pid"

paste "$out/gcc.txt" "$out/clang.txt" "$out/wasm.txt" | awk '{print $1, $2, $3, $6, $9}'
if cmp -s "$out/gcc.txt" "$out/clang.txt" && cmp -s "$out/gcc.txt" "$out/wasm.txt"; then
    echo "identical: gcc, clang and wasm32 agree on all 24 functions"
else
    echo "DIFFERENT: see out/gcc.txt, out/clang.txt and out/wasm.txt" >&2
    exit 1
fi
