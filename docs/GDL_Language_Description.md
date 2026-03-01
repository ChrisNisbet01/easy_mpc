# GDL Language Description

The Grammar Definition Language (GDL) is a simple domain-specific language used by the `gdl_compiler` tool to define context-free grammars. The `gdl_compiler` processes GDL definitions and generates C source code that uses the `easy_pc` (Easy Parser Combinators) library to build a parser for the defined language.

This document describes the syntax and features of the GDL language, how to integrate the generated code into a CMake project, and provides examples.

## 1. Basic Concepts

A GDL grammar is composed of a series of *rule definitions*. Each rule defines a named parser for a specific part of the language. GDL supports various *terminals* (basic building blocks like characters, strings, or predefined character classes) and *combinators* (functions that combine parsers in different ways, such as sequences, alternatives, or repetitions).

## 2. Lexical Elements

### 2.1 Identifiers

Identifiers are used for rule names and semantic action names. They must start with an alphabet character or an underscore, followed by zero or more alphanumeric characters or underscores.

*   **Syntax:** `(alpha | underscore) (alpha | digit | underscore)*`
*   **Examples:**
    ```gdl
    MyRule = ... ;
    another_identifier = ... ;
    _start_rule = ... ;
    ```

### 2.2 String Literals

String literals are sequences of characters enclosed in double quotes. They can contain escaped double quotes (`"`) and escaped backslashes (``).

*   **Syntax:** `"` `(char | '"' | '')*` `"`
*   **Examples:**
    ```gdl
    Greeting = "hello";
    QuotedString = ""a string"";
    Path = "C:\Program Files";
    ```

### 2.3 Character Literals

Character literals are single characters enclosed in single quotes. They can contain escaped single quotes (`'`), escaped backslashes (``), newline (`
`), tab (`	`), and carriage return (`
`).

*   **Syntax:** `'` `(char | ''' | '' | '
' | '	' | '
')` `'`
*   **Examples:**
    ```gdl
    CharA = 'a';
    SingleQuote = ''';
    NewlineChar = '
';
    ```

### 2.4 Raw Characters

Raw characters are single characters, possibly escaped, not enclosed in quotes. They are typically used within character ranges or certain combinators where a literal character is expected without the overhead of a `char_literal` parser.

*   **Syntax:** A single unreserved character or an escape sequence (e.g., `` followed by any character).
*   **Examples:**
    ```gdl
    // Used in char ranges like: '[' a '-' z ']'
    ```

### 2.5 Number Literals

Number literals are sequences of one or more digits. They are primarily used as arguments for combinators like `count()`.

*   **Syntax:** `digit+`
*   **Examples:**
    ```gdl
    CountFive = count(5, AnyChar);
    ```

## 3. Keywords and Built-in Terminals

GDL provides several predefined keywords that directly map to `easy_pc`'s built-in parsers or combinators. When these keywords are used in a rule definition, the `gdl_compiler` generates the corresponding `epc_xxx_l()` function call.

| Keyword     | Description                                                     | Generated `easy_pc` Call (Example)                        |
| :---------- | :-------------------------------------------------------------- | :-------------------------------------------------------- |
| `alpha`     | Matches any alphabetic character.                               | `epc_alpha_l(list, "alpha")`                              |
| `digit`     | Matches any digit (0-9).                                        | `epc_digit_l(list, "digit")`                              |
| `alphanum`  | Matches any alphanumeric character.                             | `epc_alphanum_l(list, "alphanum")`                        |
| `underscore`| Matches the underscore character `_`.                           | `epc_char_l(list, "underscore", '_')`                     |
| `space`     | Matches any whitespace character.                               | `epc_space_l(list, "space")`                              |
| `hex_digit` | Matches any hexadecimal digit (0-9, a-f, A-F).                  | `epc_hex_digit_l(list, "hex_digit")`                      |
| `int`       | Matches an integer number.                                      | `epc_int_l(list, "int")`                                  |
| `double`    | Matches a floating-point number.                                | `epc_double_l(list, "double")`                            |
| `eoi`       | Matches the End Of Input. Essential for top-level rules.        | `epc_eoi_l(list, "eoi")`                                  |
| `fail`      | Always fails.                                                   | `epc_fail_l(list, "fail")`                                |
| `succeed`   | Always succeeds without consuming input.                        | `epc_succeed_l(list, "succeed")`                          |

