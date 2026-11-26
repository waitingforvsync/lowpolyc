// run_wasm_tests_chrome.mjs
//
// Runs a richc test module built for wasm32 in headless Chrome, as run_wasm_tests.mjs runs one
// under node, and exits with its result: 1 if any test failed or the run did not finish.
//
//   node tools/run_wasm_tests_chrome.mjs <module.wasm> [filter]
//
// It serves a page that runs the module, cross-origin isolated so a module built with threads gets
// shared memory, and points Chrome at it. The tests run on the page's main thread, which may not
// block, so this is where the spinning waits of a threaded build get their test. The page sends
// back what the tests printed, and whether they passed. Set CHROME to the browser to use; by
// default it is the first of google-chrome, google-chrome-stable, chromium and chromium-browser
// found. Under CI, Chrome runs without its sandbox, which the runners cannot give it.

import { spawn, spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, readdirSync, rmSync } from 'node:fs';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import { dirname, extname, join, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const TIMEOUT_MS = 300000;
const scriptsDir = resolve(dirname(fileURLToPath(import.meta.url)), '../src/richc/core/src/platform/wasm');
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json' };

// The page: it loads the fixtures, runs the module as run_wasm_tests.mjs does, and posts back
// everything printed, then the result.
const page = `<!doctype html>
<meta charset="utf-8">
<script type="module">
import { createFs } from './richc/fs.js';
import { createPrint } from './richc/print.js';
import { createTest } from './richc/test.js';
import { createMemory, startThreads } from './richc/threads.js';
import { createTime } from './richc/time.js';

const lines = [];

for (const name of ['log', 'error']) {
    const original = console[name];

    console[name] = (...args) => {
        lines.push(args.join(' '));
        original(...args);
    };
}

const finish = (failed) => fetch('done', { method: 'POST', body: JSON.stringify({ failed, lines }) });

try {
    if (!crossOriginIsolated) {
        throw new Error('the page is not cross-origin isolated');
    }

    const filter = new URLSearchParams(location.search).get('filter') ?? '';
    const fs = createFs();
    const print = createPrint();
    const test = createTest(filter);
    const time = createTime();

    for (const name of await (await fetch('data.json')).json()) {
        fs.files.set(name, new Uint8Array(await (await fetch(name)).arrayBuffer()));
    }

    const bytes = await (await fetch('module.wasm')).arrayBuffer();
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
    await finish(failed);
}
catch (error) {
    lines.push(String(error && error.stack ? error.stack : error));
    await finish(-1);
}
</script>
`;


// Every file under dir, as paths, or none if there is no dir.
function filesUnder(dir) {
    try {
        return readdirSync(dir, { withFileTypes: true, recursive: true })
            .filter((entry) => entry.isFile())
            .map((entry) => join(entry.parentPath, entry.name));
    }
    catch (error) {
        if (error.code === 'ENOENT') {
            return [];
        }
        throw error;
    }
}


// The browser to run: CHROME, or the first of the usual names on the path.
function findChrome() {
    if (process.env.CHROME) {
        return process.env.CHROME;
    }

    for (const name of ['google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser']) {
        if (spawnSync('which', [name]).status === 0) {
            return name;
        }
    }

    console.error('run_wasm_tests_chrome: no Chrome found; set CHROME');
    process.exit(2);
}


const [modulePath, filter = ''] = process.argv.slice(2);
if (!modulePath) {
    console.error('usage: node run_wasm_tests_chrome.mjs <module.wasm> [filter]');
    process.exit(2);
}

const moduleDir = dirname(resolve(modulePath));
const dataNames = filesUnder(join(moduleDir, 'data')).map((path) => relative(moduleDir, path).split(sep).join('/'));
let finished = null;
const done = new Promise((resolveDone) => {
    finished = resolveDone;
});

// The page, richc's scripts, the module and its fixtures, each with the isolating headers
const server = createServer((request, response) => {
    const path = new URL(request.url, 'http://localhost').pathname.slice(1);
    let file = null;
    let body = null;

    if (request.method === 'POST' && path === 'done') {
        let text = '';

        request.on('data', (chunk) => {
            text += chunk;
        });
        request.on('end', () => {
            response.end();
            finished(JSON.parse(text));
        });
        return;
    }

    if (path === '') {
        body = page;
        file = 'index.html';
    } else if (path === 'data.json') {
        body = JSON.stringify(dataNames);
        file = path;
    } else if (path === 'module.wasm') {
        file = resolve(modulePath);
    } else if (path.startsWith('richc/') && !path.includes('..')) {
        file = join(scriptsDir, path.slice('richc/'.length));
    } else if (dataNames.includes(path)) {
        file = join(moduleDir, path);
    }

    try {
        body ??= file ? readFileSync(file) : null;
    }
    catch {
        body = null;
    }

    if (body === null) {
        response.writeHead(404);
        response.end();
        return;
    }

    response.writeHead(200, {
        'Content-Type': types[extname(file)] ?? 'application/octet-stream',
        'Cross-Origin-Opener-Policy': 'same-origin',
        'Cross-Origin-Embedder-Policy': 'require-corp',
    });
    response.end(body);
});

await new Promise((resolveListen) => server.listen(0, '127.0.0.1', resolveListen));

const profile = mkdtempSync(join(tmpdir(), 'richc-chrome-'));
const url = `http://127.0.0.1:${server.address().port}/?filter=${encodeURIComponent(filter)}`;
const flags = ['--headless=new', `--user-data-dir=${profile}`, '--no-first-run', '--no-default-browser-check',
    '--disable-extensions', '--disable-gpu'];

if (process.env.CI) {
    flags.push('--no-sandbox');
}

// Chrome's own logging is noise unless it never gets as far as running the page
let chromeLog = '';
// In a process group of its own, so stopping it stops every process it started
const chrome = spawn(findChrome(), [...flags, url], { stdio: ['ignore', 'ignore', 'pipe'], detached: true });

chrome.stderr.on('data', (chunk) => {
    chromeLog += chunk;
});

const timeout = setTimeout(() => finished(null), TIMEOUT_MS);
const result = await done;

// Chrome's processes write to its profile until they have all gone, so stop them all and wait
// before removing it
clearTimeout(timeout);
if (chrome.exitCode === null) {
    const exited = new Promise((resolveExit) => chrome.once('exit', resolveExit));

    process.kill(-chrome.pid, 'SIGTERM');
    await exited;
}

server.close();
rmSync(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });

if (result === null) {
    console.error(`run_wasm_tests_chrome: no result after ${TIMEOUT_MS / 1000} s\n${chromeLog}`);
    process.exit(1);
}

for (const line of result.lines) {
    console.log(line);
}

process.exitCode = result.failed === 0 ? 0 : 1;
