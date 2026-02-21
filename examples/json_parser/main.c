#include "json_grammar.h"
#include "json_ast.h"
#include "json_ast_actions.h"
#include "semantic_actions.h"
#include "easy_pc/easy_pc_ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to read content from a file or stdin
static char *
read_input_content(char const * filename)
{
    FILE * fp = stdin;
    long file_size = 0;
    char * buffer = NULL;

    if (filename)
    {
        fp = fopen(filename, "rb");
        if (!fp)
        {
            perror("Failed to open file");
            return NULL;
        }
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        rewind(fp);
    }
    else
    {
        return NULL; // Signal that filename is not provided, main will use getline
    }

    buffer = (char *)malloc(file_size + 1);
    if (!buffer)
    {
        perror("Failed to allocate buffer");
        if (fp != stdin)
        {
            fclose(fp);
        }
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    if (bytes_read != (size_t)file_size)
    {
        perror("Failed to read entire file");
        free(buffer);
        if (fp != stdin)
        {
            fclose(fp);
        }
        return NULL;
    }
    buffer[file_size] = '\0';

    if (fp != stdin)
    {
        fclose(fp);
    }
    return buffer;
}

static void
print_indent(int indent)
{
    printf("%*s", indent * 2, "");
}

static void
print_json_ast(json_node_t * node, int indent, bool newline_and_indent, bool end_with_newline)
{
    if (!node)
    {
        return;
    }
    if (newline_and_indent)
    {
        print_indent(indent);
    }

    switch (node->type)
    {
        case JSON_NODE_NULL:
            printf("null");
            break;
        case JSON_NODE_BOOLEAN:
            printf("%s", node->data.boolean ? "true" : "false");
            break;
        case JSON_NODE_NUMBER:
            printf("%g", node->data.number);
            break;
        case JSON_NODE_STRING:
            printf("\"%s\"", node->data.string);
            break;
        case JSON_NODE_ARRAY:
            printf("[\n");
            for (json_list_node_t * curr = node->data.list.head; curr; curr = curr->next)
            {
                bool is_last_item = curr->next == NULL;
                print_json_ast(curr->item, indent + 1, true, is_last_item);
                if (!is_last_item)
                {
                    printf(",");
                    printf("\n");
                }
            }
            print_indent(indent);
            printf("]");
            break;
        case JSON_NODE_OBJECT:
            printf("{\n");
            for (json_list_node_t * curr = node->data.list.head; curr; curr = curr->next)
            {
                bool is_last_item = curr->next == NULL;
                print_json_ast(curr->item, indent + 1, true, is_last_item);
                if (!is_last_item)
                {
                    printf(",");
                    printf("\n");
                }
            }
            print_indent(indent);
            printf("}");
            break;
        case JSON_NODE_MEMBER:
            printf("\"%s\": ", node->data.member.key);
            print_json_ast(node->data.member.value, indent + 1, false, false);
            break;
        default:
            printf("UNKNOWN NODE TYPE");
            break;
    }

    if (end_with_newline)
    {
        printf("\n");
    }
}

int
main(int argc, char * argv[])
{
    char * input_content = NULL;
    size_t len = 0;
    ssize_t read_bytes;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [json_file_path]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2)
    {
        input_content = read_input_content(argv[1]);
        if (!input_content)
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        printf("JSON parser example. Enter JSON string (or provide filename as arg):\n");
        read_bytes = getline(&input_content, &len, stdin);
        if (read_bytes == -1)
        {
            perror("Error reading from stdin");
            free(input_content);
            return EXIT_FAILURE;
        }
        if (read_bytes > 0 && input_content[read_bytes - 1] == '\n')
        {
            input_content[read_bytes - 1] = '\0';
        }
    }

    epc_parser_list * list = epc_parser_list_create();
    if (!list)
    {
        fprintf(stderr, "Failed to create parser list.\n");
        free(input_content);
        return EXIT_FAILURE;
    }

    epc_parser_t * json_root_parser = create_json_grammar(list);
    if (!json_root_parser)
    {
        fprintf(stderr, "Failed to create JSON grammar.\n");
        epc_parser_list_free(list);
        free(input_content);
        return EXIT_FAILURE;
    }

    epc_compile_result_t compile_result =
        epc_parse_and_build_ast(json_root_parser, input_content,
                                JSON_ACTION_MAX, json_ast_hook_registry_init, NULL);

    if (!compile_result.success)
    {
        if (compile_result.parse_error_message)
        {
            fprintf(stderr, "Parse Error: %s\n", compile_result.parse_error_message);
        }
        if (compile_result.ast_error_message)
        {
            fprintf(stderr, "AST Build Error: %s\n", compile_result.ast_error_message);
        }
        epc_compile_result_cleanup(&compile_result, json_node_free, NULL);
        epc_parser_list_free(list);
        free(input_content);
        return EXIT_FAILURE;
    }
    else
    {
        printf("Parsing and AST building successful!\n");
        printf("AST:\n");
        print_json_ast((json_node_t *)compile_result.ast, 0, true, true);

        epc_compile_result_cleanup(&compile_result, json_node_free, NULL);
        epc_parser_list_free(list);
        free(input_content);
        return EXIT_SUCCESS;
    }
}
