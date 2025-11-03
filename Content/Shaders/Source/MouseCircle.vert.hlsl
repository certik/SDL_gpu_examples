struct VertexOutput
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

VertexOutput main(uint vertex_index : SV_VertexID)
{
    VertexOutput output;

    // Full-screen triangle
    float x = float(int(vertex_index) - 1);
    float y = float(int(vertex_index & 1u) * 2 - 1);
    output.position = float4(x, y, 0.0, 1.0);

    // Convert to UV coordinates (0,0 to 1,1)
    output.uv = float2((x + 1.0) * 0.5, (1.0 - y) * 0.5);

    return output;
}