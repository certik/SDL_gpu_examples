struct Uniforms {
    mouse_pos: vec2<f32>,
    resolution: vec2<f32>,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;

    // Full-screen triangle
    let x = f32(i32(in_vertex_index) - 1);
    let y = f32(i32(in_vertex_index & 1u) * 2 - 1);
    output.position = vec4<f32>(x, y, 0.0, 1.0);

    // Convert to UV coordinates (0,0 to 1,1)
    output.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);

    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Convert UV to screen coordinates
    let screen_pos = input.uv * uniforms.resolution;

    // Calculate distance from mouse position
    let dist = length(screen_pos - uniforms.mouse_pos);

    // Draw a circle with radius 50 pixels
    let circle_radius = 50.0;
    let circle_color = vec4<f32>(1.0, 0.0, 0.0, 1.0); // Red
    let bg_color = vec4<f32>(0.0, 1.0, 0.0, 1.0); // Green

    // Smooth circle edge
    let edge_smoothness = 2.0;
    let t = smoothstep(circle_radius + edge_smoothness, circle_radius - edge_smoothness, dist);

    return mix(bg_color, circle_color, t);
}
