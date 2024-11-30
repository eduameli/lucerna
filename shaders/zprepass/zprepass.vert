#include "common.h"
#include "input_structures.glsl"

struct depth_only_pcs 
{
#ifdef __cplusplus
  depth_only_pcs()
    : modelMatrix{1.0f}, positions{0} {}
#endif
  mat4_ar modelMatrix;
  buffer_ar(PositionBuffer) positions;
  buffer_ar(TransformBuffer) transforms;
  uint transform_idx;
};

#ifndef __cplusplus
#extension GL_EXT_buffer_reference : require

layout ( push_constant ) uniform constants
{
  depth_only_pcs pcs;
};

void main() 
{
    // vec4 posLocal = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
    // vec3 posWorld = pcs.transforms.transforms[pcs.transform_idx] * posLocal;

    // vec4 p = vec4(posWorld, 1.0);

    // gl_Position = sceneData.viewproj * p;


    
	vec4 positionLocal = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	vec4 positionWorld = pcs.transforms.transforms[pcs.transform_idx] * positionLocal;
    gl_Position = sceneData.viewproj * positionWorld;
    

	
}

#endif
