#pragma once

#include "gdl_ast.h"

void generate_ast_bootstrap_files(
    gdl_ast_node_t *program_node,
    const char *prefix,
    const char *output_dir
);
