#ifndef INPUT_STRUCTURES
#define INPUT_STRUCTURES

#include "common.h"

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



struct BindlessMaterial
{
#ifdef __cplusplus
  BindlessMaterial()
    : tint{1.0f, 1.0f, 1.0f}, albedo{3}, emissions{1.0f}, strength{0.0f} {}
#endif
  vec3_ar tint;
  vec3_ar emissions;
  // vec3_ar emission;
  uint32_ar albedo;
  float strength;
};


struct DrawData
{

  uint32_ar material_idx;
  uint32_ar transform_idx;
  uint32_ar indexCount;
  uint32_ar firstIndex;
};


#ifndef __cplusplus



// bda glsl types
layout(scalar, buffer_reference) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

// NOTE: idk how to improve this 
layout(scalar, buffer_reference) readonly buffer PositionBuffer {
  vec3 positions[];
};


layout(scalar, buffer_reference) readonly buffer TransformBuffer{ 
	mat4 transforms[];
};


// layout(set = 0, binding = 1) uniform sampler2D shadowDepth;
// layout (set = 0, binding = 2) uniform ShadowMappingSettingsBlock {
//   ShadowFragmentSettings shadowSettings;
// };

// layout(set = 0, binding = 3) uniform sampler2D ssaoAmbient;


#endif // is glsl
#endif // input structs
