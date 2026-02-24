#pragma once

#include "gdl_ast.h"

#include <stdbool.h>

// Forward declaration for the linked list node used to collect action names
typedef struct semantic_action_node semantic_action_node_t;

typedef struct semantic_action_node
{
    char * name;
    struct semantic_action_node * next;
} semantic_action_node_t;


// Function to generate C code from the GDL AST
bool gdl_generate_c_code(gdl_ast_node_t * ast_root, const char * base_name, const char * output_dir);

// Function to collect all unique semantic action names from the AST
semantic_action_node_t * gdl_collect_semantic_actions(gdl_ast_node_t * ast_root);

// Function to free the linked list of semantic actions
void gdl_free_semantic_action_list(semantic_action_node_t * head);

