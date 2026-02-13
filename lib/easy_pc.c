#include "easy_pc_private.h"
#include "parsers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- CPT Visitor ---
static void
pt_visit_recursive(epc_cpt_node_t * node, epc_cpt_visitor_t * visitor)
{
    if (!node || !visitor)
    {
        return;
    }

    if (visitor->enter_node)
    {
        visitor->enter_node(node, visitor->user_data);
    }

    for (int i = 0; i < node->children_count; ++i)
    {
        pt_visit_recursive(node->children[i], visitor);
    }

    if (visitor->exit_node)
    {
        visitor->exit_node(node, visitor->user_data);
    }
}

EASY_PC_API void
epc_cpt_visit_nodes(epc_cpt_node_t * root, epc_cpt_visitor_t * visitor)
{
    if (!root || !visitor)
    {
        return;
    }
    pt_visit_recursive(root, visitor);
}

// --- Top-Level API ---

// Internal parser_ctx_t creation (for parse results)
static epc_parser_ctx_t *
internal_create_parse_ctx(const char * input_start)
{
    epc_parser_ctx_t * ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
    {
        return NULL;
    }

    ctx->input_start = input_start;

    return ctx;
}

// Internal parser_ctx_t destruction (for parse results)
static void
internal_destroy_parse_ctx(epc_parser_ctx_t * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    epc_parser_error_free(ctx->furthest_error);
    free(ctx);
}

EASY_PC_API epc_parse_session_t
epc_parse_input(epc_parser_t * top_parser, const char * input_string)
{
    epc_parse_session_t session_result = { 0 };

    epc_parser_ctx_t * ctx = internal_create_parse_ctx(input_string);
    if (!ctx)
    {
        session_result.result = epc_unparsed_error_result(
            input_string,
            "Failed to create internal parse context.",
            "valid parse context",
            "NULL"
        );
        return session_result;
    }
    session_result.internal_parse_ctx = ctx;

    if (top_parser == NULL)
    {
        session_result.result = epc_unparsed_error_result(
            input_string, "Top parser not set for grammar", "grammar with a top parser", "NULL top_parser");
        return session_result;
    }

    if (input_string == NULL)
    {
        session_result.result = epc_unparsed_error_result(
            input_string, "Input string is NULL", "non-NULL input string", "NULL");
        return session_result;
    }

    session_result.result = top_parser->parse_fn(top_parser, ctx, input_string);

    // After parsing, if an error occurred, check if the tracked "furthest_error"
    // is more informative than the one that caused the final failure.
    if (session_result.result.is_error)
    {
        epc_parser_error_t *furthest_error = parser_furthest_error_copy(ctx);

        // A `furthest_error` is more informative if it parsed further into the input string.
        if (furthest_error != NULL
            && (session_result.result.data.error == NULL
                || furthest_error->input_position > session_result.result.data.error->input_position
               )
           )
        {
            // If it is, replace the result's error with the furthest one.
            epc_parser_result_cleanup(&session_result.result);
            session_result.result.is_error = true;
            session_result.result.data.error = furthest_error;
        }
        else
        {
            // Otherwise, the original error is fine, so just free the copy of furthest_error.
            epc_parser_error_free(furthest_error);
        }
    }

    return session_result;
}

EASY_PC_API void
    epc_parse_session_destroy(epc_parse_session_t * session)
{
    if (session == NULL)
    {
        return;
    }

    epc_parser_result_cleanup(&session->result);

    if (session->internal_parse_ctx)
    {
        internal_destroy_parse_ctx(session->internal_parse_ctx);
        session->internal_parse_ctx = NULL;
    }
}

ATTR_NONNULL(1, 2)
EASY_PC_HIDDEN
epc_cpt_node_t *
epc_node_alloc(epc_parser_t * parser, char const * tag)
{
    epc_cpt_node_t * node = calloc(1, sizeof(*node));
    if (node == NULL)
    {
        fprintf(stderr, "node alloc failed\n");
        return NULL;
    }
    node->content = ""; /* Make non-NULL. */
    node->tag = tag;
    node->name = parser->name;
    node->ast_config = parser->ast_config;

    return node;
}

EASY_PC_HIDDEN
void
epc_node_free(epc_cpt_node_t * node)
{
    if (node == NULL)
    {
        return;
    }
    if (node->children != NULL)
    {
        for (int i = 0; i < node->children_count; i++)
        {
            epc_node_free(node->children[i]);
        }
        free(node->children);
    }
    free(node);
}

EASY_PC_API epc_parser_list *
epc_parser_list_create(void)
{
    epc_parser_list * list = calloc(1, sizeof(*list));
    if (!list)
    {
        return NULL;
    }

    list->capacity = 20; // Initial capacity
    list->parsers = calloc(list->capacity, sizeof(*list->parsers));
    if (!list->parsers)
    {
        free(list);
        return NULL;
    }

    list->count = 0;
    return list;
}

EASY_PC_API epc_parser_t *
epc_parser_list_add(epc_parser_list * list, epc_parser_t * parser)
{
    if (!list || !parser)
    {
        return NULL; // Return NULL if list or parser is NULL
    }

    if (list->count == list->capacity)
    {
        size_t new_capacity = list->capacity * 2;
        epc_parser_t ** new_parsers = realloc(list->parsers, new_capacity * sizeof(*new_parsers));
        if (!new_parsers)
        {
            epc_parser_free(parser);
            return NULL; // Reallocation failed
        }
        list->parsers = new_parsers;
        list->capacity = new_capacity;
    }

    list->parsers[list->count++] = parser;
    return parser;
}

EASY_PC_API void
epc_parser_list_free(epc_parser_list * list)
{
    if (!list)
    {
        return;
    }

    for (size_t i = 0; i < list->count; ++i)
    {
        epc_parser_free(list->parsers[i]);
    }

    free(list->parsers);
    free(list);
}

EASY_PC_API const char *
epc_cpt_node_get_semantic_content(epc_cpt_node_t * node)
{
    if (node == NULL || node->content == NULL)
    {
        return NULL;
    }
    // Ensure start offset does not go beyond the actual content.
    // If it does, effectively, there is no semantic content.
    if (node->semantic_start_offset >= node->len)
    {
        return node->content + node->len; // Point to end of string or null
    }

    return node->content + node->semantic_start_offset;
}

EASY_PC_API size_t
epc_cpt_node_get_semantic_len(epc_cpt_node_t * node)
{
    if (node == NULL)
    {
        return 0;
    }
    // Calculate the total trimmed length.
    // Ensure start offset is not beyond actual length
    if (node->semantic_start_offset >= node->len)
    {
        return 0;
    }
    size_t effective_len = node->len - node->semantic_start_offset;

    // Ensure end offset is not beyond the remaining effective length
    if (node->semantic_end_offset >= effective_len)
    {
        return 0;
    }
    effective_len -= node->semantic_end_offset;

    return effective_len;
}

