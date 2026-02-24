#include "parsers.h"
#include "easy_pc_private.h"
#include "child_list.h"

#include <ctype.h>    // For isdigit
#include <errno.h>
#include <stdarg.h> // For va_list, va_start, va_arg, va_end
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOUND_BUFFER_SIZE 21

// --- Internal Helper Functions ---
// --- Parser List free. ---
static void
parser_list_free(parser_list_t *list)
{
    if (list == NULL)
    {
        return;
    }
    free(list->parsers);
    free(list);
}

// --- Parser List Creation ---
static parser_list_t *
parser_list_create_v(int count, va_list parsers)
{
    if (count <= 0)
    {
        return NULL;
    }

    parser_list_t * list = calloc(1, sizeof(*list));
    if (list == NULL)
    {
        return NULL;
    }

    list->parsers = calloc(count, sizeof(*list->parsers));
    if (list->parsers == NULL)
    {
        free(list);
        return NULL;
    }

    for (int i = 0; i < count; ++i)
    {
        list->parsers[i] = va_arg(parsers, epc_parser_t*);
    }
    list->count = count;

    return list;
}

static void
string_set(char const * * const dst, char const * src)
{
    free((char *)*dst);
    if (src == NULL)
    {
        *dst = NULL;
    }
    else
    {
        *dst = strdup(src);
    }
}

static void
parser_data_free(parser_data_type_st * data)
{
    switch (data->data_type)
    {
        case PARSER_DATA_TYPE_OTHER:
        case PARSER_DATA_TYPE_CHAR_RANGE:
        case PARSER_DATA_TYPE_COUNT:
        case PARSER_DATA_TYPE_BETWEEN:
        case PARSER_DATA_TYPE_DELIMITED:
        case PARSER_DATA_TYPE_LEXEME:
            /* Nothing to do. */
            break;

        case PARSER_DATA_TYPE_STRING:
            free((char *)data->string);
            data->string = NULL;
            break;

        case PARSER_DATA_TYPE_PARSER_LIST:
            parser_list_free(data->parser_list);
            data->parser_list = NULL;
            break;
    }
    data->data_type = PARSER_DATA_TYPE_OTHER;
}

void
epc_parser_free(epc_parser_t * parser)
{
    if (parser == NULL)
    {
        return;
    }
    parser_data_free(&parser->data);
    string_set(&parser->name, NULL);
    free(parser);
}

void
epc_parsers_free(size_t const count, ...)
{
    va_list parsers;

    va_start(parsers, count);
    for (size_t i = 0; i < count; i++)
    {
        epc_parser_t * parser = va_arg(parsers, epc_parser_t *);
        epc_parser_free(parser);
    }
    va_end(parsers);
}

// Helper for allocating a parser_t object
ATTR_NONNULL(1)
epc_parser_t *
epc_parser_allocate(char const * name)
{
    epc_parser_t * p = calloc(1, sizeof(*p));

    if (p == NULL)
    {
        return NULL;
    }
    string_set(&p->name, name);

    return p;
}

EASY_PC_HIDDEN
void
epc_parser_error_free(epc_parser_error_t * error)
{
    if (error == NULL)
    {
        return;
    }
    free((char *)error->message);
    free((char *)error->expected);
    free((char *)error->found);
    free(error);
}

EASY_PC_HIDDEN
epc_line_col_t
epc_calculate_line_and_column(char const * start, char const * current)
{
    epc_line_col_t res = {0};

    if (start == NULL || current == NULL)
    {
        return res;
    }

    char const * nl;
    char const * line_start = start;
    for (nl = strchr(start, '\n'); nl != NULL && nl <= current; nl = strchr(nl + 1, '\n'))
    {
        res.line++;
        line_start = nl;
    }
    res.col = current - line_start;

    return res;
}

epc_parser_error_t *
epc_parser_error_alloc(
    epc_parser_ctx_t * ctx,
    size_t input_offset,
    char const * message,
    char const * expected,
    char const * found
)
{
    epc_parser_error_t * error = calloc(1, sizeof(*error));
    if (error == NULL)
    {
        return error;
    }

    char const * input_start = (ctx != NULL) ? ctx->input_start : NULL;
    char const * current;

    if (ctx == NULL || ctx->input_start == NULL || input_offset > ctx->input_len)
    {
        input_start = NULL;
        current = NULL;
    }
    else
    {
        input_start = ctx->input_start;
        current = input_start + input_offset;
    }

    error->input_position = current;
    error->position = epc_calculate_line_and_column(input_start, current);

    error->message = strdup(message != NULL ? message : "");
    error->expected = strdup(expected != NULL ? expected : "");
    error->found = strdup(found != NULL ? found : "");

    return error;
}

void
epc_parser_result_cleanup(epc_parse_result_t * result)
{
    if (result->is_error)
    {
        epc_parser_error_free(result->data.error);
    }
    else
    {
        epc_node_free(result->data.success);
    }
    memset(result, 0, sizeof(*result));
}

epc_parse_result_t
epc_unparsed_error_result(
    size_t input_offset,
    char const * message,
    char const * expected,
    char const * found
)
{
    epc_parse_result_t result = {
        .is_error = true,
        .data.error = epc_parser_error_alloc(
            NULL,
            input_offset,
            message,
            expected,
            found),
    };
    return result;
}

static void
parser_furthest_error_restore(epc_parser_ctx_t * ctx, epc_parser_error_t * * replacement)
{
    epc_parser_error_free(ctx->furthest_error);
    ctx->furthest_error = *replacement;
    *replacement = NULL;
}

static epc_parser_error_t *
parser_error_copy(epc_parser_ctx_t * ctx, epc_parser_error_t *e)
{
    if (e == NULL)
    {
        return NULL;
    }
    return epc_parser_error_alloc(ctx, e->input_position - ctx->input_start, e->message, e->expected, e->found);
}

static void
update_furthest_error(epc_parser_ctx_t * ctx, epc_parser_error_t * new_error)
{
    if (ctx == NULL || new_error == NULL)
    {
        return;
    }
    if (ctx->furthest_error == NULL
        || (new_error->input_position >= ctx->furthest_error->input_position))
    {
        epc_parser_error_t * e_copy = parser_error_copy(ctx, new_error);
        parser_furthest_error_restore(ctx, &e_copy);
    }
}

static epc_parse_result_t
epc_parser_error_result(
    epc_parser_ctx_t * ctx,
    size_t input_offset,
    char const * message,
    char const * expected,
    char const * found
)
{
    epc_parse_result_t result = {
        .is_error = true,
        .data.error = epc_parser_error_alloc(ctx, input_offset, message, expected, found),
    };
    update_furthest_error(ctx, result.data.error);
    return result;
}

epc_parse_result_t
epc_parser_success_result(epc_cpt_node_t * success_node)
{
    epc_parse_result_t result = {
        .data.success = success_node,
    };

    return result;
}

EASY_PC_HIDDEN
epc_parser_error_t*
parser_furthest_error_copy(epc_parser_ctx_t * ctx)
{
    return parser_error_copy(ctx, ctx->furthest_error);
}

static char const *
parser_get_expected_str(epc_parser_ctx_t * ctx, epc_parser_t * p)
{
    if (ctx == NULL || p == NULL)
    {
        /* Shouldn't happen. */
        return "NULL_PARSER";
    }

    if (p->expected_value != NULL)
    {
        return p->expected_value;
    }

    if (p->name != NULL)
    {
        return p->name;
    }
    else
    {
        /* Shouldn't happen. */
        return "Unnamed parser";
    }
}

#define WITH_PARSE_DEBUG 0

// Parser helper function
static epc_parse_result_t
parse(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
#if WITH_PARSE_DEBUG
    char const * input = ctx->input_start + input_offset;

    fprintf(stderr, "parsing: name: %s. input `%s`, offset: %zu\n", self->name, input, input_offset);
#endif

    epc_parse_result_t result = self->parse_fn(self, ctx, input_offset);

#if WITH_PARSE_DEBUG
    if (result.is_error)
    {
        fprintf(stderr, "\tfailed to parse: name: %s (expected: %s)\n", self->name, self->expected_value);
    }
    else
    {
        fprintf(stderr, "matched: %s `%.*s`\n", self->name, (int)result.data.success->len, input);
    }
#endif

    return result;
}


