// app.js
//
// richc_app on a web page: the canvas, the frame loop and key input, for a module whose
// rc_app_run has already run (it returns at once on wasm). The functions the C side imports are
// supplied from the module richc.

// Scancodes (richc/app/keys.h, USB HID usages) by KeyboardEvent.code, which names a key's
// position whatever the layout, as scancodes do. The X11 platform has the same table by evdev code.
const scancodes = (() => {
    const table = {
        Enter: 40, Escape: 41, Backspace: 42, Tab: 43, Space: 44, Minus: 45, Equal: 46,
        BracketLeft: 47, BracketRight: 48, Backslash: 49, Semicolon: 51, Quote: 52, Backquote: 53,
        Comma: 54, Period: 55, Slash: 56, CapsLock: 57,
        PrintScreen: 70, ScrollLock: 71, Pause: 72, Insert: 73, Home: 74, PageUp: 75, Delete: 76,
        End: 77, PageDown: 78, ArrowRight: 79, ArrowLeft: 80, ArrowDown: 81, ArrowUp: 82,
        NumLock: 83, NumpadDivide: 84, NumpadMultiply: 85, NumpadSubtract: 86, NumpadAdd: 87,
        NumpadEnter: 88, NumpadDecimal: 99, IntlBackslash: 100, ContextMenu: 101, NumpadEqual: 103,
        ControlLeft: 224, ShiftLeft: 225, AltLeft: 226, MetaLeft: 227,
        ControlRight: 228, ShiftRight: 229, AltRight: 230, MetaRight: 231,
    };
    for (let i = 0; i < 26; i++) {
        table[`Key${String.fromCharCode(65 + i)}`] = 4 + i;
    }
    for (let i = 1; i <= 9; i++) {
        table[`Digit${i}`] = 29 + i;
        table[`Numpad${i}`] = 88 + i;
    }
    table.Digit0 = 39;
    table.Numpad0 = 98;
    for (let i = 1; i <= 12; i++) {
        table[`F${i}`] = 57 + i;
    }
    return table;
})();

// Keys whose browser default would move or search the page, which we keep from it
const pageKeys = new Set([
    'Space', 'Tab', 'Backspace', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight',
    'PageUp', 'PageDown', 'Home', 'End', 'Slash', 'Quote',
]);

// rc_app_event_type values (richc/app/app.h)
const KEY_DOWN = 1;
const KEY_UP = 2;
const KEY_REPEAT = 3;

// rc_modifier bits (richc/app/keys.h)
const MODIFIER_SHIFT = 1;
const MODIFIER_CTRL = 2;
const MODIFIER_ALT = 4;
const MODIFIER_SUPER = 8;
const MODIFIER_CAPS_LOCK = 16;
const MODIFIER_NUM_LOCK = 32;

// The editing keys' rc_key values (richc/app/keys.h), by KeyboardEvent.key, which follows num lock
// as it should: with it off, the keypad's 4 is ArrowLeft
const editingKeys = {
    Backspace: 0x08, Tab: 0x09, Enter: 0x0d, Escape: 0x1b,
    ArrowUp: 0xf700, ArrowDown: 0xf701, ArrowLeft: 0xf702, ArrowRight: 0xf703,
    Insert: 0xf727, Delete: 0xf728, Home: 0xf729, End: 0xf72b, PageUp: 0xf72c, PageDown: 0xf72d,
};

// The rc_modifier bits of an event
function modifiersOf(event) {
    return (event.shiftKey ? MODIFIER_SHIFT : 0) |
        (event.ctrlKey ? MODIFIER_CTRL : 0) |
        (event.altKey ? MODIFIER_ALT : 0) |
        (event.metaKey ? MODIFIER_SUPER : 0) |
        (event.getModifierState('CapsLock') ? MODIFIER_CAPS_LOCK : 0) |
        (event.getModifierState('NumLock') ? MODIFIER_NUM_LOCK : 0);
}

// The only code point in text, or undefined if it holds more or fewer.
function onlyCodepoint(text) {
    const chars = [...text];
    return chars.length === 1 ? chars[0].codePointAt(0) : undefined;
}

// The character a key types with no modifiers in the current layout, or 0 if we cannot tell. The
// keyboard layout map says exactly, where the browser has one (Chromium); otherwise
// KeyboardEvent.key has the character with the modifiers applied, which we can undo for shift on a
// letter but not otherwise.
function layoutChar(event, layoutMap) {
    const mapped = layoutMap?.get(event.code);
    if (mapped !== undefined) {
        return onlyCodepoint(mapped) ?? 0;
    }

    if (event.altKey || event.getModifierState('AltGraph')) {
        return 0;
    }

    return unshiftedChar(event);
}

