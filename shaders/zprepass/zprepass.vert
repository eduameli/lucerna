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
};

#ifndef __cplusplus
#extension GL_EXT_buffer_reference : require

layout ( push_constant ) uniform constants
{
  depth_only_pcs pcs;
};

void main() 
{
	vec4 position = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	gl_Position = sceneData.viewproj * pcs.modelMatrix * position;
}

#endif
