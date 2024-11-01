#include "common.h"

struct Vertex
{
#ifdef __cplusplus
  Vertex()
    : padding{0.0f}, uv_x{0.0}, normal{0.0, 1.0, 0.0}, uv_y{0.0}, color{1.0, 0.0, 1.0, 1.0} {}
#endif
  vec3_ar padding;
  float_ar uv_x;
  vec3_ar normal;
  float_ar uv_y;
  vec4_ar color;
};

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

struct ShadowShadingSettings
{
#ifdef __cplusplus
  ShadowShadingSettings()
    : lightViewProj{1.0f}, near{0.1f}, far{10.0f}, light_size{0.0f}, softness{0.0025}, enabled{false} {} 
#endif
  mat4_ar lightViewProj;
  float_ar near;
  float_ar far;
  float_ar light_size;
  float_ar softness;
  uint32_ar enabled;  
};

#ifndef __cplusplus

layout(set = 0, binding = 0) uniform GPUSceneDataBlock {
  GPUSceneData sceneData;
}; 

layout(set = 0, binding = 1) uniform sampler2D shadowDepth;
layout (set = 0, binding = 2) uniform ShadowMappingSettingsBlock {
  ShadowShadingSettings shadowSettings;
};

layout(set = 1, binding = 0) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;

#endif
