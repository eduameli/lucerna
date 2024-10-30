#!/bin/bash

SHADER_DIR="shaders"
INCLUDE_DIR="shaders/include"
compile_shader() {
    local SHADER="$1"
    local OUTPUT_FILE="${SHADER}.spv"

    if glslc -I "$INCLUDE_DIR" "$SHADER" -o "$OUTPUT_FILE"; then
        echo "Compiled ${SHADER##*/}"
    else
        echo "Failed to compile $SHADER"
        echo "Aborting compilation"
        exit 1
    fi
}

find "$SHADER_DIR" -name "*.comp" -o -name "*.frag" -o -name "*.vert" | while read -r SHADER; do
    OUTPUT_FILE="${SHADER}.spv"
    compile_shader "$SHADER"
done

echo "Shader compilation completed!"
