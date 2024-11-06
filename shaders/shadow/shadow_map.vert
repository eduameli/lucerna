#include "common.h"
#include "input_structures.glsl"

struct shadow_map_pcs 
{
#ifdef __cplusplus
  shadow_map_pcs()
    : modelMatrix{1.0f}, positions{0} {}
#endif
  mat4_ar modelMatrix;
  buffer_ar(PositionBuffer) positions;
};

struct u_ShadowPass
{
#ifdef __cplusplus
  u_ShadowPass()
    : lightViewProj{1.0f} {}
#endif
  mat4_ar lightViewProj;
};

#ifndef __cplusplus
#extension GL_EXT_buffer_reference : require

layout ( push_constant ) uniform constants
{
  shadow_map_pcs pcs;
};

// uniform here
// mat4 lightView
layout(set = 0, binding = 0) uniform ShadowMappingUBO {   
  u_ShadowPass shadowData;	
};


void main() 
{
	vec4 position = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	gl_Position = shadowData.lightViewProj * pcs.modelMatrix * position;
}

#endif
