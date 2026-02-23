#include "gdl_compiler_ast_actions.h"

#include "gdl_ast.h"
#include "easy_pc/easy_pc_ast.h"
#include "easy_pc_private.h" // For epc_ast_builder_set_error

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define AST_DEBUG 1

#ifdef AST_DEBUG
static int debug_indent = 0;
#endif

// Helper to get text content from CPT node
static char *
get_cpt_node_text(epc_cpt_node_t * node)
{
    size_t len = epc_cpt_node_get_semantic_len(node);
    char * text = (char *)malloc(len + 1);
    if (text == NULL)
    {
        return NULL;
    }
    strncpy(text, epc_cpt_node_get_semantic_content(node), len);
    text[len] = '\0';
    return text;
}

// Helper to allocate a new GDL AST node, reporting errors to the context
static gdl_ast_node_t *
gdl_ast_node_alloc(epc_ast_builder_ctx_t * ctx, gdl_ast_node_type_t node_type)
{
    gdl_ast_node_t * node = (gdl_ast_node_t *)calloc(1, sizeof(*node));
    if (node == NULL)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate GDL AST node");
    }
    else
    {
        node->type = node_type;
    }
    return node;
}

// Internal list management functions for gdl_ast_list_t
static gdl_ast_list_t
gdl_ast_list_init(void)
{
    gdl_ast_list_t list = { NULL, NULL, 0 };
    return list;
}

static void
gdl_ast_list_append(gdl_ast_list_t * list, gdl_ast_node_t * item)
{
    if (list == NULL || item == NULL)
    {
        return;
    }

    gdl_ast_list_node_t * new_list_node = (gdl_ast_list_node_t *)calloc(1, sizeof(gdl_ast_list_node_t));
    if (new_list_node == NULL)
    {
        /* TODO: report error to ctx */
        perror("Failed to allocate gdl_ast_list_node_t for append");
        return;
    }
    new_list_node->item = item;
    new_list_node->next = NULL;

    if (list->tail == NULL)
    {
        list->head = new_list_node;
        list->tail = new_list_node;
    }
    else
    {
        list->tail->next = new_list_node;
        list->tail = new_list_node;
    }
    list->count++;
}

