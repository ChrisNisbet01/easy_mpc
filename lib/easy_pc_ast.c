#include "easy_pc_private.h"
#include <easy_pc/easy_pc_ast.h> // Public header for AST API

#include <stdlib.h>
#include <string.h>
#include <stdio.h> // For snprintf, vsnprintf, fprintf

// --- AST Hook Registry Implementation ---

EASY_PC_API epc_ast_hook_registry_t *
epc_ast_hook_registry_create(int action_count)
{
    if (action_count <= 0)
    {
        return NULL;
    }

    epc_ast_hook_registry_t * registry = calloc(1, sizeof(*registry));
    if (registry == NULL)
    {
        return NULL;
    }

    registry->callbacks = calloc(action_count, sizeof(*registry->callbacks));
    if (registry->callbacks == NULL)
    {
        free(registry);
        return NULL;
    }
    registry->action_count = action_count;

    return registry;
}

EASY_PC_API void
epc_ast_hook_registry_free(epc_ast_hook_registry_t * registry)
{
    if (!registry)
    {
        return;
    }
    free(registry->callbacks);
    free(registry);
}

EASY_PC_API void
epc_ast_hook_registry_set_action(
    epc_ast_hook_registry_t * registry,
    int action_index,
    epc_ast_action_cb cb
)
{
    if (!registry || action_index < 0 || action_index >= registry->action_count)
    {
        return;
    }
    registry->callbacks[action_index] = cb;
}

EASY_PC_API void
epc_ast_hook_registry_set_free_node(
    epc_ast_hook_registry_t * registry,
    epc_ast_node_free_cb cb
)
{
    if (!registry)
    {
        return;
    }
    registry->free_node = cb;
}

EASY_PC_API void
epc_ast_hook_registry_set_enter_node(
    epc_ast_hook_registry_t * registry,
    epc_ast_enter_cb cb
)
{
    if (!registry)
    {
        return;
    }
    registry->enter_node = cb;
}

// --- Internal AST Builder Context Management ---

#define EPC_AST_BUILDER_INITIAL_STACK_CAPACITY 64

static void
epc_ast_builder_ctx_init(epc_ast_builder_ctx_t * ctx, epc_ast_hook_registry_t * registry, void * user_data)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->registry = registry;
    ctx->user_data = user_data;
    ctx->capacity = EPC_AST_BUILDER_INITIAL_STACK_CAPACITY;
    ctx->stack = calloc(ctx->capacity, sizeof(*ctx->stack));
    if (!ctx->stack)
    {
        ctx->has_error = true;
        strncpy(ctx->error_message, "Failed to allocate initial AST stack.", sizeof(ctx->error_message) - 1);
        ctx->error_message[sizeof(ctx->error_message) - 1] = '\0';
    }
}

static void
epc_ast_builder_ctx_cleanup(epc_ast_builder_ctx_t * ctx)
{
    if (!ctx)
    {
        return;
    }

    // Free any remaining user nodes on the stack
    if (ctx->registry && ctx->registry->free_node)
    {
        for (int i = 0; i < ctx->top; ++i)
        {
            if (ctx->stack[i].type == EPC_AST_ITEM_USER_NODE && ctx->stack[i].ptr != NULL)
            {
                ctx->registry->free_node(ctx->stack[i].ptr, ctx->user_data);
            }
        }
    }
    free(ctx->stack);
    ctx->stack = NULL;
    ctx->top = 0;
    ctx->capacity = 0;
}

EASY_PC_API
void
epc_ast_builder_set_error(epc_ast_builder_ctx_t * ctx, const char * format, ...)
{
    if (ctx->has_error)
    {
        return; // Don't overwrite first error
    }
    ctx->has_error = true;
    va_list args;
    va_start(args, format);
    vsnprintf(ctx->error_message, sizeof(ctx->error_message), format, args);
    va_end(args);
    ctx->error_message[sizeof(ctx->error_message) - 1] = '\0';
}

static void
epc_ast_builder_grow_stack(epc_ast_builder_ctx_t * ctx)
{
    if (ctx->has_error)
    {
        return;
    }
    int new_capacity = ctx->capacity * 2;
    epc_ast_stack_entry_t * new_stack = realloc(ctx->stack, new_capacity * sizeof(*new_stack));
    if (!new_stack)
    {
        epc_ast_builder_set_error(ctx, "Failed to grow AST stack (realloc failed).");
        return;
    }
    ctx->stack = new_stack;
    ctx->capacity = new_capacity;
}

