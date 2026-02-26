#include "gdl_code_generator.h"

#include <ctype.h> // For isalnum, isdigit, etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Data structure to hold information about each rule for dependency analysis
typedef struct gdl_rule_info_t
{
    char * name;                    // Name of the rule
    bool needs_forward_declaration; // True if it needs epc_parser_allocate_l
    gdl_ast_node_t * ast_node;      // Pointer to the actual rule definition AST node
    struct gdl_rule_info_t * next;  // For linked list
} gdl_rule_info_t;

// List to manage all rules
typedef struct
{
    gdl_rule_info_t * head;
    gdl_rule_info_t * tail;
} gdl_rule_list_t;

// --- Helper Functions for C Code Generation ---

// Function to convert a string to PascalCase (for rule names in C)
static char *
to_pascal_case(char const * str)
{
    if (str == NULL || *str == '\0')
    {
        return strdup("");
    }

    // Allocate enough space (approx 2x original for underscores becoming capitals, plus null)
    char * pascal_str = (char *)malloc(strlen(str) * 2 + 1);
    if (pascal_str == NULL)
    {
        perror("Failed to allocate memory for PascalCase string");
        return NULL;
    }

    int i = 0;
    int j = 0;
    bool capitalize_next = true;

    while (str[i] != '\0')
    {
        if (str[i] == '_' || str[i] == '-')
        {
            capitalize_next = true;
        }
        else if (isalnum((unsigned char)str[i]))
        {
            if (capitalize_next)
            {
                pascal_str[j++] = toupper((unsigned char)str[i]);
                capitalize_next = false;
            }
            else
            {
                pascal_str[j++] = tolower((unsigned char)str[i]);
            }
        }
        i++;
    }
    pascal_str[j] = '\0';
    return pascal_str;
}

// Function to convert a string to uppercase with underscores, handling existing underscores gracefully
static char *
to_upper_case(char const * str)
{
    if (str == NULL || *str == '\0')
    {
        return strdup("");
    }

    char * upper_case_str = strdup(str);
    if (upper_case_str == NULL)
    {
        perror("Failed to allocate memory for snake_case string");
        return NULL;
    }

    for (size_t i = 0; i < strlen(upper_case_str); i++)
    {
        unsigned char c = str[i];

        upper_case_str[i] = toupper(c);
    }

    return upper_case_str;
}

// Forward declarations for rule generation
static bool generate_rule_definition_code(
    FILE * source_file, gdl_ast_node_t * rule_node, int indent_level, gdl_rule_list_t const * rule_list
);

// --- Semantic Actions Header Generation ---

// Helper to collect unique semantic action names

static void
add_unique_action_name(semantic_action_node_t ** head, char const * name)
{
    if (name == NULL || *name == '\0')
        return;

    semantic_action_node_t * current = *head;
    while (current != NULL)
    {
        if (strcmp(current->name, name) == 0)
        {
            return; // Already added
        }
        current = current->next;
    }

    // Add new name
    semantic_action_node_t * new_node = (semantic_action_node_t *)calloc(1, sizeof(*new_node));
    if (new_node == NULL)
    {
        perror("Failed to allocate semantic_action_node_t");
        return;
    }
    new_node->name = strdup(name);
    new_node->next = *head;
    *head = new_node;
}

void
gdl_free_semantic_action_list(semantic_action_node_t * head)
{
    semantic_action_node_t * current = head;
    while (current != NULL)
    {
        semantic_action_node_t * next = current->next;
        free(current->name);
        free(current);
        current = next;
    }
}

// New public function to collect actions
semantic_action_node_t *
gdl_collect_semantic_actions(gdl_ast_node_t * ast_root)
{
    semantic_action_node_t * action_names_head = NULL;

    if (ast_root == NULL || ast_root->type != GDL_AST_NODE_TYPE_PROGRAM)
    {
        return NULL;
    }

    gdl_ast_list_node_t * current_rule_node = ast_root->data.program.rules.head;
    while (current_rule_node != NULL)
    {
        gdl_ast_node_t * rule_def = current_rule_node->item;
        if (rule_def->type == GDL_AST_NODE_TYPE_RULE_DEFINITION)
        {
            if (rule_def->data.rule_def.semantic_action != NULL &&
                rule_def->data.rule_def.semantic_action->data.semantic_action.action_name != NULL)
            {
                add_unique_action_name(
                    &action_names_head, rule_def->data.rule_def.semantic_action->data.semantic_action.action_name
                );
            }
        }
        current_rule_node = current_rule_node->next;
    }
    return action_names_head;
}

