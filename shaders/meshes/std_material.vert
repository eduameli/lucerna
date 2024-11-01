#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outlightSpace;
layout (location = 4) out float emission;

// NOTE: afaik padding is needed until i add bitangents or sm cause ssbo alignments? to 16 or 8 - scalar works only for uniforms?
layout(buffer_reference, scalar) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

// NOTE: idk how to improve this 
layout(buffer_reference, scalar) readonly buffer PositionBuffer {
  vec3 positions[];
};

layout( push_constant, scalar ) uniform constants
{
	mat4 modelMatrix;
	VertexBuffer vertexBuffer;
  PositionBuffer positionBuffer;
  float emission;
} pcs;

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

