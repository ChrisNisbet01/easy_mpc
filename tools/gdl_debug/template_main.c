#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>

/* These are provided by the generated code */
epc_parser_t * CREATE_PARSER_FN(epc_parser_list * list);

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_string>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char * input_string = argv[1];

    epc_parser_list * parser_list = epc_parser_list_create();
    if (parser_list == NULL)
    {
        fprintf(stderr, "Failed to create parser list.\n");
        return EXIT_FAILURE;
    }

    epc_parser_t * parser = CREATE_PARSER_FN(parser_list);
    if (parser == NULL)
    {
        fprintf(stderr, "Failed to create parser.\n");
        epc_parser_list_free(parser_list);
        return EXIT_FAILURE;
    }

    epc_parse_session_t session = epc_parse_input(parser, input_string);

    if (session.result.is_error)
    {
        epc_parser_error_t * err = session.result.data.error;
        fprintf(stderr, "Parse Error: %s\n", err->message);
        fprintf(stderr, "At line %zu, col %zu\n", err->line + 1, err->col + 1);
        fprintf(stderr, "Expected: %s\n", err->expected ? err->expected : "unknown");
        fprintf(stderr, "Found: %s\n", err->found ? err->found : "unknown");

        epc_parse_session_destroy(&session);
        epc_parser_list_free(parser_list);
        return EXIT_FAILURE;
    }

    printf("Parsing successful!\n");
    char * cpt_str = epc_cpt_to_string(session.result.data.success);
    if (cpt_str)
    {
        printf("Concrete Parse Tree (CPT):\n%s\n", cpt_str);
        free(cpt_str);
    }

    epc_parse_session_destroy(&session);
    epc_parser_list_free(parser_list);

    return EXIT_SUCCESS;
}
