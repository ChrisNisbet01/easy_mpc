#!/bin/bash

# GDL Debugging Tool
# Usage: ./gdl_debug.sh <grammar.gdl> [input_string]

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <grammar.gdl> [input_string]"
    echo "If input_string is omitted, enters interactive mode."
    exit 1
fi

GDL_FILE=$1
INPUT_STRING=$2

# Resolve paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/../.." &> /dev/null && pwd )"
GDL_COMPILER="$PROJECT_ROOT/build/tools/gdl_compiler/gdl_compiler"
TEMPLATE_MAIN="$SCRIPT_DIR/template_main.c"
EASY_PC_LIB="$PROJECT_ROOT/build/lib/libeasy_pc.a"
EASY_PC_INCLUDE="$PROJECT_ROOT/include"

if [ ! -f "$GDL_COMPILER" ]; then
    echo "Error: gdl_compiler not found at $GDL_COMPILER. Please build the project first."
    exit 1
fi

if [ ! -f "$GDL_FILE" ]; then
    echo "Error: GDL file not found: $GDL_FILE"
    exit 1
fi

# Create a temporary directory for generated files
TMP_DIR=$(mktemp -d)

BASE_NAME=$(basename "$GDL_FILE" .gdl)
GEN_C="$TMP_DIR/$BASE_NAME.c"
GEN_H="$TMP_DIR/$BASE_NAME.h"
DEBUG_EXE="$TMP_DIR/gdl_debug_exe"

# 1. Compile GDL to C
"$GDL_COMPILER" "$GDL_FILE" "--output-dir=$TMP_DIR" > "$TMP_DIR/gdl_compilation.log" 2>&1
if [ $? -ne 0 ]; then
    echo "GDL Compilation Failed:"
    cat "$TMP_DIR/gdl_compilation.log"
    rm -rf "$TMP_DIR"
    exit 1
fi

# 2. Compile and Link with template_main.c
CREATE_PARSER_FN="create_${BASE_NAME}_parser"
gcc -DCREATE_PARSER_FN=$CREATE_PARSER_FN \
    "$TEMPLATE_MAIN" "$GEN_C" \
    -I"$EASY_PC_INCLUDE" -I"$TMP_DIR" \
    "$EASY_PC_LIB" -o "$DEBUG_EXE" > "$TMP_DIR/c_compilation.log" 2>&1

if [ $? -ne 0 ]; then
    echo "C Compilation Failed:"
    cat "$TMP_DIR/c_compilation.log"
    rm -rf "$TMP_DIR"
    exit 1
fi

# 3. Run the debugger
if [ -z "$INPUT_STRING" ]; then
    "$DEBUG_EXE"
else
    "$DEBUG_EXE" "$INPUT_STRING"
fi
EXIT_CODE=$?

# Cleanup
rm -rf "$TMP_DIR"

exit $EXIT_CODE
