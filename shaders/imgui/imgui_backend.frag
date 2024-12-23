#include "common.h"
#include "input_structures.glsl"

#ifndef __cplusplus

layout (set = 1, binding = 0) uniform texture2D global_textures[];
layout (set = 1, binding = 1) uniform sampler global_samplers[];

layout (location = 0) in vec4 inColour;
layout (location = 1) in vec2 inUV;
layout (location = 0) out vec4 outColour;


layout ( push_constant ) uniform constants
{
  imgui_pcs pcs;
};

void main()
{
    vec4 colour = inColour * texture(sampler2D(global_textures[pcs.textureID], global_samplers[0]), inUV);
    colour.rgb *= colour.a;

    outColour = colour;
}

#endif
