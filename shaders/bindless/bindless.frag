#include "common.h"
#include "input_structures.glsl"

#ifndef __cplusplus

layout(set = 0, binding = 0) uniform GPUSceneDataBlock { GPUSceneData sceneData; }; 
layout(set = 0, binding = 6, scalar) readonly buffer materialBuffer { BindlessMaterial materials[]; };

layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 1, rgba16f) uniform image2D global_images[];

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in flat uint material_idx;

layout (location = 0) out vec4 outFragColor;

void main() 
{

  BindlessMaterial mat = materials[material_idx];
  
  // FIXME: is it correct to multiply by this??
  vec4 albedo = texture(global_textures[mat.albedo], inUV) * vec4(inColor, 1.0) * vec4(mat.tint, 1.0);
  
  float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
  
  outFragColor = albedo * lightValue;
}

#endif
