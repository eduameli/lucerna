#include "common.h"
#include "input_structures.glsl"

struct depth_only_pcs 
{
#ifdef __cplusplus
  depth_only_pcs()
    : modelMatrix{0}, positions{0} {}
#endif
  mat4_ar modelMatrix;
  buffer_ar(PositionBuffer) positions;
  // buffer_ar(TransformBuffer) transforms;
  
  // uint transform_idx;
};

#ifndef __cplusplus
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters: require

// layout ( push_constant ) uniform constants
// {
//   depth_only_pcs pcs;
// };

layout(location = 1) out vec2 inUV;
layout (location = 2) out flat uint albedo_idx;
// layout (location = 3) out vec3 outPosition;

struct DrawData
{

  uint32_ar material_idx;
  uint32_ar transform_idx;
  uint32_ar indexCount;
  uint32_ar firstIndex;
};

struct TransformData
{
  vec4_ar mat[3];  
};

layout(set = 0, binding = 3) readonly buffer drawDataBuffer {
  DrawData yes[];
} draws;

layout(set = 0, binding = 1) readonly buffer transformBuffer {
  TransformData v[];
} transforms;


struct BindlessMaterial
{
  vec3_ar tint;  
  uint32_ar albedo;
};


layout(set = 0, binding = 5, scalar) readonly buffer materialBuffer {
  BindlessMaterial m[];
} materials;



layout(set = 0, binding = 2, scalar) readonly buffer positionBuffer {
  vec3_ar a[];
} posit;



layout(set = 0, binding = 4) readonly buffer vertexBuffer {
  Vertex value[];
} verts;


// layout(set = 0, binding = 8) readonly buffer vertexBuffer {
//   Vertex value[];
// } verts;



void main() 
{
    DrawData dd = draws.yes[gl_DrawIDARB];
    TransformData td = transforms.v[dd.transform_idx];

	vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0);
	vec3 positionWorld = mat4x3(td.mat[0], td.mat[1], td.mat[2]) * positionLocal;



    gl_Position = sceneData.viewproj * vec4(positionWorld, 1.0f);

    // outPosition = positionWorld;
    
    Vertex v = verts.value[gl_VertexIndex];
    inUV.x = v.normal_uv.z;
    inUV.y = v.normal_uv.w;

    albedo_idx = materials.m[dd.material_idx].albedo;

	
}

#endif
