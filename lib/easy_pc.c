#include "easy_pc_private.h"
#include "parsers.h"

#ifdef WITH_INPUT_STREAM_SUPPORT
#include <ctype.h>
#include <pthread.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_MMAP_INPUT_SIZE (100 * 1024 * 1024) /* 100 MB */

typedef struct mmap_input_buffer_t
{
    char * buffer;     /**< Pointer to the start of the memory-mapped input buffer. */
    size_t total_size; /**< Total size of the memory-mapped region (including guard page). */
    size_t input_size; /**< Actual size of the input string stored in the buffer. */
} mmap_input_buffer_t;

// The Parsing Context (for a single parse operation and its results)
// This will be internally managed by epc_parse_input
struct epc_parser_ctx_t
{
    char const * input_start;
    size_t input_len;

    mmap_input_buffer_t mmap_buffer; /* Internal buffer management for input string, using mmap for large inputs. */
    epc_parser_error_t * furthest_error;

#ifdef WITH_INPUT_STREAM_SUPPORT
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_streaming;
    bool is_eof;
    int input_error;
#endif
};

#ifdef WITH_INPUT_STREAM_SUPPORT
typedef struct
{
    epc_parser_t * top_parser;
    epc_parser_ctx_t * ctx;
    epc_parse_result_t result;
} ParsingThreadArgs;

static void *
epc_parsing_thread_worker(void * arg)
{
    ParsingThreadArgs * args = (ParsingThreadArgs *) arg;

    args->result = args->top_parser->parse_fn(args->top_parser, args->ctx, 0);

    return NULL;
}
#endif

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

mmap_input_buffer_t
create_mmap_input_buffer(size_t input_size)
{
    mmap_input_buffer_t buffer = {0};

    if (input_size > MAX_MMAP_INPUT_SIZE)
    {
        return buffer; // Return empty buffer if input size exceeds our limit
    }

    long const page_size = sysconf(_SC_PAGESIZE);
    buffer.total_size = MAX_MMAP_INPUT_SIZE + page_size;

    /*
     * Allocate 100MB + 1 guard page.
     * MAP_ANONYMOUS ensures no physical memory is used until written to.
     */
    void * mem = mmap(NULL, buffer.total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED)
    {
        return (mmap_input_buffer_t){0}; // Return empty buffer on failure
    }

    /* Set the guard page at the very end of the 100MB range. */
    if (mprotect((char *)mem + MAX_MMAP_INPUT_SIZE, page_size, PROT_NONE) != 0)
    {
        munmap(mem, buffer.total_size);
        return (mmap_input_buffer_t){0}; // Return empty buffer on failure
    }

    buffer.buffer = mem;
    buffer.input_size = input_size;

    return buffer;
}

// Internal parser_ctx_t creation (for parse results)
static epc_parser_ctx_t *
internal_create_parse_ctx_from_string(char const * input_start)
{
    size_t input_len = input_start == NULL ? 0 : strlen(input_start);

    mmap_input_buffer_t buffer = create_mmap_input_buffer(input_len + 1);

    if (buffer.buffer == NULL)
    {
        return NULL;
    }

    epc_parser_ctx_t * ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        munmap(buffer.buffer, buffer.total_size);
        return NULL;
    }

#ifdef WITH_INPUT_STREAM_SUPPORT
    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_cond_init(&ctx->cond, NULL);
#endif

    if (input_start != NULL)
    {
        memcpy(buffer.buffer, input_start, input_len + 1); /* +1 to include null terminator */
    }
    else
    {
        buffer.buffer[0] = '\0'; /* Ensure null termination for NULL input */
    }

    ctx->mmap_buffer = buffer;
    ctx->input_start = ctx->mmap_buffer.buffer;
    ctx->input_len = input_len;

    return ctx;
}

static epc_parser_ctx_t *
internal_create_parse_ctx_from_fp(FILE * fp)
{
    if (fp == NULL)
    {
        return NULL;
    }

    // Move to end of file to determine size
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        return NULL;
    }
    long file_size = ftell(fp);
    if (file_size < 0)
    {
        return NULL;
    }
    rewind(fp);

    mmap_input_buffer_t buffer = create_mmap_input_buffer((size_t)file_size + 1);
    if (buffer.buffer == NULL)
    {
        return NULL;
    }

    size_t total_read = fread(buffer.buffer, 1, (size_t)file_size, fp);
    if (total_read != (size_t)file_size)
    {
        munmap(buffer.buffer, buffer.total_size);
        return NULL;
    }
    buffer.buffer[total_read] = '\0'; // Null-terminate the buffer

    epc_parser_ctx_t * ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
    {
        munmap(buffer.buffer, buffer.total_size);
        return NULL;
    }

#ifdef WITH_INPUT_STREAM_SUPPORT
    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_cond_init(&ctx->cond, NULL);
