// print.js
//
// richc's standard streams on a web page (richc/print.h). The console only takes whole messages,
// so each stream holds its text until a line break comes, then logs every finished line:
// standard output through console.log and standard error through console.error.

// The streams. Call bind with the instance before the module runs.
export function createPrint() {
    let memory = null;
    const decoder = new TextDecoder();

    // One gatherer per stream: pending holds the bytes after its last line break
    function stream(log) {
        let pending = new Uint8Array(0);

        return (data, len) => {
            const bytes = new Uint8Array(memory.buffer, data >>> 0, len >>> 0);
            const joined = new Uint8Array(pending.length + bytes.length);

            joined.set(pending);
            joined.set(bytes, pending.length);

            // Decode only whole lines, so a character split between two writes stays together
            const end = joined.lastIndexOf(10);

            if (end < 0) {
                pending = joined;
                return;
            }

            for (const line of decoder.decode(joined.subarray(0, end)).split('\n')) {
                log(line);
            }
            pending = joined.slice(end + 1);
        };
    }

    return {
        imports: {
            print: stream((line) => console.log(line)),
            print_err: stream((line) => console.error(line)),
        },

        // Give the imports the instance's memory, which pointers index.
        bind(instance) {
            memory = instance.exports.memory;
        },
    };
}
