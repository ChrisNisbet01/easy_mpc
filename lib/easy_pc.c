#include "easy_pc_private.h"
#include "parsers.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_INPUT_SIZE (100 * 1024 * 1024) /* 100 MB */

// The Parsing Context (for a single parse operation and its results)
// This will be internally managed by epc_parse_input
struct epc_parser_ctx_t
{
    char const * input_start;
    size_t input_len;
    size_t mmap_total_len;
    epc_parser_error_t * furthest_error;
};

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
internal_create_parse_ctx_from_input(char const * input_start)
{
    epc_parser_ctx_t * ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
    {
        return NULL;
    }

    long const page_size = sysconf(_SC_PAGESIZE);
    ctx->mmap_total_len = MAX_INPUT_SIZE + page_size;

    /*
     * Allocate 100MB + 1 guard page.
     * MAP_ANONYMOUS ensures no physical memory is used until written to.
     */
    void * mem = mmap(NULL, ctx->mmap_total_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED)
    {
        free(ctx);
        return NULL;
    }

    /* Set the guard page at the very end of the 100MB range. */
    if (mprotect((char *)mem + MAX_INPUT_SIZE, page_size, PROT_NONE) != 0)
    {
        munmap(mem, ctx->mmap_total_len);
        free(ctx);
        return NULL;
    }

    ctx->input_start = mem;

    if (input_start != NULL)
    {
        ctx->input_len = strlen(input_start);
        if (ctx->input_len + 1 > MAX_INPUT_SIZE)
        {
            /* Input exceeds our virtual buffer. */
            munmap(mem, ctx->mmap_total_len);
            free(ctx);
            return NULL;
        }
        memcpy(mem, input_start, ctx->input_len + 1); /* +1 to include null terminator */
    }

    return ctx;
}

EASY_PC_HIDDEN
parse_get_input_result_t
parse_ctx_get_input_at_offset(epc_parser_ctx_t * const ctx, size_t const input_offset, size_t const count)
{
    if (ctx == NULL || ctx->input_start == NULL || input_offset + count > ctx->input_len)
    {
        return (parse_get_input_result_t){
            .is_eof = true,
        };
    }

    return (parse_get_input_result_t){
        .next_input = &ctx->input_start[input_offset],
        .available = ctx->input_len - input_offset,
    };
}

EASY_PC_HIDDEN
size_t
parse_ctx_get_input_len(epc_parser_ctx_t * const ctx)
{
    if (ctx == NULL || ctx->input_start == NULL)
    {
        return 0;
    }
    return ctx->input_len;
}

EASY_PC_HIDDEN
ATTR_NONNULL(1)
size_t
parse_ctx_get_offset_from_input(epc_parser_ctx_t * const ctx, char const * const input_position)
{
    if (ctx->input_start == NULL || input_position < ctx->input_start
        || input_position > ctx->input_start + ctx->input_len)
    {
        return 0;
    }
    return (size_t)(input_position - ctx->input_start);
}

EASY_PC_HIDDEN
ATTR_NONNULL(1)
epc_parser_error_t *
parse_ctx_get_furthest_error(epc_parser_ctx_t const * ctx)
{
    return ctx->furthest_error;
}

EASY_PC_HIDDEN
ATTR_NONNULL(1)
void
parser_ctx_set_furthest_error(epc_parser_ctx_t * ctx, epc_parser_error_t ** replacement)
{
    epc_parser_error_free(ctx->furthest_error);
    ctx->furthest_error = *replacement;
    *replacement = NULL;
}

// Internal parser_ctx_t destruction (for parse results)
static void
internal_destroy_parse_ctx(epc_parser_ctx_t * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->input_start != NULL && ctx->mmap_total_len > 0)
    {
        munmap((void *)ctx->input_start, ctx->mmap_total_len);
    }

    epc_parser_error_free(ctx->furthest_error);
    free(ctx);
}

EASY_PC_API epc_parse_session_t
epc_parse_input(epc_parser_t * top_parser, char const * input_string)
{
    epc_parse_session_t session = {0};

    epc_parser_ctx_t * ctx = internal_create_parse_ctx_from_input(input_string);
    if (!ctx)
    {
        session.result
            = epc_unparsed_error_result(0, "Failed to create internal parse context.", "valid parse context", "NULL");
        return session;
    }
    session.internal_parse_ctx = ctx;

    if (top_parser == NULL)
    {
        session.result = epc_unparsed_error_result(
            0, "Top parser not set for grammar", "grammar with a top parser", "NULL top_parser"
        );
        return session;
    }

    if (input_string == NULL)
    {
        session.result = epc_unparsed_error_result(0, "Input string is NULL", "non-NULL input string", "NULL");
        return session;
    }

    session.result = top_parser->parse_fn(top_parser, ctx, 0);

    // After parsing, if an error occurred, check if the tracked "furthest_error"
    // is more informative than the one that caused the final failure.
    if (session.result.is_error)
    {
        epc_parser_error_t * furthest_error = parser_furthest_error_copy(ctx);

        // A `furthest_error` is more informative if it parsed further into the input string.
        if (furthest_error != NULL
            && (session.result.data.error == NULL
                || furthest_error->input_position > session.result.data.error->input_position))
        {
            // If it is, replace the result's error with the furthest one.
            epc_parser_result_cleanup(&session.result);
            session.result.is_error = true;
            session.result.data.error = furthest_error;
        }
        else
        {
            // Otherwise, the original error is fine, so just free the copy of furthest_error.
            epc_parser_error_free(furthest_error);
        }
    }

    return session;
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

EASY_PC_API
void
epc_parse_session_print_cpt(FILE * fp, epc_parse_session_t const * session)
{
    if (session == NULL)
    {
        fprintf(fp, "NULL session\n");
        return;
    }
    if (session->result.is_error)
    {
        epc_parser_error_t * err = session->result.data.error;
        fprintf(fp, "Parse Error: %s\n", err->message);
        fprintf(fp, "At line %zu, col %zu\n", err->position.line + 1, err->position.col + 1);
        fprintf(fp, "Expected: %s\n", err->expected ? err->expected : "unknown");
        fprintf(fp, "Found: %s\n", err->found ? err->found : "unknown");
    }
    else
    {
        fprintf(fp, "Parsing successful!\n");
        char * cpt_str = epc_cpt_to_string(session->internal_parse_ctx, session->result.data.success);
        if (cpt_str)
        {
            fprintf(fp, "Concrete Parse Tree (CPT):\n%s\n", cpt_str);
            free(cpt_str);
        }
    }
}

ATTR_NONNULL(1, 2)
EASY_PC_API
epc_cpt_node_t *
epc_node_alloc(epc_parser_t * parser, char const * tag)
{
    epc_cpt_node_t * node = calloc(1, sizeof(*node));
    if (node == NULL)
    {
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

EASY_PC_HIDDEN
char const *
epc_node_id(epc_cpt_node_t const * node)
{
    if (node == NULL)
    {
        return "NULL";
    }
    if (node->name)
    {
        return node->name;
    }

    return node->tag;
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

EASY_PC_API const char *
epc_cpt_node_get_content(epc_cpt_node_t * node)
{
    if (node == NULL || node->content == NULL)
    {
        return NULL;
    }

    return node->content;
}

EASY_PC_API size_t
epc_cpt_node_get_len(epc_cpt_node_t * node)
{
    if (node == NULL)
    {
        return 0;
    }

    return node->len;
}
