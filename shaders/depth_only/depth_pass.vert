#version 450
#extension GL_EXT_buffer_reference : require

// FIXME: should only have position! (deinterleaved)
struct Vertex {
	vec3 normal;
	float uv_x;
	vec4 color;
  float uv_y;
};
// 4 * 3 + 4 + 4* 3 + 4
layout(buffer_reference, scalar) readonly buffer VertexBufferDepth{ 
	Vertex vertices[];
};

layout(buffer_reference, buffer_reference_align = 8) readonly buffer PositionBuffer {
  vec3 positions[];
};

layout( push_constant ) uniform constants
{	
	mat4 modelMatrix;
	VertexBufferDepth vertexBuffer;
  PositionBuffer positionBuffer;
} pcs;

// shadow pass ubo here CSM settings would go maybe?
layout(set = 0, binding = 0) uniform shadowData {
  mat4 viewproj;
} data;

void main() 
{
	vec4 position = vec4(pcs.positionBuffer.positions[gl_VertexIndex], 1.0);
	gl_Position = data.viewproj * pcs.modelMatrix * position;
}