// The character KeyboardEvent.key holds, with shift undone if it was a letter's, or 0.
function unshiftedChar(event) {
    const codepoint = onlyCodepoint(event.key);
    if (codepoint === undefined) {
        return 0;
    }

    if (!event.shiftKey) {
        return codepoint;
    }

    const lower = event.key.toLowerCase();
    return lower !== event.key ? onlyCodepoint(lower) ?? 0 : 0;
}

// True if ctrl or meta make the key press a hotkey rather than typing. AltGr can show up as
// ctrl+alt, and then it is typing.
function isHotkey(event) {
    return (event.ctrlKey || event.metaKey) && !event.getModifierState('AltGraph');
}

// True for a control character, which is never typed text.
function isControl(codepoint) {
    return codepoint < 0x20 || (codepoint >= 0x7f && codepoint < 0xa0);
}

// What a key press types, for a char event: an editing key's rc_key, a character, or undefined.
// KeyboardEvent.key holds the character after the layout and dead keys.
function typed(event) {
    const editing = editingKeys[event.key];
    if (editing !== undefined) {
        return editing;
    }

    if (event.isComposing) {
        return undefined;
    }

    if (isHotkey(event)) {
        return undefined;
    }

    const codepoint = onlyCodepoint(event.key);
    if (codepoint === undefined) {
        return undefined;
    }

    return isControl(codepoint) ? undefined : codepoint;
}

// Keep the keyboard layout map current, where the browser has one: it can change when the page
// regains focus. Returns a function giving the latest map, or null.
function watchLayoutMap() {
    let layoutMap = null;
    const read = () => {
        navigator.keyboard?.getLayoutMap?.().then((map) => {
            layoutMap = map;
        }, () => {});
    };

    read();
    window.addEventListener('focus', read);
    return () => layoutMap;
}

// Send the module key and char events: one key down and one key up per press, with key repeats
// between them, as natively.
function listenToKeys(exports, currentLayoutMap) {
    const { rc_wasm_key, rc_wasm_char } = exports;

    // Held keys by code, with the layout character each went down with
    const held = new Map();
    let lastModifiers = 0;

    const sendKey = (type, event, char) => {
        lastModifiers = modifiersOf(event);
        rc_wasm_key(type, scancodes[event.code] ?? 0, char, lastModifiers);
    };

    window.addEventListener('keydown', (event) => {
        if (pageKeys.has(event.code)) {
            event.preventDefault();
        }

        const repeat = held.has(event.code);
        const char = repeat ? held.get(event.code) : layoutChar(event, currentLayoutMap());
        held.set(event.code, char);
        sendKey(repeat ? KEY_REPEAT : KEY_DOWN, event, char);

        const codepoint = typed(event);
        if (codepoint !== undefined) {
            rc_wasm_char(codepoint, lastModifiers);
        }
    });

    window.addEventListener('keyup', (event) => {
        if (held.has(event.code)) {
            sendKey(KEY_UP, event, held.get(event.code));
            held.delete(event.code);
        }
    });

    // Keys released while the page cannot see them would otherwise stay down. There is no event to
    // read the modifiers from, so the locks stay as they were and nothing is held.
    window.addEventListener('blur', () => {
        const locks = lastModifiers & (MODIFIER_CAPS_LOCK | MODIFIER_NUM_LOCK);
        for (const [code, char] of held) {
            rc_wasm_key(KEY_UP, scancodes[code] ?? 0, char, locks);
        }

        held.clear();
    });
}

// Call the module's frame function on every animation frame.
function runFrames(rc_wasm_frame) {
    const frame = () => {
        rc_wasm_frame();
        requestAnimationFrame(frame);
    };

    requestAnimationFrame(frame);
}

// The app on canvas. Call bind with the instance before the module runs, and run once it has.
export function createApp(canvas) {
    let memory = null;
    const decoder = new TextDecoder();

    // From a copy, as some browsers will not decode shared memory, which a threaded module's is
    const text = (data, len) => decoder.decode(new Uint8Array(memory.buffer, data >>> 0, len >>> 0).slice());

    const imports = {
        app_set_title: (data, len) => {
            document.title = text(data, len);
        },
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
            fit();
            new ResizeObserver(fit).observe(canvas);
            listenToKeys(instance.exports, watchLayoutMap());
            runFrames(instance.exports.rc_wasm_frame);
        },
    };
}
