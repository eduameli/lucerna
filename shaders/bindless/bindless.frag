#include "common.h"

#ifndef __cplusplus

layout(location = 0) in vec2 inUV;


layout(binding = 0) uniform sampler2D global_textures[];
layout(binding = 1) uniform sampler2D global_images[];


layout (location = 0) out vec4 outFragColor;
void main() 
{
  outFragColor = texture(global_textures[10], inUV);
}

#endif
