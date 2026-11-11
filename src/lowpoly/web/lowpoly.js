// lowpoly.js
//
// Loads lowpoly.wasm and starts it. The module imports nothing yet; as the
// richc and lowpoly wasm backends grow, their host functions are supplied here.

const status = document.getElementById('status');

// Streaming compile needs the server to send application/wasm; fall back to
// compiling from the bytes when it does not.
async function load(url, imports) {
    try {
        return await WebAssembly.instantiateStreaming(fetch(url), imports);
    }
    catch {
        const bytes = await (await fetch(url)).arrayBuffer();
        return WebAssembly.instantiate(bytes, imports);
    }
}

try {
    const { instance } = await load('lowpoly.wasm', {});
    const result = instance.exports.start();
    status.textContent = result === 0
        ? 'The wasm module loaded and ran. Nothing else here yet.'
        : `start returned ${result}.`;
}
catch (error) {
    status.textContent = `Could not start lowpoly: ${error}`;
}
