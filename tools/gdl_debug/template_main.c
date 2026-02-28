#define _GNU_SOURCE
#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These are provided by the generated code */
epc_parser_t * CREATE_PARSER_FN(epc_parser_list * list);

static void
run_parse(epc_parser_t * parser, char const * input_string)
{
    epc_parse_session_t session = epc_parse_str(parser, input_string);

    epc_parse_session_print_cpt(stdout, &session);
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