// --- Terminal Parser Implementations ---

static epc_parse_result_t
pchar_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    char const * expected_str = self->data.string;
    char expected_char = expected_str[0];

    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", expected_str, "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (input[0] == expected_char)
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "char");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    // else Mismatch
    char found_str[2] = { input[0], '\0'};

    return epc_parser_error_result(ctx, input_offset, "Unexpected character", expected_str, found_str);
}

epc_parser_t *
epc_char(char const * name, char c)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "char_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pchar_parse_fn;

    char buf[2] = { c, '\0'};
    char * data = strdup(buf);
    if (data == NULL)
    {
        free(p);
        return NULL;
    }
    p->data.data_type = PARSER_DATA_TYPE_STRING;
    p->data.string = data;
    p->expected_value = p->data.string;

    return p;
}

static epc_parse_result_t
pstring_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    char const * expected_str = self->data.string;
    size_t expected_len = strlen(expected_str);

    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", expected_str, "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (strncmp(input, expected_str, expected_len) == 0)
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "string");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = expected_len;

        return epc_parser_success_result(node);
    }

    /* Match not found. */
    char found_buffer[FOUND_BUFFER_SIZE];
    snprintf(found_buffer, sizeof(found_buffer), "%.*s", (int)sizeof(found_buffer) - 1, input);
    char const * error_msg;

    size_t actual_len = strlen(input);

    if (actual_len < expected_len)
    {
        error_msg = "Unexpected end of input";
    }
    else
    {
        error_msg = "Unexpected string";
    }

    return epc_parser_error_result(ctx, input_offset, error_msg, expected_str, found_buffer);
}

epc_parser_t *
epc_string(char const * name, char const * s)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "string_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pstring_parse_fn;
    char *data = strdup(s);
    if (data == NULL)
    {
        free(p);
        return NULL;
    }
    p->data.data_type = PARSER_DATA_TYPE_STRING;
    p->data.string = data;
    p->expected_value = p->data.string;

    return p;
}

static epc_parse_result_t
peoi_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    char const * input = ctx->input_start + input_offset;

    if (input_offset < ctx->input_len)
    {
        /* Still some input left. */
        char buf[FOUND_BUFFER_SIZE];

        strncpy(buf, input, sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';

        return epc_parser_error_result(ctx, input_offset, "End of input not found", "<end of input>", buf);
    }

    epc_cpt_node_t * node = epc_node_alloc(self, "eio");
    if (node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
    }

    node->content = input;
    node->len = 0;

    return epc_parser_success_result(node);
}

epc_parser_t *
epc_eoi(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "eoi_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = peoi_parse_fn;
    return p;
}

static epc_parse_result_t
pdigit_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "digit", "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (isdigit(input[0]))
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "digit");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    // else Mismatch
    char found_str[2] = { input[0], '\0' };
    return epc_parser_error_result(ctx, input_offset, "Unexpected character", "digit", found_str);
}

epc_parser_t *
epc_digit(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "digit_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pdigit_parse_fn;
    p->expected_value = "digit";

    return p;
}

static epc_parse_result_t
pint_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "integer", "EOF");
    }

    char const * input = ctx->input_start + input_offset;
    char * endptr;
    (void)strtoll(input, &endptr, 10); // Base 10

    size_t parsed_len = endptr - input;
    size_t input_remaining = ctx->input_len - input_offset;

    // A valid integer must parse at least one digit
    if (parsed_len > 0
        && parsed_len <= input_remaining
        && (isdigit(input[0]) || (input[0] == '-' && parsed_len > 1 && isdigit(input[1]))))
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "integer");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = parsed_len;

        return epc_parser_success_result(node);
    }

    /* No match to an integer. */
    char found_buffer[FOUND_BUFFER_SIZE];
    if (input[0] != '\0')
    {
        snprintf(found_buffer, sizeof(found_buffer), "%.*s", (int)sizeof(found_buffer) - 1, input);
    }
    else
    {
        strncpy(found_buffer, "EOF", sizeof(found_buffer) - 1);
        found_buffer[sizeof(found_buffer) - 1] = '\0';
    }

    return epc_parser_error_result(ctx, input_offset, "Expected an integer", "integer", found_buffer);
}

epc_parser_t *
epc_int(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "integer");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pint_parse_fn;

    return p;
}

static epc_parse_result_t
pspace_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "whitespace", "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (isspace(input[0]))
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "space");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    // else Mismatch
    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Unexpected character", "whitespace", found_str);
}

epc_parser_t *
epc_space(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "space_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pspace_parse_fn;
    p->expected_value = "whitespace";

    return p;
}

static epc_parse_result_t
palpha_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "alpha", "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (isalpha(input[0]))
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "alpha");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    // else Mismatch
    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Unexpected character", "alpha", found_str);
}

epc_parser_t *
epc_alpha(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "alpha_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = palpha_parse_fn;
    p->expected_value = "alpha";

    return p;
}

static epc_parse_result_t
palphanum_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "alphanum", "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (isalnum(input[0]))
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "alphanum");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    // else // Mismatch
    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Unexpected character", "alphanum", found_str);
}

epc_parser_t *
epc_alphanum(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "alphanum");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = palphanum_parse_fn;

    return p;
}

static epc_parse_result_t
pdouble_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "double", "EOF");
    }

    char const * input = ctx->input_start + input_offset;
    size_t remaining_input = ctx->input_len - input_offset;

    // Use strtod to parse the double
    char * endptr;
    (void)strtod(input, &endptr);
    size_t parsed_len = endptr - input;

    if (errno == ERANGE)
    {
        char found_str[FOUND_BUFFER_SIZE];
        snprintf(found_str, sizeof(found_str), "%.*s", (int)sizeof(found_str) - 1, input);
        return epc_parser_error_result(ctx, input_offset, "Double out of range", "double", found_str);
    }

    if (parsed_len == 0 || parsed_len > remaining_input)
    {
        char found_str[FOUND_BUFFER_SIZE];
        snprintf(found_str, sizeof(found_str), "%.*s", 1, input);
        return epc_parser_error_result(ctx, input_offset, "Expected a double", "double", found_str);
    }

    epc_cpt_node_t * node = epc_node_alloc(self, "double");
    if (node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
    }

    node->content = input;
    node->len = parsed_len;

    return epc_parser_success_result(node);
}

epc_parser_t *
epc_double(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "double_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pdouble_parse_fn;
    p->expected_value = "double";

    return p;
}

static epc_parse_result_t
por_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "or", "EOF");
    }

    epc_parser_error_t * original_furthest_error = NULL;
    parser_list_t * alternatives = self->data.parser_list;

    if (alternatives == NULL || alternatives->count == 0)
    {
        return epc_parser_error_result(ctx, input_offset, "No alternatives provided to 'or' parser", self->name, "N/A");
    }

    original_furthest_error = parser_furthest_error_copy(ctx);

    for (int i = 0; i < alternatives->count; ++i)
    {
        epc_parser_t * current_parser = alternatives->parsers[i];
        if (current_parser)
        {
            epc_parse_result_t child_result = parse(current_parser, ctx, input_offset);
            if (!child_result.is_error)
            {
                // Return the child's success, but mark the CPT node with this 'or' parser
                epc_cpt_node_t * or_node = epc_node_alloc(self, "or");
                if (or_node == NULL)
                {
                    epc_parser_result_cleanup(&child_result);
                    epc_parser_error_free(original_furthest_error);

                    return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
                }

                or_node->content = child_result.data.success->content;
                or_node->len = child_result.data.success->len;
                or_node->children = calloc(1, sizeof(*or_node->children));
                if (or_node->children == NULL)
                {
                    epc_parser_result_cleanup(&child_result);
                    epc_parser_error_free(original_furthest_error);

                    return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
                }

                or_node->children[0] = child_result.data.success;
                or_node->children_count = 1;

                parser_furthest_error_restore(ctx, &original_furthest_error);

                return epc_parser_success_result(or_node);
            }
            else
            {
                epc_parser_result_cleanup(&child_result);
            }
        }
    }

    /* No alternatives matched if we get here. */
    epc_parser_error_free(original_furthest_error);

    size_t estimated_len = 0;
    for (int i = 0; i < alternatives->count; ++i)
    {
        if (alternatives->parsers[i])
        {
            char const * temp_expected = parser_get_expected_str(ctx, alternatives->parsers[i]);

            estimated_len += strlen(temp_expected);
            if (i < alternatives->count - 1)
            {
                estimated_len += strlen(" or ");
            }
        }
    }

    char const * expected_str;
    char * aggregated_expected_str = NULL;
    if (estimated_len > 0)
    {
        aggregated_expected_str = malloc(estimated_len + 1);

        if (aggregated_expected_str != NULL)
        {
            aggregated_expected_str[0] = '\0';
            for (int i = 0; i < alternatives->count; ++i)
            {
                if (alternatives->parsers[i])
                {
                    char const * child_expected = parser_get_expected_str(ctx, alternatives->parsers[i]);
                    if (child_expected)
                    {
                        strcat(aggregated_expected_str, child_expected);
                        if (i < alternatives->count - 1)
                        {
                            strcat(aggregated_expected_str, " or ");
                        }
                    }
                }
            }
        }
        expected_str = aggregated_expected_str;
    }
    else
    {
        expected_str = self->name;
    }

    char const * input = ctx->input_start + input_offset;
    char found_buffer[FOUND_BUFFER_SIZE];
    snprintf(found_buffer, sizeof(found_buffer), "%.*s", (int)sizeof(found_buffer) - 1, input);

    epc_parse_result_t result = epc_parser_error_result(
        ctx, input_offset, "No alternative matched", expected_str, found_buffer );
    free(aggregated_expected_str);

    return result;
}

