#include "simple_calc_ast_actions.h"

#include "ast.h"
#include "function_definitions.h"
#include "easy_pc/easy_pc_ast.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ast_node_t *
ast_node_alloc(ast_node_type_t node_type)
{
    ast_node_t * node = calloc(1, sizeof(*node));
    if (node != NULL)
    {
        node->type = node_type;
    }
    return node;
}

static void
ast_list_init(ast_list_t * list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

static void
ast_list_append(ast_list_t * list, ast_node_t * item)
{
    ast_list_node_t * new_node = (ast_list_node_t *)calloc(1, sizeof(*new_node));
    if (new_node == NULL)
    {
        /* TODO: handle this error more gracefully, perhaps by setting an error in ctx */
        return;
    }
    new_node->item = item;
    new_node->next = NULL;

    if (list->tail)
    {
        list->tail->next = new_node;
    }
    else
    {
        list->head = new_node;
    }
    list->tail = new_node;
    list->count++;
}

static void
free_calc_children_on_error(void * * children, int count, void * user_data)
{
    for (int i = 0; i < count; i++)
    {
        if (children[i] != NULL)
        {
            ast_node_free((ast_node_t *)children[i], user_data);
        }
    }
}

void
ast_node_free(void * node_ptr, void * user_data)
{
    ast_node_t * node = (ast_node_t *)node_ptr;

    if (node == NULL)
    {
        return;
    }

    switch(node->type)
    {
        case AST_NODE_TYPE_NUMBER:
        case AST_NODE_TYPE_OPERATOR:
        case AST_NODE_TYPE_PLACEHOLDER:
            /* Nothing to do. */
            break;

        case AST_NODE_TYPE_EXPRESSION:
            ast_node_free(node->data.expression.left, user_data);
            ast_node_free(node->data.expression.operator_node, user_data);
            ast_node_free(node->data.expression.right, user_data);
            break;

        case AST_NODE_TYPE_LIST:
        {
            if (node->data.list.count > 0)
            {
                ast_list_node_t * item = node->data.list.head;
                while (item != NULL)
                {
                    ast_list_node_t * next_item = item->next;
                    ast_node_free(item->item, user_data);
                    free(item);
                    item = next_item;
                }
            }
            break;
        }

        case AST_NODE_TYPE_FUNCTION_CALL:
        {
            ast_list_t * list = &node->data.function_call.arguments;
            ast_list_node_t * item = list->head;

            while (item != NULL)
            {
                ast_list_node_t * next_item = item->next;
                ast_node_free(item->item, user_data);
                free(item);
                item = next_item;
            }
            break;
        }

        case AST_NODE_TYPE_IDENTIFIER:
            free((char *)node->data.identifier.name);
            break;
    }

    free(node);
}

// --- Semantic Action Callbacks ---

static void
create_number_from_content_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)children;
    (void)count;

    ast_node_t * num_node = ast_node_alloc(AST_NODE_TYPE_NUMBER);
    if (num_node == NULL)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate AST number node");
        return;
    }

    size_t content_len = epc_cpt_node_get_semantic_len(node);
    char * num_str_buf = malloc(content_len + 1);
    if (num_str_buf== NULL)
    {
        ast_node_free(num_node, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate AST number node buffer");
        return;
    }
    strncpy(num_str_buf, epc_cpt_node_get_semantic_content(node), content_len);
    num_str_buf[content_len] = '\0';
    num_node->data.number.value = strtod(num_str_buf, NULL);
    free(num_str_buf);
    epc_ast_push(ctx, num_node);
}

static void
create_operator_from_char_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)children; (void)count; (void)user_data;
    ast_node_t * op_node = ast_node_alloc(AST_NODE_TYPE_OPERATOR);
    if (op_node == NULL)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate AST operator node");
        return;
    }
    op_node->data.op.operator_char = epc_cpt_node_get_semantic_content(node)[0];
    epc_ast_push(ctx, op_node);
}

static void
create_identifier_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)children; (void)count; (void)user_data;
    ast_node_t * ident_node = ast_node_alloc(AST_NODE_TYPE_IDENTIFIER);
    if (ident_node == NULL)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate AST identifier node");
        return;
    }
    ident_node->data.identifier.name =
        strndup(
            epc_cpt_node_get_semantic_content(node),
            epc_cpt_node_get_semantic_len(node)
        );
    epc_ast_push(ctx, ident_node);
}

static void
collect_child_results_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node; (void)user_data;
    ast_node_t * collected_list_node = ast_node_alloc(AST_NODE_TYPE_LIST);
    if (collected_list_node == NULL)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate AST list node");
        return;
    }
    ast_list_init(&collected_list_node->data.list);

    // Children are collected in reverse order of how they appear in the grammar
    // so we iterate backwards to get them in the correct order for the list.
    for (int i = count - 1; i >= 0; --i)
    {
        ast_list_append(&collected_list_node->data.list, (ast_node_t *)children[i]);
    }
    epc_ast_push(ctx, collected_list_node);
}