static bool
gdl_generate_semantic_actions_header(gdl_ast_node_t * ast_root, char const * base_name, char const * output_dir)
{
    char actions_header_filepath[512];
    snprintf(actions_header_filepath, sizeof(actions_header_filepath), "%s/%s_actions.h", output_dir, base_name);

    FILE * actions_header_file = fopen(actions_header_filepath, "w");
    if (actions_header_file == NULL)
    {
        perror("Failed to open actions header file for writing");
        return false;
    }

    fprintf(actions_header_file, "// Generated semantic actions header for %s\n", base_name);
    fprintf(actions_header_file, "#pragma once\n\n");

    fprintf(actions_header_file, "typedef enum {\n");

    semantic_action_node_t * action_names_head = gdl_collect_semantic_actions(ast_root);

    // Print collected action names (reverse order because of prepend)
    semantic_action_node_t * current_action_name = action_names_head;

    // Reverse the list to print in definition order (or just iterate and collect then print sorted)
    // For simplicity, printing in reverse collection order for now.
    semantic_action_node_t * reversed_head = NULL;
    while (current_action_name != NULL)
    {
        semantic_action_node_t * temp = current_action_name->next;
        current_action_name->next = reversed_head;
        reversed_head = current_action_name;
        current_action_name = temp;
    }

    current_action_name = reversed_head;
    while (current_action_name != NULL)
    {
        char * upper_case = to_upper_case(current_action_name->name);
        fprintf(actions_header_file, "    %s,\n", upper_case);
        free(upper_case);
        current_action_name = current_action_name->next;
    }

    /*
     * Finish with a 'COUNT' value, which will be handy when creating a list of
     * callbacks for the AST builder library code.
     */
    char * action_count_prefix = to_upper_case(base_name);
    fprintf(actions_header_file, "    %s_AST_ACTION_COUNT__,\n", action_count_prefix);
    free(action_count_prefix);

    fprintf(actions_header_file, "} %s_semantic_action_t;\n", base_name);
    fclose(actions_header_file);
    fprintf(stdout, "Generated: %s\n", actions_header_filepath);

    gdl_free_semantic_action_list(reversed_head); // Free the collected names
    return true;
}

static bool generate_expression_code(
    FILE * source_file,
    gdl_ast_node_t * expression_node,
    int indent_level,
    gdl_rule_list_t const * rule_list,
    char const * expression_name
);

// --- Rule List Management (for dependency analysis) ---

static void
gdl_rule_list_init(gdl_rule_list_t * list)
{
    list->head = NULL;
    list->tail = NULL;
}

static void
gdl_rule_list_add(gdl_rule_list_t * list, char const * rule_name, gdl_ast_node_t * ast_node)
{
    gdl_rule_info_t * new_node = (gdl_rule_info_t *)calloc(1, sizeof(*new_node));
    if (new_node == NULL)
    {
        perror("Failed to allocate gdl_rule_info_t");
        return;
    }
    new_node->name = strdup(rule_name);
    new_node->needs_forward_declaration = false; // Default
    new_node->ast_node = ast_node;
    new_node->next = NULL;

    if (list->head == NULL)
    {
        list->head = new_node;
        list->tail = new_node;
    }
    else
    {
        list->tail->next = new_node;
        list->tail = new_node;
    }
}

