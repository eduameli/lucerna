#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inlightSpace;
layout (location = 0) out vec4 outFragColor;

#define GOLDEN_RATIO 1.61803
#define PI 3.14159

float IGN(int pixelX, int pixelY, int index) {
    return mod(52.9829189 * mod(0.06711056 * float(pixelX) + 0.00583715 * float(pixelY) + 0.00314159, 1.0), 1.0);
}

vec2 rotate(vec2 v, float angle) {
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    return vec2(
        cosAngle * v.x - sinAngle * v.y,
        sinAngle * v.x + cosAngle * v.y
    );
}

vec3 offset_lookup(sampler2D map, vec4 loc, vec2 offset)
{
  return textureProj(map,
                   vec4(loc.xy + offset * vec2(1/1024, 1/1024) * loc.w, loc.z, loc.w)).rgb;
}


const vec2 pDisk[16] = vec2[](
vec2(0.91222, 0.38802), /* start */
vec2(0.27429, 0.72063),
vec2(-0.59791, 0.55189),
vec2(-0.67385, -0.52580),
vec2(0.09907, -0.72597),
vec2(0.66040, -0.25044), /* end */
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

float shadow_pcf(vec3 projCoords, float radius)
{
  float shadow = 0.0f;
  float currentDepth = projCoords.z;
 
  if (currentDepth < 0.0)
    return 0.0;
  for (int i = 0; i < 6; i++)
  {
    vec2 dir = pDisk[i];
    float randAngle = IGN(int(gl_FragCoord.x), int(gl_FragCoord.y), i) * 2 * PI;
    dir = rotate(dir, randAngle);

    float pcfDepth = texture(shadowDepth, projCoords.xy + (dir*radius)).r;
    shadow += pcfDepth > currentDepth /*+ bias*/? 1.0 : 0.0;
  }
  
  // NOTE: early bailing
  if (shadow < 0.01  || shadow > 5.99)
  {
    return shadow < 0.01 ? 0.0 : 1.0;
  }


  for (int i = 6; i < 16; i++)
  {
    int index = int(floor(IGN(int(gl_FragCoord.x), int(gl_FragCoord.y), i) * 16.0));
    vec2 dir = pDisk[index];

    float randAngle = IGN(int(gl_FragCoord.x), int(gl_FragCoord.y), i) * 2 * PI;
    dir = rotate(dir, randAngle);

    float pcfDepth = texture(shadowDepth, projCoords.xy + (dir*radius)).r;
    shadow += pcfDepth > currentDepth /*+ bias*/ ? 1.0 : 0.0;
  }

  return shadow /= 16;
}


void main() 
{
	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
	vec3 color = inColor * texture(colorTex,inUV).xyz;
	vec3 ambient = color *  sceneData.ambientColor.xyz;
  
  vec3 projCoords = inlightSpace.xyz / inlightSpace.w;
  projCoords = projCoords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
  float shadow_value = 1.0 - shadow_pcf(projCoords, 0.001);
  
  lightValue *= shadow_value;
	outFragColor = vec4(color * lightValue *  1.0  + ambient,1.0f);
}