static epc_parser_t *
vepc_or(char const * name, int count, va_list args)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "or_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->data.parser_list = parser_list_create_v(count, args);
    p->data.data_type = PARSER_DATA_TYPE_PARSER_LIST;

    p->parse_fn = por_parse_fn;

    return p;
}

epc_parser_t *
epc_or(char const * name, int count, ...)
{
    va_list args;

    va_start(args, count);
    epc_parser_t * p = vepc_or(name, count ,args);
    va_end(args);

    return p;
}

epc_parser_t *
epc_or_l(epc_parser_list * list, char const * name, int count, ...)
{
    va_list args;

    va_start(args, count);
    epc_parser_t * p = vepc_or(name, count ,args);
    va_end(args);

    epc_parser_list_add(list, p);
    return p;
}

static epc_parse_result_t
pand_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "and", "EOF");
    }

    parser_list_t * sequence = self->data.parser_list;

    if (sequence == NULL || sequence->count == 0)
    {
        return epc_parser_error_result(ctx, input_offset, "No parsers in 'and' sequence", self->name, "N/A");
    }

    epc_cpt_node_t ** children_nodes = calloc(sequence->count, sizeof(*children_nodes));

    if (children_nodes == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
    }

    size_t current_input_offset = input_offset;
    size_t and_start_offset = current_input_offset;
    char const * and_start_input = ctx->input_start + and_start_offset;

    epc_parse_result_t failed_child_result = {0};
    epc_parse_result_t null_child_result = {0};
    int child_count = 0;
    for (int i = 0; i < sequence->count; i++, child_count++)
    {
        epc_parser_t * current_parser = sequence->parsers[i];
        if (current_parser)
        {
            epc_parse_result_t child_result =
                parse(current_parser, ctx, current_input_offset);
            if (child_result.is_error)
            {
                failed_child_result = child_result;
                break;
            }
            children_nodes[i] = child_result.data.success;
            current_input_offset += child_result.data.success->len;
        }
        else
        {
            null_child_result = epc_parser_error_result(ctx, current_input_offset, "NULL parser found in 'and' sequence", self->name, "NULL");
            break;
        }
    }

    /* Check if any errors occurred while checking the sequence of parsers. */
    if (null_child_result.is_error || failed_child_result.is_error)
    {
        for (int i = 0; i < child_count; i++)
        {
            epc_node_free(children_nodes[i]);
        }
        free(children_nodes);
    }

    if (null_child_result.is_error)
    {
        return null_child_result;
    }
    if (failed_child_result.is_error)
    {
        return failed_child_result;
    }

    /* No child errors, so the AND condition has succeeded. */

    epc_cpt_node_t * parent_node = epc_node_alloc(self, "and");
    if (parent_node == NULL)
    {
        for (int i = 0; i < sequence->count; i++)
        {
            epc_node_free(children_nodes[i]);
        }
        return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
    }

    parent_node->children = children_nodes;
    parent_node->children_count = sequence->count;
    parent_node->content = and_start_input;
    parent_node->len = current_input_offset - and_start_offset;

    return epc_parser_success_result(parent_node);
}

// --- C++ Comment Parser Implementation ---
// Matches "//" followed by any characters until a newline or EOF.
static epc_parse_result_t
pcpp_comment_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    (void)self;
    epc_parser_list * l = epc_parser_list_create();

    epc_parse_result_t sub_result;
    epc_parser_t * slash_slash = epc_string_l(l, "slash_slash", "//");
    epc_parser_t * any_char_except_newline = epc_none_of_l(l, "any_char_except_newline", "\n");
    epc_parser_t * many_chars = epc_many_l(l, "many_chars", any_char_except_newline);
    epc_parser_t * newline_parser = epc_char_l(l, "newline", '\n');
    epc_parser_t * eoi_parser = epc_eoi_l(l, "eoi");
    epc_parser_t * line_end = epc_or_l(l, "line_end", 2, newline_parser, eoi_parser);
    epc_parser_t * comment_parser = epc_and_l(l, "cpp_comment_sequence", 3, slash_slash, many_chars, line_end);

    sub_result = parse(comment_parser, ctx, input_offset);

    epc_parse_result_t result;
    if (sub_result.is_error)
    {
        result = sub_result;
    }
    else
    {
        epc_cpt_node_t * node= epc_node_alloc(self, "cpp_comment");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = sub_result.data.success->content;
        node->len = sub_result.data.success->len;
        result = epc_parser_success_result(node);
    }

    epc_parser_list_free(l);

    return result;
}

epc_parser_t *
epc_cpp_comment(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "cpp_comment");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pcpp_comment_parse_fn;
    p->expected_value = "// C++ style comment";

    return p;
}

static epc_parser_t *
vepc_and(char const * name, int count, va_list args)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "and_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->data.parser_list = parser_list_create_v(count, args);
    p->data.data_type = PARSER_DATA_TYPE_PARSER_LIST;

    p->parse_fn = pand_parse_fn;

    return p;
}

epc_parser_t *
epc_and(char const * name, int count, ...)
{
    va_list args;

    va_start(args, count);
    epc_parser_t * p = vepc_and(name, count ,args);
    va_end(args);

    return p;
}

epc_parser_t *
epc_and_l(epc_parser_list * list, char const * name, int count, ...)
{
    va_list args;

    va_start(args, count);
    epc_parser_t * p = vepc_and(name, count ,args);
    va_end(args);

    epc_parser_list_add(list, p);
    return p;
}

static epc_parse_result_t
pskip_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "skip", "EOF");
    }
    epc_parser_t * parser_to_skip = (epc_parser_t *)self->data.other;
    if (parser_to_skip == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "p_skip received NULL child parser", self->name, "NULL");
    }

    size_t current_input_offset = input_offset;
    size_t total_skipped_len = 0;

    while (1)
    {
        epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx);
        epc_parse_result_t child_result = parse(parser_to_skip, ctx, current_input_offset);
        if (child_result.is_error)
        {
            parser_furthest_error_restore(ctx, &original_furthest_error);
            epc_parser_result_cleanup(&child_result);
            break;
        }
        if (child_result.data.success->len == 0)
        {
            /*
             * No progress is being made through the input, so this will loop
             * indefinitely.
             * Return with an error.
             */
            epc_parser_error_free(original_furthest_error);
            epc_parser_result_cleanup(&child_result);
            return epc_parser_error_result(ctx, input_offset, "Infinite recursion detected", self->name, "N/A");
        }
        total_skipped_len += child_result.data.success->len;
        current_input_offset += child_result.data.success->len;
        epc_parser_error_free(original_furthest_error);
        epc_parser_result_cleanup(&child_result);
    }

    epc_cpt_node_t * dummy_node = epc_node_alloc(self, "skip");
    if (dummy_node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
    }

    char const * input = ctx->input_start + input_offset;

    dummy_node->content = input;
    dummy_node->len = total_skipped_len;

    return epc_parser_success_result(dummy_node);
}

