#include "common.h"
#include "input_structures.glsl"

struct bindless_pcs
{
#ifdef __cplusplus
  bindless_pcs()
    : matrix{1.0f}, positions{0} {}
#endif
  mat4_ar matrix;
  buffer_ar(PositionBuffer) positions;
  buffer_ar(VertexBuffer) vertices;
  uint albedo_idx;
};

#ifndef __cplusplus
#extension GL_EXT_buffer_reference : require

layout ( push_constant ) uniform constants
{
  bindless_pcs pcs;
};

layout(location = 0) out vec2 outUV;
layout(location = 1) out uint albedo_idx;

void main() 
{
    Vertex v = pcs.vertices.vertices[gl_VertexIndex];
    
	vec4 position = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	gl_Position = pcs.matrix * position;

    
	outUV.x = v.normal_uv.z;
	outUV.y = v.normal_uv.w;

	albedo_idx = pcs.albedo_idx;
}

#endif
