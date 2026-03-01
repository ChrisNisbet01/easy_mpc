#include "gdl_bootstrap_generator.h"
#include "gdl_code_generator.h"
#include "gdl_compiler_ast_actions.h"
#include "gdl_parser.h"

#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char ** argv)
{
    int exit_code = EXIT_SUCCESS;
    char const * gdl_filepath = NULL;
    char const * output_dir = "."; // Default output directory

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--output-dir", strlen("--output-dir")) == 0)
        {
            char const * arg = argv[i];
            char const * value_start = strchr(arg, '=');
            if (value_start)
            {
                // Handle --output-dir=/path/to/dir
                output_dir = value_start + 1;
            }
            else
            {
                // Handle --output-dir /path/to/dir
                if (i + 1 < argc)
                {
                    output_dir = argv[++i];
                }
                else
                {
                    fprintf(stderr, "Error: --output-dir requires an argument.\n");
                    return EXIT_FAILURE;
                }
            }
        }
        else if (strcmp(argv[i], "--bootstrap-ast") == 0)
        {
            // This flag is handled after parsing, ignore it here.
        }
        else if (gdl_filepath == NULL)
        {
            gdl_filepath = argv[i];
        }
        else
        {
            fprintf(stderr, "Usage: %s <gdl_file> [--output-dir <directory>]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (gdl_filepath == NULL)
    {
        fprintf(stderr, "Usage: %s <gdl_file> [--output-dir <directory>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    epc_parser_list * gdl_parser_list = epc_parser_list_create();
    if (gdl_parser_list == NULL)
    {
        fprintf(stderr, "Failed to create GDL parser list.\n");
        return EXIT_FAILURE;
    }

    // 1. Create the GDL parser
    epc_parser_t * gdl_grammar_parser = create_gdl_parser(gdl_parser_list);
    if (!gdl_grammar_parser)
    {
        fprintf(stderr, "Failed to create GDL grammar parser.\n.");
        epc_parser_list_free(gdl_parser_list);
        return EXIT_FAILURE;
    }

    // 2. Parse the input GDL content
    void * user_ctx = NULL; // No user context needed for this example
    epc_parse_session_t session = epc_parse_file(gdl_grammar_parser, gdl_filepath, user_ctx);

    // 3. Process the result
    if (session.result.is_error)
    {
        fprintf(
            stderr,
            "GDL Parsing Error: %s at input position '%.10s...'\n",
            session.result.data.error->message,
            session.result.data.error->input_position
        );
        fprintf(
            stderr,
            "    Expected %s, found: %s at line %zu, col %zu'\n",
            session.result.data.error->expected,
            session.result.data.error->found,
            session.result.data.error->position.line,
            session.result.data.error->position.col
        );
        exit_code = EXIT_FAILURE;
    }
    else
    {
        printf("GDL parsed successfully! Now building AST...\n");
        // AST Builder setup
        epc_ast_hook_registry_t * ast_registry = epc_ast_hook_registry_create(GDL_AST_ACTION_MAX);

        if (ast_registry == NULL)
        {
            fprintf(stderr, "Error: Failed to create AST hook registry.\n");
            exit_code = EXIT_FAILURE;
        }
        else
        {
            gdl_ast_hook_registry_init(ast_registry, NULL); // No specific user_data needed

            epc_ast_result_t ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

            if (ast_build_result.has_error)
            {
                fprintf(stderr, "GDL AST Building Error: %s\n", ast_build_result.error_message);
                exit_code = EXIT_FAILURE;
            }
            else
            {
                printf("GDL AST built successfully!\n");

                // Call the C code generator
                char * gdl_filename = strrchr(gdl_filepath, '/');
                if (gdl_filename)
                {
                    gdl_filename++;
                }
                else
                {
                    gdl_filename = (char *)gdl_filepath;
                }
                char base_name[256];
                strncpy(base_name, gdl_filename, sizeof(base_name) - 1);
                base_name[sizeof(base_name) - 1] = '\0';

                char * dot = strrchr(base_name, '.');
                if (dot)
                {
                    *dot = '\0';
                }

                if (!gdl_generate_c_code((gdl_ast_node_t *)ast_build_result.ast_root, base_name, output_dir))
                {
                    fprintf(stderr, "C code generation failed.\n");
                    exit_code = EXIT_FAILURE;
                }
                else
                {
                    printf("C code generation completed successfully.\n");
                }

                // Check for bootstrap flag and call the generator
                for (int i = 1; i < argc; ++i)
                {
                    if (strcmp(argv[i], "--bootstrap-ast") == 0)
                    {
                        printf("AST bootstrap files generation requested.\n");
                        generate_ast_bootstrap_files(
                            (gdl_ast_node_t *)ast_build_result.ast_root, base_name, output_dir
                        );
                        break;
                    }
                }

                gdl_ast_node_free((gdl_ast_node_t *)ast_build_result.ast_root, NULL);
            }
        }
        epc_ast_hook_registry_free(ast_registry);
    }

    // 4. Cleanup
    epc_parse_session_destroy(&session);
    epc_parser_list_free(gdl_parser_list);

    return exit_code;
}
