#include "common.h"
#include "input_structures.glsl"

struct bindless_pcs
{
#ifdef __cplusplus
  bindless_pcs()
    : mvp{1.0f}, positions{0} {}
#endif
  mat4_ar mvp;
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


layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;

layout(location = 3) out flat uint albedo_idx;


vec3 decode_normal(vec2 f)
{
	f = f * 2.0 - 1.0;
	vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
	float t = max(-n.z, 0.0);
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;
	return normalize(n);
}

void main() 
{
    Vertex v = pcs.vertices.vertices[gl_VertexIndex];
    
	vec4 position = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	gl_Position = pcs.mvp * position;

vec3 normal_unpacked = decode_normal(v.normal_uv.xy);
	outNormal = (mat4(1.0f) * vec4(normal_unpacked, 0.f)).xyz;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	//NOTE: move to big fat ssbo
    
	outUV.x = v.normal_uv.z;
	outUV.y = v.normal_uv.w;

	albedo_idx = pcs.albedo_idx;
}

#endif
