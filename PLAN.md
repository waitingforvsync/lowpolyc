# Plan

Where richc_app and lowpoly are heading. Unlike the reference docs, this file
describes things that do not exist yet. Entries move out of here and into the
docs as they land.

## The app and graphics layer

richc_app gives lowpoly a window, input and a GPU API, using only system APIs
(X11, EGL, the browser) and no third-party libraries.

We work top-down: lowpoly's needs decide what richc_app grows next, and each
low-level piece arrives when something above it needs it.

### Decisions

- **The host owns the event loop.** `rc_app_run(&desc)` takes an `rc_app_desc`
  holding the window settings, the callbacks (init, event, frame, deinit) and a
  user pointer passed back to each of them.
  - Natively, `rc_app_run` runs the loop and returns once the window closes.
  - On wasm, it sets things up and returns at once. The page's script then
    calls exported functions for each frame and each event.
  - So app state lives in static storage or an arena, never on the stack of
    the function that calls `rc_app_run`.
  - `rc_app_desc` is where later options go, such as running rendering on its
    own thread natively.
- **The graphics API follows WebGPU's object model**: device, queue, surface,
  command encoders, render passes, pipelines, bind groups and descriptor
  structs. Names follow richc's conventions:
  - `rc_gfx_<object>_make(device, &desc)` to create;
  - `rc_gfx_<object>_<verb>` for operations, such as
    `rc_gfx_command_encoder_begin_render_pass`;
  - `rc_gfx_<object>_release` to destroy.

  Handles are generational, made with `handle.template.h`, so `{0}` is none and
  a handle to a released object is caught. Each one indexes a genpool in the
  backend.
- **Device creation is synchronous on every target.** In the browser, the
  loader script has the context ready before it calls into C.
- **The first backend is OpenGL ES 3.0 / WebGL2**, one C backend for both
  targets. The two APIs are nearly identical and share a shading language
  (GLSL ES 3.00).
  - Natively it calls GL through an EGL context. We declare the GL functions
    and constants we use by hand and load them with `eglGetProcAddress`, so we
    need no loader library and no GLES headers.
  - On wasm each GL call is an import, which `gl.js` forwards to WebGL2. The
    WebGL objects live in JavaScript tables indexed by the integer names C
    uses.
- **The GL backend keeps WebGPU's behaviour.** Encoders record commands and
  `rc_gfx_queue_submit` replays them, so ordering matches WebGPU. Where GL
  differs, the backend emulates WebGPU: 0 to 1 clip-space depth, a y-down
  framebuffer, and bind groups mapped onto buffer bindings and texture units.
- **Later backends sit behind the same API**, with no change to lowpoly:
  WebGPU in the browser, and Vulkan natively.
- **Windowing is X11 through Xlib for now.** It runs under XWayland on a
  Wayland session. EGL also works on Wayland, so a native Wayland backend can
  reuse the GL context code.

### Layout

All paths are under `src/richc/app/`.

| Path | Contents |
|------|----------|
| `include/richc/app/app.h` | `rc_app_desc`, `rc_app_run`, events, key codes |
| `include/richc/gfx/gfx.h` | The GPU API |
| `src/platform/x11/app.c` | `rc_app_run` natively: the Xlib window, its events and the frame loop |
| `src/platform/wasm/app.c`, `app.js` | `rc_app_run` on wasm: canvas, `requestAnimationFrame` loop, input |
| `src/log.h` | Private: reporting failures from shared code (stderr, or the console) |
| `src/gl_context.h` | Private GL context interface: create, present, look up functions |
| `src/platform/x11/x11.h` | Private: the open X11 window, for the GL context |
| `src/platform/x11/gl_context.c` | EGL on the X11 window |
| `src/platform/wasm/gl_context.c`, `gl.js` | WebGL2 context and GL call forwarding |
| `src/gfx/gl/` | The GL backend: `gfx.c`, plus `gl.h` declaring the GL subset it uses and `gl.c` loading it natively |

Windowing gets its own platform names (`x11`, later `wayland`, `win32`,
`cocoa`) rather than core's `posix`, since macOS is POSIX but not X11. The
JavaScript files sit beside the wasm code they serve, use the import module
name `richc`, and the build copies them into the site.

## Core features the app layer needs

richc_app leans on core for things it does not have yet. Each arrives when the
first thing needs it, and its API is reviewed before it comes over from the old
richc (`~/dev/richc/src/core`).

| Feature | First needed for | In the old richc |
|---------|------------------|------------------|
| Debug output | reporting errors natively (stderr) and on wasm (the console). richc_app has a private `rc_app_log` for now | nothing; a new platform capability |
| Monotonic time | frame timing, once something animates; a frame cap where presenting does not wait | `time.h` |
| `vec2i` and friends | window size and mouse position, after milestone 1 | `math/vec2i.h` and the rest of `math/` |

## Milestone 1: clear the screen

lowpoly opens a window natively, or fills a canvas in the browser. Every frame
it clears to a colour, and space toggles between two colours. Natively, closing
the window exits; in the browser, closing the tab is the end of it.

1. **lowpoly's side first** (done). Write the frame and event code in `editor.c`
   against the API we want, plus draft `app.h` and `gfx.h` holding exactly
   what it uses. Review them before building anything behind them. Until the
   backends exist, `placeholder.c` stands in behind both headers so everything
   links.
2. **X11 window and events** (done). Open the window, handle the close button
   through `WM_DELETE_WINDOW`, map key codes, and run the native frame loop.
3. **The GL backend, natively** (done). An EGL context on the X11 window, then
   the gfx subset: device, queue, surface (presenting with `eglSwapBuffers`),
   command encoder, a render pass that clears, and submit.
   - Presenting waits for the display, which paces the loop. While the window
     is unmapped, the loop draws nothing and sleeps until an event comes.
   - Where presenting does not wait (Xvfb, or a driver ignoring the swap
     interval), the loop spins. A fallback needs monotonic time.
4. **The same backend on wasm.** Canvas setup, the frame loop and key events in
   `app.js`, and `gl.js` forwarding to WebGL2. This puts milestone 1 on GitHub
   Pages.
5. **Tidy up.**
   - CI installs the X11 and EGL headers (`libx11-dev`, `libegl-dev`, both
     added) and builds the native app.
   - Unit tests for whatever needs no GPU, such as key mapping. The graphical
     part is checked by running it.
   - Update `CODING_STANDARDS.md`, `TODO.md` and this file.

## After milestone 1

What lowpoly needs next sets the order. Likely:

- Window resize and mouse input.
- Buffers, shaders, pipelines and bind groups, for the first mesh. This needs
  the shader decision below.
- Textures, then text.
- A WebGPU backend in the browser, and a Vulkan backend natively.
- Rendering on its own thread natively, chosen through `rc_app_desc`.
- Wayland, Windows and macOS.

## Open questions

1. **Shader language.** WebGPU uses WGSL, GL wants GLSL ES 3.00 and Vulkan
   wants SPIR-V. Options:
   - Write WGSL and translate it at build time with a third-party tool (Tint
     or Naga), which never ships with the app.
   - Write our own translator for the subset of WGSL we use. WGSL to GLSL is
     text to text; SPIR-V can wait for Vulkan.
   - Write each shader once per language.

   Decide when the first shader is needed.
2. **A render thread.** What it means for the callbacks, and for which thread
   owns gfx objects.
3. **High-DPI.** Whether window sizes are in pixels or in points, and how the
   frame callback learns the scale.