## 4. Repetition Operators

GDL supports postfix repetition operators for expressions, directly mapping to `easy_pc`'s repetition combinators.

| Operator | Description                    | Generated `easy_pc` Call (Example for `expr`) |
| :------- | :----------------------------- | :-------------------------------------------- |
| `*`      | Zero or more occurrences.      | `epc_many_l(list, "many", expr)`              |
| `+`      | One or more occurrences.       | `epc_plus_l(list, "plus", expr)`              |
| `?`      | Zero or one occurrence (optional). | `epc_optional_l(list, "optional_rep", expr)`  |

*   **Example:**
    ```gdl
    OptionalA = 'a'?;
    ManyDigits = digit*;
    OneOrMoreWords = Identifier+;
    ```

## 5. Combinators (Function Calls)

GDL provides a rich set of combinators, which are functions that combine simpler parsers into more complex ones. These are invoked using a function-call like syntax.

| Combinator  | Arguments                               | Description                                                                 | Generated `easy_pc` Call (Example)                                          |
| :---------- | :-------------------------------------- | :-------------------------------------------------------------------------- | :-------------------------------------------------------------------------- |
| `lexeme`    | `(expression)`                          | Groups the matched input of `expression` into a single semantic token.      | `epc_lexeme_l(list, "lexeme", expression_parser)`                           |
| `optional`  | `(expression)`                          | Makes `expression` optional. (Equivalent to `expr?`)                        | `epc_optional_l(list, "optional", expression_parser)`                       |
| `count`     | `(number, expression)`                  | Matches `expression` exactly `number` times.                                | `epc_count_l(list, "count", number, expression_parser)`                     |
| `delimited` | `(item_expr, delimiter_expr)`           | Matches `item_expr`s separated by `delimiter_expr`. Returns only `item_expr`s. | `epc_delimited_l(list, "delimited", item_parser, delimiter_parser)`      |
| `between`   | `(open_expr, content_expr, close_expr)` | Matches `open_expr`, then `content_expr`, then `close_expr`.                | `epc_between_l(list, "between", open_parser, content_parser, close_parser)` |
| `chainl1`   | `(item_expr, op_expr)`                  | Matches one or more `item_expr`s separated by `op_expr`, left-associative. | `epc_chainl1_l(list, "chainl1", item_parser, op_parser)`                     |
| `chainr1`   | `(item_expr, op_expr)`                  | Matches one or more `item_expr`s separated by `op_expr`, right-associative. | `epc_chainr1_l(list, "chainr1", item_parser, op_parser)`                    |
| `lookahead` | `(expression)`                          | Succeeds if `expression` matches, but does not consume input.               | `epc_lookahead_l(list, "lookahead", expression_parser)`                     |
| `not`       | `(expression)`                          | Succeeds if `expression` fails, fails if it succeeds. Does not consume input. | `epc_not_l(list, "not", expression_parser)`                               |
| `skip`      | `(expression)`                          | Matches `expression` and consumes input, but does not add to CPT.           | `epc_skip_l(list, "skip", expression_parser)`                               |
| `one_of`    | `(string_literal)`                      | Matches any single character from the provided list string.                 | `epc_one_of_l(list, "one_of", "abc")`                                       |
| `none_of`   | `(string_literal)`                      | Matches any single character NOT from the provided string.                  | `epc_none_of_l(list, "none_of", "abc")`                                     |