#endif

    ctx->mmap_buffer = buffer;
    ctx->input_start = buffer.buffer;
    ctx->input_len = total_read;

    return ctx;
}

#ifdef WITH_INPUT_STREAM_SUPPORT
static epc_parser_ctx_t *
internal_create_parse_ctx_streaming(void)
{
    mmap_input_buffer_t buffer = create_mmap_input_buffer(0);
    if (buffer.buffer == NULL)
    {
        return NULL;
    }

    epc_parser_ctx_t * ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        munmap(buffer.buffer, buffer.total_size);
        return NULL;
    }

    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    ctx->mmap_buffer = buffer;
    ctx->input_start = buffer.buffer;
    ctx->input_len = 0;
    ctx->is_streaming = true;

    return ctx;
}
#endif

// Internal parser_ctx_t destruction (for parse results)
static void
internal_destroy_parse_ctx(epc_parser_ctx_t * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    epc_parser_error_free(ctx->furthest_error);

#ifdef WITH_INPUT_STREAM_SUPPORT
    pthread_mutex_destroy(&ctx->mutex);
    pthread_cond_destroy(&ctx->cond);
#endif

    if (ctx->mmap_buffer.buffer != NULL)
    {
        munmap((void *)ctx->mmap_buffer.buffer, ctx->mmap_buffer.total_size);
    }

    free(ctx);
}

EASY_PC_HIDDEN
parse_get_input_result_t
parse_ctx_get_input_at_offset(epc_parser_ctx_t * const ctx, size_t const input_offset, size_t const count)
{
    if (ctx == NULL || ctx->input_start == NULL)
    {
        return (parse_get_input_result_t){
            .is_eof = true,
        };
    }

#ifdef WITH_INPUT_STREAM_SUPPORT
    if (ctx->is_streaming)
    {
        pthread_mutex_lock(&ctx->mutex);
        while (input_offset + count > ctx->input_len && !ctx->is_eof && ctx->input_error == 0)
        {
            pthread_cond_wait(&ctx->cond, &ctx->mutex);
        }

        if (input_offset + count > ctx->input_len)
        {
            // We've reached EOF or an error occurred before enough data was available
            pthread_mutex_unlock(&ctx->mutex);
            return (parse_get_input_result_t){
                .next_input = &ctx->input_start[input_offset],
                .available = ctx->input_len - input_offset,
                .is_eof = true,
                /* We might want to pass back the error code somehow in the future */
            };
        }

        parse_get_input_result_t result = {
            .next_input = &ctx->input_start[input_offset],
            .available = ctx->input_len - input_offset,
            .is_eof = false,
        };
        pthread_mutex_unlock(&ctx->mutex);
        return result;
    }
#endif

    if (input_offset + count > ctx->input_len)
    {
        return (parse_get_input_result_t){
            .next_input = &ctx->input_start[input_offset],
            .available = ctx->input_len - input_offset,
            .is_eof = true,
        };
    }

    return (parse_get_input_result_t){
        .next_input = &ctx->input_start[input_offset],
        .available = ctx->input_len - input_offset,
    };
}

EASY_PC_HIDDEN
bool
parse_ctx_is_streaming(epc_parser_ctx_t const * ctx)
{
#ifdef WITH_INPUT_STREAM_SUPPORT
    return ctx ? ctx->is_streaming : false;
#else
    (void)ctx;
    return false;
#endif
}

EASY_PC_HIDDEN
bool
parse_ctx_is_eof(epc_parser_ctx_t * ctx)
{
    if (!ctx)
    {
        return true;
    }
#ifdef WITH_INPUT_STREAM_SUPPORT
    if (ctx->is_streaming)
    {
        pthread_mutex_lock(&ctx->mutex);
        bool const is_eof = ctx->is_eof;
        pthread_mutex_unlock(&ctx->mutex);
        return is_eof;
    }
#endif
    return true; // For non-streaming, data is always loaded up to "EOF"
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

#ifdef WITH_INPUT_STREAM_SUPPORT
static epc_parse_result_t
parse_in_thread(epc_parser_t * top_parser, epc_parser_ctx_t * ctx, epc_parse_input_t input)
{
        ParsingThreadArgs args = {
            .top_parser = top_parser,
            .ctx = ctx,
            .result = {0},
        };

        pthread_t thread;
        if (pthread_create(&thread, NULL, epc_parsing_thread_worker, &args) != 0)
        {
            return epc_unparsed_error_result(0, "Failed to create parsing thread", "parsing thread created", "pthread_create failed");
        }

        // Producer Loop (Main Thread)
        char read_buf[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(input.fd, read_buf, sizeof(read_buf))) > 0)
        {
            pthread_mutex_lock(&ctx->mutex);
            
            // Check if we have space in mmap buffer (100MB limit currently)
            if (ctx->input_len + (size_t)bytes_read > MAX_MMAP_INPUT_SIZE)
            {
                ctx->input_error = EFBIG;
                pthread_cond_broadcast(&ctx->cond);
                pthread_mutex_unlock(&ctx->mutex);
                break;
            }

            memcpy((void *)(ctx->input_start + ctx->input_len), read_buf, (size_t)bytes_read);
            ctx->input_len += (size_t)bytes_read;
            
            pthread_cond_broadcast(&ctx->cond);
            pthread_mutex_unlock(&ctx->mutex);
        }

        pthread_mutex_lock(&ctx->mutex);
        if (bytes_read == 0)
        {
            ctx->is_eof = true;
        }
        else if (bytes_read < 0)
        {
            ctx->input_error = errno;
        }
        pthread_cond_broadcast(&ctx->cond);
        pthread_mutex_unlock(&ctx->mutex);

        pthread_join(thread, NULL);

        return args.result;
}
#endif

