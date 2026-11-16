// fs.js
//
// richc's files on a web page (richc/file.h). A browser gives us no file system to read without
// waiting, so the page keeps one in memory: files, a Map from names to Uint8Arrays. The page puts
// files in it (dropped, picked or fetched) before the module asks for them, and finds the module's
// saves there, or hears of each one through onWrite. The functions the C side imports are supplied
// from the module richc.

// rc_file_error values (richc/file.h)
const FILE_ERROR_NONE = 0;
const FILE_ERROR_NOT_FOUND = 1;
const FILE_ERROR_IO = 4;

// The store. Call bind with the instance before the module runs. The page may change files freely
// between calls into the module.
export function createFs() {
    let memory = null;
    let nextHandle = 1;
    const files = new Map();
    const opened = new Map();  // handle -> {bytes, at}
    const decoder = new TextDecoder();

    // Pointers and sizes arrive as signed 32-bit numbers, so >>> 0 makes them unsigned
    const bytesAt = (data, size) => new Uint8Array(memory.buffer, data >>> 0, size >>> 0);
    const nameAt = (data, len) => decoder.decode(bytesAt(data, len));

    const fs = {
        files,

        // Called as onWrite(name, bytes) after each save, if set
        onWrite: null,

        imports: {
            fs_open: (path, len) => {
                const bytes = files.get(nameAt(path, len));

                if (bytes === undefined) {
                    return -FILE_ERROR_NOT_FOUND;
                }
                const handle = nextHandle++;
                opened.set(handle, { bytes, at: 0 });
                return handle;
            },
            fs_size: (handle) => opened.get(handle).bytes.length,
            fs_read: (handle, dst, size) => {
                const file = opened.get(handle);
                const end = file.at + (size >>> 0);

                if (end > file.bytes.length) {
                    return FILE_ERROR_IO;
                }
                bytesAt(dst, size).set(file.bytes.subarray(file.at, end));
                file.at = end;
                return FILE_ERROR_NONE;
            },
            fs_close: (handle) => {
                opened.delete(handle);
            },
            fs_write: (path, len, data, size) => {
                const name = nameAt(path, len);
                const bytes = bytesAt(data, size).slice();

                files.set(name, bytes);
                fs.onWrite?.(name, bytes);
                return FILE_ERROR_NONE;
            },
            fs_delete: (path, len) => (files.delete(nameAt(path, len)) ? FILE_ERROR_NONE : FILE_ERROR_NOT_FOUND),
            fs_exists: (path, len) => (files.has(nameAt(path, len)) ? 1 : 0),
        },

        // Give the imports the instance's memory, which pointers index.
        bind(instance) {
            memory = instance.exports.memory;
        },
    };
    return fs;
}
