// Names GLSL keeps for itself, used freely in WGSL, and other corners of the language.

struct input {
    output: f32,
    sample: vec2f,
}

const rc_scale = 0x1.8p1;
const smallest = -2147483647 - 1;

fn texture(smoothly: input) -> f32 {
    let flat = smoothly.output * rc_scale;
    let centroid = smoothly.sample.x + f32(smallest) * 1e-10;
    return flat + centroid;
}

@vertex
fn main(@location(0) inout: vec2f) -> @builtin(position) vec4f {
    var buffer = input(inout.x, inout);
    buffer.output += texture(buffer);
    return vec4(buffer.sample, buffer.output, 1.0);
}

@fragment
fn uniform() -> @location(0) vec4f {
    let float = 0.5;
    return vec4(float);
}
