// app.js
//
// richc_app on a web page: the canvas, the frame loop and key input, for a module whose
// rc_app_run has already run (it returns at once on wasm). The functions the C side imports are
// supplied from the module richc.

// rc_key values (richc/app/app.h), by KeyboardEvent.code. Codes name a key's position, whatever
// the layout, as rc_key does.
const keys = {
    Space: 1,
};

// rc_app_event_type values (richc/app/app.h)
const KEY_DOWN = 1;
const KEY_UP = 2;

// The app on canvas. Call bind with the instance before the module runs, and run once it has.
export function createApp(canvas) {
    let memory = null;
    const decoder = new TextDecoder();
    const text = (data, len) => decoder.decode(new Uint8Array(memory.buffer, data, len));

    const imports = {
        app_set_title: (data, len) => {
            document.title = text(data, len);
        },
        app_log: (data, len) => console.error(text(data, len)),
    };

    // Draw in device pixels, filling the canvas's size on the page
    function fit() {
        canvas.width = Math.max(1, Math.round(canvas.clientWidth * devicePixelRatio));
        canvas.height = Math.max(1, Math.round(canvas.clientHeight * devicePixelRatio));
    }

    return {
        imports,

        // Give the imports the instance's memory, which pointers index.
        bind(instance) {
            memory = instance.exports.memory;
        },

        // Start the frames and key events.
        run(instance) {
            const { rc_wasm_frame, rc_wasm_key } = instance.exports;

            fit();
            new ResizeObserver(fit).observe(canvas);

            // Held keys repeat as presses alone, which we drop, as natively. Keys we use are kept
            // from the page, so space does not scroll it.
            const onKey = (type) => (event) => {
                if (event.repeat) {
                    return;
                }
                const key = keys[event.code] ?? 0;
                if (key !== 0) {
                    event.preventDefault();
                }
                rc_wasm_key(type, key);
            };
            window.addEventListener('keydown', onKey(KEY_DOWN));
            window.addEventListener('keyup', onKey(KEY_UP));

            const frame = () => {
                rc_wasm_frame();
                requestAnimationFrame(frame);
            };
            requestAnimationFrame(frame);
        },
    };
}
