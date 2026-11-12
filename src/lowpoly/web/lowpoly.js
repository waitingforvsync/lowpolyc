// lowpoly.js
//
// Loads lowpoly.wasm and runs it on the page's canvas. richc's app.js and gl.js supply the
// module's imports and drive it; lowpoly itself adds nothing to them yet.

import { createApp } from './richc/app.js';
import { createGl } from './richc/gl.js';

const canvas = document.getElementById('canvas');
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
    // The GL context comes first: the module expects it ready when it starts
    const gl = createGl(canvas);
    const app = createApp(canvas);
    const { instance } = await load('lowpoly.wasm', { richc: { ...app.imports, ...gl.imports } });

    app.bind(instance);
    gl.bind(instance);

    const result = instance.exports.start();
    if (result !== 0) {
        throw new Error(`start returned ${result}`);
    }
    app.run(instance);

    if (gl.available) {
        status.hidden = true;
    }
    else {
        status.textContent = 'Lowpoly needs WebGL2, which this browser does not have.';
    }
}
catch (error) {
    status.textContent = `Could not start lowpoly: ${error}`;
}
