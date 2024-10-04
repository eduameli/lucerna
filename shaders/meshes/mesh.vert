#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outlightSpace;

layout (location = 4) out float LIGHT_SIZE;
layout (location = 5) out float NEAR;
layout (location = 6) out float EMISSION;

struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 modelMatrix;
	VertexBuffer vertexBuffer;
	float LIGHT_SIZE;
	float NEAR;
	float emission;
} pcs;

void main() 
{
	Vertex v = pcs.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position =  sceneData.viewproj * pcs.modelMatrix *position;

	outNormal = (pcs.modelMatrix * vec4(v.normal, 0.f)).xyz;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

  outlightSpace = sceneData.lightViewProj * (pcs.modelMatrix * position);

  NEAR = pcs.NEAR;
  LIGHT_SIZE = pcs.LIGHT_SIZE;
  EMISSION = pcs.emission;
}