static gdl_rule_info_t *
gdl_rule_list_find(gdl_rule_list_t * list, char const * rule_name)
{
    gdl_rule_info_t * current = list->head;
    while (current != NULL)
    {
        if (strcmp(current->name, rule_name) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void
gdl_rule_list_free(gdl_rule_list_t * list)
{
    gdl_rule_info_t * current = list->head;
    while (current != NULL)
    {
        gdl_rule_info_t * next = current->next;
        free(current->name);
        free(current);
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
}

// Helper for gdl_analyze_rule_dependencies to traverse rule definitions
static void
traverse_expression_for_references(
    gdl_ast_node_t * expression_node, gdl_rule_info_t const * current_rule_info, gdl_rule_list_t * all_rules
)
{
    if (expression_node == NULL)
    {
        return;
    }

    switch (expression_node->type)
    {
    case GDL_AST_NODE_TYPE_IDENTIFIER_REF:
    {
        gdl_rule_info_t * referenced_rule = gdl_rule_list_find(all_rules, expression_node->data.identifier_ref.name);
        if (referenced_rule != NULL && referenced_rule != current_rule_info)
        {
            // Check if the referenced rule appears later in the list than the current rule
            gdl_rule_info_t * temp_current = all_rules->head;
            bool found_current = false;
            bool found_referenced = false;

            while (temp_current != NULL)
            {
                if (temp_current == current_rule_info)
                    found_current = true;
                if (temp_current == referenced_rule)
                    found_referenced = true;

                if (found_current && !found_referenced)
                {
                    // Current rule is before the referenced rule in the list order
                    // Thus, this is a forward reference.
                    referenced_rule->needs_forward_declaration = true;
                    break;
                }
                if (found_referenced && !found_current)
                {
                    // Referenced rule is before the current rule, no forward declaration needed here.
                    break;
                }
                temp_current = temp_current->next;
            }
        }
        break;
    }
    case GDL_AST_NODE_TYPE_SEQUENCE:
    {
        gdl_ast_list_node_t * current = expression_node->data.sequence.elements.head;
        while (current != NULL)
        {
            traverse_expression_for_references(current->item, current_rule_info, all_rules);
            current = current->next;
        }
        break;
    }
    case GDL_AST_NODE_TYPE_ALTERNATIVE:
    {
        gdl_ast_list_node_t * current = expression_node->data.alternative.alternatives.head;
        while (current != NULL)
        {
            traverse_expression_for_references(current->item, current_rule_info, all_rules);
            current = current->next;
        }
        break;
    }
    case GDL_AST_NODE_TYPE_REPETITION_EXPRESSION:
    {
        traverse_expression_for_references(
            expression_node->data.repetition_expr.expression, current_rule_info, all_rules
        );
        break;
    }
    case GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION:
    {
        traverse_expression_for_references(expression_node->data.optional.expr, current_rule_info, all_rules);
        break;
    }

    case GDL_AST_NODE_TYPE_TERMINAL:
    {
        // Terminals wrap other expressions (string, char, keyword, identifier_ref)
        traverse_expression_for_references(expression_node->data.terminal.expression, current_rule_info, all_rules);
        break;
    }

        // Add other composite types here
    case GDL_AST_NODE_TYPE_STRING_LITERAL:
    case GDL_AST_NODE_TYPE_CHAR_LITERAL:
    case GDL_AST_NODE_TYPE_NUMBER_LITERAL:
    case GDL_AST_NODE_TYPE_CHAR_RANGE:
    case GDL_AST_NODE_TYPE_REPETITION_OPERATOR:
    case GDL_AST_NODE_TYPE_SEMANTIC_ACTION: // action name, not a rule reference
    case GDL_AST_NODE_TYPE_RAW_CHAR_LITERAL:
    case GDL_AST_NODE_TYPE_KEYWORD: // GDL keywords like 'eoi', 'digit' are not rule references
    case GDL_AST_NODE_TYPE_COMBINATOR_ONEOF:
    case GDL_AST_NODE_TYPE_COMBINATOR_NONEOF:
    case GDL_AST_NODE_TYPE_COMBINATOR_COUNT:
    case GDL_AST_NODE_TYPE_COMBINATOR_BETWEEN:
    case GDL_AST_NODE_TYPE_COMBINATOR_DELIMITED:
    case GDL_AST_NODE_TYPE_COMBINATOR_LOOKAHEAD:
    case GDL_AST_NODE_TYPE_COMBINATOR_NOT:
    case GDL_AST_NODE_TYPE_COMBINATOR_LEXEME:
    case GDL_AST_NODE_TYPE_COMBINATOR_SKIP:
    case GDL_AST_NODE_TYPE_COMBINATOR_CHAINL1:
    case GDL_AST_NODE_TYPE_COMBINATOR_CHAINR1:
    case GDL_AST_NODE_TYPE_FAIL_CALL:
    case GDL_AST_NODE_TYPE_PROGRAM:         // Should not happen here
    case GDL_AST_NODE_TYPE_RULE_DEFINITION: // Should not happen here
    case GDL_AST_NODE_TYPE_ARGUMENT_LIST:   // Should be handled by FUNCTION_CALL
    case GDL_AST_NODE_TYPE_PLACEHOLDER:
        // These nodes do not contain further rule references in this context
        break;
    }
}

static bool
gdl_analyze_rule_dependencies(gdl_ast_node_t * ast_root, gdl_rule_list_t * rule_list)
{
    if (ast_root == NULL || ast_root->type != GDL_AST_NODE_TYPE_PROGRAM || rule_list == NULL)
    {
        fprintf(stderr, "Error: Invalid arguments to gdl_analyze_rule_dependencies.\n");
        return false;
    }

    gdl_rule_list_init(rule_list);

    // Pass 1: Collect all rule names and their AST nodes
    gdl_ast_list_node_t * current_ast_rule = ast_root->data.program.rules.head;
    while (current_ast_rule != NULL)
    {
        gdl_ast_node_t * rule_def_node = current_ast_rule->item;
        if (rule_def_node->type == GDL_AST_NODE_TYPE_RULE_DEFINITION)
        {
            gdl_rule_list_add(rule_list, rule_def_node->data.rule_def.name, rule_def_node);
        }
        current_ast_rule = current_ast_rule->next;
    }

    // Pass 2: Analyze dependencies and mark rules needing forward declarations
    gdl_rule_info_t * current_rule_info = rule_list->head;
    while (current_rule_info != NULL)
    {
        gdl_ast_node_t * rule_def_node = current_rule_info->ast_node;
        traverse_expression_for_references(rule_def_node->data.rule_def.definition, current_rule_info, rule_list);
        current_rule_info = current_rule_info->next;
    }

    return true;
}

// --- Main Code Generation Logic ---

bool
gdl_generate_c_code(gdl_ast_node_t * ast_root, char const * base_name, char const * output_dir)
{
    if (ast_root == NULL || ast_root->type != GDL_AST_NODE_TYPE_PROGRAM || base_name == NULL || output_dir == NULL)
    {
        fprintf(stderr, "Error: Invalid arguments or AST root type to gdl_generate_c_code.\n");
        return false;
    }

    fprintf(stdout, "Generating C code for '%s' in '%s'...\n", base_name, output_dir);

    bool success = true;
    gdl_rule_list_t rule_dependencies;

    // Analyze rule dependencies to determine forward declarations
    if (!gdl_analyze_rule_dependencies(ast_root, &rule_dependencies))
    {
        return false;
    }

    // --- File Paths ---
    char header_filepath[512];
    char source_filepath[512];

    snprintf(header_filepath, sizeof(header_filepath), "%s/%s.h", output_dir, base_name);
    snprintf(source_filepath, sizeof(source_filepath), "%s/%s.c", output_dir, base_name);

    // --- Generate Semantic Actions Header (placeholder for now) ---
    // This function will now be implemented.
    if (!gdl_generate_semantic_actions_header(ast_root, base_name, output_dir))
    {
        success = false;
    }

    // --- Generate Parser Header ---
    FILE * header_file = fopen(header_filepath, "w");
    if (header_file == NULL)
    {
        perror("Failed to open header file for writing");
        return false;
    }
    fprintf(header_file, "// Generated header for %s\n", base_name);
    fprintf(header_file, "#pragma once\n\n");
    fprintf(header_file, "#include <easy_pc/easy_pc.h>\n");
    fprintf(header_file, "#include \"%s_actions.h\"\n\n", base_name); // Include actions header
    fprintf(header_file, "epc_parser_t * create_%s_parser(epc_parser_list * list);\n", base_name);
    fclose(header_file);
    fprintf(stdout, "Generated: %s\n", header_filepath);

    // --- Generate Parser Source ---
    FILE * source_file = fopen(source_filepath, "w");
    if (source_file == NULL)
    {
        perror("Failed to open source file for writing");
        return false;
    }
    fprintf(source_file, "// Generated source for %s\n", base_name);
    fprintf(source_file, "#include \"%s.h\"\n", base_name);
    fprintf(source_file, "#include \"%s_actions.h\"\n", base_name); // Include actions header
    fprintf(source_file, "#include <easy_pc/easy_pc.h>\n");
    fprintf(source_file, "#include <stddef.h>\n"); // For NULL, size_t, etc.
    fprintf(source_file, "#include <stdio.h>\n");  // For debugging, if needed
    fprintf(source_file, "\n");

    fprintf(source_file, "epc_parser_t * create_%s_parser(epc_parser_list * list)\n", base_name);
    fprintf(source_file, "{\n");

    // Validate list argument
    fprintf(source_file, "    if (list == NULL)\n");
    fprintf(source_file, "    {\n");
    fprintf(
        source_file, "        fprintf(stderr, \"Error: Parser list is NULL in create_%s_parser.\\n\");\n", base_name
    );
    fprintf(source_file, "        return NULL;\n");
    fprintf(source_file, "    }\n\n");

    // Iterate through rules to generate declarations and forward declarations
    gdl_rule_info_t * current_rule_info = rule_dependencies.head;
    if (current_rule_info != NULL)
    {
        fprintf(source_file, "    // Forward references:\n");
    }
    while (current_rule_info != NULL)
    {
        // Only allocate if a forward declaration is needed
        if (current_rule_info->needs_forward_declaration)
        {
            char * pascal_rule_name = to_pascal_case(current_rule_info->name);
            fprintf(
                source_file,
                "    epc_parser_t * %s = epc_parser_allocate_l(list, \"%s\");\n",
                pascal_rule_name,
                current_rule_info->name
            );
            free(pascal_rule_name);
        }
        current_rule_info = current_rule_info->next;
    }
    fprintf(source_file, "\n");

    // Now, iterate again to define each rule
    current_rule_info = rule_dependencies.head;
    while (current_rule_info != NULL)
    {
        gdl_ast_node_t * rule_def = current_rule_info->ast_node;
        if (!generate_rule_definition_code(source_file, rule_def, 1, &rule_dependencies))
        {
            success = false;
            break;
        }
        fprintf(source_file, "\n");
        current_rule_info = current_rule_info->next;
    }

    // Return the Program rule parser
    char * pascal_program_name = to_pascal_case(ast_root->data.program.rules.tail->item->data.rule_def.name);
    fprintf(source_file, "    return %s;\n", pascal_program_name);
    free(pascal_program_name);

    fprintf(source_file, "}\n");
    fclose(source_file);
    fprintf(stdout, "Generated: %s\n", source_filepath);

    gdl_rule_list_free(&rule_dependencies);
    return success;
}

// --- Rule Definition Code Generation ---
static bool
generate_rule_definition_code(
    FILE * source_file, gdl_ast_node_t * rule_node, int indent_level, gdl_rule_list_t const * rule_list
)
{
    if (rule_node == NULL || rule_node->type != GDL_AST_NODE_TYPE_RULE_DEFINITION)
    {
        fprintf(stderr, "Error: Invalid rule node for code generation.\n");
        return false;
    }

    char * pascal_rule_name = to_pascal_case(rule_node->data.rule_def.name);
    fprintf(source_file, "%*s// Rule: %s\n", indent_level * 4, "", rule_node->data.rule_def.name);

    gdl_rule_info_t * current_rule_info =
        gdl_rule_list_find((gdl_rule_list_t *)rule_list, rule_node->data.rule_def.name);
    if (current_rule_info == NULL)
    {
        fprintf(stderr, "Error: Rule '%s' not found in dependency list.\n", rule_node->data.rule_def.name);
        free(pascal_rule_name);
        return false;
    }

    if (!current_rule_info->needs_forward_declaration)
    {
        // If no forward declaration was needed, define it directly (epc_parser_t * RuleName = ...)
        fprintf(source_file, "%*sepc_parser_t * %s = ", indent_level * 4, "", pascal_rule_name);
        if (!generate_expression_code(
                source_file, rule_node->data.rule_def.definition, indent_level, rule_list, pascal_rule_name
            ))
        {
            free(pascal_rule_name);
            return false;
        }
        fprintf(source_file, ";\n");

        // Apply semantic action if present
        if (rule_node->data.rule_def.semantic_action != NULL &&
            rule_node->data.rule_def.semantic_action->data.semantic_action.action_name != NULL)
        {
            char * upper_case_action =
                to_upper_case(rule_node->data.rule_def.semantic_action->data.semantic_action.action_name);
            fprintf(
                source_file,
                "%*sepc_parser_set_ast_action(%s, %s);\n",
                indent_level * 4,
                "",
                pascal_rule_name,
                upper_case_action
            );
            free(upper_case_action);
        }
    }
    else
    {
        // If a forward declaration was made, define it using a _def temporary and duplicate
        fprintf(source_file, "%*sepc_parser_t * %s_def = ", indent_level * 4, "", pascal_rule_name);

        // Generate code for the definition part of the rule
        if (!generate_expression_code(
                source_file, rule_node->data.rule_def.definition, indent_level, rule_list, pascal_rule_name
            ))
        {
            free(pascal_rule_name);
            return false;
        }
        fprintf(source_file, ";\n"); // End of definition assignment

        // Apply semantic action if present
        if (rule_node->data.rule_def.semantic_action != NULL &&
            rule_node->data.rule_def.semantic_action->data.semantic_action.action_name != NULL)
        {
            char * upper_case_action =
                to_upper_case(rule_node->data.rule_def.semantic_action->data.semantic_action.action_name);
            fprintf(
                source_file,
                "%*sepc_parser_set_ast_action(%s_def, %s);\n",
                indent_level * 4,
                "",
                pascal_rule_name,
                upper_case_action
            );
            free(upper_case_action);
        }
        // Be sure to assign the action _before_ duplicating the parser_list_duplicate so the forward
        // reference also gets the action assigned
        fprintf(
            source_file,
            "%*sepc_parser_duplicate(%s, %s_def);\n",
            indent_level * 4,
            "",
            pascal_rule_name,
            pascal_rule_name
        );
    }

    free(pascal_rule_name);
    return true;
}

// --- Expression Code Generation (Recursive) ---
static bool
generate_expression_code(
    FILE * source_file,
    gdl_ast_node_t * expression_node,
    int indent_level,
    gdl_rule_list_t const * rule_list,
    char const * expression_name
)
{
    char const * dquote = "\"";
    char const * empty = "";
    char const * null = "NULL";
    char const * q;
    char const * expr_name;
    if (expression_name == NULL)
    {
        q = empty;
        expr_name = null;
    }
    else
    {
        q = dquote;
        expr_name = expression_name;
    }

    if (expression_node == NULL)
    {
        return false;
    }

    switch (expression_node->type)
    {
    case GDL_AST_NODE_TYPE_COMBINATOR_COUNT:
        fprintf(
            source_file,
            "epc_count_l(list, %s%s%s, %llu, ",
            q,
            expr_name,
            q,
            expression_node->data.count_call.count_node->data.number_literal.value
        );
        if (!generate_expression_code(
                source_file, expression_node->data.count_call.expression, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");

        break;

    case GDL_AST_NODE_TYPE_STRING_LITERAL:
        fprintf(
            source_file,
            "epc_string_l(list, %s%s%s, \"%s\")",
            q,
            expr_name,
            q,
            expression_node->data.string_literal.value
        );
        break;

    case GDL_AST_NODE_TYPE_CHAR_LITERAL:
    {
        char * ch = expression_node->data.char_literal.value; /* Acutally a string, to deal with escaping. */

        fprintf(source_file, "epc_char_l(list, %s%s%s, '%s')", q, expr_name, q, ch);
        break;
    }

    case GDL_AST_NODE_TYPE_IDENTIFIER_REF:
    {
        // This is now purely a reference to another rule
        char * pascal_ref_name = to_pascal_case(expression_node->data.identifier_ref.name);
        fprintf(source_file, "%s", pascal_ref_name);
        free(pascal_ref_name);
        break;
    }

    case GDL_AST_NODE_TYPE_KEYWORD: // Handle GDL keywords directly
    {
        char const * keyword_name = expression_node->data.keyword.name;
        // These are the keywords that map directly to epc_xxx_l functions
        if (strcmp(keyword_name, "eoi") == 0)
        {
            fprintf(source_file, "epc_eoi_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "digit") == 0)
        {
            fprintf(source_file, "epc_digit_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "alpha") == 0)
        {
            fprintf(source_file, "epc_alpha_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "alphanum") == 0)
        {
            fprintf(source_file, "epc_alphanum_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "space") == 0)
        {
            fprintf(source_file, "epc_space_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "any") == 0)
        {
            fprintf(source_file, "epc_any_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "succeed") == 0)
        {
            fprintf(source_file, "epc_succeed_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "hex_digit") == 0)
        {
            fprintf(source_file, "epc_hex_digit_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "int") == 0)
        {
            fprintf(source_file, "epc_int_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "double") == 0)
        {
            fprintf(source_file, "epc_double_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "cpp_comment") == 0)
        {
            fprintf(source_file, "epc_cpp_comment_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "c_comment") == 0)
        {
            fprintf(source_file, "epc_c_comment_l(list, \"%s\")", keyword_name);
        }
        else if (strcmp(keyword_name, "bash_comment") == 0)
        {
            fprintf(source_file, "epc_bash_comment_l(list, \"%s\")", keyword_name);
        }
        // TODO: Add more keyword mappings as needed
        else
        {
            fprintf(stderr, "Error: Unsupported GDL keyword '%s' for code generation.\n", keyword_name);
            return false;
        }
        break;
    }

    case GDL_AST_NODE_TYPE_FAIL_CALL:
    {
        char const * fail_message = expression_node->data.string_literal.value;

        fprintf(source_file, "epc_fail_l(list, \"%s\", \"%s\")", expr_name, fail_message);

        break;
    }
    case GDL_AST_NODE_TYPE_TERMINAL: // Handle generic terminal expressions
        // A TERMINAL node simply wraps another expression (string, char, keyword, identifier_ref)
        // Recurse into its wrapped expression.
        if (!generate_expression_code(
                source_file, expression_node->data.terminal.expression, indent_level, rule_list, expression_name
            ))
        {
            return false;
        }
        break;

    case GDL_AST_NODE_TYPE_SEQUENCE:
        if (expression_node->data.sequence.elements.count == 0)
        {
            // Empty sequence means succeed
            fprintf(source_file, "epc_succeed_l(list, \"empty_seq\")");
        }
        else if (expression_node->data.sequence.elements.count == 1)
        {
            // Promote the single child directly
            if (!generate_expression_code(
                    source_file,
                    expression_node->data.sequence.elements.head->item,
                    indent_level,
                    rule_list,
                    expression_name
                ))
            {
                return false;
            }
        }
        else
        {
            // Generate epc_and_l for multiple elements
            fprintf(
                source_file,
                "epc_and_l(list, %s%s%s, %d",
                q,
                expr_name,
                q,
                expression_node->data.sequence.elements.count
            );
            gdl_ast_list_node_t * current_element = expression_node->data.sequence.elements.head;
            while (current_element != NULL)
            {
                fprintf(source_file, ", ");
                if (!generate_expression_code(source_file, current_element->item, indent_level + 1, rule_list, NULL))
                {
                    return false;
                }
                current_element = current_element->next;
            }
            fprintf(source_file, ")");
        }
        break;

    case GDL_AST_NODE_TYPE_ALTERNATIVE:
        if (expression_node->data.alternative.alternatives.count == 0)
        {
            // Empty alternative means fail
            fprintf(source_file, "epc_fail_l(list, \"empty_alt\")");
        }
        else if (expression_node->data.alternative.alternatives.count == 1)
        {
            // Promote the single child directly
            if (!generate_expression_code(
                    source_file,
                    expression_node->data.alternative.alternatives.head->item,
                    indent_level,
                    rule_list,
                    expression_name
                ))
            {
                return false;
            }
        }
        else
        {
            // Generate epc_or_l for multiple alternatives
            fprintf(
                source_file,
                "epc_or_l(list, %s%s%s, %d",
                q,
                expr_name,
                q,
                expression_node->data.alternative.alternatives.count
            );
            gdl_ast_list_node_t * current_alt = expression_node->data.alternative.alternatives.head;
            while (current_alt != NULL)
            {
                fprintf(source_file, ", ");
                if (!generate_expression_code(source_file, current_alt->item, indent_level + 1, rule_list, NULL))
                {
                    return false;
                }
                current_alt = current_alt->next;
            }
            fprintf(source_file, ")");
        }
        break;
    case GDL_AST_NODE_TYPE_REPETITION_EXPRESSION:
    {
        char operator_char = expression_node->data.repetition_expr.repetition->data.repetition_op.operator_char;
        if (operator_char == '*')
        {
            fprintf(source_file, "epc_many_l(list, %s%s%s, ", q, expr_name, q);
        }
        else if (operator_char == '+')
        {
            fprintf(source_file, "epc_plus_l(list, %s%s%s, ", q, expr_name, q);
        }
        else if (operator_char == '?')
        {
            fprintf(source_file, "epc_optional_l(list, %s%s%s, ", q, expr_name, q);
        }
        else
        {
            fprintf(stderr, "Error: Unknown repetition operator '%c'.\n", operator_char);
            return false;
        }
        if (!generate_expression_code(
                source_file, expression_node->data.repetition_expr.expression, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");
        break;
    }

    case GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION:
        // This is for the 'optional()' combinator, not the '?' repetition operator
        fprintf(source_file, "epc_optional_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.optional.expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");
        break;

    case GDL_AST_NODE_TYPE_NUMBER_LITERAL:
        // If it's a direct number literal (e.g., as an argument to count()), it should be an easy_pc integer literal.
        // If it's defining a number token, it should be GDL_AST_NODE_TYPE_KEYWORD or similar.
        // This needs to be context-aware, but for now, we'll assume it's for combinator args.
        // The arithmetic_parser_language.gdl uses 'double' for number_literal, so we can emit epc_double_l or epc_int_l
        // depending on what's expected for `number_literal = double;`
        // Given arithmetic_parser_language.gdl uses `double;` keyword, it's effectively a built-in terminal.
        // The NUMBER_LITERAL AST node should only appear as a value passed to combinators (e.g., `count(5, Rule)`).
        fprintf(source_file, "%lld", expression_node->data.number_literal.value);
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_BETWEEN: // GDL_AST_NODE_TYPE_COMBINATOR_BETWEEN
        fprintf(source_file, "epc_between_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.between_call.open_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Open
        }
        fprintf(source_file, ", ");
        if (!generate_expression_code(
                source_file, expression_node->data.between_call.content_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Content
        }
        fprintf(source_file, ", ");
        if (!generate_expression_code(
                source_file, expression_node->data.between_call.close_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Close
        }
        fprintf(source_file, ")");
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_NOT:
    {
        fprintf(source_file, "epc_not_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.unary_combinator_call.expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");
        break;
    }

    case GDL_AST_NODE_TYPE_COMBINATOR_LOOKAHEAD:
    {
        fprintf(source_file, "epc_lookahead_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.unary_combinator_call.expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");
        break;
    }

    case GDL_AST_NODE_TYPE_COMBINATOR_SKIP:
    {
        fprintf(source_file, "epc_skip_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.unary_combinator_call.expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");
        break;
    }

    case GDL_AST_NODE_TYPE_COMBINATOR_CHAINL1:
    case GDL_AST_NODE_TYPE_COMBINATOR_CHAINR1:
    {
        char lr = (expression_node->type == GDL_AST_NODE_TYPE_COMBINATOR_CHAINL1) ? 'l' : 'r';
        fprintf(source_file, "epc_chain%c1_l(list, %s%s%s, ", lr, q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.chain_combinator_call.item_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Item
        }
        fprintf(source_file, ", ");
        if (!generate_expression_code(
                source_file, expression_node->data.chain_combinator_call.op_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Operator
        }
        fprintf(source_file, ")");
        break;
    }

    case GDL_AST_NODE_TYPE_COMBINATOR_DELIMITED:
        fprintf(source_file, "epc_delimited_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.delimited_call.item_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Item
        }
        fprintf(source_file, ", ");
        if (!generate_expression_code(
                source_file, expression_node->data.delimited_call.delimiter_expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false; // Delimiter
        }
        fprintf(source_file, ")");
        break;

    case GDL_AST_NODE_TYPE_CHAR_RANGE:
        fprintf(
            source_file,
            "epc_char_range_l(list, %s%s%s, '%c', '%c')",
            q,
            expr_name,
            q,
            expression_node->data.char_range.start_char,
            expression_node->data.char_range.end_char
        );
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_LEXEME:
        fprintf(source_file, "epc_lexeme_l(list, %s%s%s, ", q, expr_name, q);
        if (!generate_expression_code(
                source_file, expression_node->data.unary_combinator_call.expr, indent_level + 1, rule_list, NULL
            ))
        {
            return false;
        }
        fprintf(source_file, ")");
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_ONEOF:
    case GDL_AST_NODE_TYPE_COMBINATOR_NONEOF:
    {
        /* Need to build a string to pass to epc_none_of(). */
        char const * args_str = expression_node->data.none_or_one_of_call.args;

        if (args_str == NULL)
        {
            return NULL;
        }

        char const * parser_name =
            (expression_node->type == GDL_AST_NODE_TYPE_COMBINATOR_NONEOF) ? "none_of" : "one_of";
        fprintf(source_file, "epc_%s_l(list, %s%s%s, \"%s\")", parser_name, q, expr_name, q, args_str);

        break;
    }

        // Handle other AST node types as needed
    default:
        fprintf(stderr, "Error: Unsupported AST node type for code generation: %d\n", expression_node->type);
        return false;
    }
    return true;
}
