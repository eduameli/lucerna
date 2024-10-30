layout(set = 0, binding = 0) uniform  SceneData{   

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 0, binding = 1) uniform sampler2D shadowDepth;

layout (set = 0, binding = 2) uniform ShadowMappingSettings
{
  mat4 lightViewProj;
  float near;
  float far;
  float light_size;
  bool pcss_enabled;
} shadowSettings;

/* 
set 0 binding 1
ubo
> near
> far
> search multiplier
> light size
>  pcss on or off
> pcf radius / quality

*/
layout(set = 1, binding = 0) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;

