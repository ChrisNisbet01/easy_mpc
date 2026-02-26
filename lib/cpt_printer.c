#include "easy_pc_private.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal struct for the visitor's user_data
typedef struct
{
    char const * input_start;
    char * buffer;
    size_t current_offset;
    size_t buffer_capacity;

    int indent_level;
} cpt_printer_data_t;

static size_t
num_to_str_len(size_t num)
{
    if (num == 0)
    {
        return 1;
    }
    size_t len = 0;
    size_t temp_num = num;
    while (temp_num > 0)
    {
        temp_num /= 10;
        len++;
    }
    return len;
}

static void
ensure_buffer_capacity(cpt_printer_data_t * data, size_t needed_space)
{
    if (data->current_offset + needed_space > data->buffer_capacity)
    {
        size_t new_capacity = data->buffer_capacity * 2;
        if (new_capacity < data->current_offset + needed_space)
        {
            new_capacity = data->current_offset + needed_space + 1024;
        }
        char * new_buffer = (char *)realloc(data->buffer, new_capacity);
        if (!new_buffer)
        {
            fprintf(stderr, "CPT Printer: Failed to reallocate buffer!\n");
            if (data->buffer)
            {
                free(data->buffer);
            }
            data->buffer = NULL;
            data->current_offset = 0;
            data->buffer_capacity = 0;
            return;
        }
        data->buffer = new_buffer;
        data->buffer_capacity = new_capacity;
    }
}

static void
cpt_printer_enter_node(epc_cpt_node_t * node, void * user_data)
{
    cpt_printer_data_t * data = (cpt_printer_data_t *)user_data;
    // include content and length
    epc_line_col_t position = epc_calculate_line_and_column(data->input_start, node->content);
    char const * scontent = epc_cpt_node_get_semantic_content(node);
    size_t scontent_len = epc_cpt_node_get_semantic_len(node);
    epc_line_col_t sposition = epc_calculate_line_and_column(data->input_start, scontent);
    int required_len;
    char const * node_id = epc_node_id(node);
    size_t estimated_line_len
        = data->indent_level * 4 + strlen(node->tag) + strlen(node_id) + 5 +               // <tag> + (<name>) ()
          (node->content && node->len > 0 ? node->len + 3 : 0) + num_to_str_len(node->len) // 'content'
          + num_to_str_len(position.line) + num_to_str_len(position.col) + 20 + 1;         // (line=X, col=X, len=X)\n

    if (scontent == node->content && scontent_len == node->len)
    {
        scontent = NULL;
        scontent_len = 0;
    }
    if (scontent != NULL && scontent_len > 0)
    {
        estimated_line_len += scontent_len + 3 + num_to_str_len(scontent_len) // 'content'
                              + num_to_str_len(sposition.line) + num_to_str_len(sposition.col) + 20
                              + 1; // (line=X, col=X, len=X)\n
    }

    ensure_buffer_capacity(data, estimated_line_len);
    if (!data->buffer)
    {
        return;
    }

    // Indentation
    for (int i = 0; i < data->indent_level * 4; ++i)
    {
        data->buffer[data->current_offset++] = ' ';
    }

    // Tag: <tag>
    data->buffer[data->current_offset++] = '<';
    required_len = (int)strlen(node->tag);
    memcpy(data->buffer + data->current_offset, node->tag, required_len);
    data->current_offset += required_len;
    data->buffer[data->current_offset++] = '>';

    // Name: (name)
    data->buffer[data->current_offset++] = ' ';
    data->buffer[data->current_offset++] = '(';
    required_len = strlen(node_id);
    memcpy(data->buffer + data->current_offset, node_id, required_len);
    data->current_offset += required_len;
    data->buffer[data->current_offset++] = ')';

    // Content: 'content'
    if (node->content && node->len > 0)
    {
        data->buffer[data->current_offset++] = ' ';
        data->buffer[data->current_offset++] = '\'';
        strncpy(data->buffer + data->current_offset, node->content, node->len);
        data->current_offset += node->len;
        data->buffer[data->current_offset++] = '\'';
    }

    // Line/Col/Length: (line=X, col=X, len=X)
    required_len = snprintf(
        data->buffer + data->current_offset,
        data->buffer_capacity - data->current_offset,
        " (line=%zu, col=%zu, len=%zu)",
        position.line,
        position.col,
        node->len
    );
    if (required_len < 0 || (size_t)required_len >= (data->buffer_capacity - data->current_offset))
    {
        data->current_offset = data->buffer_capacity - 1; // Mark as full to prevent further writes
        data->buffer[data->current_offset] = '\0';        // Null-terminate
        return;
    }
    data->current_offset += required_len;

    if (scontent != NULL && scontent_len > 0)
    {
        // Semantic content: 'scontent'
        data->buffer[data->current_offset++] = ' ';
        data->buffer[data->current_offset++] = '\'';
        strncpy(data->buffer + data->current_offset, scontent, scontent_len);
        data->current_offset += scontent_len;
        data->buffer[data->current_offset++] = '\'';

        // Line/Col/Length: (line=X, col=X, len=X)
        required_len = snprintf(
            data->buffer + data->current_offset,
            data->buffer_capacity - data->current_offset,
            " (line=%zu, col=%zu, len=%zu)",
            sposition.line,
            sposition.col,
            scontent_len
        );
        if (required_len < 0 || (size_t)required_len >= (data->buffer_capacity - data->current_offset))
        {
            data->current_offset = data->buffer_capacity - 1; // Mark as full to prevent further writes
            data->buffer[data->current_offset] = '\0';        // Null-terminate
            return;
        }
        data->current_offset += required_len;
    }

    data->buffer[data->current_offset++] = '\n';

    // Increment indent for children
    data->indent_level++;
}

