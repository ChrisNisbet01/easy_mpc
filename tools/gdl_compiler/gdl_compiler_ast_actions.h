#pragma once

#include "gdl_ast.h"
#include "easy_pc/easy_pc_ast.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Function to initialize the GDL AST hook registry
void
gdl_ast_hook_registry_init(epc_ast_hook_registry_t * registry, void * user_data);

#ifdef __cplusplus
}
#endif