static void
gdl_ast_list_free_recursive(gdl_ast_list_t * list, void * user_data)
{
    if (list == NULL)
    {
        return;
    }

    gdl_ast_list_node_t * current = list->head;
    while (current != NULL)
    {
        gdl_ast_list_node_t * next = current->next;
        gdl_ast_node_free(current->item, user_data); // Free the AST node itself
        free(current);                     // Free the list node wrapper
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}


// --- GDL AST Node Free (adapted to epc_ast_node_free_cb signature) ---
void
gdl_ast_node_free(void * node_ptr, void * user_data)
{
    gdl_ast_node_t * node = (gdl_ast_node_t *)node_ptr;
    if (node == NULL)
    {
        return;
    }

    // Free specific data based on type (recursive for child nodes)
    switch (node->type)
    {
    case GDL_AST_NODE_TYPE_PROGRAM:
        gdl_ast_list_free_recursive(&node->data.program.rules, user_data);
        break;

    case GDL_AST_NODE_TYPE_RULE_DEFINITION:
        free((char *)node->data.rule_def.name); // Allocated by strdup
        gdl_ast_node_free(node->data.rule_def.definition, user_data);
        gdl_ast_node_free(node->data.rule_def.semantic_action, user_data);
        break;

    case GDL_AST_NODE_TYPE_IDENTIFIER_REF:
        free((char *)node->data.identifier_ref.name);
        break;

    case GDL_AST_NODE_TYPE_STRING_LITERAL:
    case GDL_AST_NODE_TYPE_FAIL_CALL:
        free((char *)node->data.string_literal.value);
        break;

    case GDL_AST_NODE_TYPE_KEYWORD:
        free((char *)node->data.keyword.name);
        break;

    case GDL_AST_NODE_TYPE_TERMINAL:
        gdl_ast_node_free(node->data.terminal.expression, user_data);
        break;

    case GDL_AST_NODE_TYPE_SEMANTIC_ACTION:
        free((char *)node->data.semantic_action.action_name);
        break;

    case GDL_AST_NODE_TYPE_REPETITION_EXPRESSION:
        gdl_ast_node_free(node->data.repetition_expr.expression, user_data);
        gdl_ast_node_free(node->data.repetition_expr.repetition, user_data);
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_ONEOF:
    case GDL_AST_NODE_TYPE_COMBINATOR_NONEOF:
        free((char *)node->data.none_or_one_of_call.args); // Common arg list for both
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_COUNT:
        gdl_ast_node_free(node->data.count_call.count_node, user_data);
        gdl_ast_node_free(node->data.count_call.expression, user_data);
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_BETWEEN:
        gdl_ast_node_free(node->data.between_call.open_expr, user_data);
        gdl_ast_node_free(node->data.between_call.content_expr, user_data);
        gdl_ast_node_free(node->data.between_call.close_expr, user_data);
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_DELIMITED:
        gdl_ast_node_free(node->data.delimited_call.item_expr, user_data);
        gdl_ast_node_free(node->data.delimited_call.delimiter_expr, user_data);
        break;

    case GDL_AST_NODE_TYPE_COMBINATOR_LOOKAHEAD:
    case GDL_AST_NODE_TYPE_COMBINATOR_NOT:
    case GDL_AST_NODE_TYPE_COMBINATOR_LEXEME:
    case GDL_AST_NODE_TYPE_COMBINATOR_SKIP:
        gdl_ast_node_free(node->data.unary_combinator_call.expr, user_data);
        break;

    case GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION:
        gdl_ast_node_free(node->data.optional.expr, user_data);
        break;


    case GDL_AST_NODE_TYPE_COMBINATOR_CHAINL1:
    case GDL_AST_NODE_TYPE_COMBINATOR_CHAINR1:
        gdl_ast_node_free(node->data.chain_combinator_call.item_expr, user_data);
        gdl_ast_node_free(node->data.chain_combinator_call.op_expr, user_data);
        break;

    case GDL_AST_NODE_TYPE_SEQUENCE:
        gdl_ast_list_free_recursive(&node->data.sequence.elements, user_data);
        break;

    case GDL_AST_NODE_TYPE_ALTERNATIVE:
        gdl_ast_list_free_recursive(&node->data.alternative.alternatives, user_data);
        break;

    case GDL_AST_NODE_TYPE_ARGUMENT_LIST:
        gdl_ast_list_free_recursive(&node->data.argument_list, user_data);
        break;

        /* The following nod types have no dynamic data to free. */
    case GDL_AST_NODE_TYPE_NUMBER_LITERAL: // No dynamic data to free
    case GDL_AST_NODE_TYPE_CHAR_LITERAL:   // No dynamic data to free
    case GDL_AST_NODE_TYPE_CHAR_RANGE:     // No dynamic data to free
    case GDL_AST_NODE_TYPE_REPETITION_OPERATOR: // No dynamic data to free
    case GDL_AST_NODE_TYPE_RAW_CHAR_LITERAL: // No dynamic data to free
    case GDL_AST_NODE_TYPE_PLACEHOLDER: // Placeholder has no data
        break;
    }

    free(node);
}

// --- Semantic Action Callbacks ---

#ifdef AST_DEBUG
static void
handle_node_entry(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void * user_data)
{
    (void)ctx;
    (void)user_data;
    if (node->ast_config.assigned)
    {
        debug_indent += 4;
        fprintf(stderr, "%*sAST_DEBUG: ENTER CPT node '%s' (action %d)\n",
                debug_indent - 4, "", node->name, node->ast_config.action);
    }
}
#endif


static void
handle_create_identifier_ref(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num children %d\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        epc_ast_builder_set_error(ctx, "Create identifier got unexpected children.");
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_IDENTIFIER_REF);
    if (ast_node)
    {
        ast_node->data.identifier_ref.name = get_cpt_node_text(node);
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_keyword(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        epc_ast_builder_set_error(ctx, "Create keyword got unexpected children.");
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_KEYWORD);
    if (ast_node)
    {
        ast_node->data.keyword.name = get_cpt_node_text(node);
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_terminal(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Terminal action expects exactly 1 child, got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * terminal_expression_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_TERMINAL);
    if (ast_node)
    {
        ast_node->data.terminal.expression = terminal_expression_node;
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_semantic_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;

    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Semantic action expects 1 child (identifier), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * identifier_node = (gdl_ast_node_t *)children[0];
    if (identifier_node->type != GDL_AST_NODE_TYPE_IDENTIFIER_REF)
    {
        epc_ast_builder_set_error(ctx, "Semantic action expects an identifier reference.");
        gdl_ast_node_free(identifier_node, user_data);
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_SEMANTIC_ACTION);
    if (ast_node == NULL)
    {
        gdl_ast_node_free(identifier_node, user_data);
        return;
    }
    ast_node->data.semantic_action.action_name = identifier_node->data.identifier_ref.name;
    identifier_node->data.identifier_ref.name = NULL; // Transfer ownership
    gdl_ast_node_free(identifier_node, user_data); // Free the wrapper node
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_optional_semantic_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    gdl_ast_node_t * semantic_action = NULL;

    if (count == 1)
    {
        semantic_action = (gdl_ast_node_t *)children[0];
        if (semantic_action->type != GDL_AST_NODE_TYPE_SEMANTIC_ACTION)
        {
            epc_ast_builder_set_error(ctx, "Optional semantic action expected a semantic action node.");
            gdl_ast_node_free(semantic_action, user_data);
            return;
        }
    }
    else if (count > 1)
    {
        epc_ast_builder_set_error(ctx, "Optional semantic action expects 0 or 1 child, got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * ast_node = semantic_action;

    if (ast_node == NULL)
    {
        /* Create an empty semantic action node with no semantic action name to push onto the stack. */
        ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_SEMANTIC_ACTION);
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_repetition_operator(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        epc_ast_builder_set_error(ctx, "Create repitition operator expects 0 child, got %d", count);
        for (int i = 0; i < count; ++i)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_REPETITION_OPERATOR);
    if (ast_node)
    {
        ast_node->data.repetition_op.operator_char = epc_cpt_node_get_semantic_content(node)[0];
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_number_literal(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        epc_ast_builder_set_error(ctx, "Create number literal expects 0 child, got %d", count);
        for (int i = 0; i < count; ++i)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_NUMBER_LITERAL);
    if (ast_node)
    {
        ast_node->data.number_literal.value = atoll(epc_cpt_node_get_semantic_content(node));
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_char_literal(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        epc_ast_builder_set_error(ctx, "Create char literal expects 0 child, got %d", count);
        for (int i = 0; i < count; ++i)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_CHAR_LITERAL);
    if (ast_node)
    {
        // Assuming char literal might includes quotes, so extract the char in between
        size_t len = epc_cpt_node_get_semantic_len(node);
        if (len >= 3 && epc_cpt_node_get_semantic_content(node)[0] == '\'' && epc_cpt_node_get_semantic_content(node)[len - 1] == '\'')
        {
            ast_node->data.char_literal.value = epc_cpt_node_get_semantic_content(node)[1];
        }
        else if (len >= 1)
        {
            ast_node->data.char_literal.value = epc_cpt_node_get_semantic_content(node)[0];
        }
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_string_literal(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        epc_ast_builder_set_error(ctx, "Create string literal expects 0 child, got %d", count);
        for (int i = 0; i < count; ++i)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_STRING_LITERAL);
    if (ast_node)
    {
        // Remove quotes from string literal
        size_t len = epc_cpt_node_get_semantic_len(node);
        char * temp_str = get_cpt_node_text(node); // Includes quotes
        if (temp_str)
        {
            if (len >= 2 && temp_str[0] == '"' && temp_str[len - 1] == '"')
            {
                ast_node->data.string_literal.value = strndup(temp_str + 1, len - 2);
            }
            else
            {
                ast_node->data.string_literal.value = strdup(temp_str);
            }
            free(temp_str);
        }
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_raw_char_literal(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    if (count > 0)
    {
        epc_ast_builder_set_error(ctx, "Create raw char literal expects 0 child, got %d", count);
        for (int i = 0; i < count; ++i)
        {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * ast_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_RAW_CHAR_LITERAL);
    if (ast_node)
    {
        ast_node->data.raw_char_literal.value = epc_cpt_node_get_semantic_content(node)[0];
    }
    epc_ast_push(ctx, ast_node);
}

static void
handle_create_program(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Program expects 1 child (sequence of rules), got %d", count);
        // Free children not consumed
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * rule_sequence_node = (gdl_ast_node_t *)children[0];
    if (rule_sequence_node->type != GDL_AST_NODE_TYPE_SEQUENCE)
    {
        epc_ast_builder_set_error(ctx, "Program expects sequence of rules, but got something else.");
        gdl_ast_node_free(rule_sequence_node, user_data);
        return;
    }

    gdl_ast_node_t * program_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_PROGRAM);
    if (program_node)
    {
        program_node->data.program.rules = rule_sequence_node->data.sequence.elements;
        // Transfer ownership of list nodes
        rule_sequence_node->data.sequence.elements.head = NULL;
        rule_sequence_node->data.sequence.elements.tail = NULL;
        rule_sequence_node->data.sequence.elements.count = 0;
    }
    gdl_ast_node_free(rule_sequence_node, user_data); // Free the wrapper node (sequence node which held the list)
    epc_ast_push(ctx, program_node); // This is the final root
}

static void
handle_create_rule_definition(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count < 2 || count > 3)
    {
        epc_ast_builder_set_error(ctx, "Rule definition expects 2 or 3 children (identifier, definition, optional_semantic_action), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * identifier_ref_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * definition_node = (gdl_ast_node_t *)children[1];
    gdl_ast_node_t * semantic_action_node = NULL;

    if (count == 3)
    {
        semantic_action_node = (gdl_ast_node_t *)children[2];
    }

    if (identifier_ref_node->type != GDL_AST_NODE_TYPE_IDENTIFIER_REF)
    {
        epc_ast_builder_set_error(ctx, "Expected identifier node for rule definition.");
        gdl_ast_node_free(identifier_ref_node, user_data);
        gdl_ast_node_free(definition_node, user_data);
        gdl_ast_node_free(semantic_action_node, user_data);
        return;
    }

    gdl_ast_node_t * rule_def_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_RULE_DEFINITION);
    if (rule_def_node)
    {
        rule_def_node->data.rule_def.name = identifier_ref_node->data.identifier_ref.name; // Transfer ownership
        identifier_ref_node->data.identifier_ref.name = NULL; // Prevent double free
        rule_def_node->data.rule_def.definition = definition_node;
        rule_def_node->data.rule_def.semantic_action = semantic_action_node;
    }
    gdl_ast_node_free(identifier_ref_node, user_data); // Free wrapper node (IdentifierRef node)
    epc_ast_push(ctx, rule_def_node);
}

static void
handle_create_char_range(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 2)
    {
        epc_ast_builder_set_error(ctx, "Char range expects 2 children (start_char, end_char), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * start_char_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * end_char_node = (gdl_ast_node_t *)children[1];

    if (start_char_node->type != GDL_AST_NODE_TYPE_RAW_CHAR_LITERAL
        || end_char_node->type != GDL_AST_NODE_TYPE_RAW_CHAR_LITERAL)
    {
        epc_ast_builder_set_error(ctx, "Char range expects raw char literals for start and end.");
        gdl_ast_node_free(start_char_node, user_data);
        gdl_ast_node_free(end_char_node, user_data);
        return;
    }

    gdl_ast_node_t * char_range_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_CHAR_RANGE);
    if (char_range_node)
    {
        char_range_node->data.char_range.start_char = start_char_node->data.raw_char_literal.value;
        char_range_node->data.char_range.end_char = end_char_node->data.raw_char_literal.value;
    }
    gdl_ast_node_free(start_char_node, user_data);
    gdl_ast_node_free(end_char_node, user_data);
    epc_ast_push(ctx, char_range_node);
}

static void
handle_create_oneof_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node; (void)user_data;
    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Oneof call expects 1 child (argument list), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * args_list_node = (gdl_ast_node_t *)children[0];
    if (args_list_node == NULL || args_list_node->type != GDL_AST_NODE_TYPE_STRING_LITERAL)
    {
        epc_ast_builder_set_error(ctx, "Oneof call expects a string literal.");
        gdl_ast_node_free(args_list_node, user_data);
        return;
    }

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_ONEOF);
    if (result_node)
    {
        result_node->data.none_or_one_of_call.args = args_list_node->data.string_literal.value;
        args_list_node->data.string_literal.value = NULL; // Transfer ownership
        gdl_ast_node_free(args_list_node, user_data); // Free the wrapper node
    }
    epc_ast_push(ctx, result_node);
}

static void
handle_create_noneof_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node; (void)user_data;
    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Noneof call expects 1 child (argument list), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * args_list_node = (gdl_ast_node_t *)children[0];
    if (args_list_node == NULL || args_list_node->type != GDL_AST_NODE_TYPE_STRING_LITERAL)
    {
        epc_ast_builder_set_error(ctx, "Noneof call expects a string literal.");
        gdl_ast_node_free(args_list_node, user_data);
        return;
    }

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_NONEOF);
    if (result_node)
    {
        result_node->data.none_or_one_of_call.args = args_list_node->data.string_literal.value;
        args_list_node->data.string_literal.value = NULL; // Transfer ownership
        gdl_ast_node_free(args_list_node, user_data); // Free the wrapper node
    }
    epc_ast_push(ctx, result_node);
}

static void
handle_create_count_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 2)
    {
        epc_ast_builder_set_error(ctx, "Count call expects 2 children (count_node, expression), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * count_val_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * expr_node = (gdl_ast_node_t *)children[1];

    if (count_val_node == NULL || count_val_node->type != GDL_AST_NODE_TYPE_NUMBER_LITERAL)
    {
        epc_ast_builder_set_error(ctx, "Count call expects a number literal for the count value.");
        gdl_ast_node_free(count_val_node, user_data);
        gdl_ast_node_free(expr_node, user_data);
        return;
    }

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_COUNT);
    if (result_node)
    {
        result_node->data.count_call.count_node = count_val_node;
        result_node->data.count_call.expression = expr_node;
    }
    epc_ast_push(ctx, result_node);
}

static void
handle_create_delimited_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 2)
    {
        epc_ast_builder_set_error(ctx, "Delimited call expects 2 children (item, delimiter), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * item_expr_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * delimiter_expr_node = (gdl_ast_node_t *)children[1];

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_DELIMITED);
    if (result_node)
    {
        result_node->data.delimited_call.item_expr = item_expr_node;
        result_node->data.delimited_call.delimiter_expr = delimiter_expr_node;
    }
    epc_ast_push(ctx, result_node);
}

static void
handle_create_between_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 3)
    {
        epc_ast_builder_set_error(ctx, "Between call expects 3 children (open, content, close), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * open_expr_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * content_expr_node = (gdl_ast_node_t *)children[1];
    gdl_ast_node_t * close_expr_node = (gdl_ast_node_t *)children[2];

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_BETWEEN);
    if (result_node)
    {
        result_node->data.between_call.open_expr = open_expr_node;
        result_node->data.between_call.content_expr = content_expr_node;
        result_node->data.between_call.close_expr = close_expr_node;
    }
    epc_ast_push(ctx, result_node);
}

// Unary combinator helper action
static void
handle_unary_combinator_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data,
    gdl_ast_node_type_t node_type
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Unary combinator call expects 1 child, got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * expr_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, node_type);
    if (result_node)
    {
        result_node->data.unary_combinator_call.expr = expr_node;
    }
    epc_ast_push(ctx, result_node);
}

static void handle_create_lookahead_call(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data) {
    handle_unary_combinator_call(ctx, node, children, count, user_data, GDL_AST_NODE_TYPE_COMBINATOR_LOOKAHEAD);
}
static void handle_create_not_call(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data) {
    handle_unary_combinator_call(ctx, node, children, count, user_data, GDL_AST_NODE_TYPE_COMBINATOR_NOT);
}
static void handle_create_lexeme_call(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data) {
    handle_unary_combinator_call(ctx, node, children, count, user_data, GDL_AST_NODE_TYPE_COMBINATOR_LEXEME);
}
static void handle_create_skip_call(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data) {
    handle_unary_combinator_call(ctx, node, children, count, user_data, GDL_AST_NODE_TYPE_COMBINATOR_SKIP);
}

static void
handle_create_chainl1_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 2)
    {
        epc_ast_builder_set_error(ctx, "Chainl1 call expects 2 children (item, op), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * item_expr_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * op_expr_node = (gdl_ast_node_t *)children[1];

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_CHAINL1);
    if (result_node)
    {
        result_node->data.chain_combinator_call.item_expr = item_expr_node;
        result_node->data.chain_combinator_call.op_expr = op_expr_node;
    }
    epc_ast_push(ctx, result_node);
}

