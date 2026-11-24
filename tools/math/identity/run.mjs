// run.mjs
//
// Prints driver.c's 24 hashes from the wasm build, one per line, in the same form as native.c
// does for a native build.
//
//   node run.mjs [math.wasm]    (or: ./run.sh; the module is out/math.wasm by default)

import fs from 'fs';

const F1_STRIDE = 17;
const SAMPLES = 20000000;

const path = process.argv[2] ?? new URL('./out/math.wasm', import.meta.url);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(path), {});
const e = instance.exports;
const hex = (v) => BigInt.asUintN(64, v).toString(16).padStart(16, '0');

for (let w = 0; w < 10; w++) {
    console.log(`f1 ${w} ${hex(e.hash_f1(w, F1_STRIDE))}`);
}
for (let w = 0; w < 10; w++) {
    console.log(`d1 ${w} ${hex(e.hash_d1(w, SAMPLES))}`);
}
for (let w = 0; w < 4; w++) {
    console.log(`t2 ${w} ${hex(e.hash_2(w, SAMPLES))}`);
}
