#!/bin/bash

SHADER_DIR="shaders"
INCLUDE_DIR="shaders/include"
compile_shader() {
    local SHADER="$1"
    local OUTPUT_FILE="${SHADER}.spv"
    
    glslangValidator -V -I"$INCLUDE_DIR" \
    	-g \
        --P "#extension GL_GOOGLE_include_directive: enable" \
        --P "#extension GL_EXT_scalar_block_layout: enable" \
        "$SHADER" -o "$OUTPUT_FILE"
}

echo "Compiling shaders (*.comp, *.frag, *.vert)"

find "$SHADER_DIR" -name "*.comp" -o -name "*.frag" -o -name "*.vert" | while read -r SHADER; do
    OUTPUT_FILE="${SHADER}.spv"
    compile_shader "$SHADER"
done
