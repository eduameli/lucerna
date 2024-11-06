#include "common.h"
#include "input_structures.glsl"

struct std_material_pcs
{
#ifdef __cplusplus
  std_material_pcs()
    : modelMatrix{1.0f}, vertexBuffer{0}, positionBuffer{0}, emission{0.0f} {}
#endif
  mat4_ar modelMatrix;
  buffer_ar(VertexBuffer) vertexBuffer;
  buffer_ar(PositionBuffer) positionBuffer;
  float_ar emission;
};

#ifndef __cplusplus
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outlightSpace;
layout (location = 4) out float emission;

layout( push_constant, scalar ) uniform constants
{
  std_material_pcs pcs;
};

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
	Vertex v = pcs.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(pcs.positionBuffer.positions[gl_VertexIndex], 1.0);

	gl_Position =  sceneData.viewproj * pcs.modelMatrix * position;
  
  vec3 normal_unpacked = decode_normal(v.normal_uv.xy);

	outNormal = (pcs.modelMatrix * vec4(normal_unpacked, 0.f)).xyz;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.normal_uv.z;
	outUV.y = v.normal_uv.w;

  outlightSpace = shadowSettings.lightViewProj * (pcs.modelMatrix * position);

  emission = pcs.emission;
}

#endif
