#version 460


layout(set = 1, binding = 0) uniform texture2D global_textures[];
layout(set = 1, binding = 1) uniform sampler global_samplers[];
layout(set = 1, binding = 2, rgba16f) uniform image2D global_images[];

layout (location = 1) in vec2 inUV;
layout (location = 2) in flat uint albedo_idx;

void main() 
{
    uint sampled = albedo_idx & 0x00FFFFFF;
    uint samp = albedo_idx >> 24;

    // if (texture(sampler2D(global_textures[sampled], global_samplers[samp]), inUV).w < 0.5)
    // {
    //     discard;
    // }
}

