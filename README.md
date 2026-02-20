[![C/C++ CI](https://github.com/ChrisNisbet01/easy_pc/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/ChrisNisbet01/easy_pc/actions/workflows/c-cpp.yml)
# easy_pc: A C-based Parser Combinator Library

`easy_pc` is a lightweight, embeddable parser combinator library written in C. It provides a set of small, composable functions (parsers) that can be combined to build more complex parsers. This approach allows for the construction of grammar definitions directly in C code, making it easy to create parsers for various domain-specific languages, configuration files, or other structured text formats.

The library focuses on:
- **Parser Combinators:** Building complex parsing logic from simple, reusable primitives.
- **Concrete Parse Tree (CPT) Generation:** Producing a detailed tree representation of the parsed input, showing how each parser contributed to the match.
- **Abstract Syntax Tree (AST) Support:** Providing mechanisms for attaching semantic actions to parsers, facilitating the transformation of the CPT into a more abstract AST suitable for interpretation or compilation.

## Building the Project

`easy_pc` uses CMake for its build system.

**Prerequisites:**
-   A C compiler (e.g., GCC, Clang)
-   CMake (version 3.10 or higher recommended)

**Steps to build:**

1.  **Clone the repository (if you haven't already):**
    ```bash
    git clone <repository_url>
    cd easy_pc
    ```

2.  **Create a build directory and navigate into it:**
    ```bash
    mkdir build
    cd build
    ```

3.  **Run CMake to configure the project:**
    ```bash
    cmake ..
    ```
    *   For multi-configuration generators (like Visual Studio), you might need to specify a generator:
        `cmake -G "Visual Studio 16 2019" ..`

4.  **Build the project:**
    ```bash
    cmake --build .
    ```
    *   Alternatively, for Unix Makefiles:
        `make`

## AST Builder API

The `easy_pc` library includes a powerful and flexible API for transforming Concrete Parse Trees (CPTs) into Abstract Syntax Trees (ASTs). This callback-driven system simplifies memory management and tree traversal, allowing developers to define custom semantic actions for each grammar rule. This approach facilitates the creation of language-specific ASTs suitable for further processing, interpretation, or compilation.

[Learn more about the AST Builder API here.](AST_BUILDER_API.md)

## Build Options (Examples and Tests)

You can customize the build process using CMake options:

-   `BUILD_EXAMPLES`: Controls whether example executables are built.
    -   Default: `ON`
    -   To disable: `cmake -DBUILD_EXAMPLES=OFF ..`
-   `BUILD_TESTS`: Controls whether unit tests are built.
    -   Default: `ON`
    -   To disable: `cmake -DBUILD_TESTS=OFF ..`

To configure with specific options, run CMake like this from your `build` directory:

```bash
cmake -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=ON ..
```

After building, browse the generated API documentation in `docs/help/html/index.html` (open in any web browser). This includes detailed function descriptions from the header comments.

## Getting Started: Bare Minimum Usage

Here's a minimal example demonstrating how to parse a simple string:

```c
#include "easy_pc/easy_pc.h"
#include <stdio.h>
#include <stdlib.h> // For malloc, free
#include <string.h> // For strlen

int main() {
    const char* input_string = "hello world";
    epc_parser_list * list = epc_parser_list_create();
    if (list == NULL)
    {
        fprintf(stderr, "Failed to create parser list.\n");
        return 1;
    }

    // 1. Define a simple parser (e.g., to match the string "hello") using _l helpers
    epc_parser_t* p_hello = epc_string_l(list, "hello_literal", "hello");
    if (p_hello == NULL)
    {
        fprintf(stderr, "Failed to create 'hello' parser.\n");
        epc_parser_list_free(list);
        return 1;
    }

    // 2. Combine with an End-Of-Input parser to ensure the whole string is matched
    epc_parser_t* p_eoi_marker = epc_eoi_l(list, "eoi_marker");
    if (p_eoi_marker == NULL)
    {
        fprintf(stderr, "Failed to create EOI parser.\n");
        epc_parser_list_free(list);
        return 1;
    }

    epc_parser_t* p_full_match = epc_and(2, p_hello, p_eoi_marker);
    if (p_full_match == NULL)
    {
        fprintf(stderr, "Failed to create 'full_match' parser.\n");
        epc_parser_list_free(list);
        return 1;
    }

    // 3. Parse the input string
    epc_parse_session_t session = epc_parse_input(p_full_match, input_string);

    // 4. Check the result
    if (!session.result.is_error)
    {
        fprintf(stdout, "Parse successful!\n");
        fprintf(stdout, "Matched: '%.*s'\n", (int)session.result.data.success->len, session.result.data.success->content);
        
        // Optionally, print the Concrete Parse Tree (CPT) for debugging
        char* cpt_str = epc_cpt_to_string(session.internal_parse_ctx, session.result.data.success, 0);
        if (cpt_str != NULL)
        {
            fprintf(stdout, "--- CPT ---\n%s\n-----------\n", cpt_str);
            free(cpt_str);
        }
    }
    else
    {
        fprintf(stderr, "Parse failed!\n");
        if (session.result.data.error) {
            fprintf(stderr, "Error: %s at '%.*s' (expected '%s', found '%.*s')\n",
                    session.result.data.error->message,
                    (int)(input_string + strlen(input_string) - session.result.data.error->input_position),
                    session.result.data.error->input_position,
                    session.result.data.error->expected ? session.result.data.error->expected : "N/A",
                    (int)(session.result.data.error->found ? strlen(session.result.data.error->found) : 3), // max 3 for EOF
                    session.result.data.error->found ? session.result.data.error->found : "EOF"
                   );
        }
    }

    int exit_code = session.result.is_error ? 1 : 0;
    // 5. Clean up resources
    epc_parse_session_destroy(&session); // Frees CPT nodes or error
    epc_parser_list_free(list); // Free the parser list

    return exit_code;
}

## Using Parser List Helper Functions (the `_l` functions)

The `easy_pc` library provides convenience helper functions, denoted by an `_l` suffix (e.g., `epc_char_l`, `epc_string_l`), which combine the creation of a parser with automatically adding it to an `epc_parser_list`. This helps manage memory for parsers, especially when building complex grammars with many intermediate parser objects.

All `_l` functions take an `epc_parser_list * list` as their first argument, followed by the arguments of their non-`_l` counterpart.

**Example:**

Instead of:
```c
epc_parser_t* my_char_parser = epc_char("my_char", 'A');
epc_parser_list_add(list, my_char_parser);
```

You can simply use:
```c
epc_parser_t* my_char_parser = epc_char_l(list, "my_char", 'A');
```

## Example Applications

The `easy_pc` library comes with several example applications demonstrating its usage and capabilities. You can explore their source code to understand how to build parsers for different scenarios.

*   **GDL Compiler:** A tool located in `tools/gdl_compiler/` that allows you to define grammars using the Grammar Definition Language (GDL) and automatically generates C parser code. This simplifies the process of creating complex parsers without writing boilerplate `easy_pc` code by hand.
    *   [Learn more about the GDL Language and its usage here.](docs/GDL_Language_Description.md)

*   **Arithmetic Parser:** Located in [examples/arithmetic_parser/](examples/arithmetic_parser/)
    This example demonstrates how to build a parser for simple arithmetic expressions, including support for basic operations (+, -, *, /), parentheses, and integer/double numbers. It also includes an AST builder and evaluator.

*   **JSON Parser:** Located in [examples/json_parser/](examples/json_parser/)
    This example showcases how to create a parser for a subset of the JSON specification. It illustrates parsing complex data structures like objects and arrays, and handling string, number, boolean, and null literals.
