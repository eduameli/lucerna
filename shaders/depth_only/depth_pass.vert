#version 450
#extension GL_EXT_buffer_reference : require

// FIXME: should only have position! (deinterleaved)
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

layout( push_constant ) uniform constants
{	
	mat4 modelMatrix;
	VertexBuffer vertexBuffer;
} pcs;

// shadow pass ubo here CSM settings would go maybe?
layout(set = 0, binding = 0) uniform shadowData {
  mat4 viewproj;
} data;

void main() 
{
	Vertex v = pcs.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);
	gl_Position = data.viewproj * pcs.modelMatrix  * position;
}

