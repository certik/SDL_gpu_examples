cbuffer Uniforms : register(b0, space3)
{
    float2 mouse_pos : packoffset(c0);
    float2 resolution : packoffset(c0.z);
};

struct VertexOutput
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

float4 main(VertexOutput input) : SV_Target0
{
    // Convert UV to screen coordinates
    float2 screen_pos = input.uv * resolution;

    // Calculate distance from mouse position
    float dist = length(screen_pos - mouse_pos);

    // Draw a circle with radius 50 pixels
    float circle_radius = 50.0;
    float4 circle_color = float4(1.0, 0.0, 0.0, 1.0); // Red
    float4 bg_color = float4(0.0, 1.0, 0.0, 1.0); // Green

    // Smooth circle edge
    float edge_smoothness = 2.0;
    float t = smoothstep(circle_radius + edge_smoothness, circle_radius - edge_smoothness, dist);

    return lerp(bg_color, circle_color, t);
}