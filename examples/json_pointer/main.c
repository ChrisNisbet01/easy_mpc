#include "json_pointer.h" // Generated header
#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <json_pointer_string>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char * input_string = argv[1];

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

    epc_parse_session_t session = epc_parse_input(json_pointer_parser, input_string);

    int exit_code = EXIT_SUCCESS;

    if (session.result.is_error)
    {
        fprintf(stderr, "Parsing Error for '%s': %s at input position '%.10s...'\n",
                input_string,
                session.result.data.error->message,
                session.result.data.error->input_position);
        fprintf(stderr, "    Expected %s, found: %s at column %u\n",
                session.result.data.error->expected,
                session.result.data.error->found,
                session.result.data.error->col);
        exit_code = EXIT_FAILURE;
    }
    else
    {
        printf("Successfully parsed JSON Pointer: '%s'\n", input_string);
        char* cpt_str = epc_cpt_to_string(session.result.data.success);
        if (cpt_str) {
            printf("CPT:\n%s\n", cpt_str);
            free(cpt_str);
        }
    }

    epc_parse_session_destroy(&session);
    epc_parser_list_free(parser_list);

    return exit_code;
}
