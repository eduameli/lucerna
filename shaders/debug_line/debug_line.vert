#include "common.h"
#include "input_structures.glsl"

struct debug_line_pcs 
{
#ifdef __cplusplus
  debug_line_pcs()
    : viewproj{1.0f}, positions{0} {}
#endif
  mat4_ar viewproj;
  buffer_ar(PositionBuffer) positions;
};

#ifndef __cplusplus
layout ( push_constant ) uniform constants
{
  debug_line_pcs pcs;
};

void main() 
{
	vec4 position = vec4(pcs.positions.positions[gl_VertexIndex], 1.0);
	gl_Position = pcs.viewproj * position;
}

#endif