epc_parser_t *
epc_skip(char const * name, epc_parser_t * parser_to_skip)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "skip_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pskip_parse_fn;
    p->data.other = parser_to_skip;

    return p;
}

static epc_parse_result_t
pplus_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "plus", "EOF");
    }
    epc_parser_t * parser_to_repeat = (epc_parser_t *)self->data.other;

    if (parser_to_repeat == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "p_plus received NULL child parser", self->name, "NULL");
    }

    child_list_t children = {0};
    if (!child_list_init(&children, 4))
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_plus children", self->name, "N/A");
    }

    size_t current_input_offset = input_offset;
    size_t plus_start_input_offset = input_offset;
    char const * plus_start_input = ctx->input_start + input_offset;

    epc_parse_result_t first_child_result = parse(parser_to_repeat, ctx, current_input_offset);
    if (first_child_result.is_error)
    {
        child_list_release(&children);
        return first_child_result;
    }

    if (!child_list_append(&children, first_child_result.data.success))
    {
        child_list_release(&children);
        return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_plus children", self->name, "N/A");
    }
    current_input_offset += first_child_result.data.success->len;

    bool infinite_recursion_detected = false;
    while (!infinite_recursion_detected)
    {
        size_t loop_start_input_offset = current_input_offset;
        epc_parse_result_t child_result = parse(parser_to_repeat, ctx, current_input_offset);
        if (!child_result.is_error)
        {
            if (!child_list_append(&children, child_result.data.success))
            {
                child_list_release(&children);
                return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_plus children", self->name, "N/A");
            }
            current_input_offset += child_result.data.success->len;
        }
        else
        {
            epc_parser_result_cleanup(&child_result);
            break;
        }
        infinite_recursion_detected = current_input_offset == loop_start_input_offset;
    }

    if (infinite_recursion_detected)
    {
        child_list_release(&children);
        return epc_parser_error_result(
            ctx,
            current_input_offset,
            "Infinite recursion detected",
            "Progress",
            "No progress"
        );
    }

    epc_cpt_node_t * parent_node = epc_node_alloc(self, "plus");
    if (parent_node == NULL)
    {
        child_list_release(&children);
        return epc_parser_error_result(ctx, plus_start_input_offset, "Memory allocation failure for p_plus parent node", self->name, "N/A");
    }

    child_list_transfer(&children, parent_node);
    parent_node->content = plus_start_input;
    parent_node->len = current_input_offset - plus_start_input_offset;

    return epc_parser_success_result(parent_node);
}

epc_parser_t *
epc_plus(char const * name, epc_parser_t * parser_to_repeat)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "plus_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pplus_parse_fn;
    p->data.other = parser_to_repeat;

    return p;
}

static epc_parse_result_t
pchar_range_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    char_range_data_t * range = &self->data.range;

    char expected_str[32]; // e.g., "character in range [a-z]"
    snprintf(expected_str, sizeof(expected_str), "character in range [%c-%c]", range->start, range->end);

    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", expected_str, "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (input[0] >= range->start && input[0] <= range->end)
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "char_range");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }
        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    /* else not in range. */
    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Unexpected character", expected_str, found_str);
}

EASY_PC_API epc_parser_t *
epc_char_range(char const * name, char char_start, char char_end)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "char_range");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pchar_range_parse_fn;

    p->data.data_type = PARSER_DATA_TYPE_CHAR_RANGE;
    p->data.range.start = char_start;
    p->data.range.end = char_end;

    return p;
}

static epc_parse_result_t
pany_char_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "any character", "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    epc_cpt_node_t * node = epc_node_alloc(self, "any_char");
    if (node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
    }
    node->content = input;
    node->len = 1;

    return epc_parser_success_result(node);
}

EASY_PC_API epc_parser_t *
epc_any_char(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "any_char");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pany_char_parse_fn;
    return p;
}

static epc_parse_result_t
pnone_of_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    char const * chars_to_avoid = self->data.string;
    char expected_str[64];
    snprintf(expected_str, sizeof(expected_str), "character not in set '%s'", chars_to_avoid);

    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", expected_str, "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (strchr(chars_to_avoid, input[0]) == NULL)
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "none_of");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }
        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Character found in forbidden set", expected_str, found_str);
}

EASY_PC_API epc_parser_t *
epc_none_of(char const * name, char const * chars_to_avoid)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "none_of");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pnone_of_parse_fn;
    char * duplicated_chars = strdup(chars_to_avoid);
    if (duplicated_chars == NULL)
    {
        free(p);
        return NULL;
    }
    p->data.data_type = PARSER_DATA_TYPE_STRING;
    p->data.string = duplicated_chars;

    return p;
}

static epc_parse_result_t
pmany_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    epc_parser_t * parser_to_repeat = (epc_parser_t *)self->data.other;

    /* Allow many to pass eventfd(at end of input. */
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "many", "EOF");
    }

    if (parser_to_repeat == NULL)
    {
        // Should not happen if grammar is well-formed
        return epc_parser_error_result(ctx, input_offset, "p_many received NULL child parser", self->name, "NULL");
    }

    size_t current_input_offset = input_offset;

    child_list_t children = {0};

    if (!child_list_init(&children, 4))
    {
        return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_many children", self->name, "N/A");
    }

    bool infinite_recursion_detected = false;
    while (!infinite_recursion_detected) // Loop as long as child parser matches
    {
        size_t loop_start_input_offset = current_input_offset;
        epc_parse_result_t child_result = parse(parser_to_repeat, ctx, current_input_offset);
        if (child_result.is_error)
        {
            epc_parser_result_cleanup(&child_result);
            break;
        }
        if (!child_list_append(&children, child_result.data.success))
        {
            child_list_release(&children);
            return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_many children", self->name, "N/A");
        }
        current_input_offset += child_result.data.success->len;

        infinite_recursion_detected = current_input_offset == loop_start_input_offset;
    }

    if (infinite_recursion_detected)
    {
        child_list_release(&children);
        return epc_parser_error_result(
            ctx,
            current_input_offset,
            "Infinite recursion detected",
            "Progress",
            "No progress"
        );
    }

    epc_cpt_node_t * parent_node = epc_node_alloc(self, "many");
    if (parent_node == NULL)
    {
        child_list_release(&children);
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_many parent node", self->name, "N/A");
    }

    char const * input = ctx->input_start + input_offset;

    child_list_transfer(&children, parent_node);
    parent_node->content = input;
    parent_node->len = current_input_offset - input_offset;

    return epc_parser_success_result(parent_node);
}

EASY_PC_API epc_parser_t *
epc_many(char const * name, epc_parser_t * p_to_repeat)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "many_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pmany_parse_fn;
    p->data.other = p_to_repeat;

    return p;
}

static epc_parse_result_t
pcount_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "count", "EOF");
    }

    count_data_t * count_data = &self->data.count;
    epc_parser_t * parser_to_repeat = count_data->parser;
    int num_to_match = count_data->count;

    if (parser_to_repeat == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "p_count received NULL child parser", self->name, "NULL");
    }

    char const * input = ctx->input_start + input_offset;

    if (num_to_match <= 0) // Matching 0 times is always a success (empty match)
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "count");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }
        node->content = input;
        node->len = 0;

        return epc_parser_success_result(node);
    }

    size_t current_input_offset = input_offset;
    child_list_t children = {0};

    if (!child_list_init(&children, 4))
    {
        return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_count children", self->name, "N/A");
    }

    for (int i = 0; i < num_to_match; ++i)
    {
        epc_parse_result_t child_result = parse(parser_to_repeat, ctx, current_input_offset);
        if (child_result.is_error)
        {
            // Child parser failed to match required number of times
            char msg[64];

            snprintf(msg, sizeof(msg), "Count failed to match child at count %u", i + 1);
            epc_parse_result_t error_result =
                epc_parser_error_result(
                    ctx,
                    current_input_offset,
                    msg,
                    child_result.data.error->expected,
                    child_result.data.error->found
                );

            epc_parser_result_cleanup(&child_result);

            return error_result;
        }
        if (!child_list_append(&children, child_result.data.success))
        {
            child_list_release(&children);
            return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_count children", self->name, "N/A");
        }
        current_input_offset += child_result.data.success->len;
    }

    epc_cpt_node_t * parent_node = epc_node_alloc(self, "count");
    if (parent_node == NULL)
    {
        child_list_release(&children);
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_count parent node", self->name, "N/A");
    }

    child_list_transfer(&children, parent_node);
    parent_node->content = input;
    parent_node->len = current_input_offset - input_offset;

    return epc_parser_success_result(parent_node);
}

