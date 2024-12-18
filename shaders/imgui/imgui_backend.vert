#include "common.h"
#include "input_structures.glsl"

#ifndef __cplusplus

layout (location = 0) out vec4 outColour;
layout (location = 1) out vec2 outUV;


layout ( push_constant ) uniform constants
{
  imgui_pcs pcs;
};

void main()
{
    ImVertexFormat v = pcs.vertexBuffer.vertices[gl_VertexIndex];
    vec4 colour = unpackUnorm4x8(v.colour);
    outColour = colour;
    outUV = v.uv;

    gl_Position = vec4(v.position * pcs.scale + pcs.translate, 0.0, 1.0f);        
    // gl_Position = vec4(v.position, 0.0, 1.0);
}

#endif
