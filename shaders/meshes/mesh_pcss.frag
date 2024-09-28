#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inlightSpace;
layout (location = 0) out vec4 outFragColor;
layout (location = 4) in float LIGHT_SIZE;
layout (location = 5) in float NEAR;
layout (location = 6) in float EMISSION;

/*
FIX:
- bias is random
- doesnt follow camera / CSM
- self shadowing
*/

/*
FIXME:
- subtracted 1.0 - depth for every equation to carry out similar triangles and other stuff correctly??
- add regular pcf instead of pcss toggle uniform- looks good enough 99% of the time and much faster
*/

float random(vec3 seed, int i)
{
  vec4 seed4 = vec4(seed, i);
  float dot_product = dot(seed4, vec4(12.9898, 78.233, 45.164, 94.673));
  return fract(sin(dot_product) * 43758.5453);
}

vec2 rotate(vec2 v, float angle) {
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    return vec2(
        cosAngle * v.x - sinAngle * v.y,
        sinAngle * v.x + cosAngle * v.y
    );
}

const vec2 pDisk[36] = vec2[](
vec2(0.91222, 0.38802),
vec2(0.27429, 0.72063),
vec2(-0.59791, 0.55189),
vec2(-0.67385, -0.52580),
vec2(0.09907, -0.72597),
vec2(0.66040, -0.25044),

vec2(0.06297, 0.05615),
vec2(0.42948, 0.25584),
vec2(0.39460, 0.53219),
vec2(0.38904, 0.60419),
vec2(0.79025, 0.30594),
vec2(0.15336, 0.27664),
vec2(0.11991, 0.51297),
vec2(-0.03102, 0.62509),
vec2(-0.21704, 0.81630),
vec2(-0.38587, 0.89740),
vec2(-0.14198, 0.08338),
vec2(-0.29371, 0.40433),
vec2(-0.63121, 0.13820),
vec2(-0.76322, 0.43612),
vec2(-0.52605, 0.82813),
vec2(-0.20279, -0.24069),
vec2(-0.46256, -0.18254),
vec2(-0.32887, -0.55556),
vec2(-0.72768, -0.07980),
vec2(-0.91896, -0.02984),
vec2(-0.06493, -0.33446),
vec2(-0.24581, -0.51638),
vec2(-0.29337, -0.53914),
vec2(-0.32309, -0.82319),
vec2(-0.32972, -0.93972),
vec2(0.15520, -0.01004),
vec2(0.24412, -0.38117),
vec2(0.69427, -0.18588),
vec2(0.84051, -0.14852),
vec2(0.50518, -0.76159));


/*
percentage closer soft shadows (https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf)
1) blocker finder - sample around each fragment dependant on light size(sceneData.sunlightDirection.w) and recievers distance from light source -
blue noise stochastic selecting SEED - early bailing
2) w_penumbra = (distance_reciever - distance_blocker) * light_size /  distance_blocker (?)
3) variable pcf - carry out pcf with a kernel size dependant on w_penumbra
*/
float search_radius(float zReceiver)
{
  return LIGHT_SIZE * (zReceiver - NEAR) / zReceiver; 
}

float find_blocker_avg_dist(vec3 projCoords)
{
  float radius = search_radius(1.0 - projCoords.z);
  float blockerSum = 0;
  int numBlockers = 0;
  
  for (int i = 0; i < 36; i++)
  {
    float shadowDepth = texture(shadowDepth, projCoords.xy + (pDisk[i] * radius)).r;
    if (shadowDepth > projCoords.z)
    {
      blockerSum += shadowDepth;
      numBlockers++;

    }
  }
  
  if (numBlockers == 0)
  {
    return -1;
  }

  return blockerSum / numBlockers;
}

//FIXME: 64 samples?
float shadow_pcf(vec3 projCoords, float radius)
{
  
  float shadow = 0.0f;
  float currentDepth = projCoords.z;

  //float bias = max(0.05 * (1.0 - dot(inNormal, sceneData.sunlightDirection.xyz)), 0.005);  

  for (int i = 0; i < 6; i++)
  {
    vec2 dir = pDisk[i];
    dir = rotate(dir, random(gl_FragCoord.xyz, i));

    float pcfDepth = texture(shadowDepth, projCoords.xy + (dir*radius)).r;
    shadow += pcfDepth > currentDepth /*+ bias*/? 1.0 : 0.0;
  }

  if (shadow < 0.01  || shadow > 5.99)
  {
    return shadow < 0.01 ? 0.0 : 1.0;
  }


  for (int i = 6; i < 36; i++)
  {
    vec2 dir = pDisk[i];
    dir = rotate(dir, random(gl_FragCoord.xyz, i));

    float pcfDepth = texture(shadowDepth, projCoords.xy + (dir*radius)).r;
    shadow += pcfDepth > currentDepth /*+ bias*/ ? 1.0 : 0.0;
  }

  return shadow /= 36;
}

// FIXME: use hardware to sample many at the same time, billinear?? sampler2DShadow, etc
// vectorize to optimise..
float shadow_map_pcss(vec4 fragCoord)
{
  vec3 projCoords = fragCoord.xyz / fragCoord.w;
  projCoords = projCoords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);

  float blockerDepth = find_blocker_avg_dist(projCoords);
  if (blockerDepth < 0)
  {
    return 0.0;
  }

  float penumbraRatio = LIGHT_SIZE * ((1.0 -blockerDepth) - (1.0 -projCoords.z)) / (1.0-blockerDepth);
  float filterRadiusUV = penumbraRatio * LIGHT_SIZE * (1.0-NEAR) / (1.0-projCoords.z);
  
  return shadow_pcf(projCoords, filterRadiusUV);
}




// missing specular (blinn-phong) & pbr normal roughness etc
void main() 
{
	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
	vec3 color = inColor * texture(colorTex,inUV).xyz * EMISSION;
	vec3 ambient = color *  sceneData.ambientColor.xyz * EMISSION;
  
  float shadow_value = 1.0 - shadow_map_pcss(inlightSpace);
  // 0 to 1 
  lightValue = max(lightValue * shadow_value, 0.1f);
  (1.0 - shadow_value);

	outFragColor = vec4(color * lightValue *  1.0  + ambient ,1.0f);

}