EASY_PC_HIDDEN epc_parse_session_t
epc_parse_input(epc_parser_t * top_parser, epc_parse_input_t input)
{
    epc_parse_session_t session = {0};

    if (top_parser == NULL)
    {
        session.result = epc_unparsed_error_result(
            0, "Top parser not set for grammar", "grammar with a top parser", "NULL top_parser"
        );
        return session;
    }

    epc_parser_ctx_t * ctx = NULL;

    switch (input.type)
    {
    case EPC_PARSE_TYPE_STRING:
        if (input.input_string == NULL)
        {
            session.result = epc_unparsed_error_result(0, "Input string is NULL", "non-NULL input string", "NULL");
            return session;
        }
        ctx = internal_create_parse_ctx_from_string(input.input_string);
        break;

    case EPC_PARSE_TYPE_FILE:
        if (input.fp == NULL)
        {
            session.result = epc_unparsed_error_result(0, "Input file is NULL", "non-NULL input file", "NULL");
            return session;
        }
        ctx = internal_create_parse_ctx_from_fp(input.fp);
        break;

    case EPC_PARSE_TYPE_FILENAME:
        if (input.filename == NULL)
        {
            session.result = epc_unparsed_error_result(0, "Input filename is NULL", "non-NULL input filename", "NULL");
            return session;
        }
        FILE * fp = fopen(input.filename, "r");
        if (fp == NULL)
        {
            char error_message[256];
            snprintf(error_message, sizeof(error_message), "Failed to open file '%s': %s", input.filename, strerror(errno));
            session.result = epc_unparsed_error_result(0, error_message, "file that can be opened", "unopenable file");
            return session;
        }
        ctx = internal_create_parse_ctx_from_fp(fp);
        fclose(fp);

        break;

#ifdef WITH_INPUT_STREAM_SUPPORT
    case EPC_PARSE_TYPE_FD:
        ctx = internal_create_parse_ctx_streaming();
        break;
#endif

    default:
        session.result = epc_unparsed_error_result(0, "Invalid input type", "valid input type", "invalid input type");
        return session;
    }

    if (ctx == NULL)
    {
        session.result
            = epc_unparsed_error_result(0, "Failed to create parse context.", "valid parse context", "NULL");
        return session;
    }
    session.internal_parse_ctx = ctx;

#ifdef WITH_INPUT_STREAM_SUPPORT
    if (ctx->is_streaming)
    {
        session.result = parse_in_thread(top_parser, ctx, input);
    }
    else
#endif
    {
        session.result = top_parser->parse_fn(top_parser, ctx, 0);
    }

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

EASY_PC_API epc_parse_session_t
epc_parse_str(epc_parser_t * top_parser, char const * input_string)
{
    epc_parse_input_t input = {.type = EPC_PARSE_TYPE_STRING, .input_string = input_string};
    
    return epc_parse_input(top_parser, input);
}

EASY_PC_API epc_parse_session_t
epc_parse_fp(epc_parser_t * top_parser, FILE * fp)
{
    epc_parse_input_t input = {.type = EPC_PARSE_TYPE_FILE, .fp = fp};
    
    return epc_parse_input(top_parser, input);
}

EASY_PC_API epc_parse_session_t epc_parse_file(epc_parser_t * top_parser, char const * filename)
{
    epc_parse_input_t input = {.type = EPC_PARSE_TYPE_FILENAME, .filename = filename};
    
    return epc_parse_input(top_parser, input);
}

#ifdef WITH_INPUT_STREAM_SUPPORT
EASY_PC_API epc_parse_session_t
epc_parse_fd(epc_parser_t * top_parser, int fd)
{
    epc_parse_input_t input = {.type = EPC_PARSE_TYPE_FD, .fd = fd};

    return epc_parse_input(top_parser, input);
}
#endif

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