EASY_PC_API epc_parser_t *
epc_count(char const * name, int num, epc_parser_t * p_to_repeat)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "count_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pcount_parse_fn;

    p->data.data_type = PARSER_DATA_TYPE_COUNT;
    p->data.count.count = num;
    p->data.count.parser = p_to_repeat;
    return p;
}

static epc_parse_result_t
pbetween_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "between", "EOF");
    }

    epc_parser_error_t * original_furthest_error = NULL;
    between_data_t * between_data = &self->data.between;
    epc_parser_t * p_open = between_data->open;
    epc_parser_t * p_wrapped = between_data->parser;
    epc_parser_t * p_close = between_data->close;

    if (p_open == NULL || p_wrapped == NULL || p_close == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "p_between received NULL child parser(s)", self->name, "NULL");
    }

    size_t current_input_offset = input_offset;
    original_furthest_error = parser_furthest_error_copy(ctx);

    // 1. Match 'open'
    epc_parse_result_t open_result = parse(p_open, ctx, current_input_offset);
    if (open_result.is_error)
    {
        epc_parser_error_free(original_furthest_error);

        return open_result;
    }

    current_input_offset += open_result.data.success->len;
    epc_parser_result_cleanup(&open_result);

    // 2. Match 'wrapped' parser
    epc_parse_result_t wrapped_result = parse(p_wrapped, ctx, current_input_offset);
    if (wrapped_result.is_error)
    {
        epc_parser_error_free(original_furthest_error);

        return wrapped_result;
    }
    current_input_offset += wrapped_result.data.success->len;
    /* Don't clean up the wrapped result as that is what gets returned on success. */

    // 3. Match 'close'
    epc_parse_result_t close_result = parse(p_close, ctx, current_input_offset);
    if (close_result.is_error)
    {
        epc_parser_error_free(original_furthest_error);

        return close_result;
    }
    current_input_offset += close_result.data.success->len;
    epc_parser_result_cleanup(&close_result);

    // Success - create a node for 'between'
    epc_cpt_node_t * parent_node = epc_node_alloc(self, "between");
    if (parent_node == NULL)
    {
        epc_parser_result_cleanup(&wrapped_result);
        epc_parser_error_free(original_furthest_error);

        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_between parent node", self->name, "N/A");
    }

    parent_node->children = calloc(1, sizeof(*parent_node->children));
    if (parent_node->children == NULL)
    {
        epc_parser_result_cleanup(&wrapped_result);
        epc_parser_error_free(original_furthest_error);

        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_between children array", self->name, "N/A");
    }

    // Restore furthest error as this parser suppresses it
    parser_furthest_error_restore(ctx, &original_furthest_error);

    parent_node->children[0] = wrapped_result.data.success; // Only the wrapped result is kept as a child
    parent_node->children_count = 1;

    char const * input = ctx->input_start + input_offset;

    parent_node->content = input;
    parent_node->len = current_input_offset - input_offset;

    return epc_parser_success_result(parent_node);
}

EASY_PC_API epc_parser_t *
epc_between(char const * name, epc_parser_t * p_open, epc_parser_t * p_wrapped, epc_parser_t * p_close)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "between_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pbetween_parse_fn;

    p->data.data_type = PARSER_DATA_TYPE_BETWEEN;
    p->data.between.open = p_open;
    p->data.between.parser = p_wrapped;
    p->data.between.close = p_close;

    return p;
}

static epc_parse_result_t
pdelimited_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "delimited", "EOF");
    }

    delimited_data_t * delimited_data = &self->data.delimited;
    epc_parser_t * item_parser = delimited_data->item;
    epc_parser_t * delimiter_parser = delimited_data->delimiter;

    if (item_parser == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "p_delimited received NULL item parser", self->name, "NULL");
    }
    // Delimiter can be NULL, meaning no delimiter, just sequence of items

    size_t current_input_offset = input_offset;
    child_list_t children = {0};

    if (!child_list_init(&children, 4))
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_delimited children", self->name, "N/A");
    }

    // First item (must match)
    epc_parse_result_t first_item_result = parse(item_parser, ctx, current_input_offset);

    if (first_item_result.is_error)
    {
        child_list_release(&children);
        return first_item_result;
    }
    if (!child_list_append(&children, first_item_result.data.success))
    {
        child_list_release(&children);
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_delimited children", self->name, "N/A");
    }

    current_input_offset += first_item_result.data.success->len;

    // Remaining items (item + delimiter)
    bool infinite_recursion_detected = false;

    while (!infinite_recursion_detected)
    {
        size_t loop_start_input_offset = current_input_offset;

        if (delimiter_parser != NULL)
        {
            epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx);
            epc_parse_result_t delim_result = parse(delimiter_parser, ctx, current_input_offset);

            if (delim_result.is_error)
            {
                // Delimiter not found, stop parsing further items
                epc_parser_result_cleanup(&delim_result);
                parser_furthest_error_restore(ctx, &original_furthest_error);
                break;
            }
            epc_parser_error_free(original_furthest_error);
            current_input_offset += delim_result.data.success->len;
            epc_parser_result_cleanup(&delim_result);
        }
        epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx);
        epc_parse_result_t item_result = parse(item_parser, ctx, current_input_offset);
        if (item_result.is_error)
        {
            if (delimiter_parser != NULL)
            {
                char const * current_input = ctx->input_start + current_input_offset;
                char found_buffer[FOUND_BUFFER_SIZE];
                snprintf(found_buffer, sizeof(found_buffer), "%.*s", (int)sizeof(found_buffer) - 1, current_input);

                child_list_release(&children);
                parser_furthest_error_restore(ctx, &original_furthest_error);
                epc_parser_result_cleanup(&item_result);
                return epc_parser_error_result(
                    ctx,
                    current_input_offset,
                    "Unexpected trailing delimiter",
                    parser_get_expected_str(ctx, item_parser),
                    found_buffer
                );
            }
            // Item not found, stop parsing further items
            parser_furthest_error_restore(ctx, &original_furthest_error);
            epc_parser_result_cleanup(&item_result);
            break;
        }
        parser_furthest_error_restore(ctx, &original_furthest_error);
        if (!child_list_append(&children, item_result.data.success))
        {
            child_list_release(&children);
            return epc_parser_error_result(ctx, current_input_offset, "Memory allocation failure for p_delimited children", self->name, "N/A");
        }
        current_input_offset += item_result.data.success->len;

        infinite_recursion_detected = current_input_offset == loop_start_input_offset;
    }

    if (infinite_recursion_detected)
    {
        child_list_release(&children);
        return epc_parser_error_result(
            ctx,
            current_input_offset,
            "Infinite recursion detected",
            "Progress",
            "No progress"
        );
    }

    epc_cpt_node_t * parent_node = epc_node_alloc(self, "delimited");
    if (parent_node == NULL)
    {
        child_list_release(&children);
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for p_delimited parent node", self->name, "N/A");
    }

    char const * input = ctx->input_start + input_offset;

    child_list_transfer(&children, parent_node);
    parent_node->content = input;
    parent_node->len = current_input_offset - input_offset;

    return epc_parser_success_result(parent_node);
}

EASY_PC_API epc_parser_t *
epc_delimited(char const * name, epc_parser_t * item_parser, epc_parser_t * delimiter_parser)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "delimited_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pdelimited_parse_fn;

    p->data.data_type = PARSER_DATA_TYPE_DELIMITED;
    p->data.delimited.item = item_parser;
    p->data.delimited.delimiter = delimiter_parser;

    return p;
}

