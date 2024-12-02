#include "common.h"
// #include "input_structures.glsl"

#ifndef __cplusplus

struct GPUSceneData
{
#ifdef __cplusplus
  GPUSceneData()
    : view{1.0f}, proj{1.0f}, viewproj{1.0f}, ambientColor{1.0f}, sunlightDirection{1.0f}, sunlightColor{1.0f} {}
#endif
  mat4_ar view;
  mat4_ar proj;
  mat4_ar viewproj;
  vec4_ar ambientColor;
  vec4_ar sunlightDirection;
  vec4_ar sunlightColor;
};


// layout(scalar, buffer_reference) readonly buffer VertexBuffer{ 
//   Vertex vertices[];
// } verts;

// // NOTE: idk how to improve this 
// layout(scalar, buffer_reference) readonly buffer PositionBuffer {
//   vec3_ar positions[];
// } posit;


// layout(scalar, buffer_reference) readonly buffer TransformBuffer {
//   mat4x4 transforms[];
// };


// layout(scalar, buffer_reference) readonly buffer MaterialBuffer {
//   uint32_ar albedos[];
// };

layout(set = 0, binding = 0) uniform GPUSceneDataBlock {
  GPUSceneData sceneData;
}; 

#endif

// struct bindless_pcs
// {
// #ifdef __cplusplus
//   bindless_pcs()
//     : vertices{0}, positions{0}, transforms{0}, materials{0}, transform_idx{0}, material_idx{0} {} // use default img?
// #endif
//   buffer_ar(PositionBuffer) positions;
//   buffer_ar(VertexBuffer) vertices;
//   buffer_ar(TransformBuffer) transforms;
//   buffer_ar(MaterialBuffer) materials;
//   uint transform_idx;
//   uint material_idx;
// };

#ifndef __cplusplus
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_debug_printf : enable

// layout ( push_constant ) uniform constants
// {
//   bindless_pcs pcs;
// };


layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;

layout(location = 3) out flat uint albedo_idx;

struct DrawData
{

  uint32_ar material_idx;
  uint32_ar transform_idx;
  uint32_ar indexCount;
  uint32_ar firstIndex;
};

layout(set = 0, binding = 4) readonly buffer drawDataBuffer {
  DrawData yes[];
} draws;
layout(set = 0, binding = 5) readonly buffer transformBuffer {
  mat4 mat[];
} transforms;
layout(set = 0, binding = 6) readonly buffer materialBuffer {
  uint32_ar albedos[];
} materials;



layout(set = 0, binding = 7, scalar) readonly buffer positionBuffer {
  vec3_ar a[];
} posit;


layout(set = 0, binding = 8) readonly buffer vertexBuffer {
  Vertex value[];
} verts;

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

    DrawData dd = draws.yes[gl_DrawIDARB];
    mat4 td = transforms.mat[dd.transform_idx];

	vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0);
	vec4 positionWorld = td * positionLocal;

    gl_Position = sceneData.viewproj * positionWorld;


    // debugPrintfEXT("%i", gl_VertexIndex);


    Vertex v = verts.value[gl_VertexIndex];
    
    albedo_idx = materials.albedos[dd.material_idx];


    vec3 normal_unpacked = decode_normal(v.normal_uv.xy);

    outNormal = (td * vec4(normal_unpacked, 0.f)).xyz;
    outColor = v.color.xyz;	
    outUV.x = v.normal_uv.z;
    outUV.y = v.normal_uv.w;

    // outlightSpace = shadowSettings.lightViewProj * (pcs.modelMatrix * position);

    // emission = pcs.emission;



}

#endif
