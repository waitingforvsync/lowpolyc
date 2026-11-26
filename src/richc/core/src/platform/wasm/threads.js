// threads.js
//
// richc's threads (richc/thread/thread.h) on a web page or under node, for a module built with
// RICHC_WASM_THREADS. Such a module imports its memory, shared, so every worker can instantiate
// it on the same one. Threads run on a pool of workers that must all be up before the module
// runs: a browser starts a worker only once the page's main thread has returned to it, and a
// thread started on one that never came up would never run.
//
// A module built without threads defines its own memory, and needs none of this.

// The limits of the memory a module imports, read from its bytes, or null if it imports none.
// JavaScript has no other way to ask.
function importedMemoryLimits(bytes) {
    let at = 8;

    // An unsigned LEB128 number, as every count and size in a module is
    function leb() {
        let value = 0;
        let shift = 0;
        let byte = 0x80;

        while (byte & 0x80) {
            byte = bytes[at++];
            value += (byte & 0x7f) * 2 ** shift;
            shift += 7;
        }
        return value;
    }

    // Not at += leb(), which would add to at as it was before leb moved it
    function skipName() {
        const length = leb();

        at += length;
    }

    // Find the import section, id 2, and walk its entries for the memory
    while (at < bytes.length) {
        const id = bytes[at++];
        const size = leb();
        const end = at + size;

        if (id !== 2) {
            at = end;
            continue;
        }

        for (let count = leb(); count > 0; count--) {
            skipName();
            skipName();

            const kind = bytes[at++];

            if (kind === 0) {
                leb();
            } else if (kind === 1) {
                const flags = bytes[at + 1];

                at += 2;
                leb();
                if (flags & 1) {
                    leb();
                }
            } else if (kind === 2) {
                const flags = bytes[at++];
                const initial = leb();
                const maximum = flags & 1 ? leb() : undefined;

                return { initial, maximum, shared: (flags & 2) !== 0 };
            } else if (kind === 3) {
                at += 2;
            } else {
                at += 1;
                leb();
            }
        }
        return null;
    }
    return null;
}


// The memory to give a module, if it imports one: shared, as it would only import one for threads.
export function createMemory(bytes) {
    const limits = importedMemoryLimits(new Uint8Array(bytes));

    return limits ? new WebAssembly.Memory(limits) : null;
}


// A worker, under node or in a browser, that runs thread_worker.js beside this.
async function startWorker() {
    const url = new URL('./thread_worker.js', import.meta.url);

    if (typeof window === 'undefined' && typeof process !== 'undefined') {
        const { Worker } = await import('node:worker_threads');
        const worker = new Worker(url);

        return {
            post: (message) => worker.postMessage(message),
            onMessage: (handler) => worker.on('message', handler),
            onError: (handler) => worker.on('error', (error) => handler(String(error))),
            terminate: () => worker.terminate(),
        };
    }

    const worker = new Worker(url, { type: 'module' });

    return {
        post: (message) => worker.postMessage(message),
        onMessage: (handler) => worker.addEventListener('message', (event) => handler(event.data)),
        onError: (handler) => worker.addEventListener('error', (event) => handler(event.message ?? 'failed to load')),
        terminate: () => worker.terminate(),
    };
}


// Start the module's workers, once instance is up and before it runs anything else, resolving
// when every one is ready, and rejecting if one fails to start. Does nothing for a module built
// without threads. Under node the main thread may block, as a page's may not.
//
// A thread that fails later is reported on the console. Under node that also ends the process, but
// on a page nothing more can be done: a main thread waiting for the failed thread is spinning, and
// will not see the report.
export async function startThreads(instance, module, memory) {
    const exports = instance.exports;
    const underNode = typeof window === 'undefined' && typeof process !== 'undefined';
    const cores = underNode ? (await import('node:os')).availableParallelism() : navigator.hardwareConcurrency;
    const workers = [];

    if (!exports.rc_wasm_threads_init) {
        return { terminate: async () => {} };
    }

    const count = exports.rc_wasm_threads_init(cores, underNode ? 1 : 0);
    const ready = [];

    for (let index = 0; index < count; index++) {
        const worker = await startWorker();

        ready.push(new Promise((resolve, reject) => {
            let started = false;

            const fail = (error) => {
                if (started) {
                    console.error(`richc thread on worker ${index}: ${error}`);
                } else {
                    reject(new Error(`worker ${index}: ${error}`));
                }
            };

            worker.onMessage((message) => {
                if (message.ready) {
                    started = true;
                    resolve();
                } else if (message.error) {
                    fail(message.error);
                }
            });
            worker.onError(fail);
        }));

        worker.post({
            module,
            memory,
            index,
            stackTop: exports.rc_wasm_thread_stack_top(index) >>> 0,
            timeOrigin: performance.timeOrigin,
        });
        workers.push(worker);
    }

    await Promise.all(ready);
    return {
        // Stop every worker, which otherwise waits for its next thread for ever
        terminate: () => Promise.all(workers.map((worker) => worker.terminate())),
    };
}
