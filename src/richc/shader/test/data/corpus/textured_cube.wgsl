// A textured, spinning cube: a model-view-projection matrix, and a colour texture.

struct Uniforms {
    model_view_projection: mat4x4f,
    tint: vec4f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) shade: f32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var surface: texture_2d<f32>;
@group(0) @binding(2) var surface_sampler: sampler;

@vertex
fn vs(@location(0) position: vec4f, @location(1) uv: vec2f) -> VertexOut {
    var out: VertexOut;

    out.position = uniforms.model_view_projection * position;
    out.uv = uv;
    out.shade = 0.5 * (position.y + 1.0);
    return out;
}

@fragment
fn fs(in: VertexOut) -> @location(0) vec4f {
    let colour = textureSample(surface, surface_sampler, in.uv);

    return vec4(colour.rgb * mix(0.6, 1.0, in.shade), colour.a) * uniforms.tint;
}
