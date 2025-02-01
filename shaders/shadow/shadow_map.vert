#include "common.h"
#include "input_structures.glsl"


#ifndef __cplusplus

layout(set = 0, binding = 0, scalar) uniform ShadowPassBlock {
  u_ShadowPass shadowSettings;
};

layout(set = 0, binding = 1, scalar) readonly buffer drawDataBuffer { DrawData draws[]; };
layout(set = 0, binding = 2, scalar) readonly buffer transformBuffer { mat4x3 transforms[]; };
layout(set = 0, binding = 3, scalar) readonly buffer positionBuffer { vec3_ar positions[]; };

void main() 
{
    DrawData dd = draws[gl_BaseInstance];

	vec4 positionLocal = vec4(positions[gl_VertexIndex], 1.0);
	vec3 positionWorld = transforms[dd.mesh_idx] * positionLocal;

    gl_Position = shadowSettings.lightViewProj * vec4(positionWorld, 1.0f);
}
#endif
