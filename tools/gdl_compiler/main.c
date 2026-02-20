#include "gdl_parser.h"
#include "gdl_compiler_ast_actions.h"
#include "gdl_code_generator.h"

#include <easy_pc/easy_pc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to read the entire content of a file into a dynamically allocated string
char *
read_file_content(const char * filepath)
{
    FILE * file = fopen(filepath, "rb");
    if (file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char * buffer = (char *)malloc(length + 1);
    if (buffer == NULL)
    {
        perror("Error allocating buffer");
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, length, file);
    if (read_bytes != (size_t)length)
    {
        perror("Error reading file");
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[length] = '\0'; // Null-terminate the string

    fclose(file);
    return buffer;
}

int
main(int argc, char ** argv)
{
    int exit_code = EXIT_SUCCESS;
    const char * gdl_filepath = NULL;
    const char * output_dir = "."; // Default output directory

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--output-dir", strlen("--output-dir")) == 0)
        {
            const char * arg = argv[i];
            const char * value_start = strchr(arg, '=');
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

    char * gdl_content = read_file_content(gdl_filepath);
    if (!gdl_content)
    {
        return EXIT_FAILURE;
    }

    printf("Parsing: '%s'\n", gdl_content);
    epc_parser_list * gdl_parser_list = epc_parser_list_create();
    if (gdl_parser_list == NULL)
    {
        fprintf(stderr, "Failed to create GDL parser list.\n");
        free(gdl_content);
        return EXIT_FAILURE;
    }

    // 1. Create the GDL parser
    epc_parser_t * gdl_grammar_parser = create_gdl_parser(gdl_parser_list);
    if (!gdl_grammar_parser)
    {
        fprintf(stderr, "Failed to create GDL grammar parser.\n.");
        free(gdl_content);
        epc_parser_list_free(gdl_parser_list);
        return EXIT_FAILURE;
    }

    // 2. Parse the input GDL content
    epc_parse_session_t session = epc_parse_input(gdl_grammar_parser, gdl_content);

    // 3. Process the result
    if (session.result.is_error)
    {
        fprintf(stderr, "GDL Parsing Error: %s at input position '%.10s...'\n",
                session.result.data.error->message,
                session.result.data.error->input_position);
        fprintf(stderr, "    Expected %s, found: %s at col %u'\n",
                session.result.data.error->expected,
                session.result.data.error->found,
                session.result.data.error->col);
        exit_code = EXIT_FAILURE;
    }
    else
    {
        printf("GDL parsed successfully! Now building AST...\n");
        // AST Builder setup
        epc_ast_hook_registry_t * ast_registry =
            epc_ast_hook_registry_create(GDL_AST_ACTION_MAX);

        if (ast_registry == NULL)
        {
            fprintf(stderr, "Error: Failed to create AST hook registry.\n");
            exit_code = EXIT_FAILURE;
        }
        else
        {
            gdl_ast_hook_registry_init(ast_registry, NULL); // No specific user_data needed

            epc_ast_result_t ast_build_result =
                epc_ast_build(session.result.data.success, ast_registry, NULL);

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
                gdl_ast_node_free((gdl_ast_node_t *)ast_build_result.ast_root, NULL);
            }
        }
        epc_ast_hook_registry_free(ast_registry);
    }

    // 4. Cleanup
    epc_parse_session_destroy(&session);
    epc_parser_list_free(gdl_parser_list);
    free(gdl_content);

    return exit_code;
}
