#include "json_grammar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strlen

// Function to read content from a file or stdin
static char *
read_input_content(char const * filename)
{
    FILE * fp = stdin;
    long file_size = 0;
    char * buffer = NULL;

    if (filename)
    {
        fp = fopen(filename, "rb"); // Open in binary mode for size calculation
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
        // For stdin, we can't easily get the size beforehand.
        // Will read line by line or dynamically reallocate.
        // For simplicity, let's assume stdin will be handled by getline.
        // The calling main function will handle this.
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
    buffer[file_size] = '\0'; // Null-terminate the string

    if (fp != stdin)
    {
        fclose(fp);
    }
    return buffer;
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
        // Read from stdin using getline
        read_bytes = getline(&input_content, &len, stdin);
        if (read_bytes == -1)
        {
            perror("Error reading from stdin");
            free(input_content);
            return EXIT_FAILURE;
        }
        // Remove trailing newline if present, getline includes it
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

    // Get the top-level JSON parser
    epc_parser_t * json_root_parser = create_json_grammar(list);
    if (!json_root_parser)
    {
        fprintf(stderr, "Failed to create JSON grammar.\n");
        epc_parser_list_free(list);
        free(input_content);
        return EXIT_FAILURE;
    }

    epc_parse_session_t session = epc_parse_input(json_root_parser, input_content);

    if (session.result.is_error)
    {
        fprintf(stderr, "Parsing error at col %d: %s\n",
                session.result.data.error->col,
                session.result.data.error->message);
        fprintf(stderr, "Expected: `%s`, found: `%s`\n",
                session.result.data.error->expected, session.result.data.error->found);
        epc_parse_session_destroy(&session);
        epc_parser_list_free(list);
        free(input_content);
        return EXIT_FAILURE;
    }
    else
    {
        printf("Parsing successful!\n");
        char * cpt_str = epc_cpt_to_string(session.result.data.success);
        if (cpt_str)
        {
            printf("CPT:\n%s\n", cpt_str);
            free(cpt_str);
        }
        epc_parse_session_destroy(&session);
        epc_parser_list_free(list);
        free(input_content);
        return EXIT_SUCCESS;
    }
}
