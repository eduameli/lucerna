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


struct DrawData
{

  uint32_ar material_idx;
  uint32_ar transform_idx;
  uint32_ar indexCount;
  uint32_ar firstIndex;
};

layout(set = 0, binding = 3) readonly buffer drawDataBuffer {
  DrawData yes[];
} draws;

layout(set = 0, binding = 1) readonly buffer transformBuffer {
  mat4 mat[];
} transforms;

// layout(set = 0, binding = 6) readonly buffer materialBuffer {
//   uint32_ar albedos[];
// } materials;



layout(set = 0, binding = 2, scalar) readonly buffer positionBuffer {
  vec3_ar a[];
} posit;


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


    DrawData dd = draws.yes[gl_DrawIDARB];
    mat4 td = transforms.mat[dd.transform_idx];

	vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0);
	vec4 positionWorld = td * positionLocal;

    gl_Position = sceneData.viewproj * positionWorld;
 
    

	
}

#endif
