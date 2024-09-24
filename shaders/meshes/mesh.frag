#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inlightSpace;
layout (location = 0) out vec4 outFragColor;

const vec2 Poisson32[32] = vec2[](
    vec2(-0.975402, -0.0711386),
    vec2(-0.920347, -0.41142),
    vec2(-0.883908, 0.217872),
    vec2(-0.884518, 0.568041),
    vec2(-0.811945, 0.90521),
    vec2(-0.792474, -0.779962),
    vec2(-0.614856, 0.386578),
    vec2(-0.580859, -0.208777),
    vec2(-0.53795, 0.716666),
    vec2(-0.515427, 0.0899991),
    vec2(-0.454634, -0.707938),
    vec2(-0.420942, 0.991272),
    vec2(-0.261147, 0.588488),
    vec2(-0.211219, 0.114841),
    vec2(-0.146336, -0.259194),
    vec2(-0.139439, -0.888668),
    vec2(0.0116886, 0.326395),
    vec2(0.0380566, 0.625477),
    vec2(0.0625935, -0.50853),
    vec2(0.125584, 0.0469069),
    vec2(0.169469, -0.997253),
    vec2(0.320597, 0.291055),
    vec2(0.359172, -0.633717),
    vec2(0.435713, -0.250832),
    vec2(0.507797, -0.916562),
    vec2(0.545763, 0.730216),
    vec2(0.56859, 0.11655),
    vec2(0.743156, -0.505173),
    vec2(0.736442, -0.189734),
    vec2(0.843562, 0.357036),
    vec2(0.865413, 0.763726),
    vec2(0.872005, -0.927));

/*
FIX:
- bias is random
- doesnt follow camera / CSM
- self shadowing
*/

float shadow_map(vec4 fragLightCoord)
{
  vec3 projCoords = fragLightCoord.xyz / fragLightCoord.w;
  projCoords = projCoords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0);

  float mapDepth = texture(shadowDepth, projCoords.xy).r;
  float currentDepth = projCoords.z;

  float bias = max(0.05 * (1.0 - dot(inNormal, sceneData.sunlightDirection.xyz)), 0.005);  
  return mapDepth < currentDepth + bias ? 1.0 : 0.0;
}

float shadow_map_pcf(vec4 fragLightSpace)
{
  vec3 projCoords = fragLightSpace.xyz / fragLightSpace.w;
  projCoords = projCoords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
  
  float closestDepth = texture(shadowDepth, projCoords.xy).r;
  float currentDepth = projCoords.z;
  
  float bias = max(0.005 * (1.0 - dot(inNormal, sceneData.sunlightDirection.xyz)), 0.0005);

  float shadow = 0.0f;
  vec2 texelSize = 1.0 / textureSize(shadowDepth, 0);

  for (int x = -1; x <= 1; ++x)
  {
    for (int y = -1; y <= 1; ++y)
    {
      float pcfDepth = texture(shadowDepth, projCoords.xy + vec2(x, y) * texelSize).r;
      shadow += pcfDepth < currentDepth + bias ? 1.0 : 0.0;
    }
  }

  return shadow /= 9.0;
}

/*
percentage closer soft shadows (https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf)
1) blocker finder - sample around each fragment dependant on light size(sceneData.sunlightDirection.w) and recievers distance from light source -
blue noise stochastic selecting SEED - early bailing
2) w_penumbra = (distance_reciever - distance_blocker) * light_size /  distance_blocker (?)
3) variable pcf - carry out pcf with a kernel size dependant on w_penumbra
*/


// NOTE: might be wrong
// uv based radius dependant or distance to light and light size
// similar triangle light_width/search_radius = fragdepth/fragdepth - near
float occluder_search_radius(float fragDepth)
{
  // might be the orther way around... reverse depth
  float NEAR = 0.1;
  float FAR = 20.0;
  return (sceneData.sunlightDirection.w * ((fragDepth - NEAR)/fragDepth)); 
}

// return average occludder distance -> determines kernel size
float blocker_distance(vec3 projCoords, float fragDepth)
{
  int occluders = 0;
  float totalOccluderDistance = 0;
  float searchWidth = occluder_search_radius(fragDepth);

  for (int i = 0; i < 32; i++)
  {
    vec2 uv = projCoords.xy + Poisson32[i]*searchWidth;
    float mapDepth = texture(shadowDepth, uv.xy).r;

    if (mapDepth > fragDepth)
    {
      occluders++;
      totalOccluderDistance += mapDepth;
    }
  }

  //debugPrintfEXT("%i, %f", occluders, totalOccluderDistance);

  if (occluders > 0)
  {
    return totalOccluderDistance / occluders;
  }
  else
  {
    return -1;
  }
}

// depth values are from the perspective of the light! 
float shadow_map_pcss()
{
  vec3 fragCoord = inlightSpace.xyz / inlightSpace.w;

  float fragDepth = fragCoord.z;
  //fragDepth += max(0.005 * (1.0 - dot(inNormal, sceneData.sunlightDirection.xyz)), 0.0005) // FIXME: move to vertex shader & cannonical viewing volume -> texture transform
  // need normals at any point to use normal for calculating bias

  float blockerDist = blocker_distance(fragCoord, fragDepth);
  if (blockerDist < 0)
  {
    debugPrintfEXT("no occluders!");
    return 1.0;
  }

  // calculate penumbra
  float penumbra = (fragDepth - blockerDist) * sceneData.sunlightDirection.w / blockerDist;
 
  float sum = 0.0;
  for (int i = 0; i < 32; i++)
  {
    vec2 offset = Poisson32[i];
    offset *= penumbra;
    float shadow_map_depth = texture(shadowDepth, fragCoord.xy + offset).r;
    sum += shadow_map_depth < fragDepth ? 0.0 : 1.0;
  }
  return sum / 32.0;
  // pcf with dynamic kernel size
}


// missing specular (blinn-phong) & pbr normal roughness etc
void main() 
{
	float lightValue = max(dot(normalize(inNormal), sceneData.sunlightDirection.xyz), 0.1f);

	vec3 color = inColor * texture(colorTex,inUV).xyz;
	vec3 ambient = color *  sceneData.ambientColor.xyz;
  
  float shadow_value = shadow_map_pcss();
  lightValue *= shadow_value;

	outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient ,1.0f);
}
