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

layout(set = 0, binding = 5) readonly buffer materialBuffer {
  uint32_ar albedos[];
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
    // vec4 posLocal = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
    // vec3 posWorld = pcs.transforms.transforms[pcs.transform_idx] * posLocal;

    // vec4 p = vec4(posWorld, 1.0);

    // gl_Position = sceneData.viewproj * p;


    
	// vec4 positionLocal = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	// vec4 positionWorld = pcs.modelMatrix * positionLocal;
 //    gl_Position = sceneData.viewproj * positionWorld;


 //    DrawData dd = draws.yes[gl_DrawIDARB];
 //    // mat4 td = mat4x3(transforms.mat[dd.transform_idx], vec4(0.0f));

 //    mat4x3 td = transforms.mat[dd.transform_idx];
 //    // mat4 td = mat4(td1[0], td1[1], td1[2], vec4(0.0f));


	// vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0f);
	// vec3 positionWorld = td * positionLocal, 1.0f;


    DrawData dd = draws.yes[gl_DrawIDARB];
    TransformData td = transforms.v[dd.transform_idx];
    // mat4x3 td = transforms.v[dd.transform_idx];
    // mat4 td = mat4(td1[0], td1[1], td1[2], vec4(0.0f));

	vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0);
	vec3 positionWorld = mat4x3(td.mat[0], td.mat[1], td.mat[2]) * positionLocal;



    gl_Position = sceneData.viewproj * vec4(positionWorld, 1.0f);

    Vertex v = verts.value[gl_VertexIndex];
    inUV.x = v.normal_uv.z;
    inUV.y = v.normal_uv.w;

    albedo_idx = materials.albedos[dd.material_idx];

	
}

#endif
