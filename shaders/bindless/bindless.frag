#include "common.h"

#ifndef __cplusplus

// layout(location = 0) in vec2 inUV;


layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 1, rgba16f) uniform image2D global_images[];

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in flat uint albedo_idx;

layout (location = 0) out vec4 outFragColor;

void main() 
{
  vec4 albedo = texture(global_textures[albedo_idx], inUV);

  
  float lightValue = max(dot(inNormal, vec3(1, 1, 1)), 0.1f);
  
  outFragColor = albedo * lightValue;
}

#endif