static void
handle_create_chainr1_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count != 2)
    {
        epc_ast_builder_set_error(ctx, "Chainr1 call expects 2 children (item, op), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * item_expr_node = (gdl_ast_node_t *)children[0];
    gdl_ast_node_t * op_expr_node = (gdl_ast_node_t *)children[1];

    gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_COMBINATOR_CHAINR1);
    if (result_node)
    {
        result_node->data.chain_combinator_call.item_expr = item_expr_node;
        result_node->data.chain_combinator_call.op_expr = op_expr_node;
    }
    epc_ast_push(ctx, result_node);
}

static void
handle_create_expression_factor(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif

    (void)node;
    if (count < 1 || count > 2)
    {
        epc_ast_builder_set_error(ctx, "Expression factor expects 1 or 2 children, got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * optional_repetition_node = NULL;
    gdl_ast_node_t * primary_expression_node = (gdl_ast_node_t *)children[0];

    if (primary_expression_node->type == GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION)
    {
        epc_ast_builder_set_error(ctx, "Expression factor expected primary expression, but got optional expression.");
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }
    if (count == 2)
    {
        optional_repetition_node = (gdl_ast_node_t *)children[1];
    }

    gdl_ast_node_t * repetition_op = NULL;

    if (optional_repetition_node != NULL)
    {
        repetition_op = optional_repetition_node->data.optional.expr;
        optional_repetition_node->data.optional.expr = NULL;
        gdl_ast_node_free(optional_repetition_node, NULL);
    }

    if (repetition_op != NULL)
    {
        gdl_ast_node_t * result_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_REPETITION_EXPRESSION);
        if (result_node != NULL)
        {
            result_node->data.repetition_expr.expression = primary_expression_node;
            result_node->data.repetition_expr.repetition = repetition_op;
        }
        epc_ast_push(ctx, result_node);
    }
    else
    {
        epc_ast_push(ctx, primary_expression_node);
    }
}

static void
handle_create_sequence(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif


    (void)node;
    gdl_ast_node_t * combined_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_SEQUENCE);
    if (combined_node == NULL)
    {
        // Error already set by gdl_ast_node_alloc
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    combined_node->data.sequence.elements = gdl_ast_list_init();
    for (int i = 0; i < count; ++i)
    {
        gdl_ast_list_append(&combined_node->data.sequence.elements, (gdl_ast_node_t *)children[i]);
    }
    epc_ast_push(ctx, combined_node);
}

static void
handle_create_alternative(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif


    (void)node;
    gdl_ast_node_t * combined_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_ALTERNATIVE);
    if (combined_node == NULL)
    {
        // Error already set by gdl_ast_node_alloc
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    combined_node->data.alternative.alternatives = gdl_ast_list_init();
    for (int i = 0; i < count; ++i)
    {
        gdl_ast_list_append(&combined_node->data.alternative.alternatives, (gdl_ast_node_t *)children[i]);
    }
    epc_ast_push(ctx, combined_node);
}

static void
handle_create_optional_expression(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif


    (void)node;
    if (count > 1)
    {
        epc_ast_builder_set_error(ctx, "Optional expression expects 0 or 1 child, got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * optional_content_node = (count == 1) ? (gdl_ast_node_t *)children[0] : NULL;

    gdl_ast_node_t * optional_expr_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION);
    if (optional_expr_node)
    {
        optional_expr_node->data.optional.expr = optional_content_node;
    }
    epc_ast_push(ctx, optional_expr_node);
}

static void
handle_create_fail_call(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
#ifdef AST_DEBUG
    debug_indent -= 4;
    fprintf(stderr, "%*sAST_DEBUG: EXIT CPT node '%s' (action %d) num_children %u\n",
            debug_indent, "", node->name, node->ast_config.action, count);
    for (int i = 0; i < count; i++)
    {
        gdl_ast_node_t * child = (gdl_ast_node_t *)children[i];
        fprintf(stderr, "%*schild %d type %d\n", debug_indent, "", i, child->type);
    }
#endif


    (void)node;
    if (count != 1)
    {
        epc_ast_builder_set_error(ctx, "Fail call expects 1 child (string literal), got %d", count);
        for (int i = 0; i < count; ++i) {
            gdl_ast_node_free(children[i], user_data);
        }
        return;
    }

    gdl_ast_node_t * str_lit_node = (gdl_ast_node_t *)children[0];
    if (str_lit_node == NULL || str_lit_node->type != GDL_AST_NODE_TYPE_STRING_LITERAL)
    {
        epc_ast_builder_set_error(ctx, "Fail call expects a string literal.");
        gdl_ast_node_free(str_lit_node, user_data);
        return;
    }

    gdl_ast_node_t * fail_call_node = gdl_ast_node_alloc(ctx, GDL_AST_NODE_TYPE_FAIL_CALL);
    if (fail_call_node != NULL)
    {
        fail_call_node->data.string_literal.value = str_lit_node->data.string_literal.value;
        str_lit_node->data.string_literal.value = NULL; // Transfer ownership
        gdl_ast_node_free(str_lit_node, user_data); // Free the wrapper node
    }
    epc_ast_push(ctx, fail_call_node);
}


// --- Registry Initialization ---
void
gdl_ast_hook_registry_init(epc_ast_hook_registry_t * registry, void * user_data)
{
    (void)user_data; // Unused for now

    epc_ast_hook_registry_set_free_node(registry, gdl_ast_node_free);
#ifdef AST_DEBUG
   epc_ast_hook_registry_set_enter_node(registry, handle_node_entry);
#endif
    // Terminal actions
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_IDENTIFIER_REF, handle_create_identifier_ref);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_STRING_LITERAL, handle_create_string_literal);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_CHAR_LITERAL, handle_create_char_literal);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_NUMBER_LITERAL, handle_create_number_literal);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_RAW_CHAR_LITERAL, handle_create_raw_char_literal);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_REPETITION_OPERATOR, handle_create_repetition_operator);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_KEYWORD, handle_create_keyword); // New handler for keywords

    // Composite actions
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_PROGRAM, handle_create_program);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_RULE_DEFINITION, handle_create_rule_definition);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_CHAR_RANGE, handle_create_char_range);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_EXPRESSION_FACTOR, handle_create_expression_factor);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_SEQUENCE, handle_create_sequence);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_ALTERNATIVE, handle_create_alternative);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_OPTIONAL, handle_create_optional_expression);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_SEMANTIC_ACTION, handle_create_semantic_action);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_OPTIONAL_SEMANTIC_ACTION, handle_create_optional_semantic_action);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_TERMINAL, handle_create_terminal);

    // Combinator calls
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_ONEOF_CALL, handle_create_oneof_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_NONEOF_CALL, handle_create_noneof_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_COUNT_CALL, handle_create_count_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_BETWEEN_CALL, handle_create_between_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_DELIMITED_CALL, handle_create_delimited_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_LOOKAHEAD_CALL, handle_create_lookahead_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_NOT_CALL, handle_create_not_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_LEXEME_CALL, handle_create_lexeme_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_SKIP_CALL, handle_create_skip_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_CHAINL1_CALL, handle_create_chainl1_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_CHAINR1_CALL, handle_create_chainr1_call);
    epc_ast_hook_registry_set_action(registry, GDL_AST_ACTION_CREATE_FAIL_CALL, handle_create_fail_call);
}