static epc_parse_result_t
poptional_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "optional", "EOF");
    }

    epc_parser_error_t * original_furthest_error = NULL;
    epc_parser_t * child_parser = (epc_parser_t *)self->data.other;

    if (child_parser == NULL) // Should not happen if grammar is well-formed
    {
        return epc_parser_error_result(ctx, input_offset, "p_optional received NULL child parser", self->name, "NULL");
    }

    original_furthest_error = parser_furthest_error_copy(ctx); // Save before child parse
    epc_parse_result_t child_result = parse(child_parser, ctx, input_offset);

    if (!child_result.is_error)
    {
        // Child matched, return its success result wrapped in an optional node
        epc_cpt_node_t * parent_node = epc_node_alloc(self, "optional");
        if (parent_node == NULL)
        {
            epc_parser_result_cleanup(&child_result);
            epc_parser_error_free(original_furthest_error);
            return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for optional parent node", self->name, "N/A");
        }
        parent_node->children = calloc(1, sizeof(*parent_node->children));
        if (parent_node->children == NULL)
        {
            epc_parser_result_cleanup(&child_result);
            epc_parser_error_free(original_furthest_error);
            return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for optional children array", self->name, "N/A");
        }
        parent_node->children[0] = child_result.data.success;
        parent_node->children_count = 1;

        parser_furthest_error_restore(ctx, &original_furthest_error);

        parent_node->content = child_result.data.success->content;
        parent_node->len = child_result.data.success->len;

        return epc_parser_success_result(parent_node);
    }
    // Child failed, p_optional still succeeds, consuming no input.
    // Return an empty optional node.
    epc_parser_result_cleanup(&child_result);
    epc_parser_error_free(original_furthest_error);

    epc_cpt_node_t * node = epc_node_alloc(self, "optional");
    if (node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for optional node", self->name, "N/A");
    }

    char const * input = ctx->input_start + input_offset;

    node->content = input;
    node->len = 0;

    return epc_parser_success_result(node);
}

EASY_PC_API epc_parser_t *
epc_optional(char const * name, epc_parser_t * p_to_make_optional)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "optional_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = poptional_parse_fn;
    p->data.other = p_to_make_optional;
    return p;
}

static epc_parse_result_t
plookahead_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "lookahead", "EOF");
    }

    epc_parser_t * child_parser = self->data.other;

    if (child_parser == NULL) // Should not happen if grammar is well-formed
    {
        return epc_parser_error_result(ctx, input_offset, "p_lookahead received NULL child parser", self->name, "NULL");
    }

    epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx);
    epc_parse_result_t child_result = parse(child_parser, ctx, input_offset);

    parser_furthest_error_restore(ctx, &original_furthest_error);

    if (child_result.is_error)
    {
        // Child failed, p_lookahead fails. Propagate the child's error.
        return child_result;
    }

    epc_parser_result_cleanup(&child_result);

    // Child matched, but p_lookahead consumes no input.
    // Return a dummy success node of length 0.
    epc_cpt_node_t * node = epc_node_alloc(self, "lookahead");
    if (node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for lookahead node", self->name, "N/A");
    }

    char const * input = ctx->input_start + input_offset;

    node->content = input;
    node->len = 0;

    return epc_parser_success_result(node);
}

EASY_PC_API epc_parser_t *
epc_lookahead(char const * name, epc_parser_t * p_to_lookahead)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "lookahead_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = plookahead_parse_fn;
    p->data.other = p_to_lookahead;
    return p;
}

static epc_parse_result_t
pnot_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "not", "EOF");
    }

    epc_parser_t * child_parser = (epc_parser_t *)self->data.other;

    if (child_parser == NULL) // Should not happen if grammar is well-formed
    {
        return epc_parser_error_result(ctx, input_offset, "p_not received NULL child parser", self->name, "NULL");
    }

    epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx); // Save before child parse
    epc_parse_result_t child_result = parse(child_parser, ctx, input_offset);

    parser_furthest_error_restore(ctx, &original_furthest_error);

    if (child_result.is_error)
    {
        // Child failed, p_not succeeds.
        epc_parser_result_cleanup(&child_result);
        // Return a dummy success node of length 0.
        epc_cpt_node_t * node = epc_node_alloc(self, "not");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for not node", self->name, "N/A");
        }

        char const * input = ctx->input_start + input_offset;

        node->content = input;
        node->len = 0;

        return epc_parser_success_result(node);
    }

    // Child succeeded, p_not fails.
    // Create a specific error message for p_not.
    char expected_str[64];

    snprintf(expected_str, sizeof(expected_str), "not %s", parser_get_expected_str(ctx, child_parser));

    epc_parse_result_t result =
        epc_parser_error_result(ctx, input_offset, "Parser unexpectedly matched", expected_str, child_result.data.success->content);
    epc_parser_result_cleanup(&child_result);

    return result;
}

EASY_PC_API epc_parser_t *
epc_not(char const * name, epc_parser_t * p_to_not_match)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "not_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pnot_parse_fn;
    p->data.other = p_to_not_match;
    return p;
}

static epc_parse_result_t
pfail_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    char const * failure_message = self->data.string;

    return epc_parser_error_result(ctx, input_offset, failure_message, "Failure", "Failure");
}

EASY_PC_API epc_parser_t *
epc_fail(char const * name, char const * message)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "fail_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pfail_parse_fn;
    char * duplicated_message = strdup(message);
    if (duplicated_message == NULL)
    {
        free(p);
        return NULL;
    }
    p->data.data_type = PARSER_DATA_TYPE_STRING;
    p->data.string = duplicated_message;
    return p;
}

static epc_parse_result_t
psucceed_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    /* We'll say that succeed will succeed even if exactly at end of input. */
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "succeed", "EOF");
    }

    epc_cpt_node_t * node = epc_node_alloc(self, "succeed");
    if (node == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for succeed node", self->name, "N/A");
    }

    char const * input = ctx->input_start + input_offset;

    node->content = input;
    node->len = 0;

    return epc_parser_success_result(node);
}

EASY_PC_API epc_parser_t *
epc_succeed(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "succeed_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = psucceed_parse_fn;

    return p;
}

static epc_parse_result_t
phex_digit_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "hex_digit", "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (isxdigit(input[0]))
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "hex_digit");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }

        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    // else Mismatch
    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Unexpected character", "hex_digit", found_str);
}

EASY_PC_API epc_parser_t *
epc_hex_digit(char const * name)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "hex_digit_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = phex_digit_parse_fn;
    p->expected_value = "hex_digit";

    return p;
}

static epc_parse_result_t
pone_of_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "one_of", "EOF");
    }

    char const * chars_to_match = self->data.string;

    char expected_str[64];
    snprintf(expected_str, sizeof(expected_str), "character in set '%s'", chars_to_match);

    if (input_offset >= ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", expected_str, "EOF");
    }

    char const * input = ctx->input_start + input_offset;

    if (strchr(chars_to_match, input[0]) != NULL) // If char is found in the set
    {
        epc_cpt_node_t * node = epc_node_alloc(self, "one_of");
        if (node == NULL)
        {
            return epc_parser_error_result(ctx, input_offset, "Memory allocation error", self->name, "N/A");
        }
        node->content = input;
        node->len = 1;

        return epc_parser_success_result(node);
    }

    char found_str[2] = { input[0], '\0' };

    return epc_parser_error_result(ctx, input_offset, "Character not found in set", expected_str, found_str);
}

EASY_PC_API epc_parser_t *
epc_one_of(char const * name, char const * chars_to_match)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "one_of_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pone_of_parse_fn;
    char * duplicated_chars = strdup(chars_to_match);
    if (duplicated_chars == NULL)
    {
        free(p);
        return NULL;
    }
    p->data.data_type = PARSER_DATA_TYPE_STRING;
    p->data.string = duplicated_chars;

    return p;
}

static size_t
consume_whitespace(char const *input, bool consume_comments)
{
    if (input == NULL)
    {
        return 0;
    }
    size_t len = 0;
    bool consumed_something;

    do {
        consumed_something = false;

        // Consume standard whitespace
        while (input[len] != '\0' && isspace(input[len]))
        {
            len++;
            consumed_something = true;
        }

        // Consume C++ style single-line comments "//"
        if (consume_comments && input[len] == '/' && input[len+1] == '/')
        {
            len += 2; // Skip "//"
            while (input[len] != '\0' && input[len] != '\n')
            {
                len++; // Skip characters until newline or EOF
            }
            if (input[len] == '\n')
            {
                len++; // Skip the newline character itself
            }
            consumed_something = true;
        }
    } while (consumed_something); // Loop if any whitespace or comment was consumed in this pass

    return len;
}

