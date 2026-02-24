#define _GNU_SOURCE
#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These are provided by the generated code */
epc_parser_t * CREATE_PARSER_FN(epc_parser_list * list);

static void
run_parse(epc_parser_t * parser, const char * input_string)
{
    epc_parse_session_t session = epc_parse_input(parser, input_string);

    if (session.result.is_error)
    {
        epc_parser_error_t * err = session.result.data.error;
        fprintf(stderr, "Parse Error: %s\n", err->message);
        fprintf(stderr, "At line %zu, col %zu\n", err->position.line + 1, err->position.col + 1);
        fprintf(stderr, "Expected: %s\n", err->expected ? err->expected : "unknown");
        fprintf(stderr, "Found: %s\n", err->found ? err->found : "unknown");
    }
    else
    {
        printf("Parsing successful!\n");
        char * cpt_str = epc_cpt_to_string(session.result.data.success);
        if (cpt_str)
        {
            printf("Concrete Parse Tree (CPT):\n%s\n", cpt_str);
            free(cpt_str);
        }
    }
    epc_parse_session_destroy(&session);
}

int
main(int argc, char ** argv)
{
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

    if (argc >= 2)
    {
        run_parse(parser, argv[1]);
    }
    else
    {
        printf("Interactive mode. Type input and press Enter. Ctrl+D to exit.\n> ");
        fflush(stdout);

        char * line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, stdin)) != -1)
        {
            /* Remove newline */
            if (read > 0 && line[read - 1] == '\n')
            {
                line[read - 1] = '\0';
            }

            if (strlen(line) > 0)
            {
                run_parse(parser, line);
            }

            printf("\n> ");
            fflush(stdout);
        }
        free(line);
        printf("\nExiting.\n");
    }

    epc_parser_list_free(parser_list);
    return EXIT_SUCCESS;
}
