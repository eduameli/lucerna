#include "common.h"


#ifndef __cplusplus

struct GPUSceneData
{
#ifdef __cplusplus
  GPUSceneData()
    : view{1.0f}, proj{1.0f}, viewproj{1.0f}, ambientColor{1.0f}, sunlightDirection{1.0f}, sunlightColor{1.0f} {}
#endif
  mat4_ar view;
  mat4_ar proj;
  mat4_ar viewproj;
  vec4_ar ambientColor;
  vec4_ar sunlightDirection;
  vec4_ar sunlightColor;
};


layout(set = 0, binding = 0) uniform GPUSceneDataBlock {
  GPUSceneData sceneData;
}; 

// layout(location = 0) in vec2 inUV;

// layout(set = 0, binding = 0) uniform GP
layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 1, rgba16f) uniform image2D global_images[];

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in flat uint albedo_idx;
layout(location = 4) in flat vec3 mat_tint;

layout (location = 0) out vec4 outFragColor;

void main() 
{
  vec4 albedo = texture(global_textures[albedo_idx], inUV) * vec4(inColor, 1.0) * vec4(mat_tint, 1.0);
  // vec4 albedo = vec4(0.7, 1.0, 0.25, 1.0);
  
  float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
  
  outFragColor = albedo * lightValue;
  // outFragColor = vec4(mat_tint, 1.0);
  // outFragColor = vec4(inNormal, 1.0);
}

#endif
