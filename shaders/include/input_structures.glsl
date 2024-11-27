#ifndef INPUT_STRUCTURES
#define INPUT_STRUCTURES

#include "common.h"

struct Vertex
{
#ifdef __cplusplus
  Vertex()
    : normal_uv{0.0f}, color{1.0, 0.0, 1.0, 1.0} {}
#endif
  vec4_ar normal_uv;
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

struct ShadowFragmentSettings
{
#ifdef __cplusplus
  ShadowFragmentSettings()
    : lightViewProj{1.0f}, near{0.1f}, far{10.0f}, light_size{0.0f}, softness{0.0025}, enabled{false} {} 
#endif
  mat4_ar lightViewProj;
  float_ar near;
  float_ar far;
  float_ar light_size;
  float_ar softness;
  uint32_ar enabled;  
};

struct bloom_pcs 
{
#ifdef __cplusplus
  bloom_pcs()
    : srcResolution{0.0f}, filterRadius{0.0f}, mipLevel{0} {}
#endif
  vec2_ar srcResolution;
  float_ar filterRadius;
  uint32_ar mipLevel;
};

#ifndef __cplusplus
// ONLY GLSL DOWN HERE

layout(scalar, buffer_reference) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

// NOTE: idk how to improve this 
layout(scalar, buffer_reference) readonly buffer PositionBuffer {
  vec3 positions[];
};

layout(set = 0, binding = 0) uniform GPUSceneDataBlock {
  GPUSceneData sceneData;
}; 

layout(set = 0, binding = 1) uniform sampler2D shadowDepth;
layout (set = 0, binding = 2) uniform ShadowMappingSettingsBlock {
  ShadowFragmentSettings shadowSettings;
};

layout(set = 0, binding = 3) uniform sampler2D ssaoAmbient;


// MATERIAL SHADING -- all of this replaced by ssbo indirect draw
layout(set = 1, binding = 0) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;

#endif // is glsl
#endif // input structs
