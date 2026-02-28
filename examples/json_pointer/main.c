#include "json_pointer.h" // Generated header
#include "json_pointer_actions.h"
#include "json_pointer_ast.h"
#include "json_pointer_ast_actions.h"

#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_json_pointer_ast(json_pointer_node_t * node)
{
    if (node == NULL)
    {
        return;
    }

    switch (node->type)
    {
    case JSON_POINTER_NODE_STRING:
        printf("\"%s\"\n", node->data.string);
        break;
    case JSON_POINTER_NODE_LIST:
        for (json_pointer_list_node_t * curr = node->data.list.head; curr; curr = curr->next)
        {
            print_json_pointer_ast(curr->item);
        }
        break;
    default:
        printf("UNKNOWN NODE TYPE");
        break;
    }
}

int
main(int argc, char ** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <json_pointer_string>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char const * input_string = argv[1];

    epc_parser_list * parser_list = epc_parser_list_create();
    if (parser_list == NULL)
    {
        fprintf(stderr, "Failed to create parser list.\n");
        return EXIT_FAILURE;
    }

    epc_parser_t * json_pointer_parser = create_json_pointer_parser(parser_list);

    if (json_pointer_parser == NULL)
    {
        fprintf(stderr, "Failed to create json_pointer parser.\n");
        epc_parser_list_free(parser_list);
        return EXIT_FAILURE;
    }
    epc_parse_input_t input = {.type = EPC_PARSE_TYPE_STRING, .input_string = input_string};

    epc_compile_result_t compile_result = epc_parse_and_build_ast(
        json_pointer_parser, input, JSON_POINTER_AST_ACTION_COUNT__, json_pointer_ast_hook_registry_init, NULL
    );

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
        epc_compile_result_cleanup(&compile_result, json_pointer_node_free, NULL);
        epc_parser_list_free(parser_list);
        return EXIT_FAILURE;
    }
    else
    {
        printf("Parsing and AST building successful!\n");
        printf("AST:\n");
        print_json_pointer_ast((json_pointer_node_t *)compile_result.ast);

        epc_compile_result_cleanup(&compile_result, json_pointer_node_free, NULL);
        epc_parser_list_free(parser_list);
        return EXIT_SUCCESS;
    }
}