static void
build_binary_expression_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node; (void)user_data;
    if (count != 3)
    {
        free_calc_children_on_error(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Binary expression expects 3 children (left, op, right), got %d", count);
        return;
    }

    ast_node_t * left_operand_node = (ast_node_t *)children[0];
    ast_node_t * operator_node = (ast_node_t *)children[1];
    ast_node_t * right_operand_node = (ast_node_t *)children[2];

    if (operator_node == NULL || operator_node->type != AST_NODE_TYPE_OPERATOR)
    {
        free_calc_children_on_error(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Expected operator node for binary expression");
        return;
    }

    ast_node_t * expression_node = ast_node_alloc(AST_NODE_TYPE_EXPRESSION);
    if (expression_node == NULL)
    {
        free_calc_children_on_error(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate AST expression node");
        return;
    }

    expression_node->data.expression.left = left_operand_node;
    expression_node->data.expression.operator_node = operator_node;
    expression_node->data.expression.right = right_operand_node;

    epc_ast_push(ctx, expression_node);
}

static void
create_function_call_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node;

    if (count == 0 || count > 2)
    {
        free_calc_children_on_error(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Function call expects 1 or 2 children (identifier [, args_list]), got %d", count);
        return;
    }

    ast_node_t * func_name_node = (ast_node_t *)children[0];
    ast_node_t * args_list_node = NULL;

    if (func_name_node == NULL || func_name_node->type != AST_NODE_TYPE_IDENTIFIER)
    {
        epc_ast_builder_set_error(ctx, "Expected function name identifier on stack for function call");
        free_calc_children_on_error(children, count, user_data);
        return;
    }

    if (count == 2)
    {
        args_list_node = (ast_node_t *)children[1];
        if (args_list_node->type != AST_NODE_TYPE_LIST)
        {
            epc_ast_builder_set_error(ctx, "Expected arguments list on stack for function call");
            free_calc_children_on_error(children, count, user_data);
            return;
        }
    }

    const char * func_name_str = func_name_node->data.identifier.name;
    const function_t * func_def = function_lookup_by_name(func_name_str);

    if (!func_def)
    {
        epc_ast_builder_set_error(ctx, "Unknown function '%s'", func_name_str);
        free_calc_children_on_error(children, count, user_data);
        return;
    }
    size_t args_count = args_list_node != NULL ? (size_t)args_list_node->data.list.count : 0;
    if (func_def->num_args != args_count)
    {
        epc_ast_builder_set_error(ctx, "Function '%s' expects %zu args, got %d", func_def->name, func_def->num_args, args_count);
        free_calc_children_on_error(children, count, user_data);
        return;
    }

    ast_node_t * func_call_node = ast_node_alloc(AST_NODE_TYPE_FUNCTION_CALL);
    if (func_call_node == NULL)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate AST function call node");
        free_calc_children_on_error(children, count, user_data);
        return;
    }

    func_call_node->data.function_call.func_def = func_def;
    if (args_list_node != NULL)
    {
        func_call_node->data.function_call.arguments = args_list_node->data.list;

        // The function call node now owns the list's contents, so clear the list node's pointers
        // so it doesn't try to free them again when it is freed.
        args_list_node->data.list.head = NULL;
        args_list_node->data.list.tail = NULL;
        args_list_node->data.list.count = 0;
    }

    for (size_t i = 0; i < (size_t)count; i++)
    {
        ast_node_free(children[i], user_data);
    }

    epc_ast_push(ctx, func_call_node);
}

static void
assign_root_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node;

    if (count != 1)
    {
        free_calc_children_on_error(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Assign root action expects 1 child, got %d", count);
        return;
    }
    epc_ast_push(ctx, children[0]);
}

void
simple_calc_ast_hook_registry_init(epc_ast_hook_registry_t * registry)
{
    epc_ast_hook_registry_set_free_node(registry, ast_node_free);

    epc_ast_hook_registry_set_action(registry, AST_ACTION_CREATE_NUMBER_FROM_CONTENT, create_number_from_content_action);
    epc_ast_hook_registry_set_action(registry, AST_ACTION_CREATE_OPERATOR_FROM_CHAR, create_operator_from_char_action);
    epc_ast_hook_registry_set_action(registry, AST_ACTION_CREATE_IDENTIFIER, create_identifier_action);
    epc_ast_hook_registry_set_action(registry, AST_ACTION_COLLECT_CHILD_RESULTS, collect_child_results_action);
    epc_ast_hook_registry_set_action(registry, AST_ACTION_BUILD_BINARY_EXPRESSION, build_binary_expression_action);
    epc_ast_hook_registry_set_action(registry, AST_ACTION_CREATE_FUNCTION_CALL, create_function_call_action);
    epc_ast_hook_registry_set_action(registry, AST_ACTION_ASSIGN_ROOT, assign_root_action);

    // Optionally set enter_node callback if needed for global context management
    // epc_ast_hook_registry_set_enter_node(registry, my_enter_node_cb);
}
