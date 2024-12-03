#version 460


layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 1, rgba16f) uniform image2D global_images[];

layout (location = 1) in vec2 inUV;
layout (location = 2) in flat uint albedo_idx;
// NOTE: alpha cutoff would go here! if alpha < ... discard
void main() 
{
    if (texture(global_textures[albedo_idx], inUV).w < 0.5)
    {
        discard;
    }
}