static epc_parse_result_t
plexeme_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "lexeme", "EOF");
    }

    lexeme_data_t * data = &self->data.lexeme;
    epc_parser_t * child_parser = data->parser;
    bool consume_comments = data->consume_comments;

    if (child_parser == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "epc_lexeme received NULL child parser", self->name, "NULL");
    }

    epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx);
    size_t current_input_offset = input_offset;
    char const * current_input;

    current_input = ctx->input_start + current_input_offset;
    // 1. Consume leading whitespace
    size_t leading_ws_len = consume_whitespace(current_input, consume_comments);
    current_input_offset += leading_ws_len;

    // 2. Parse the actual item
    epc_parse_result_t item_result = parse(child_parser, ctx, current_input_offset);
    if (item_result.is_error)
    {
        epc_parser_error_free(original_furthest_error);
        return item_result; // Propagate item's error
    }
    current_input_offset += item_result.data.success->len;

    // 3. Consume trailing whitespace
    current_input = ctx->input_start + current_input_offset;
    size_t trailing_ws_len = consume_whitespace(current_input, consume_comments);
    current_input_offset += trailing_ws_len;

    // Success - create a node for 'lexeme'
    epc_cpt_node_t * parent_node = epc_node_alloc(self, "lexeme");
    if (parent_node == NULL)
    {
        epc_parser_result_cleanup(&item_result);
        epc_parser_error_free(original_furthest_error);
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for lexeme parent node", self->name, "N/A");
    }

    parent_node->children = calloc(1, sizeof(*parent_node->children));
    if (parent_node->children == NULL)
    {
        epc_parser_result_cleanup(&item_result);
        epc_parser_error_free(original_furthest_error);
        epc_node_free(parent_node);
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for lexeme children array", self->name, "N/A");
    }

    parser_furthest_error_restore(ctx, &original_furthest_error);

    parent_node->children[0] = item_result.data.success; // Only the wrapped result is kept as a child
    parent_node->children_count = 1;

    char const * input = ctx->input_start + input_offset;

    parent_node->content = input;
    parent_node->len = current_input_offset - input_offset;
    parent_node->semantic_start_offset = leading_ws_len;
    parent_node->semantic_end_offset = trailing_ws_len;

    return epc_parser_success_result(parent_node);
}


EASY_PC_API epc_parser_t *
epc_lexeme(char const * name, epc_parser_t * p)
{
    epc_parser_t * lex = epc_parser_allocate(name != NULL ? name : "lexeme_parser");
    if (lex == NULL)
    {
        return NULL;
    }
    lex->parse_fn = plexeme_parse_fn;
    lex->data.data_type = PARSER_DATA_TYPE_LEXEME;
    lex->data.lexeme.parser = p;
    lex->data.lexeme.consume_comments = true;

    return lex;
}

static epc_parse_result_t
pchainl1_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "chainl1", "EOF");
    }

    delimited_data_t * chain_data = &self->data.delimited;
    epc_parser_t * item_parser = chain_data->item;
    epc_parser_t * op_parser = chain_data->delimiter;

    if (item_parser == NULL || op_parser == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "epc_chainl1 received NULL child parser(s)", self->name, "NULL");
    }

    size_t current_input_offset = input_offset;
    epc_parse_result_t left_result;
    epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx);

    // Parse the first item (must succeed)
    left_result = parse(item_parser, ctx, current_input_offset);
    if (left_result.is_error)
    {
        epc_parser_error_free(original_furthest_error); // Cleanup in error path
        return left_result;
    }
    current_input_offset += left_result.data.success->len;

    // Loop to parse (op item) pairs
    while (1)
    {
        epc_parser_error_t * loop_furthest_error = parser_furthest_error_copy(ctx); // Save for loop iteration
        epc_parse_result_t op_result = parse(op_parser, ctx, current_input_offset);
        if (op_result.is_error)
        {
            epc_parser_result_cleanup(&op_result);
            parser_furthest_error_restore(ctx, &loop_furthest_error); // Restore if op fails
            break; // No more operators, chain ends
        }
        epc_parser_error_free(loop_furthest_error); // Operator matched, clear previous furthest error
        current_input_offset += op_result.data.success->len;

        epc_parse_result_t right_result = parse(item_parser, ctx, current_input_offset);
        if (right_result.is_error)
        {
            epc_parser_result_cleanup(&op_result); // op succeeded, but not used in a final success
            epc_parser_result_cleanup(&left_result); // accumulated left part needs to be freed. It's not part of the final CPT.
            epc_parser_error_free(original_furthest_error); // Cleanup in error path
            return right_result; // Item after operator failed, so chain fails
        }
        current_input_offset += right_result.data.success->len;

        // Combine left_result, op_result, and right_result into a new left_result
        epc_cpt_node_t * new_parent_node = epc_node_alloc(self, "chainl1_combined");
        if (new_parent_node == NULL)
        {
            epc_parser_result_cleanup(&op_result);
            epc_parser_result_cleanup(&right_result);
            epc_parser_result_cleanup(&left_result);
            epc_parser_error_free(original_furthest_error); // Cleanup in error path
            return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for chainl1 node", self->name, "N/A");
        }

        new_parent_node->children = calloc(3, sizeof(*new_parent_node->children));
        if (new_parent_node->children == NULL)
        {
            epc_parser_result_cleanup(&op_result);
            epc_parser_result_cleanup(&right_result);
            epc_parser_result_cleanup(&left_result);
            epc_node_free(new_parent_node);
            epc_parser_error_free(original_furthest_error); // Cleanup in error path
            return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for chainl1 children", self->name, "N/A");
        }

        new_parent_node->children[0] = left_result.data.success;
        new_parent_node->children[1] = op_result.data.success;
        new_parent_node->children[2] = right_result.data.success;
        new_parent_node->children_count = 3;

        new_parent_node->content = left_result.data.success->content;
        new_parent_node->len = current_input_offset - input_offset;

        // This becomes the new 'left' result
        left_result = epc_parser_success_result(new_parent_node);
    }

    // Restore furthest error before returning final success
    parser_furthest_error_restore(ctx, &original_furthest_error);

    // Final result is the accumulated left_result
    return left_result;
}

EASY_PC_API epc_parser_t *
epc_chainl1(char const * name, epc_parser_t * item_parser, epc_parser_t * op_parser)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "chainl1_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pchainl1_parse_fn;
    p->data.data_type = PARSER_DATA_TYPE_DELIMITED; // Reusing this for item/op
    p->data.delimited.item = item_parser;
    p->data.delimited.delimiter = op_parser;

    return p;
}

typedef struct {
    epc_cpt_node_t *op_node;
    epc_cpt_node_t *item_node;
} op_item_pair_t;