EASY_PC_API void
epc_ast_push(epc_ast_builder_ctx_t * ctx, void * node)
{
    if (!ctx || ctx->has_error)
    {
        // If there's an error, new nodes are leaks unless freed immediately.
        // If the user's free_node is available, use it.
        if (ctx && ctx->registry && ctx->registry->free_node && node)
        {
            ctx->registry->free_node(node, ctx->user_data);
        }
        return;
    }

    if (ctx->top == ctx->capacity)
    {
        epc_ast_builder_grow_stack(ctx);
        if (ctx->has_error)
        {
            if (ctx->registry && ctx->registry->free_node && node)
            {
                ctx->registry->free_node(node, ctx->user_data);
            }
            return;
        }
    }

    ctx->stack[ctx->top].type = EPC_AST_ITEM_USER_NODE;
    ctx->stack[ctx->top].ptr = node;
    ctx->top++;
}

static void
epc_ast_builder_push_placeholder(epc_ast_builder_ctx_t * ctx)
{
    if (ctx->has_error)
    {
        return;
    }
    if (ctx->top == ctx->capacity)
    {
        epc_ast_builder_grow_stack(ctx);
        if (ctx->has_error)
        {
            return;
        }
    }
    ctx->stack[ctx->top].type = EPC_AST_ITEM_PLACEHOLDER;
    ctx->stack[ctx->top].ptr = NULL; // Placeholders don't hold user nodes
    ctx->top++;
}

// Pops items from the stack until a placeholder is found,
// collecting USER_NODE items into a temporary array.
// Returns a dynamically allocated array of child AST nodes.
static void **
epc_ast_builder_pop_until_placeholder(epc_ast_builder_ctx_t * ctx, int * out_count)
{
    if (!ctx || ctx->has_error || ctx->top == 0)
    {
        *out_count = 0;
        return NULL;
    }

    // First, count how many user nodes are above the last placeholder
    int child_count = 0;
    int placeholder_idx = ctx->top - 1;
    while (placeholder_idx >= 0 && ctx->stack[placeholder_idx].type != EPC_AST_ITEM_PLACEHOLDER)
    {
        if (ctx->stack[placeholder_idx].type == EPC_AST_ITEM_USER_NODE)
        {
            child_count++;
        }
        placeholder_idx--;
    }

    if (placeholder_idx < 0)
    {
        epc_ast_builder_set_error(ctx, "AST stack underflow: placeholder not found.");
        *out_count = 0;
        return NULL;
    }

    // Allocate memory for children array
    void ** children = NULL;
    if (child_count > 0)
    {
        children = calloc(child_count, sizeof(*children));
        if (!children)
        {
            epc_ast_builder_set_error(ctx, "Failed to allocate children array.");
            *out_count = 0;
            return NULL;
        }
    }

    // Pop from stack and fill children array in correct order (reverse from stack)
    int current_child_idx = child_count - 1;
    while (ctx->top > placeholder_idx)
    {
        ctx->top--; // Decrement top first
        if (ctx->stack[ctx->top].type == EPC_AST_ITEM_USER_NODE)
        {
            if (current_child_idx >= 0)
            {
                children[current_child_idx] = ctx->stack[ctx->top].ptr;
                current_child_idx--;
            }
        }
    }

    *out_count = child_count;
    return children;
}

// --- CPT Visitor for AST Building ---
static void
epc_ast_builder_enter_node_cb(epc_cpt_node_t * node, void * user_data)
{
    epc_ast_builder_ctx_t * ctx = (epc_ast_builder_ctx_t *)user_data;

    if (ctx->has_error)
    {
        return;
    }

    epc_ast_builder_push_placeholder(ctx);
    if (ctx->has_error)
    {
        return;
    }

    if (ctx->registry->enter_node != NULL)
    {
        ctx->registry->enter_node(ctx, node, ctx->user_data);
    }
}

static void
epc_ast_builder_exit_node_cb(epc_cpt_node_t * node, void * user_data)
{
    epc_ast_builder_ctx_t * ctx = (epc_ast_builder_ctx_t *)user_data;

    if (ctx->has_error)
    {
        return;
    }

    int children_count = 0;
    void ** children = epc_ast_builder_pop_until_placeholder(ctx, &children_count);

    if (ctx->has_error)
    {
        free(children); // Free array, nodes handled by cleanup
        return;
    }

    bool has_action_assigned =
        node->ast_config.assigned
        && node->ast_config.action >= 0
        && node->ast_config.action < ctx->registry->action_count;

    if (has_action_assigned)
    {
        epc_ast_action_cb action_cb = ctx->registry->callbacks[node->ast_config.action];

        if (action_cb != NULL)
        {
            action_cb(ctx, node, children, children_count, ctx->user_data);
        }
    }
    else // Default behavior: if no action, push children back (flatten)
    {
        for (int i = 0; i < children_count; ++i)
        {
            epc_ast_push(ctx, children[i]);
            if (ctx->has_error)
            {
                break; // Stop pushing if an error occurred (e.g. realloc failed)
            }
        }
    }

    free(children);
}

// --- Public AST Building API ---

