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
  vec4 positions[];
};

layout( push_constant, scalar ) uniform constants
{
	mat4 modelMatrix;
	VertexBuffer vertexBuffer;
  PositionBuffer positionBuffer;
  float emission;
} pcs;

void main() 
{
	Vertex v = pcs.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = pcs.positionBuffer.positions[gl_VertexIndex];

	gl_Position =  sceneData.viewproj * pcs.modelMatrix * position;

	outNormal = (pcs.modelMatrix * vec4(v.normal, 0.f)).xyz;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

  outlightSpace = shadowSettings.lightViewProj * (pcs.modelMatrix * position);

  emission = pcs.emission;
}