static epc_parse_result_t
pchainr1_parse_fn(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset)
{
    if (input_offset > ctx->input_len)
    {
        return epc_parser_error_result(ctx, input_offset, "Unexpected end of input", "chainr1", "EOF");
    }

    delimited_data_t * chain_data = &self->data.delimited;
    epc_parser_t * item_parser = chain_data->item;
    epc_parser_t * op_parser = chain_data->delimiter;

    if (item_parser == NULL || op_parser == NULL)
    {
        return epc_parser_error_result(ctx, input_offset, "epc_chainr1 received NULL child parser(s)", self->name, "NULL");
    }

    size_t current_input_offset = input_offset;
    epc_parse_result_t first_item_result;
    epc_parser_error_t * original_furthest_error = parser_furthest_error_copy(ctx); // Declare here

    // Parse the first item (must succeed)
    first_item_result = parse(item_parser, ctx, current_input_offset);
    if (first_item_result.is_error)
    {
        epc_parser_error_free(original_furthest_error); // Cleanup in error path
        return first_item_result;
    }
    current_input_offset += first_item_result.data.success->len;

    // Collect (op item) pairs
    op_item_pair_t *pairs = NULL;
    int pair_count = 0;
    int pair_capacity = 4; // Initial capacity
    pairs = calloc(pair_capacity, sizeof(op_item_pair_t));
    if (pairs == NULL) {
        epc_parser_result_cleanup(&first_item_result); // Cleanup the first item's result
        epc_parser_error_free(original_furthest_error); // Cleanup in error path
        return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for chainr1 pairs", self->name, "N/A");
    }

    while (1)
    {
        epc_parser_error_t * loop_furthest_error = parser_furthest_error_copy(ctx); // Save for loop iteration
        epc_parse_result_t op_result = parse(op_parser, ctx, current_input_offset);
        if (op_result.is_error)
        {
            epc_parser_result_cleanup(&op_result);
            parser_furthest_error_restore(ctx, &loop_furthest_error); // Restore if op fails
            break; // No more operators, chain ends
        }
        epc_parser_error_free(loop_furthest_error); // Operator matched, clear previous furthest error
        current_input_offset += op_result.data.success->len;

        epc_parse_result_t item_result = parse(item_parser, ctx, current_input_offset);
        if (item_result.is_error)
        {
            epc_parser_result_cleanup(&op_result);
            epc_parser_result_cleanup(&first_item_result);
            for(int i = 0; i < pair_count; ++i) {
                epc_node_free(pairs[i].op_node);
                epc_node_free(pairs[i].item_node);
            }
            free(pairs);
            epc_parser_error_free(original_furthest_error); // Cleanup in error path
            return item_result; // Item after operator failed, so chain fails
        }
        current_input_offset += item_result.data.success->len;

        // Store pair
        if (pair_count == pair_capacity) {
            pair_capacity *= 2;
            op_item_pair_t *new_pairs = realloc(pairs, pair_capacity * sizeof(op_item_pair_t));
            if (new_pairs == NULL) {
                epc_parser_result_cleanup(&op_result);
                epc_parser_result_cleanup(&item_result);
                epc_parser_result_cleanup(&first_item_result); // The first item's result
                for(int i = 0; i < pair_count; ++i) {
                    epc_node_free(pairs[i].op_node);
                    epc_node_free(pairs[i].item_node);
                }
                free(pairs);
                epc_parser_error_free(original_furthest_error); // Cleanup in error path
                return epc_parser_error_result(ctx, input_offset, "Memory allocation failure during realloc for chainr1", self->name, "N/A");
            }
            pairs = new_pairs;
        }
        pairs[pair_count].op_node = op_result.data.success;
        pairs[pair_count].item_node = item_result.data.success;
        pair_count++;
    }

    // Build the CPT for right-associativity
    epc_cpt_node_t * final_cpt_node = first_item_result.data.success; // Default if no operators

    // If there are collected (op item) pairs, construct the right-associative tree
    if (pair_count > 0) {
        // The initial right-hand side of the innermost expression is the rightmost item.
        // Example: 1 ^ 2 ^ 3. The innermost is (2 ^ 3). So '3' is the initial right-hand side for (2 ^ 3).
        // The last item from the 'pairs' is the base for the right-hand side construction.
        epc_cpt_node_t * current_right_operand = pairs[pair_count - 1].item_node;

        // Loop backwards from the second-to-last operator/item pair
        // to form the structure: Left_Operand op Right_Subtree
        for (int i = pair_count - 1; i >= 0; --i) {
            epc_cpt_node_t * new_parent_node = epc_node_alloc(self, "chainr1_combined");
            if (new_parent_node == NULL) {
                epc_node_free(current_right_operand);
                // Free any op/item nodes from pairs that haven't been adopted yet
                for (int j = 0; j <= i; ++j) {
                    epc_node_free(pairs[j].op_node);
                    epc_node_free(pairs[j].item_node);
                }
                epc_node_free(first_item_result.data.success); // The initial item
                free(pairs);
                epc_parser_error_free(original_furthest_error);
                return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for chainr1 node", self->name, "N/A");
            }

            epc_cpt_node_t * left_operand_node;
            if (i == 0)
            {
                // For the outermost operation, the left operand is the very first item matched
                left_operand_node = first_item_result.data.success;
            }
            else
            {
                // For inner operations, the left operand is the item from the previous pair (i-1)
                left_operand_node = pairs[i - 1].item_node;
            }
            epc_cpt_node_t * operator_node = pairs[i].op_node;

            new_parent_node->children = calloc(3, sizeof(*new_parent_node->children));
            if (new_parent_node->children == NULL) {
                epc_node_free(current_right_operand);
                epc_node_free(left_operand_node);
                epc_node_free(operator_node);
                // Free any op/item nodes from pairs that haven't been adopted yet
                for (int j = 0; j <= i; ++j) {
                    epc_node_free(pairs[j].op_node);
                    epc_node_free(pairs[j].item_node);
                }
                epc_node_free(first_item_result.data.success);
                epc_node_free(new_parent_node);
                free(pairs);
                epc_parser_error_free(original_furthest_error);
                return epc_parser_error_result(ctx, input_offset, "Memory allocation failure for chainr1 children", self->name, "N/A");
            }

            new_parent_node->children[0] = left_operand_node;
            new_parent_node->children[1] = operator_node;
            new_parent_node->children[2] = current_right_operand;
            new_parent_node->children_count = 3;

            new_parent_node->content = left_operand_node->content;
            new_parent_node->len =
                current_right_operand->content
                + current_right_operand->len
                - left_operand_node->content;

            current_right_operand = new_parent_node; // This newly formed node becomes the right operand for the next outer iteration
        }
        final_cpt_node = current_right_operand; // The fully built right-associative tree
    }

    free(pairs); // Free the array of op_item_pair_t structs, not the nodes they point to

    // Restore furthest error before returning final success
    parser_furthest_error_restore(ctx, &original_furthest_error);

    return epc_parser_success_result(final_cpt_node);
}

EASY_PC_API epc_parser_t *
epc_chainr1(char const * name, epc_parser_t * item_parser, epc_parser_t * op_parser)
{
    epc_parser_t * p = epc_parser_allocate(name != NULL ? name : "chainr1_parser");
    if (p == NULL)
    {
        return NULL;
    }
    p->parse_fn = pchainr1_parse_fn;
    p->data.data_type = PARSER_DATA_TYPE_DELIMITED; // Reusing for item/op
    p->data.delimited.item = item_parser;
    p->data.delimited.delimiter = op_parser;

    return p;
}


static parser_list_t *
parser_list_duplicate(parser_list_t * src)
{
    parser_list_t * l;
    if (src == NULL)
    {
        return NULL;
    }
    l = calloc(1, sizeof(*l));
    if (l == NULL)
    {
        return NULL;
    }
    l->parsers = calloc(src->count, sizeof(*l->parsers));
    if (l->parsers == NULL)
    {
        free(l);
        return NULL;
    }
    for (int i = 0; i < src->count; i++)
    {
        l->parsers[i] = src->parsers[i];
    }
    l->count = src->count;
    return l;
}

void
epc_parser_duplicate(epc_parser_t * const dst, epc_parser_t const * const src)
{
    dst->parse_fn = src->parse_fn;
    dst->ast_config = src->ast_config;
    string_set(&dst->name, src->name);

    parser_data_free(&dst->data);
    dst->data.data_type = src->data.data_type;
    switch (src->data.data_type)
    {
        case PARSER_DATA_TYPE_OTHER:
        case PARSER_DATA_TYPE_CHAR_RANGE:
        case PARSER_DATA_TYPE_COUNT:
        case PARSER_DATA_TYPE_BETWEEN:
        case PARSER_DATA_TYPE_DELIMITED:
        case PARSER_DATA_TYPE_LEXEME:
            dst->data = src->data;
            break;

        case PARSER_DATA_TYPE_STRING:
            dst->data.string = strdup(src->data.string);
            break;

        case PARSER_DATA_TYPE_PARSER_LIST:
            dst->data.parser_list = parser_list_duplicate(src->data.parser_list);
            break;
    }

    if (src->expected_value == src->data.string)
    {
        dst->expected_value = dst->data.string;
    }
    else
    {
        dst->expected_value = src->expected_value;
    }
}

void
epc_parser_set_ast_action(epc_parser_t * p, int action_type)
{
    if (p == NULL)
    {
        return;
    }
    p->ast_config.action = action_type;
    p->ast_config.assigned = true;
}

