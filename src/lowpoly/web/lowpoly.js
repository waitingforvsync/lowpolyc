// lowpoly.js
//
// Loads lowpoly and runs it on the page's canvas. richc's app.js, gl.js, fs.js, print.js and
// time.js supply the module's imports and drive it; lowpoly itself adds nothing to them yet.
//
// There are two builds. lowpoly-threads.wasm needs shared memory, which a browser allows only on
// a page that is cross-origin isolated, so the page loads it only then, and only if the site has
// it; otherwise it loads lowpoly.wasm, which has no threads. GitHub Pages cannot send the headers
// that isolate a page, so there it is always lowpoly.wasm.

import { createApp } from './richc/app.js';
import { createFs } from './richc/fs.js';
import { createGl } from './richc/gl.js';
import { createPrint } from './richc/print.js';
import { createTime } from './richc/time.js';

const canvas = document.getElementById('canvas');
const status = document.getElementById('status');


// The bytes of the build to run, and whether it is the one with threads.
async function fetchBuild() {
    if (crossOriginIsolated) {
        const response = await fetch('lowpoly-threads.wasm');

        if (response.ok) {
            return { bytes: await response.arrayBuffer(), threaded: true };
        }
    }

    const response = await fetch('lowpoly.wasm');

    if (!response.ok) {
        throw new Error(`lowpoly.wasm: ${response.status} ${response.statusText}`);
    }

    return { bytes: await response.arrayBuffer(), threaded: false };
}


try {
    // The GL context comes first: the module expects it ready when it starts
    const gl = createGl(canvas);
    const app = createApp(canvas);
    const fs = createFs();
    const print = createPrint();
    const time = createTime();
    const imports = { richc: { ...app.imports, ...gl.imports, ...fs.imports, ...print.imports, ...time.imports } };
    const { bytes, threaded } = await fetchBuild();
    const module = await WebAssembly.compile(bytes);

    // Only the threaded build needs threads.js, which only its site has
    const threads = threaded ? await import('./richc/threads.js') : null;
    const memory = threads ? threads.createMemory(bytes) : null;

    if (memory) {
        imports.env = { memory };
    }

    const instance = await WebAssembly.instantiate(module, imports);

    app.bind(instance);
    gl.bind(instance);
    fs.bind(instance);
    print.bind(instance);
    time.bind(instance);

    if (threads) {
        await threads.startThreads(instance, module, memory);
    }

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
