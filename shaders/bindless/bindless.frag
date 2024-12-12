#include "common.h"
#include "input_structures.glsl"

#ifndef __cplusplus

layout(set = 0, binding = 0) uniform GPUSceneDataBlock { GPUSceneData sceneData; }; 
layout(set = 0, binding = 6, scalar) readonly buffer materialBuffer { BindlessMaterial materials[]; };


layout (set = 0, binding = 2) uniform ShadowMappingSettingsBlock {
  ShadowFragmentSettings shadowSettings;
};
layout(set = 0, binding = 1) uniform sampler2D shadowDepth;
layout(set = 0, binding = 3) uniform sampler2D ssaoAmbient;

layout(set = 1, binding = 0) uniform texture2D global_textures[];
layout(set = 1, binding = 1) uniform sampler global_samplers[];
layout(set = 1, binding = 2, rgba16f) uniform image2D global_images[];

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in flat uint material_idx;
layout (location = 4) in vec4 inPosLightSpace;


layout (location = 0) out vec4 outColour;

const vec2 pDisk[16] = vec2[](
vec2(0.91222, 0.38802), /* start early bailing samples*/
vec2(0.27429, 0.72063),
vec2(-0.59791, 0.55189),
vec2(-0.67385, -0.52580),
vec2(0.09907, -0.72597),
vec2(0.66040, -0.25044), /* end early bailing samples*/
vec2(0.06297, 0.05615),
vec2(0.42948, 0.25584),
vec2(0.39460, 0.53219),
vec2(0.38904, 0.60419),
vec2(0.79025, 0.30594),
vec2(0.15336, 0.27664),
vec2(0.11991, 0.51297),
vec2(-0.03102, 0.62509),
vec2(-0.21704, 0.81630),
vec2(-0.38587, 0.89740));


float IGN(vec2 fragCoord) {
    return mod(52.9829189 * mod(0.06711056 * fragCoord.x + 0.00583715 * fragCoord.y + 0.00314159, 1.0), 1.0);
}

vec2 rotate(vec2 v, float angle) {
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    return vec2(
        cosAngle * v.x - sinAngle * v.y,
        sinAngle * v.x + cosAngle * v.y
    );
}

float shadow_pcf(vec3 projCoords, float radius)
{
  float shadow = 0.0f;
  float currentDepth = projCoords.z;
   
  if (currentDepth < 0.0)
    return 0.0;

  for (int i = 0; i < 6; i++)
  {
    vec2 dir = pDisk[i];
    float randAngle = IGN(gl_FragCoord.xy) * 2 * PI;
    dir = rotate(dir, randAngle);

    float pcfDepth = texture(shadowDepth, projCoords.xy + (dir*radius)).r;
    shadow += pcfDepth > currentDepth /*+ bias*/ ? 1.0 : 0.0;
  }
  
  // NOTE: early bailing
  if (shadow < 0.01  || shadow > 5.99)
  {
    return shadow < 0.01 ? 0.0 : 1.0;
  }


  for (int i = 6; i < 16; i++)
  {
    vec2 dir = pDisk[i];
    float randAngle = IGN(gl_FragCoord.xy) * 2 * PI;
    dir = rotate(dir, randAngle);

    float pcfDepth = texture(shadowDepth, projCoords.xy + (dir*radius)).r;
    shadow += pcfDepth > currentDepth /*+ bias*/ ? 1.0 : 0.0;
  }

  return shadow /= 16;
}



void main() 
{

  BindlessMaterial mat = materials[material_idx];

  uint sampled = mat.albedo & 0x00FFFFFF;
  uint samp = mat.albedo >> 24;


  debugPrintfEXT("%i, %i", sampled, samp);
  
  vec4 albedo = texture(sampler2D(global_textures[sampled], global_samplers[samp]), inUV) * vec4(inColor, 1.0) * vec4(mat.modulate, 1.0);
  
  float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);

  
  if (shadowSettings.enabled == 1 && mat.strength < 1.01)
  {
    vec3 projCoords = inPosLightSpace.xyz / inPosLightSpace.w;
    projCoords = projCoords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
    float shadow_value = 1.0 - shadow_pcf(projCoords, shadowSettings.softness); // shadow softness parameter!
    lightValue *= shadow_value;
    lightValue = max(lightValue, 0.1f);
  }


  float ssao = texture(ssaoAmbient, gl_FragCoord.xy / vec2(1280, 800)).r;

  vec4 color = albedo * lightValue * (ssao);
  // color += vec4(mat.emissions * mat.strength, 1.0f);
  
    
  outColour = color;   
}

#endif
