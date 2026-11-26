// run_wasm_tests.mjs
//
// Runs a richc test module built for wasm32 under node, as a native test executable runs, and exits
// with its result: 1 if any test failed.
//
//   node tools/run_wasm_tests.mjs <module.wasm> [filter]
//
// The module gets richc's page scripts for its imports, as on a web page. The tests read their
// fixtures from data/ beside the module, so every file there goes into the in-memory file store
// first, under the name a native test opens it by from its own directory. A module built with
// threads gets its shared memory and its pool of workers too.

import { readFileSync, readdirSync } from 'node:fs';
import { dirname, join, relative, sep } from 'node:path';

import { createFs } from '../src/richc/core/src/platform/wasm/fs.js';
import { createPrint } from '../src/richc/core/src/platform/wasm/print.js';
import { createTest } from '../src/richc/core/src/platform/wasm/test.js';
import { createMemory, startThreads } from '../src/richc/core/src/platform/wasm/threads.js';
import { createTime } from '../src/richc/core/src/platform/wasm/time.js';

// Every file under dir, as paths.
function filesUnder(dir) {
    return readdirSync(dir, { withFileTypes: true, recursive: true })
        .filter((entry) => entry.isFile())
        .map((entry) => join(entry.parentPath, entry.name));
}

const [modulePath, filter = ''] = process.argv.slice(2);
if (!modulePath) {
    console.error('usage: node run_wasm_tests.mjs <module.wasm> [filter]');
    process.exit(2);
}

const fs = createFs();
const print = createPrint();
const test = createTest(filter);
const time = createTime();
const moduleDir = dirname(modulePath);
const dataDir = join(moduleDir, 'data');

try {
    for (const path of filesUnder(dataDir)) {
        fs.files.set(relative(moduleDir, path).split(sep).join('/'), new Uint8Array(readFileSync(path)));
    }
}
catch (error) {
    // A module with no fixtures has no data/
    if (error.code !== 'ENOENT') {
        throw error;
    }
}

const bytes = readFileSync(modulePath);
const module = await WebAssembly.compile(bytes);
const memory = createMemory(bytes);
const imports = { richc: { ...fs.imports, ...print.imports, ...test.imports, ...time.imports } };

if (memory) {
    imports.env = { memory };
}

const instance = await WebAssembly.instantiate(module, imports);

fs.bind(instance);
print.bind(instance);
test.bind(instance);
time.bind(instance);

const threads = await startThreads(instance, module, memory);
const failed = instance.exports.test_main();

await threads.terminate();
process.exitCode = failed;
