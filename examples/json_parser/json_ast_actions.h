#pragma once

#include "easy_pc/easy_pc_ast.h"

/* Function to initialize the AST hook registry for the JSON parser */
void
json_ast_hook_registry_init(epc_ast_hook_registry_t * registry);
