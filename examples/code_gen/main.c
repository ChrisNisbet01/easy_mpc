#include "arithmetic_expression.h" // Generated header
#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strlen, strncpy etc. if needed for error messages

int main(int argc, char ** argv) // Modified signature
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char * input_expression = argv[1];

    epc_parser_list * parser_list = epc_parser_list_create();
    if (parser_list == NULL)
    {
        fprintf(stderr, "Failed to create parser list.\n");
        return EXIT_FAILURE;
    }

    epc_parser_t * arithmetic_parser = create_arithmetic_expression_parser(parser_list);

    if (arithmetic_parser == NULL)
    {
        fprintf(stderr, "Failed to create arithmetic parser.\n");
        epc_parser_list_free(parser_list);
        return EXIT_FAILURE;
    }

    // Attempt to parse the input expression
    epc_parse_session_t session = epc_parse_input(arithmetic_parser, input_expression);

    int exit_code = EXIT_SUCCESS;

    if (session.result.is_error)
    {
        fprintf(stderr, "Parsing Error for '%s': %s at input position '%.10s...'\n",
                input_expression,
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
        printf("Successfully parsed expression: '%s'\n", input_expression);
        // In a full application, you would now traverse the CPT or AST
        // For this task, we just confirm parsing success.
    }

    epc_parse_session_destroy(&session);
    epc_parser_list_free(parser_list);

    return exit_code;
}