*   **Examples:**
    ```gdl
    // Lexemizes an identifier
    Word = lexeme(Identifier);

    // Matches 'a' zero or one times
    OptionalA = optional('a');

    // Matches 3 digits
    ThreeDigits = count(3, digit);

    // Matches a list of Identifiers separated by Commas
    IdList = delimited(Identifier, ',');

    // Matches an expression enclosed in parentheses
    ParenthesizedExpr = between('(', Expression, ')');

    // Matches "hello" then "world", consuming input for both
    HelloWorld = "hello" "world";

    // Matches "foo" OR "bar"
    FooOrBar = "foo" | "bar";
    ```

## 6. Expressions

Expressions combine terminals and combinators to define complex parsing logic.

### 6.1 Character Ranges

A character range matches any single character within a specified ASCII range.

*   **Syntax:** `[` `raw_char` `-` `raw_char` `]`
*   **Example:**
    ```gdl
    LowerCase = ['a'-'z'];
    UpperCase = ['A'-'Z'];
    ```

### 6.2 Primary Expressions

The most basic expressions.
*   `terminal` (string literal, char literal, keyword, identifier referring to another rule)
*   `char_range`
*   `combinator_call`
*   Parenthesized expressions: `(` `DefinitionExpression` `)`
*   `number_literal` (used as an argument to combinators)

### 6.3 Sequences (Implicit `and`)

Multiple expressions placed next to each other form a sequence. All parts must match in order for the sequence to succeed.

*   **Syntax:** `Expression1` `Expression2` ... `ExpressionN`
*   **Generated `easy_pc` Call:** `epc_and_l(list, "seq_label", N, Exp1_parser, Exp2_parser, ...)`
*   **Example:**
    ```gdl
    FullName = FirstName space LastName;
    ```
    (Assuming `FirstName`, `space`, `LastName` are defined rules/terminals)

### 6.4 Alternatives (Explicit `|`)

The `|` operator specifies alternatives. The parser attempts to match the first alternative; if it fails, it tries the next, and so on. The first successful match wins.

*   **Syntax:** `Expression1` `|` `Expression2` `|` ... `|` `ExpressionN`
*   **Generated `easy_pc` Call:** `epc_or_l(list, "alt_label", N, Exp1_parser, Exp2_parser, ...)`
*   **Example:**
    ```gdl
    DigitOrLetter = digit | alpha;
    ```

## 7. Rule Definition

A rule defines a named parser. The `gdl_compiler` will generate a C variable (`epc_parser_t *`) named after the PascalCase version of the rule's identifier.

*   **Syntax:** `Identifier` `=` `DefinitionExpression` `SemanticAction`? `;`

    *   `Identifier`: The name of the rule.
    *   `DefinitionExpression`: The parsing logic for this rule, combining terminals, combinators, and other rules.
    *   `SemanticAction`: An optional action to be associated with the AST node created for this rule. It starts with `@` followed by an `Identifier` (the action name). These actions are then defined as an enum in the generated `_actions.h` file.
    *   `;`: Terminates the rule definition.

*   **Example:**
    ```gdl
    // Defines a rule for a simple word
    Word = alpha+;

    // Defines a rule for a statement with a semantic action
    Statement = "print" StringLiteral @CREATE_PRINT_STATEMENT;
    ```

## 8. Program Structure

A GDL program consists of one or more rule definitions, ending with the `eoi` (End Of Input) keyword, typically as part of the main `Program` rule. The `gdl_compiler` expects the last rule defined in the GDL file to be the top-level grammar rule for the generated `create_LANGUAGE_parser` function.

*   **Example GDL Program (`MyLanguage.gdl`):**
    ```gdl
    // Define a rule for numbers
    Number = digit+;

    // Define a rule for simple arithmetic expressions
    Expression = Number ('+' Number)*;

    // The top-level rule, must match an Expression followed by End Of Input
    Program = Expression eoi @CREATE_PROGRAM_AST;
    ```

## 9. CMake Setup for Code Generation

To integrate GDL code generation into a CMake project, you'll typically use `add_custom_command` to invoke the `gdl_compiler`.

Assume you have `MyLanguage.gdl` in `src/`.

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyProject C)

# Assuming easy_pc library is available in your build system
# (e.g., via add_subdirectory, find_package, or installed)

