// gl.js
//
// richc_app's GL on a web page: the WebGL2 context on the canvas, and the GL functions the GL
// backend imports (gl.h lists them), each forwarded to WebGL2. They are supplied as imports from
// the module richc, under their GL names.
//
// C names GL objects by integer ids. Those will index tables of WebGL objects here as the backend
// starts making them; so far the only framebuffer is the canvas's, id 0.

// Make the context on canvas. Call bind with the instance before the module runs.
export function createGl(canvas) {
    // WebGPU's canvas has no depth or stencil buffer and is not multisampled, so neither is ours
    const gl = canvas.getContext('webgl2', {
        alpha: false,
        antialias: false,
        depth: false,
        stencil: false,
    });
    let memory = null;

    const imports = {
        // The context's id, or 0 if the browser has no WebGL2
        gl_context_id: () => (gl ? 1 : 0),

        glBindFramebuffer: (target, framebuffer) => {
            // Only the canvas's framebuffer exists so far
            console.assert(framebuffer === 0, 'glBindFramebuffer: no framebuffer', framebuffer);
            gl.bindFramebuffer(target, null);
        },
        glClearBufferfv: (buffer, drawbuffer, value) => {
            gl.clearBufferfv(buffer, drawbuffer, new Float32Array(memory.buffer, value, 4));
        },
        glColorMask: (red, green, blue, alpha) => {
            gl.colorMask(red !== 0, green !== 0, blue !== 0, alpha !== 0);
        },
        glDisable: (cap) => gl.disable(cap),
        glGetError: () => gl.getError(),
        glInvalidateFramebuffer: (target, count, attachments) => {
            gl.invalidateFramebuffer(target, Array.from(new Uint32Array(memory.buffer, attachments, count)));
        },
    };

    return {
        // False if the browser has no WebGL2
        available: gl !== null,
        imports,

        // Give the functions the instance's memory, which pointers index.
        bind(instance) {
            memory = instance.exports.memory;
        },
    };
}
