// thread_worker.js
//
// One worker of a threaded module's pool (threads.js starts them). It instantiates the module on
// the shared memory, points its stack pointer at its own stack, says it is ready, and hands
// itself to the module for good.
//
// The worker can print and read the clock. Anything else the module imports, such as the file
// store, lives on the main thread, and calling it from a thread traps. So does a trap in any
// thread: under node it ends the process, as a crash in a native thread would.

import { createPrint } from './print.js';
import { createTime } from './time.js';

const underNode = typeof self === 'undefined';
const port = underNode ? (await import('node:worker_threads')).parentPort : self;


// Report a failure on this worker, and under node end the whole process.
async function fail(error) {
    const text = `richc thread: ${error && error.stack ? error.stack : error}\n`;

    if (!underNode) {
        console.error(text);
        port.postMessage({ error: String(error) });
        return;
    }

    // The main thread may be blocked waiting for this one, so nothing it could do would help
    const { writeSync } = await import('node:fs');

    writeSync(2, text);
    process.kill(process.pid, 'SIGABRT');
}


// Instantiate the module as message says, and run the worker.
async function run(message) {
    const { module, memory, index, stackTop, timeOrigin } = message;
    const print = createPrint();
    const time = createTime(timeOrigin);
    const known = { ...print.imports, ...time.imports };
    const imports = { env: { memory } };

    for (const { module: from, name, kind } of WebAssembly.Module.imports(module)) {
        if (kind !== 'function') {
            continue;
        }

        imports[from] ??= {};
        imports[from][name] = known[name] ?? (() => {
            throw new Error(`${from}.${name} is not available on a thread`);
        });
    }

    const instance = await WebAssembly.instantiate(module, imports);

    print.bind(instance);
    time.bind(instance);
    instance.exports.__stack_pointer.value = stackTop;
    port.postMessage({ ready: true });
    instance.exports.rc_wasm_thread_worker(index);
}


const onMessage = (message) => {
    run(message).catch(fail);
};

if (underNode) {
    port.once('message', onMessage);
} else {
    port.addEventListener('message', (event) => onMessage(event.data), { once: true });
}