# Define where the gdl_compiler executable is located
# (Adjust path as necessary based on your project structure)
set(GDL_COMPILER_EXECUTABLE ${CMAKE_BINARY_DIR}/path/to/gdl_compiler)

# Define input GDL file
set(GDL_INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/MyLanguage.gdl)

# Define output directory for generated C files
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Set output file paths
set(GENERATED_HEADER ${GENERATED_DIR}/my_language.h)
set(GENERATED_SOURCE ${GENERATED_DIR}/my_language.c)
set(GENERATED_ACTIONS_HEADER ${GENERATED_DIR}/my_language_actions.h)

# Add custom command to run gdl_compiler
add_custom_command(
    OUTPUT ${GENERATED_HEADER}
           ${GENERATED_SOURCE}
           ${GENERATED_ACTIONS_HEADER}
    COMMAND ${GDL_COMPILER_EXECUTABLE} ${GDL_INPUT_FILE} --output-dir=${GENERATED_DIR}
    DEPENDS ${GDL_INPUT_FILE} ${GDL_COMPILER_EXECUTABLE} # Crucial: re-run if GDL file or compiler changes
    VERBATIM
    COMMENT "Generating parser code for ${GDL_INPUT_FILE}"
)

# Add the generated files as a library to your project
add_library(my_language_generated_parser STATIC ${GENERATED_SOURCE})
target_include_directories(my_language_generated_parser
    PUBLIC
    ${GENERATED_DIR} # To find my_language.h
    ${PROJECT_SOURCE_DIR}/include # To find easy_pc/easy_pc.h
)

# Explicitly ensure gdl_compiler is built before code generation runs
add_dependencies(my_language_generated_parser gdl_compiler)

# Now, use this generated parser library in your application
add_executable(my_app main.c)
target_sources(my_app PRIVATE main.c)
target_link_libraries(my_app PRIVATE my_language_generated_parser easy_pc)
target_include_directories(my_app PRIVATE ${PROJECT_SOURCE_DIR}/include ${GENERATED_DIR})
```

## 10. Using Generated Parser in a C Project

Once you have generated the C parser files (e.g., `my_language.h`, `my_language.c`, `my_language_actions.h`), you can use them in your C application.

**`main.c` example:**

```c
#include "my_language.h" // Include your generated parser header
#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define your semantic actions if you have any
// (These would correspond to the enum in my_language_actions.h)
// For a simple parser, you might not need custom actions initially.

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_string>
", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_string = argv[1];

    epc_parser_list *parser_list = epc_parser_list_create();
    if (parser_list == NULL) {
        fprintf(stderr, "Failed to create parser list.
");
        return EXIT_FAILURE;
    }

    // Create the parser for your language
    epc_parser_t *my_language_parser = create_my_language_parser(parser_list);

    if (my_language_parser == NULL) {
        fprintf(stderr, "Failed to create parser for MyLanguage.
");
        epc_parser_list_free(parser_list);
        return EXIT_FAILURE;
    }

    // Attempt to parse the input string
    epc_parse_session_t session = epc_parse_str(my_language_parser, input_string);

    int exit_code = EXIT_SUCCESS;

    if (session.result.is_error) {
        fprintf(stderr, "Parsing Error for '%s': %s at input position '%.10s...'
",
                input_string,
                session.result.data.error->message,
                session.result.data.error->input_position);
        fprintf(stderr, "    Expected %s, found: %s at column %u
",
                session.result.data.error->expected,
                session.result.data.error->found,
                session.result.data.error->col);
        exit_code = EXIT_FAILURE;
    } else {
        printf("Successfully parsed input: '%s'
", input_string);
        // If your grammar builds an AST, you would access it here:
        // epc_cpt_node_t *parse_tree = session.result.data.success;
        // ... then traverse or process the parse_tree ...
    }

    // Clean up resources
    epc_parse_session_destroy(&session);
    epc_parser_list_free(parser_list);

    return exit_code;
}
