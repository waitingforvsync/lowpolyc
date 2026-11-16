// A triangle with its corners in the shader, picked by vertex index.

@vertex
fn vs(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    var corners = array<vec2f, 3>(
        vec2(0.0, 0.5),
        vec2(-0.5, -0.5),
        vec2(0.5, -0.5)
    );
    return vec4f(corners[index], 0.0, 1.0);
}

@fragment
fn fs() -> @location(0) vec4f {
    return vec4(1.0, 0.0, 0.0, 1.0);
}
