#include "common.h"

struct GPUSceneData
{
#ifdef __cplusplus
  GPUSceneData():
    view{1.0f}, proj{1.0f}, viewproj{1.0f}, ambientColor{1.0f}, sunlightDirection{1.0f}, sunlightColor{1.0f} {}
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
  ShadowShadingSettings():
    lightViewProj{1.0f}, near{0.1f}, far{10.0f}, light_size{0.0f}, pcss_enabled{false} {} 
#endif
  mat4_ar lightViewProj;
  float_ar near;
  float_ar far;
  float_ar light_size;
  bool_ar pcss_enabled;  
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
