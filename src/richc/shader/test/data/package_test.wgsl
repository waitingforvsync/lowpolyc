// A shader for the package tests, which the build translates with the shader tool. Its comments
// hold the characters a C string must escape: " \ ?? and a tab:	.

struct Uniforms {
    mvp: mat4x4f,
    tint: vec4f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var colour: texture_2d<f32>;
@group(0) @binding(2) var smooth_sampler: sampler;

@vertex
fn vs(@location(0) position: vec3f, @location(1) uv: vec2f) -> VertexOut {
    return VertexOut(uniforms.mvp * vec4f(position, 1.0), uv);
}

@fragment
fn fs(in: VertexOut) -> @location(0) vec4f {
    return textureSample(colour, smooth_sampler, in.uv) * uniforms.tint;
}
