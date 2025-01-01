#!/bin/bash

SHADER_DIR="shaders"
INCLUDE_DIR="shaders/include"

compile_shader() {
    local SHADER="$1"
    local OUTPUT_FILE="${SHADER}.spv"
    
    # Check if .spv file exists and is not older than the shader file
    if [ -f "$OUTPUT_FILE" ] && [ "$OUTPUT_FILE" -nt "$SHADER" ]; then
        return
    fi
    
    echo "Compiling $SHADER..."
    glslangValidator -V -I"$INCLUDE_DIR" \
        -g \
        --P "#extension GL_GOOGLE_include_directive: enable" \
        --P "#extension GL_EXT_scalar_block_layout: enable" \
        --P "#extension GL_EXT_buffer_reference: require" \
        --P "#extension GL_EXT_nonuniform_qualifier: enable" \
        --P "#extension GL_EXT_debug_printf: enable" \
        --P "#extension GL_KHR_shader_subgroup_basic: enable" \
        --P "#extension GL_KHR_shader_subgroup_arithmetic: enable" \
        --P "#extension GL_KHR_shader_subgroup_ballot: enable" \
        --target-env vulkan1.3 \
        --glsl-version 460 \
        "$SHADER" -o "$OUTPUT_FILE"
}

echo "Compiling shaders (*.comp, *.frag, *.vert)"

# Loop through all shader files and compile if necessary
find "$SHADER_DIR" -name "*.comp" -o -name "*.frag" -o -name "*.vert" | while read -r SHADER; do
    compile_shader "$SHADER"
done

echo "Finished compilation!"