static void
cpt_printer_exit_node(epc_cpt_node_t * node, void * user_data)
{
    (void)node;
    cpt_printer_data_t * data = (cpt_printer_data_t *)user_data;
    // Decrement indent after children have been visited
    data->indent_level--;
}

// Function to print a Concrete Parse Tree (CPT) to a string.
static char *
epc_cpt_to_string_private(epc_cpt_node_t * node, int initial_indent_level)
{
    if (node == NULL)
    {
        return NULL;
    }

    // Initialize printer data
    cpt_printer_data_t printer_data = {0};
    // Assume that the top node's content points to the start of the input.
    printer_data.input_start = epc_cpt_node_get_content(node);
    printer_data.current_offset = 0;
    printer_data.indent_level = initial_indent_level;

    // Allocate initial buffer
    printer_data.buffer_capacity = 256; // Initial small capacity
    printer_data.buffer = (char *)malloc(printer_data.buffer_capacity);
    if (!printer_data.buffer)
    {
        return NULL;
    }

    // Set up visitor
    epc_cpt_visitor_t printer_visitor = {
        .enter_node = cpt_printer_enter_node,
        .exit_node = cpt_printer_exit_node,
        .user_data = &printer_data,
    };

    // Perform the visit to fill the buffer
    epc_cpt_visit_nodes(node, &printer_visitor);

    // After visit, null-terminate the string
    if (printer_data.buffer)
    {
        ensure_buffer_capacity(&printer_data, 1); // Ensure space for null terminator
        if (printer_data.buffer)
        {
            printer_data.buffer[printer_data.current_offset] = '\0';
        }
    }

    // Allocate final string. User must free the final string.
    char * final_string = NULL;
    if (printer_data.buffer)
    {
        size_t final_len = printer_data.current_offset;
        final_string = malloc(final_len + 1);
        if (final_string)
        {
            strncpy(final_string, printer_data.buffer, final_len);
            final_string[final_len] = '\0';
        }
    }

    free(printer_data.buffer);

    return final_string;
}

char *
epc_cpt_to_string(epc_cpt_node_t * node)
{
    return epc_cpt_to_string_private(node, 0);
}
