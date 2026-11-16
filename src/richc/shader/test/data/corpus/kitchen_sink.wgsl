// Most of what the translator handles, in one module.

alias float3 = vec3<f32>;

struct Light {
    colour: float3,
    intensity: f32,
}

struct Lights {
    lights: array<Light, 4>,
    count: u32,
    flags: vec4u,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) @interpolate(flat) id: u32,
    @location(1) @interpolate(perspective, centroid) uv: vec2f,
    @location(2) @interpolate(flat) cell: vec2i,
}

struct FragmentOut {
    @location(0) colour: vec4f,
    @location(1) id: vec4u,
    @builtin(frag_depth) depth: f32,
}

@group(0) @binding(0) var<uniform> lighting: Lights;
@group(0) @binding(1) var sky: texture_cube<f32>;
@group(0) @binding(2) var layers: texture_2d_array<f32>;
@group(0) @binding(3) var ids: texture_2d<u32>;
@group(0) @binding(4) var linear_sampler: sampler;

const LIMIT = 8;
const SCALE: f32 = 0.25;
const BIG = 4000000000;

var<private> seed: u32 = 12345u;

fn next_random() -> u32 {
    seed = seed * 747796405u + 2891336453u;
    let word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    return (word >> 22u) ^ word;
}

fn accumulate(total: ptr<function, float3>, light: Light) {
    *total += light.colour * light.intensity;
}

fn shade(id: u32) -> float3 {
    var total = float3();

    for (var i = 0u; i < min(lighting.count, 4u); i++) {
        if (lighting.flags[i % 4u] & 1u) == 0u {
            continue;
        }
        accumulate(&total, lighting.lights[i]);
    }

    var n = 0;
    loop {
        n++;
        if n % 3 == 1 {
            continue;
        }
        continuing {
            break if n >= LIMIT;
        }
    }

    switch id % 3u {
        case 0u: {
            total *= 0.5;
        }
        case 1u, 2u: {
            total = total.zyx;
        }
        default: {
        }
    }
    return total * f32(n) / f32(LIMIT);
}

fn integers(a: i32, b: i32) -> i32 {
    let q = a / b;
    let r = a % b;
    let s = (a << u32(b & 31)) >> 2u;
    let wide = BIG / 4;
    return q + r + s + i32(wide / 1000000000) + bitcast<i32>(f32(a)) % 7;
}

@vertex
fn vs(@builtin(vertex_index) vertex: u32, @builtin(instance_index) instance: u32,
    @location(0) position: vec3f, @location(1) cell: vec2i) -> VertexOut {
    let packed = pack2x16unorm(vec2(0.25, 0.75));
    let unpacked = unpack2x16unorm(packed);
    let m = mat2x2f(unpacked, unpacked.yx);
    let flipped = transpose(m) * determinant(m);

    return VertexOut(
        vec4(position * SCALE, 1.0),
        vertex + instance * 1000u + next_random() % 2u,
        flipped[0] + vec2(f32(integers(i32(vertex), 3))),
        cell
    );
}

@fragment
fn fs(in: VertexOut, @builtin(front_facing) front: bool) -> FragmentOut {
    var out: FragmentOut;
    let direction = normalize(vec3(in.uv, 1.0));
    let sky_colour = textureSampleLevel(sky, linear_sampler, direction, 0.0);
    let layer = textureSample(layers, linear_sampler, in.uv, i32(in.id % 4u));
    let id = textureLoad(ids, in.cell, 0);
    let size = textureDimensions(ids, 0);
    let edge = fwidth(in.uv.x) + dpdx(in.uv.y) + dpdyFine(in.uv.y);
    let mask = select(vec3(0.0), vec3(1.0), in.uv.xyx > vec3(0.5));

    if !front && all(id == vec4u(0u)) {
        discard;
    }
    out.colour = vec4(shade(in.id) * mask + sky_colour.rgb + layer.rgb, saturate(edge));
    out.id = vec4(id.xyz, size.x);
    out.depth = clamp(in.position.z, 0.0, 1.0);
    return out;
}