EASY_PC_API epc_ast_result_t
epc_ast_build(
    epc_cpt_node_t * root,
    epc_ast_hook_registry_t * registry,
    void * user_data
)
{
    epc_ast_result_t result = { 0 };
    if (!root || !registry || !registry->callbacks || registry->action_count <= 0)
    {
        result.has_error = true;
        strncpy(result.error_message, "Invalid arguments to epc_ast_build.", sizeof(result.error_message) - 1);
        result.error_message[sizeof(result.error_message) - 1] = '\0';
        return result;
    }

    epc_ast_builder_ctx_t ctx;
    epc_ast_builder_ctx_init(&ctx, registry, user_data);
    if (ctx.has_error)
    {
        result.has_error = true;
        memcpy(result.error_message, ctx.error_message, sizeof(result.error_message));
        result.error_message[sizeof(result.error_message) - 1] = '\0';
        return result;
    }

    epc_cpt_visitor_t ast_builder_visitor = {
        .enter_node = epc_ast_builder_enter_node_cb,
        .exit_node = epc_ast_builder_exit_node_cb,
        .user_data = &ctx
    };

    epc_cpt_visit_nodes(root, &ast_builder_visitor);

    if (ctx.has_error)
    {
        result.has_error = true;
        memcpy(result.error_message, ctx.error_message, sizeof(result.error_message));
        result.error_message[sizeof(result.error_message) - 1] = '\0';
        epc_ast_builder_ctx_cleanup(&ctx); // Cleanup allocated nodes on stack
        return result;
    }

    if (ctx.top == 1)
    {
        // The single remaining item on the stack is the root of the AST
        result.ast_root = ctx.stack[0].ptr;
        ctx.stack[0].ptr = NULL; // Ownership transferred
    }
    else if (ctx.top > 1)
    {
        epc_ast_builder_set_error(&ctx, "AST stack not empty after build. Multiple roots or unhandled nodes remain.");
        result.has_error = true;
        memcpy(result.error_message, ctx.error_message, sizeof(result.error_message));
        result.error_message[sizeof(result.error_message) - 1] = '\0';
        epc_ast_builder_ctx_cleanup(&ctx);
        return result;
    }
    // If ctx.top == 0, it means the AST was completely pruned or empty, ast_root remains NULL.

    epc_ast_builder_ctx_cleanup(&ctx); // Frees stack memory, but not the root node if transferred
    return result;
}

EASY_PC_API epc_compile_result_t
epc_parse_and_build_ast(
    epc_parser_t * parser,
    char const * input,
    int ast_action_count,
    epc_ast_registry_init_cb registry_init_cb,
    void * user_data
)
{
    epc_compile_result_t result = {0};
    epc_parse_session_t parse_session = epc_parse_input(parser, input);

    if (parse_session.result.is_error)
    {
        result.success = false;
        char *msg = NULL;
        // The error structure from the parser has all the necessary details.
        epc_parser_error_t *err = parse_session.result.data.error;
        int len = asprintf(
            &msg,
            "Parse error: %s at '%.*s' (expected '%s', found '%.*s')Error err: line: %zu, col: %zu",
            err->message,
            (int)(input + strlen(input) - err->input_position), err->input_position,
            err->expected ? err->expected : "N/A",
            (int)strlen(err->found), err->found ? err->found : "N/A",
            err->line, err->col
        );
        if (len < 0)
        {
            result.parse_error_message = strdup("Failed to allocate memory for parse error message.");
        }
        else
        {
            result.parse_error_message = msg;
        }
    }
    else
    {
        epc_ast_hook_registry_t * ast_registry = epc_ast_hook_registry_create(ast_action_count);
        if (ast_registry == NULL)
        {
            result.success = false;
            result.ast_error_message = strdup("Failed to create AST hook registry.");
        }
        else
        {
            if (registry_init_cb != NULL)
            {
                registry_init_cb(ast_registry);
            }

            epc_ast_result_t ast_build_result =
                epc_ast_build(parse_session.result.data.success, ast_registry, user_data);

            if (ast_build_result.has_error)
            {
                result.success = false;
                result.ast_error_message = strdup(ast_build_result.error_message);
            }
            else
            {
                result.success = true;
                result.ast = ast_build_result.ast_root;
            }
            epc_ast_hook_registry_free(ast_registry);
        }
    }

    epc_parse_session_destroy(&parse_session);
    return result;
}

EASY_PC_API void
epc_compile_result_cleanup(
    epc_compile_result_t * result,
    epc_ast_node_free_cb ast_free_cb,
    void * user_data
)
{
    if (result == NULL)
    {
        return;
    }

    free(result->parse_error_message);
    free(result->ast_error_message);

    if (result->success && result->ast != NULL && ast_free_cb != NULL)
    {
        ast_free_cb(result->ast, user_data);
    }

    memset(result, 0, sizeof(*result));
}
