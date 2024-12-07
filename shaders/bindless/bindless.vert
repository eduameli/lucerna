#include "common.h"
#include "input_structures.glsl"

#ifndef __cplusplus

layout(set = 0, binding = 0, scalar) uniform GPUSceneDataBlock {
  GPUSceneData sceneData;
}; 


layout (set = 0, binding = 2) uniform ShadowMappingSettingsBlock {
  ShadowFragmentSettings shadowSettings;
};



layout(set = 0, binding = 4, scalar) readonly buffer drawDataBuffer { DrawData draws[]; };
layout(set = 0, binding = 5, scalar) readonly buffer transformBuffer { mat4x3 transforms[]; };
layout(set = 0, binding = 6, scalar) readonly buffer materialBuffer { BindlessMaterial materials[]; };
layout(set = 0, binding = 7, scalar) readonly buffer positionBuffer { vec3_ar positions[]; };
layout(set = 0, binding = 8, scalar) readonly buffer vertexBuffer { Vertex vertices[]; };

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out flat uint material_idx;
layout (location = 4) out vec4 outPosLightSpace;

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

  DrawData dd = draws[gl_BaseInstance];
  vec4 positionLocal = vec4(positions[gl_VertexIndex], 1.0);
  vec3 positionWorld = transforms[dd.transform_idx] * positionLocal;

  gl_Position = sceneData.viewproj * vec4(positionWorld, 1.0f);

  Vertex v = vertices[gl_VertexIndex];
  vec3 normal_unpacked = decode_normal(v.normal_uv.xy);
  outNormal = normalize(transforms[dd.transform_idx] * vec4(normal_unpacked, 0.0));
  outNormal = normal_unpacked;
  outColor = v.color.xyz;
  outUV.x = v.normal_uv.z;
  outUV.y = v.normal_uv.w;


  material_idx = dd.material_idx;

  outPosLightSpace = shadowSettings.lightViewProj * vec4(positionWorld, 1.0f);
}

#endif
