#include "common.h"

#ifndef __cplusplus

layout(location = 0) in vec2 inUV;


layout(binding = 0) uniform sampler2D global_textures[];
layout(binding = 1, rgba16f) uniform image2D global_images[];

layout (location = 1) in flat uint albedo_idx;
layout (location = 0) out vec4 outFragColor;
void main() 
{
  outFragColor = texture(global_textures[albedo_idx], inUV);
}

#endif
