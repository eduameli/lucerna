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



layout(set = 0, binding = 0) uniform GPUSceneDataBlock {
  GPUSceneData sceneData;
}; 

#endif

#ifndef __cplusplus

#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_debug_printf : enable


layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout(location = 3) out flat uint albedo_idx;
layout(location = 4) out flat vec3 mat_tint;

struct DrawData
{

  uint32_ar material_idx;
  uint32_ar transform_idx;
  uint32_ar indexCount;
  uint32_ar firstIndex;
};


// struct TransformData
// {
//   vec4_ar mat[3];    
// };

layout(set = 0, binding = 4) readonly buffer drawDataBuffer {
  DrawData yes[];
} draws;



struct TransformData
{
  vec4_ar mat[3];  
};

layout(set = 0, binding = 5) readonly buffer transformBuffer {
  TransformData v[];
} transforms;



struct BindlessMaterial
{
  vec3_ar tint;
  uint32_ar albedo;
};

layout(set = 0, binding = 6, scalar) readonly buffer materialBuffer {
  BindlessMaterial m[];
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

 //    DrawData dd = draws.yes[gl_DrawIDARB];
 //    TransformData td = transforms.v[dd.transform_idx];
 //    // mat4x3 td = transforms.v[dd.transform_idx];
 //    // mat4 td = mat4(td1[0], td1[1], td1[2], vec4(0.0f));

	// vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0);
	// vec3 positionWorld = mat4x3(td.mat[0], td.mat[1], td.mat[2]) * positionLocal;

 //    gl_Position = sceneData.viewproj * vec4(positionWorld, 1.0f);



 DrawData dd = draws.yes[gl_DrawIDARB];
    TransformData td = transforms.v[dd.transform_idx];
	vec4 positionLocal = vec4(posit.a[gl_VertexIndex], 1.0);
	vec3 positionWorld = mat4x3(td.mat[0], td.mat[1], td.mat[2]) * positionLocal;

    gl_Position = sceneData.viewproj * vec4(positionWorld, 1.0f);

    Vertex v = verts.value[gl_VertexIndex];
    vec3 normal_unpacked = decode_normal(v.normal_uv.xy);

    outNormal = normalize((mat4x3(td.mat[0], td.mat[1], td.mat[2]) * vec4(normal_unpacked, 0.0)));
    // outNormal = normal_unpacked;
    outColor = v.color.xyz;

    
    outUV.x = v.normal_uv.z;
    outUV.y = v.normal_uv.w;

    
    albedo_idx = materials.m[dd.material_idx].albedo;
    mat_tint = materials.m[dd.material_idx].tint;

    
    // outlightSpace = shadowSettings.lightViewProj * (pcs.modelMatrix * position);
    // emission = pcs.emission;
}

#endif
