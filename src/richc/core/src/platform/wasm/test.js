// test.js
//
// What richc's test framework (richc/test.h) needs from whatever runs a test module. wasm has no
// setjmp, so a test is left part-way through by throwing: test_call calls back into the module
// inside a try, and test_leave throws what it catches.

// The framework's imports, running the tests filter names (every test if it is empty). Call bind
// with the instance before the module runs.
export function createTest(filter) {
    let exports = null;
    const encoder = new TextEncoder();

    // Thrown to leave the guarded call at level
    class Leave {
        constructor(level) {
            this.level = level;
        }
    }

    return {
        imports: {
            test_call: (level, fn, context) => {
                const stack = exports.__stack_pointer.value;

                try {
                    exports.test_called(fn, context);
                    return 1;
                }
                catch (error) {
                    // A leave at an outer level, or a trap, carries on out
                    if (!(error instanceof Leave) || error.level !== level) {
                        throw error;
                    }

                    // The throw skipped the epilogues that would have popped the module's stack
                    exports.__stack_pointer.value = stack;
                    return 0;
                }
            },

            test_leave: (level) => {
                throw new Leave(level);
            },

            // Encoded into a buffer of our own and copied, as browsers will not encode into shared
            // memory, which a threaded module's is
            test_filter: (data, cap) => {
                const encoded = new Uint8Array(cap >>> 0);
                const written = encoder.encodeInto(filter, encoded).written;

                new Uint8Array(exports.memory.buffer, data >>> 0, written).set(encoded.subarray(0, written));
                return written;
            },
        },

        // Give the imports the instance's exports: its memory, its stack pointer and test_called.
        bind(instance) {
            exports = instance.exports;
        },
    };
}
