#version 460


layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 1, rgba16f) uniform image2D global_images[];

layout (location = 1) in vec2 inUV;
layout (location = 2) in flat uint albedo_idx;
// layout (location = 3) in vec3 position;

// NOTE: alpha cutoff would go here! if alpha < ... discard

// float hash(vec2 a)
// {
//     // return fract( 1.0e4 * sin( 17.0*in.x + 0.1*in.y ) *( 0.1 + abs( sin( 13.0*in.y + in.x ))));
//     return fract(1.0e4 * sin(17.0*a.x + 0.1*a.y) * (0.1 + abs(sin(13.0* a.y + a.x))));
// }


// float hash3D( vec3 a ) {
//     return hash( vec2( hash( a.xy ), a.z ) );
// }

void main() 
{
    if (texture(global_textures[albedo_idx], inUV).w < 0.5)
    {
        discard;
    }
}

