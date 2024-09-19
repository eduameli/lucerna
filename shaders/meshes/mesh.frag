#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inlightSpace;
layout (location = 0) out vec4 outFragColor;

float shadow_mapping_calc(vec4 fragLightSpace)
{
  vec3 projCoords = fragLightSpace.xyz / fragLightSpace.w;
  projCoords = projCoords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
  
  float closestDepth = texture(shadowDepth, projCoords.xy).r;
  float currentDepth = projCoords.z;
  
  float visibility = 1.0f;
  if (closestDepth < currentDepth + 0.0005)
  {
    visibility = 0.5f;
  }
  
  return visibility;
}

void main() 
{
	float lightValue = max(dot(normalize(inNormal), sceneData.sunlightDirection.xyz), 0.1f);

	vec3 color = inColor * texture(colorTex,inUV).xyz;
	vec3 ambient = color *  sceneData.ambientColor.xyz;
  
  float shadow_value = (1.0 - shadow_mapping_calc(inlightSpace));
  lightValue *= shadow_value;

	outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient ,1.0f);
}

