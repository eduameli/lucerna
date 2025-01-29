#include "common.h"
#include "input_structures.glsl"

#ifndef __cplusplus


layout(location = 1) out vec2 inUV;
layout (location = 2) out flat uint albedo_idx;

layout(set = 0, binding = 0, scalar) uniform GPUSceneDataBlock {
  GPUSceneData sceneData; 
}; 



layout(set = 0, binding = 1, scalar) readonly buffer drawDataBuffer { DrawData draws[]; };
layout(set = 0, binding = 2, scalar) readonly buffer transformBuffer { mat4x3 transforms[]; };
layout(set = 0, binding = 3, scalar) readonly buffer materialBuffer { StandardMaterial materials[]; };
layout(set = 0, binding = 4, scalar) readonly buffer positionBuffer { vec3_ar positions[]; };
layout(set = 0, binding = 5, scalar) readonly buffer vertexBuffer { Vertex vertices[]; };

void main() 
{
    DrawData dd = draws[gl_BaseInstance];

	vec4 positionLocal = vec4(positions[gl_VertexIndex], 1.0);
	vec3 positionWorld = transforms[dd.transform_idx] * positionLocal;



    gl_Position = sceneData.viewproj * vec4(positionWorld, 1.0f);

    Vertex v = vertices[gl_VertexIndex];
    inUV.x = v.normal_uv.z;
    inUV.y = v.normal_uv.w;

    albedo_idx = materials[dd.material_idx].albedo;
	
}
#endif
