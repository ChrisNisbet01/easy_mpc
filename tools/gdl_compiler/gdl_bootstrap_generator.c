#include "gdl_bootstrap_generator.h"
#include "gdl_code_generator.h" // For gdl_collect_semantic_actions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Convenience typedef
typedef semantic_action_node_t action_name_node_t;

static char *
string_to_lower(const char * str)
{
    if (str == NULL)
        return NULL;
    char * lower_str = strdup(str);
    if (lower_str == NULL)
        return NULL;
    for (int i = 0; lower_str[i]; i++)
    {
        lower_str[i] = tolower((unsigned char)lower_str[i]);
    }
    return lower_str;
}


static void
generate_ast_h(const char * prefix, const char * output_dir)
{
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s_ast.h", output_dir, prefix);
    FILE * f = fopen(filepath, "w");
    if (!f)
    {
        perror("fopen");
        return;
    }

    fprintf(f, "#pragma once\n\n");
    fprintf(f, "#include <stdbool.h>\n");
    fprintf(f, "#include <stddef.h>\n\n");
    fprintf(f, "typedef enum {\n");
    fprintf(f, "    %s_NODE_DUMMY,\n", prefix);
    fprintf(f, "} %s_node_type_t;\n\n", prefix);
    fprintf(f, "typedef struct %s_node_t {\n", prefix);
    fprintf(f, "    %s_node_type_t type;\n", prefix);
    fprintf(f, "    int dummy; // Replace with actual data\n");
    fprintf(f, "} %s_node_t;\n\n", prefix);
    fprintf(f, "void\n%s_node_free(void * node, void * user_data);\n", prefix);

    fclose(f);
    printf("Generated skeleton file: %s\n", filepath);
}

static void
generate_ast_actions_h(const char * prefix, const char * output_dir)
{
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s_ast_actions.h", output_dir, prefix);
    FILE * f = fopen(filepath, "w");
    if (!f)
    {
        perror("fopen");
        return;
    }

    fprintf(f, "#pragma once\n\n");
    fprintf(f, "#include \"easy_pc/easy_pc_ast.h\"\n\n");
    fprintf(f, "void\n%s_ast_hook_registry_init(epc_ast_hook_registry_t * registry);\n", prefix);

    fclose(f);
    printf("Generated skeleton file: %s\n", filepath);
}

static void
generate_ast_actions_c(const char * prefix, const char * output_dir, const action_name_node_t * list)
{
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s_ast_actions.c", output_dir, prefix);
    FILE * f = fopen(filepath, "w");
    if (!f)
    {
        perror("fopen");
        return;
    }

    fprintf(f, "#include \"%s_ast_actions.h\"\n", prefix);
    fprintf(f, "#include \"%s_ast.h\"\n", prefix);
    fprintf(f, "#include \"%s_actions.h\"\n", prefix);
    fprintf(f, "#include <stdio.h>\n\n");

    fprintf(f, "void\n%s_node_free(void * node, void * user_data)\n{\n", prefix);
    fprintf(f, "    // TODO: Implement node cleanup\n");
    fprintf(f, "}\n\n");

    fprintf(f, "/* --- Semantic Action Callbacks --- */\n\n");

    for (const action_name_node_t * curr = list; curr != NULL; curr = curr->next)
    {
        char * lower_name = string_to_lower(curr->name);
        fprintf(f, "static void\n%s_action(\n", lower_name);
        fprintf(f, "    epc_ast_builder_ctx_t * ctx,\n");
        fprintf(f, "    epc_cpt_node_t * node,\n");
        fprintf(f, "    void * * children,\n");
        fprintf(f, "    int count,\n");
        fprintf(f, "    void * user_data\n");
        fprintf(f, ")\n{\n");
        fprintf(f, "    // TODO: Implement action handler\n");
        fprintf(f, "}\n\n");
        free(lower_name);
    }

    fprintf(f, "void\n%s_ast_hook_registry_init(epc_ast_hook_registry_t * registry)\n{\n", prefix);
    fprintf(f, "    epc_ast_hook_registry_set_free_node(registry, %s_node_free);\n", prefix);
    for (const action_name_node_t * curr = list; curr != NULL; curr = curr->next)
    {
        char * lower_name = string_to_lower(curr->name);
        fprintf(f, "    epc_ast_hook_registry_set_action(registry, %s, %s_action);\n", curr->name, lower_name);
        free(lower_name);
    }
    fprintf(f, "}\n");

    fclose(f);
    printf("Generated skeleton file: %s\n", filepath);
}


// --- Public API ---

void
generate_ast_bootstrap_files(
    gdl_ast_node_t * program_node,
    const char * prefix,
    const char * output_dir)
{
    action_name_node_t * action_list = gdl_collect_semantic_actions(program_node);

    int count = 0;
    for (action_name_node_t * curr = action_list; curr != NULL; curr = curr->next)
    {
        count++;
    }
    printf("Found %d unique semantic actions.\n", count);

    generate_ast_h(prefix, output_dir);
    generate_ast_actions_h(prefix, output_dir);
    generate_ast_actions_c(prefix, output_dir, action_list);

    gdl_free_semantic_action_list(action_list);
}
