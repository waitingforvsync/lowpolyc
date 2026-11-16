// A lit scene with a shadow map, filtered over a few taps.

struct Scene {
    light_view_projection: mat4x4f,
    camera_view_projection: mat4x4f,
    light_position: vec3f,
}

struct Model {
    matrix: mat4x4f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) shadow_position: vec3f,
    @location(1) world_position: vec3f,
    @location(2) normal: vec3f,
}

@group(0) @binding(0) var<uniform> scene: Scene;
@group(0) @binding(1) var shadow_map: texture_depth_2d;
@group(0) @binding(2) var shadow_sampler: sampler_comparison;
@group(1) @binding(0) var<uniform> model: Model;

const ambient = 0.2;
const albedo = vec3(0.9);

@vertex
fn vs(@location(0) position: vec3f, @location(1) normal: vec3f) -> VertexOut {
    let world = model.matrix * vec4(position, 1.0);
    let from_light = scene.light_view_projection * world;

    // From clip space to the shadow map's texture space, y flipped
    let shadow = vec3(from_light.xy * vec2(0.5, -0.5) + vec2(0.5), from_light.z);
    let rotation = mat3x3f(model.matrix[0].xyz, model.matrix[1].xyz, model.matrix[2].xyz);

    return VertexOut(scene.camera_view_projection * world, shadow, world.xyz, rotation * normal);
}

@fragment
fn fs(in: VertexOut) -> @location(0) vec4f {
    let texel = 1.0 / vec2f(textureDimensions(shadow_map));
    var visible = 0.0;

    for (var y = -1; y <= 1; y++) {
        for (var x = -1; x <= 1; x++) {
            let offset = vec2f(vec2(x, y)) * texel;

            visible += textureSampleCompare(shadow_map, shadow_sampler,
                in.shadow_position.xy + offset, in.shadow_position.z - 0.007);
        }
    }
    visible /= 9.0;

    let light = normalize(scene.light_position - in.world_position);
    let lambert = max(dot(light, normalize(in.normal)), 0.0);

    return vec4(albedo * min(ambient + visible * lambert, 1.0), 1.0);
}
