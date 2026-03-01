# How to use the easy_pc Parser Combinator Library

This document provides a comprehensive guide to using the `easy_pc` library to construct parsers and build Abstract Syntax Trees (ASTs). We'll cover the fundamental concepts of parser combinators, grammar definition, AST construction using semantic actions, and how to debug your parsers with CPT printouts.

This guide uses examples inspired by the `simple_ast_test.cpp` provided with the library.

## Table of Contents

1.  [Introduction to easy_pc](#1-introduction-to-easy_pc)
2.  [Core Concepts](#2-core-concepts)
    *   [Parsers and `epc_parser_t`](#parsers-and-epc_parser_t)
    *   [Parse Context (`epc_parser_ctx_t`)](#parse-context-epc_parser_ctx_t)
    *   [Parse Tree Nodes (`epc_cpt_node_t`)](#parse-tree-nodes-epc_cpt_node_t)
3.  [Building Blocks: Terminal Parsers](#3-building-blocks-terminal-parsers)
    *   [`epc_char`](#epc_char)
    *   [`epc_string`](#epc_string)
    *   [`epc_digit`, `epc_alpha`, `epc_alphanum`, `epc_int`, `epc_double`, `epc_space`](#epc_digit-epc_alpha-epc_alphanum-epc_int-epc_double-epc_space)
4.  [Combining Parsers: Combinators](#4-combining-parsers-combinators)
    *   [`epc_and`](#epc_and)
    *   [`epc_or`](#epc_or)
    *   [`epc_plus` (one or more) and `epc_many` (zero or more)](#epc_plus-one-or-more-and-epc_many-zero-or-more)
    *   [`epc_chainl1` (Left-Associative Chain) and `epc_chainr1` (Right-Associative Chain)](#epc_chainl1-left-associative-chain-and-epc_chainr1-right-associative-chain)
    *   [`epc_skip`](#epc_skip)
    *   [`epc_eoi` (End Of Input)](#epc_eoi-end-of-input)
5.  [Defining Your Grammar](#5-defining-your-grammar)
6.  [Abstract Syntax Tree (AST) Construction with Semantic Actions](#6-abstract-syntax-tree-ast-construction-with-semantic-actions)
    *   Purpose of Semantic Actions
    *   `epc_parser_set_ast_action`
    *   Example AST Actions (`ast_action_type_t`)
    *   Implementing `enter_node` and `exit_node`
7.  [Parsing Input (`epc_parse_str`)](#7-parsing-input-epc_parse_str)
8.  [Traversing the CPT/AST with `epc_cpt_visit_nodes`](#8-traversing-the-cptast-with-epc_cpt_visit_nodes)
9.  [Debugging with CPT Printouts (`epc_cpt_to_string`)](#9-debugging-with-cpt-printouts-epc_cpt_to_string)
10. [Full Example: Simple Arithmetic Parser](#10-full-example-simple-arithmetic-parser)
11. [Streaming Input](#11-streaming-input)

---

## 1. Introduction to easy_pc

`easy_pc` is a C library for building parsers using the parser combinator paradigm. This approach allows you to construct complex parsers by combining simpler, well-defined parsing functions. It's particularly useful for creating domain-specific languages (DSLs), configuration file readers, or interpreters.

The library provides tools for:
*   Defining basic "terminal" parsers (e.g., characters, strings, digits).
*   Combining these terminals into more complex grammar rules (e.g., sequences, alternatives, repetitions).
*   Building a Concrete Parse Tree (CPT) and then an Abstract Syntax Tree (AST).
*   Error reporting and debugging.

## 2. Core Concepts

Before diving into code, let's understand the key data structures and concepts in `easy_pc`.

### Parsers and `epc_parser_t`

At the heart of `easy_pc` is the `epc_parser_t` struct. Each `epc_parser_t` instance represents a single parsing rule. This rule is implemented by a function pointer (`parse_fn`) that takes the current parsing context and input string, and attempts to match a pattern.

An `epc_parser_t` can be a terminal parser (like `epc_char` for a single character) or a combinator parser (like `epc_and` for a sequence of parsers).

### Parse Context (`epc_parser_ctx_t`)

The `epc_parser_ctx_t` struct holds transient state for a single parsing operation. This includes:
*   Pointers to the original input string for error reporting.
*   A `furthest_error` tracker to pinpoint the most significant parsing error.

```c
// Defined in easy_pc_private.h
struct epc_parser_ctx_t{
    const char* input_start;        // Original start of input for error reporting
    epc_parser_error_t* furthest_error; // Tracks the furthest parsing error
};
```

### Parse Tree Nodes (`epc_cpt_node_t`)

When a parser successfully matches a portion of the input, it typically produces an `epc_cpt_node_t` (Parse Tree Node). These nodes form the Concrete Parse Tree (CPT), which is a hierarchical representation of the input string according to your grammar rules.

```c
// Defined in parser_types.h
struct epc_cpt_node_t {
    const char* tag;           // A short identifier for the node type (e.g., "char", "string", "and")
    const char* name;          // The name of the parser that created this node
    const char* content;       // Pointer to the matched substring in the original input
    size_t len;                // Length of the matched substring
    epc_cpt_node_t** children; // Array of child nodes
    int children_count;        // Number of child nodes
    epc_ast_semantic_action_t ast_config; // A copy of the ast action assigned to the associated parser that created the node.
};
```

## 3. Building Blocks: Terminal Parsers

Terminal parsers are the most basic parsers; they match specific characters or sequences directly from the input stream. They don't have child parsers.

### `epc_char`

Matches a single, specific character.

```c
// Matches the character 'a'
epc_parser_t* p_a = epc_char_l(list, "a", 'a');
// On success, creates an epc_cpt_node_t with tag "char", name "char", content "a", len 1.
```

### `epc_string`

Matches an exact sequence of characters (a string literal).

```c
// Matches the string "hello"
epc_parser_t* p_hello = epc_string_l(list, "hello_string", "hello");
// On success, creates an epc_cpt_node_t with tag "string", name "string", content "hello", len 5.
```

### `epc_digit`, `epc_alpha`, `epc_alphanum`, `epc_int`, `epc_double`, `epc_space`

These are convenience parsers for common patterns:

*   `epc_digit()`: Matches any single digit character (`0`-`9`). Tag: `"digit"`.
*   `epc_alpha()`: Matches any single alphabetic character (`a`-`z`, `A`-`Z`). Tag: `"alpha"`.
*   `epc_alphanum()`: Matches any single alphanumeric character (`a`-`z`, `A`-`Z`, `0`-`9`). Tag: `"alphanum"`.
*   `epc_int()`: Matches an integer number (e.g., "123", "-45"). Tag: `"integer"`.
*   `epc_double()`: Matches a floating-point number (e.g., "3.14", "-.5", "1e-3"). Tag: `"double"`.
*   `epc_space()`: Matches any single whitespace character (space, tab, newline, etc.). Tag: `"space"`.

## 4. Combining Parsers: Combinators

Combinators are functions that take one or more parsers as arguments and return a new parser. They are the core mechanism for building complex grammars.

### `epc_and` (Sequence)

`epc_and` attempts to match a sequence of parsers in order. If all child parsers succeed, `epc_and` succeeds and its resulting `epc_cpt_node_t` will have children corresponding to the successful matches of its child parsers.

```c
// Matches "abc"
epc_parser_t* p_a = epc_char_l(list, "a", 'a');
epc_parser_t* p_b = epc_char_l(list, "b", 'b');
epc_parser_t* p_c = epc_char_l(list, "c", 'c');
epc_parser_t* p_abc = epc_and(3, p_a, p_b, p_c);
// On success, p_abc node will have tag "and", content "abc", len 3, and three children ("a", "b", "c").
```

### `epc_or` (Alternative)

`epc_or` attempts to match one of several alternative parsers. It tries them in the order they are provided. The first child parser that succeeds determines the result of `epc_or`.

```c
// Matches either 'x' or 'y'
epc_parser_t* p_x = epc_char_l(list, "x", 'x');
epc_parser_t* p_y = epc_char_l(list, "y", 'y');
epc_parser_t* p_x_or_y = epc_or(2, p_x, p_y);
// On success, p_x_or_y node will have tag "or", content "x" (or "y"), len 1, and one child (the successful char node).
```

### `epc_plus` (one or more) and `epc_many` (zero or more)

These combinators handle repetition:

*   `epc_plus(child_parser)`: Matches one or more occurrences of `child_parser`.
*   `epc_many(child_parser)`: Matches zero or more occurrences of `child_parser`.

```c
// Matches one or more digits (e.g., "1", "12", "123")
epc_parser_t* p_digits_plus = epc_plus_l(list, "digits_plus", epc_digit_l(list, "digit_char"));
// On success with "123", p_digits_plus node will have tag "plus", content "123", len 3, and three "digit" children.

// Matches zero or more spaces (e.g., "", " ", "   ")
epc_parser_t* p_spaces_many = epc_many_l(list, "space_many", epc_space_l(list, "space_char"));
// On success with "   ", p_spaces_many node will have tag "many", content "   ", len 3, and three "space" children.
// On success with "", p_spaces_many node will have tag "many", content "", len 0, and zero children.
```

### `epc_chainl1` (Left-Associative Chain) and `epc_chainr1` (Right-Associative Chain)

These combinators are specifically designed for parsing sequences of `item`s separated by `op`erators, forming left- or right-associative structures, commonly used for arithmetic expressions.

*   `epc_chainl1(name, item_parser, operator_parser)`: Matches one or more `item_parser`s, separated by `operator_parser`, applying `operator_parser` left-associatively. This is ideal for operators like `+`, `-`, `*`, `/`.

*   `epc_chainr1(name, item_parser, operator_parser)`: Matches one or more `item_parser`s, separated by `operator_parser`, applying `operator_parser` right-associatively. This is ideal for operators like `^` (exponentiation).

**Example: Left-Associative Arithmetic Expression**

```c
// Assume p_factor parses numbers or parenthesized expressions
// Assume p_mul_div_op parses '*' or '/'
// Assume p_add_sub_op parses '+' or '-'

// term = factor (mul_div_op factor)*
// This will parse "2 * 3 / 4" as ((2 * 3) / 4)
epc_parser_t* p_term = epc_chainl1_l(list, "term", p_factor, p_mul_div_op);

// expr = term (add_sub_op term)*
// This will parse "1 + 2 - 3" as ((1 + 2) - 3)
epc_parser_t* p_expr = epc_chainl1_l(list, "expr", p_term, p_add_sub_op);
```

**Example: Right-Associative Exponentiation**

```c
// Assume p_base parses a number
// Assume p_exp_op parses '^'

// power = base (exp_op power)*
// This will parse "2 ^ 3 ^ 4" as (2 ^ (3 ^ 4))
epc_parser_t* p_power = epc_chainr1_l(list, "power", p_base, p_exp_op);
```

### `epc_skip`

`epc_skip` is a special repetition combinator designed for ignoring whitespace or comments. It repeatedly applies its child parser and discards all matched content, effectively consuming input without producing any meaningful `epc_cpt_node_t` children. Its own node will have `tag "skip"` and `len` equal to the total skipped length.

```c
// Skips any amount of whitespace
epc_parser_t* p_ws = epc_skip_l(list, "ws_skip", epc_space_l(list, "space_char"));
// If input is "   hello", p_ws succeeds, consumes "   ", and produces a node with len 3 but no children.
// The next parser would then attempt to match "hello".
```

### `epc_eoi` (End Of Input)

`epc_eoi` matches the exact end of the input string. It's crucial for ensuring that your parser consumes *all* expected input and doesn't leave any unparsed characters.

```c
// Matches a complete "hello" string
epc_parser_t* p_full_hello = epc_and(2, epc_string_l(list, "hello_str", "hello"), epc_eoi_l(list, "eoi_marker"));
// p_full_hello succeeds for "hello" but fails for "hello world" because " world" remains.
```

## 5. Defining Your Grammar

A grammar in `easy_pc` is essentially a collection of `epc_parser_t` objects, some of which are terminal parsers and others are combinators that refer to other parsers within the same grammar.

The typical workflow involves:

2.  **Define your grammar rules using `epc_parser_t` and combinators**: This often involves creating mutually recursive parsers, which means you'll declare parser pointers first, and then assign their definitions.

    ```c
    epc_parser_t *p_expr, *p_term, *p_factor, *p_number;
    epc_parser_t *p_plus_op, *p_minus_op, *p_times_op, *p_divide_op;
    epc_parser_t *p_open_paren, *p_close_paren, *p_whitespace;

    // Define terminals
    p_number = epc_double_l(list, "number_double"); // Or epc_int_l, depending on your needs
    p_plus_op = epc_char_l(list, "plus_op", '+');
    p_minus_op = epc_char_l(list, "minus_op", '-');
    p_times_op = epc_char_l(list, "times_op", '*');
    p_divide_op = epc_char_l(list, "divide_op", '/');
    p_open_paren = epc_char_l(list, "open_paren", '(');
    p_close_paren = epc_char_l(list, "close_paren", ')');
    p_whitespace = epc_skip_l(list, "whitespace_skip", epc_space_l(list, "space_char")); // Skip all whitespace

    // Define higher-level rules (example of simple arithmetic grammar)
    // p_factor = epc_or(2,
    //     p_number,
    //     epc_and(3, p_open_paren, p_expr, p_close_paren) // Not quite right for AST building, needs adjustment
    // );
    // ... more complex rules
    ```

3.  **Set the top-level parser**: This is the `epc_parser_t` that represents the entire language you want to parse.

    ```c
    // epc_parse_str will use p_expr as the starting point.
    // epc_parse_str(p_expr, "1 + 2 * 3");
    ```

## 6. Abstract Syntax Tree (AST) Construction with Semantic Actions

The CPT (Concrete Parse Tree) directly reflects your grammar rules, including intermediate steps and insignificant tokens (like whitespace if not skipped). An AST (Abstract Syntax Tree) is a simplified, more abstract representation that captures only the essential structural and semantic information of the input.

`easy_pc` uses **semantic actions** to transform CPT nodes into AST nodes. Each `epc_parser_t` has an `ast_config` field, which includes an `action` type. When `epc_cpt_visit_nodes` is used with a custom visitor, these actions guide the AST construction logic.

### Purpose of Semantic Actions

Semantic actions serve several purposes:
*   **Node Creation**: Decide when and how to create an AST node from an CPT node (e.g., creating an `AST_NUMBER` node from an `epc_int` match).
*   **Node Transformation**: Modify an AST node based on CPT children (e.g., building a binary expression from an operator and its operands).
*   **Node Suppression/Flattening**: Ignore certain CPT nodes in the AST (e.g., whitespace, parentheses) or absorb the properties of a child node.

### `epc_parser_set_ast_action`

This function assigns a specific semantic action to an `epc_parser_t`.

```c
void epc_parser_set_ast_action(epc_parser_t* p, int action_type);
```

**Example:**

```c
epc_parser_t* p_number_parser = epc_int_l(list, "number");
epc_parser_set_ast_action(p_number_parser, AST_ACTION_CREATE_NUMBER_FROM_CONTENT);
// When p_number_parser succeeds, the visitor will be told to create a number AST node.
```

### Example AST Actions (`ast_action_type_t`)

The `simple_ast_builder.h` defines example actions for a simple arithmetic AST:

```c
typedef enum {
    AST_ACTION_NONE,                     // No specific AST action
    AST_ACTION_CREATE_NUMBER_FROM_CONTENT, // Create AST_TYPE_NUMBER from parser content (long long)
    AST_ACTION_CREATE_OPERATOR_FROM_CHAR,  // Create AST_TYPE_OPERATOR from parser content (char)
    AST_ACTION_COLLECT_CHILD_RESULTS,    // Collect all AST results from successful children into a list
    AST_ACTION_BUILD_BINARY_EXPRESSION,  // Build binary expression from (left, op_list, right_list)
    AST_ACTION_PROMOTE_LAST_CHILD_AST,   // For structural nodes that just pass through one child's AST
    AST_ACTION_CREATE_FUNCTION_NAME,     // For epc_string("sin"), epc_string("pow")
    AST_ACTION_CREATE_FUNCTION_CALL,     // For function_call = name '(' args ')'
    AST_ACTION_ASSIGN_ROOT,
} ast_action_type_t;
```
**Note:** You would define similar `enum` values tailored to *your* specific AST structure and needs. These are just examples.

### Implementing `enter_node` and `exit_node`

To build the AST, you provide callback functions (`enter_node` and `exit_node`) to an `epc_cpt_visitor_t` struct, which is then used with `epc_cpt_visit_nodes`. These functions are called as the CPT is traversed.

*   `enter_node(epc_cpt_node_t* pt_node, void* user_data)`: Called when the traversal enters a CPT node.
*   `exit_node(epc_cpt_node_t* pt_node, void* user_data)`: Called when the traversal exits a CPT node. This is typically where AST nodes are constructed or manipulated, often using a stack-based approach.

The `user_data` argument allows you to pass state (e.g., an AST builder context with a stack) between these calls. Inside these functions, you inspect `epc_node_config_get(pt_node).action` to decide what AST operation to perform.

**Example (simplified from `simple_ast_builder.c`):**

```c
// Example user data for AST building
typedef struct {
    SimpleAstNode* ast_root;
    SimpleAstNode* ast_stack[256];
    size_t stack_size;
} MyAstBuilderData;

void my_ast_builder_enter_node(epc_cpt_node_t* pt_node, void* user_data) {
    MyAstBuilderData* data = (MyAstBuilderData*)user_data;
    // Potentially push a placeholder or prepare for child processing
}

void my_ast_builder_exit_node(epc_cpt_node_t* pt_node, void* user_data) {
    MyAstBuilderData* data = (MyAstBuilderData*)user_data;
    SimpleAstNode* new_ast_node = NULL;

    switch (epc_node_config_get(pt_node).action) {
        case AST_ACTION_CREATE_NUMBER_FROM_CONTENT: {
            long long value = strtoll(pt_node->content, NULL, 10);
            new_ast_node = simple_ast_node_create(AST_TYPE_NUMBER);
            new_ast_node->val.ll = value;
            break;
        }
        case AST_ACTION_CREATE_OPERATOR_FROM_CHAR: {
            new_ast_node = simple_ast_node_create(AST_TYPE_OPERATOR);
            new_ast_node->val.op = pt_node->content[0];
            break;
        }
        case AST_ACTION_BUILD_BINARY_EXPRESSION: {
            // Pop children from stack, build binary node, push result
            // This requires careful stack management based on epc_and children structure
            break;
        }
        case AST_ACTION_PROMOTE_LAST_CHILD_AST: {
            // Pop the last child's AST node and use it directly, discarding the current pt_node's context
            if (data->stack_size > 0) {
                new_ast_node = data->ast_stack[--data->stack_size];
            }
            break;
        }
        // ... handle other actions
    }

    if (new_ast_node) {
        data->ast_stack[data->stack_size++] = new_ast_node;
        if (epc_node_config_get(pt_node).action == AST_ACTION_ASSIGN_ROOT) {
            data->ast_root = new_ast_node;
        }
    }
}
```

## 7. Parsing Input (`epc_parse_str`)

Once your grammar is defined and you have an input string, you can use `epc_parse_str` to initiate the parsing process. For convenience, several wrappers are available:

```c
// For static null-terminated strings
epc_parse_session_t epc_parse_str(epc_parser_t * top_parser, char const * input_string);

// For streaming from a Linux file descriptor (sockets, pipes, etc.)
epc_parse_session_t epc_parse_fd(epc_parser_t * top_parser, int fd);
```

The function returns an `epc_parse_session_t`, which contains the `epc_parse_result_t` (either a successful CPT root node or an error) and an internal context for cleanup.

```c
epc_parse_session_t session = epc_parse_str(p_full_expression, "1 + 2 * 3");

if (!session.result.is_error) {
    epc_cpt_node_t* cpts_root = session.result.data.success;
    // Process CPT, build AST
} else {
    epc_parser_error_t* error = session.result.data.error;
    fprintf(stderr, "Parsing Error: %s at '%.10s' (expected '%s', found '%s')\n",
            error->message, error->input_position, error->expected, error->found);
}

// Always remember to clean up the session!
epc_parse_session_destroy(&session);
```

## 8. Traversing the CPT/AST with `epc_cpt_visit_nodes`

The `epc_cpt_visit_nodes` function allows you to traverse the generated CPT (and indirectly build your AST). It takes a root `epc_cpt_node_t` and an `epc_cpt_visitor_t` struct containing `enter_node` and `exit_node` callbacks, along with user data.

```c
void epc_cpt_visit_nodes(epc_cpt_node_t* root, epc_cpt_visitor_t* visitor);

// Inside your parsing logic:
MyAstBuilderData builder_data;
simple_ast_builder_init(&builder_data); // Initialize your AST builder

epc_cpt_visitor_t ast_builder_visitor = {
    .enter_node = my_ast_builder_enter_node,
    .exit_node = my_ast_builder_exit_node,
    .user_data = &builder_data
};

epc_cpt_visit_nodes(cpts_root, &ast_builder_visitor);
SimpleAstNode* final_ast = builder_data.ast_root;
```

## 9. Debugging with CPT Printouts (`epc_cpt_to_string`)

`easy_pc` provides a utility function `epc_cpt_to_string` to visualize the Concrete Parse Tree, which is invaluable for debugging your grammar rules.

```c
// Function to print a Concrete Parse Tree (CPT) to a string.
char* epc_cpt_to_string(epc_parser_ctx_t* ctx, epc_cpt_node_t* node, int indent_level);
```

**Usage Example:**

```c
if (!session.result.is_error) {
    epc_cpt_node_t* cpts_root = session.result.data.success;
    char* cpt_output = epc_cpt_to_string(session.internal_parse_ctx, cpts_root, 0);
    printf("--- CPT ---\n%s\n", cpt_output);
    free(cpt_output);
}
```

**Example Output of CPT for "1 + 2 * 3":**

(Note: Actual output will depend on your specific grammar definition for an arithmetic expression, including whitespace handling. This is a hypothetical simplified example.)

```
--- CPT ---
epc_and (expression_parser) @ '1 + 2 * 3' (len 9)
  epc_plus (expression_parser) @ '1 + 2 * 3' (len 9)
    integer (int_parser) @ '1' (len 1)
    space (space_parser) @ ' ' (len 1)
    char (char_parser) @ '+' (len 1)
    space (space_parser) @ ' ' (len 1)
    epc_and (term_parser) @ '2 * 3' (len 5)
      many (term_parser) @ '2 * 3' (len 5)
        integer (int_parser) @ '2' (len 1)
        space (space_parser) @ ' ' (len 1)
        char (char_parser) @ '*' (len 1)
        space (space_parser) @ ' ' (len 1)
        integer (int_parser) @ '3' (len 1)
```

## 10. Full Example: Simple Arithmetic Parser

This section would typically contain a complete, runnable C code example demonstrating all the concepts discussed, similar in scope to `simple_ast_test.cpp` but explained step-by-step. Due to the length constraints of this document, we'll provide a conceptual outline.

**Conceptual Outline of `simple_ast_test.cpp` inspired example:**

1.  **Includes**: `easy_pc/parser_types.h`, `easy_pc.h`, `simple_ast.h`, `simple_ast_builder.h`, `string.h`, `stdio.h`, `stdlib.h`.
3.  **AST Node Creation Helpers**: `simple_ast_node_create`, `simple_ast_print`.
4.  **AST Builder Setup**: `SimpleAstBuilderData`, `simple_ast_builder_init`, `simple_ast_builder_enter_node`, `simple_ast_builder_exit_node`.
5.  **Grammar Definition**:
    *   Define all terminal parsers (`epc_double`, `epc_char` for operators, `epc_space`, `epc_eoi`).
    *   Define combinator parsers using `epc_and`, `epc_or`, `epc_plus`, `epc_many`.
    *   Crucially, define recursive rules like `expression = term ( ( '+' | '-' ) term )*` and `term = factor ( ( '*' | '/' ) factor )*`.
    *   Assign `epc_parser_set_ast_action` to relevant parsers (e.g., `AST_ACTION_CREATE_NUMBER_FROM_CONTENT` for numbers, `AST_ACTION_CREATE_OPERATOR_FROM_CHAR` for operators, `AST_ACTION_BUILD_BINARY_EXPRESSION` for combining terms and factors).
6.  **Parsing Function**:
    *   Call `epc_parse_str` top-level parser and input string.
7.  **CPT Visualization**:
    *   If parsing succeeds, call `epc_cpt_to_string` to print the CPT.
8.  **AST Building**:
    *   Initialize `epc_cpt_visitor_t` with `simple_ast_builder_enter_node` and `simple_ast_builder_exit_node`.
    *   Call `epc_cpt_visit_nodes` on the CPT root to build the AST.
9.  **AST Visualization/Evaluation**:
    *   Print or evaluate the resulting `SimpleAstNode*`.
10. **Cleanup**:
    *   Call `epc_parse_session_destroy`
    *   Free up all parsers that were created during the construction of the grammar.

By following this structure and filling in the detailed parser definitions and AST action logic, you can create powerful and flexible parsers using `easy_pc`.

## 11. Streaming Input

Starting with version 1.1, `easy_pc` supports streaming input from Linux file descriptors (e.g., sockets, pipes, character devices). This allows you to parse data as it arrives over a network or from another process without waiting for the entire input to be available in memory.

### How it Works

Streaming is implemented using a producer-consumer model:
1.  **Producer (Main Thread)**: When you call `epc_parse_fd()`, the main thread enters a loop that reads from the provided file descriptor and appends data to an internal memory-mapped buffer.
2.  **Consumer (Parsing Thread)**: A dedicated thread is spawned to run the parsing logic. When it needs more data than is currently available, it automatically blocks and waits for the producer to signal that new data has arrived.

### Greedy Parsers and Blocking

Terminal parsers like `epc_int()`, `epc_double()`, and `epc_lexeme()` (which consumes whitespace and comments) are designed to be "streaming-aware." If they encounter the end of the currently available data while in the middle of a token, they will wait for more data to ensure they don't return a partial match (e.g., parsing `123` from a stream that will eventually deliver `12345`).

### Example: Parsing from a Pipe

```c
#include "easy_pc/easy_pc.h"
#include <unistd.h>

void parse_from_pipe(epc_parser_t * grammar) {
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) return;

    // In a real scenario, another process/thread would write to pipe_fds[1]
    
    // This call blocks until the grammar is satisfied, 
    // an error occurs, or the write-end of the pipe is closed (EOF).
    epc_parse_session_t session = epc_parse_fd(grammar, pipe_fds[0]);

    if (!session.result.is_error) {
        printf("Successfully parsed streaming input!\n");
    }

    epc_parse_session_destroy(&session);
    close(pipe_fds[1]);
}
```

### Build-time Configuration

Streaming support requires `pthreads` and is enabled by default. You can exclude it at compile-time to reduce dependencies or binary size:

```bash
# Enable (Default)
cmake -DWITH_INPUT_STREAM_SUPPORT=ON ..

# Disable
cmake -DWITH_INPUT_STREAM_SUPPORT=OFF ..
```

When disabled, the `epc_parse_fd` function and related threading logic are removed from the library.