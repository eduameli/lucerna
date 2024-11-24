#include "common.h"

#ifndef __cplusplus

layout(location = 0) in vec2 inUV;

layout(binding = 1) uniform sampler2D textures[];


layout (location = 0) out vec4 outFragColor;
void main() 
{
  outFragColor = vec4(1.0, 1.0, 1.0, 1.0);
}

#endif
