#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// For symbol visibility control
#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_EASY_PC
#ifdef __GNUC__
#define EASY_PC_API __attribute__((dllexport))
#else
#define EASY_PC_API __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define EASY_PC_API __attribute__((dllimport))
#else
#define EASY_PC_API __declspec(dllimport)
#endif
#endif
#define EASY_PC_HIDDEN
#else
#if __GNUC__ >= 4
#define EASY_PC_API __attribute__((visibility("default")))
#define EASY_PC_HIDDEN __attribute__((visibility("hidden")))
#else
#define EASY_PC_API
#define EASY_PC_HIDDEN
#endif
#endif

#ifdef __GNUC__
#define ATTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
#define ATTR_NONNULL(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of structs
typedef struct epc_parser_t epc_parser_t;
typedef struct epc_cpt_node_t epc_cpt_node_t;
typedef struct epc_parser_ctx_t epc_parser_ctx_t;
typedef struct epc_parser_list epc_parser_list;

// line and column information.
typedef struct epc_line_col_t
{
    size_t line;
    size_t col;
} epc_line_col_t;

/**
 * @brief Represents the input to be parsed, which can be either a string or a file stream.
 */
typedef enum epc_parse_type_t
{
    EPC_PARSE_TYPE_STRING,
    EPC_PARSE_TYPE_FILE,
    EPC_PARSE_TYPE_FILENAME, /* Future extension for file path input */
} epc_parse_type_t;

/**
 * @brief A union type that encapsulates the different forms of input that can be parsed.
 *
 * The `type` field indicates which member of the union is valid. For `EPC_PARSE_TYPE_STRING`,
 * the `input_string` member should be used, and for `EPC_PARSE_TYPE_FILE`, the `fp` member
 * should be used. This structure allows the parsing API to accept multiple input types in a
 * flexible manner.
 */
typedef struct epc_parse_input_t
{
    epc_parse_type_t type;
    union
    {
        char const * input_string;
        FILE * fp;
        char const * filename;
    };
} epc_parse_input_t;

// Error Handling struct
/**
 * @brief Represents a detailed parsing error.
 *
 * This structure captures information about where and why a parsing operation failed,
 * including the specific input position, an error message, and what was expected
 * versus what was actually found.
 */
typedef struct epc_parser_error_t
{
    char const * message; /**< @brief A descriptive error message. */
    char const *
        input_position; /**< @brief Pointer to the exact position in the input string where the error occurred. */
    epc_line_col_t
        position; /**< @brief Line and column if the input where the error occurred (0-indexed, calculated later). */
    char const * expected; /**< @brief A string describing what the parser expected at the error position. */
    char const * found;    /**< @brief A string describing what the parser actually found at the error position. */
} epc_parser_error_t;

// Structure to hold AST-related metadata for each parser
/**
 * @brief Configuration for semantic actions to be performed during AST generation.
 *
 * This structure is associated with each parser and dictates how the parser's
 * result should contribute to the Abstract Syntax Tree (AST).
 * The `action` field specifies a particular semantic action to take.
 */
typedef struct
{
    bool assigned; /**< @brief True if a semantic action identifier has been assigned to the node. */
    int action;    /**< @brief The identifier for the semantic action to perform.
                    *          Concrete values for actions are defined elsewhere (e.g., in ast_builder.h).
                    */
} epc_ast_semantic_action_t;

// The Result of a Parse Attempt
typedef struct
{
    bool is_error; /**< @brief A flag: false for success, true for error. */
    union
    {
        epc_cpt_node_t * success;   /**< @brief Pointer to the root of the generated CPT on successful parsing. */
        epc_parser_error_t * error; /**< @brief Pointer to detailed error information on parsing failure. */
    } data;                         /**< @brief Union holding either the success node or error details. */
} epc_parse_result_t;

// --- Parse Session Result ---
// Contains the parse result and the transient context for cleanup
/**
 * @brief Represents the complete result and context of a parsing session.
 *
 * This structure holds the final outcome of a top-level parsing operation
 * (either a successful CPT or an error) along with the internal parser context
 * necessary for proper memory cleanup. Users should always destroy a session
 * using `easy_pc_parse_session_destroy` to prevent memory leaks.
 */
typedef struct epc_parse_session_t
{
    epc_parse_result_t result; /**< @brief The actual parse success/failure and CPT/error information. */
    epc_parser_ctx_t *
        internal_parse_ctx; /**< @brief Internal context for the parsing operation, managing CPT/error memory. */
} epc_parse_session_t;

// Visitor struct for CPT traversal
/**
 * @brief Structure for defining a visitor pattern to traverse the Concrete Parse Tree (CPT).
 *
 * A `pt_visitor_t` allows defining custom actions to be performed when entering
 * and exiting each node of a CPT during a traversal. This is useful for tasks
 * like AST building, printing, or validation.
 */
typedef struct
{
    /**
     * @brief Function pointer called when entering a CPT node.
     * @param node The CPT node being entered.
     * @param user_data A user-defined data pointer passed through the visitation.
     */
    void (*enter_node)(epc_cpt_node_t * node, void * user_data);
    /**
     * @brief Function pointer called when exiting a CPT node.
     * @param node The CPT node being exited.
     * @param user_data A user-defined data pointer passed through the visitation.
     */
    void (*exit_node)(epc_cpt_node_t * node, void * user_data);
    void *
        user_data; /**< @brief A user-defined data pointer that is passed to `enter_node` and `exit_node` callbacks. */
} epc_cpt_visitor_t;

// Function to visit a CPT (remains)
/**
 * @brief Traverses a Concrete Parse Tree (CPT) in a depth-first manner.
 *
 * This function applies a given `pt_visitor_t` to each node in the CPT,
 * calling `enter_node` before visiting children and `exit_node` after
 * all children have been visited.
 *
 * @param root A pointer to the root `pt_node_t` of the CPT to traverse.
 * @param visitor A pointer to a `pt_visitor_t` structure defining the callbacks
 *                to be executed during the traversal.
 */
EASY_PC_API void epc_cpt_visit_nodes(epc_cpt_node_t * root, epc_cpt_visitor_t * visitor);

/**
 * @brief Creates a new parser list.
 *
 * @return A new `epc_parser_list` instance, or NULL on error.
 */
EASY_PC_API epc_parser_list * epc_parser_list_create(void);

/**
 * @brief Adds a parser to the parser list.
 *
 * If the parser passed is NULL, nothing is added to the list and NULL is returned.
 *
 * @param list The parser list to add to.
 * @param parser The parser to add.
 * @return The parser that was added, or NULL if the input parser was NULL or an error occurred.
 */
EASY_PC_API epc_parser_t * epc_parser_list_add(epc_parser_list * list, epc_parser_t * parser);

/**
 * @brief Frees all parsers in the list and the list itself.
 *
 * @param list The parser list to free.
 */
EASY_PC_API void epc_parser_list_free(epc_parser_list * list);

// --- Parser List Helpers (Auto-generated) ---

/**
 * @brief Creates a parser that matches a single specific character and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @param c The character to match.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_char(char const * name, char c);

/**
 * @brief Creates a parser that matches a single specific character and adds it to the list.
 *        This is a convenience wrapper for `epc_char()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param c The character to match.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_char_l(epc_parser_list * list, char const * name, char c)
{
    return epc_parser_list_add(list, epc_char(name, c));
}

/**
 * @brief Creates a parser that matches a specific string literal and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @param s The string literal to match. The lifetime of `s` must exceed the parser's.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_string(char const * name, char const * s);

/**
 * @brief Creates a parser that matches a specific string literal and adds it to the list.
 *        This is a convenience wrapper for `epc_string()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param s The string literal to match. The lifetime of `s` must exceed the parser's.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_string_l(epc_parser_list * list, char const * name, char const * s)
{
    return epc_parser_list_add(list, epc_string(name, s));
}

/**
 * @brief Creates a parser that matches a single digit character (0-9) and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_digit(char const * name);

/**
 * @brief Creates a parser that matches a single digit character (0-9) and adds it to the list.
 *        This is a convenience wrapper for `epc_digit()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_digit_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_digit(name));
}

/**
 * @brief Creates a parser that matches a single alphabetic character (a-z, A-Z) and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_alpha(char const * name);

/**
 * @brief Creates a parser that matches a single alphabetic character (a-z, A-Z) and adds it to the list.
 *        This is a convenience wrapper for `epc_alpha()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_alpha_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_alpha(name));
}

/**
 * @brief Creates a parser that matches a single alphanumeric character (a-z, A-Z, 0-9) and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_alphanum(char const * name);

/**
 * @brief Creates a parser that matches a single alphanumeric character (a-z, A-Z, 0-9) and adds it to the list.
 *        This is a convenience wrapper for `epc_alphanum()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_alphanum_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_alphanum(name));
}

/**
 * @brief Creates a parser that matches an integer number (e.g., "123", "-45") and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_int(char const * name);

/**
 * @brief Creates a parser that matches an integer number (e.g., "123", "-45") and adds it to the list.
 *        This is a convenience wrapper for `epc_int()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_int_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_int(name));
}

/**
 * @brief Creates a parser that matches a floating-point number (e.g., "3.14", "-.5", "1e-3") and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_double(char const * name);

/**
 * @brief Creates a parser that matches a floating-point number (e.g., "3.14", "-.5", "1e-3") and adds it to the list.
 *        This is a convenience wrapper for `epc_double()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_double_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_double(name));
}

/**
 * @brief Creates a parser that matches a single whitespace character and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_space(char const * name);

/**
 * @brief Creates a parser that matches a single whitespace character and adds it to the list.
 *        This is a convenience wrapper for `epc_space()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_space_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_space(name));
}

/**
 * @brief Creates a parser that matches any single character.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_any(char const * name);

/**
 * @brief Creates a parser that matches any single character and adds it to the list.
 *        This is a convenience wrapper for `epc_any()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_any_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_any(name));
}

/**
 * @brief Creates a parser that always succeeds and produces a node with specified content and adds it to the list.
 *
 * This parser consumes no input and always returns a successful result
 * containing a CPT node with the given `content` and its length.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_succeed(char const * name);

/**
 * @brief Creates a parser that always succeeds and produces a node with specified content and adds it to the list.
 *        This is a convenience wrapper for `epc_succeed()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * This parser consumes no input and always returns a successful result
 * containing a CPT node with the given `content` and its length.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_succeed_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_succeed(name));
}

/**
 * @brief Creates a parser that matches a single hexadecimal digit (0-9, a-f, A-F) and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_hex_digit(char const * name);

/**
 * @brief Creates a parser that matches a single hexadecimal digit (0-9, a-f, A-F) and adds it to the list.
 *        This is a convenience wrapper for `epc_hex_digit()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_hex_digit_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_hex_digit(name));
}

/* Combinators, or parsers that take more than one argument. */
/**
 * @brief Creates a parser that matches a single character within a specified range and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @param char_start The beginning character of the range (inclusive).
 * @param char_end The ending character of the range (inclusive).
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_char_range(char const * name, char char_start, char char_end);

/**
 * @brief Creates a parser that matches a single character within a specified range and adds it to the list.
 *        This is a convenience wrapper for `epc_char_range()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param char_start The beginning character of the range (inclusive).
 * @param char_end The ending character of the range (inclusive).
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_char_range_l(epc_parser_list * list, char const * name, char char_start, char char_end)
{
    return epc_parser_list_add(list, epc_char_range(name, char_start, char_end));
}

/**
 * @brief Creates a parser that matches any single character NOT in the provided set and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @param chars_to_avoid A null-terminated string of characters that should NOT be matched.
 *                       The lifetime of this string must exceed the parser's.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_none_of(char const * name, char const * chars_to_avoid);

/**
 * @brief Creates a parser that matches any single character NOT in the provided set and adds it to the list.
 *        This is a convenience wrapper for `epc_none_of()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param chars_to_avoid A null-terminated string of characters that should NOT be matched.
 *                       The lifetime of this string must exceed the parser's.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_none_of_l(epc_parser_list * list, char const * name, char const * chars_to_avoid)
{
    return epc_parser_list_add(list, epc_none_of(name, chars_to_avoid));
}

/**
 * @brief Creates a parser that matches the given parser zero or more times and adds it to the list.
 *
 * This parser always succeeds. If the child parser matches zero times, it
 * consumes no input and returns a success result with an empty list of children.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to repeat.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_many(char const * name, epc_parser_t * p);

/**
 * @brief Creates a parser that matches the given parser zero or more times and adds it to the list.
 *        This is a convenience wrapper for `epc_many()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * This parser always succeeds. If the child parser matches zero times, it
 * consumes no input and returns a success result with an empty list of children.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to repeat.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_many_l(epc_parser_list * list, char const * name, epc_parser_t * p)
{
    return epc_parser_list_add(list, epc_many(name, p));
}

/**
 * @brief Creates a parser that matches the given parser exactly `num` times and adds it to the list.
 *
 * @param name The name of the parser for debugging/CPT.
 * @param num The exact number of times the child parser must match.
 *            If `num` is 0, it always succeeds, consuming no input.
 * @param p The child parser to repeat.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_count(char const * name, int num, epc_parser_t * p);

/**
 * @brief Creates a parser that matches the given parser exactly `num` times and adds it to the list.
 *        This is a convenience wrapper for `epc_count()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param num The exact number of times the child parser must match.
 *            If `num` is 0, it always succeeds, consuming no input.
 * @param p The child parser to repeat.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_count_l(epc_parser_list * list, char const * name, int num, epc_parser_t * p)
{
    return epc_parser_list_add(list, epc_count(name, num, p));
}

/**
 * @brief Creates a parser that matches `open`, then `p` (wrapped content), then `close` and adds it to the list.
 *
 * The resulting CPT node will represent the entire `open` + `p` + `close` sequence,
 * but its direct children will only include the result of the `p` (wrapped) parser.
 * @param name The name of the parser for debugging/CPT.
 * @param open The parser for the opening delimiter.
 * @param p The parser for the content wrapped by the delimiters.
 * @param close The parser for the closing delimiter.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_between(char const * name, epc_parser_t * open, epc_parser_t * p, epc_parser_t * close);

/**
 * @brief Creates a parser that matches `open`, then `p` (wrapped content), then `close` and adds it to the list.
 *        This is a convenience wrapper for `epc_between()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * The resulting CPT node will represent the entire `open` + `p` + `close` sequence,
 * but its direct children will only include the result of the `p` (wrapped) parser.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param open The parser for the opening delimiter.
 * @param p The parser for the content wrapped by the delimiters.
 * @param close The parser for the closing delimiter.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_between_l(epc_parser_list * list, char const * name, epc_parser_t * open, epc_parser_t * p, epc_parser_t * close)
{
    return epc_parser_list_add(list, epc_between(name, open, p, close));
}

/**
 * @brief Creates a parser that matches one or more `item` parsers, optionally separated by a `delimiter` and adds it to
 * the list.
 *
 * It requires at least one `item` to match. If a `delimiter_parser` is provided,
 * it attempts to match it between items.
 * @param name The name of the parser for debugging/CPT.
 * @param item_parser The parser for the items in the list.
 * @param delimiter_parser An optional parser for the delimiter between items. Can be NULL.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t *
epc_delimited(char const * name, epc_parser_t * item_parser, epc_parser_t * delimiter_parser);

/**
 * @brief Creates a parser that matches one or more `item` parsers, optionally separated by a `delimiter` and adds it to
 * the list. This is a convenience wrapper for `epc_delimited()` that automatically adds the created parser to the
 * provided `epc_parser_list`.
 *
 * It requires at least one `item` to match. If a `delimiter_parser` is provided,
 * it attempts to match it between items.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param item_parser The parser for the items in the list.
 * @param delimiter_parser An optional parser for the delimiter between items. Can be NULL.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_delimited_l(epc_parser_list * list, char const * name, epc_parser_t * item_parser, epc_parser_t * delimiter_parser)
{
    return epc_parser_list_add(list, epc_delimited(name, item_parser, delimiter_parser));
}

/**
 * @brief Creates a parser that attempts to match `p`, but always succeeds and adds it to the list.
 *
 * If `p` matches, its result is included. If `p` fails, `p_optional` succeeds
 * without consuming any input and produces an empty (zero-length) node.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to make optional.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_optional(char const * name, epc_parser_t * p);

/**
 * @brief Creates a parser that attempts to match `p`, but always succeeds and adds it to the list.
 *        This is a convenience wrapper for `epc_optional()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * If `p` matches, its result is included. If `p` fails, `p_optional` succeeds
 * without consuming any input and produces an empty (zero-length) node.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to make optional.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_optional_l(epc_parser_list * list, char const * name, epc_parser_t * p)
{
    return epc_parser_list_add(list, epc_optional(name, p));
}

/**
 * @brief Creates a parser that attempts to match `p` but consumes no input and adds it to the list.
 *
 * `p_lookahead` succeeds if `p` succeeds, and fails if `p` fails.
 * However, regardless of success or failure, it never consumes any input.
 * Useful for asserting conditions without advancing the input stream.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to look ahead for.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_lookahead(char const * name, epc_parser_t * p);

/**
 * @brief Creates a parser that attempts to match `p` but consumes no input and adds it to the list.
 *        This is a convenience wrapper for `epc_lookahead()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * `p_lookahead` succeeds if `p` succeeds, and fails if `p` fails.
 * However, regardless of success or failure, it never consumes any input.
 * Useful for asserting conditions without advancing the input stream.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to look ahead for.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_lookahead_l(epc_parser_list * list, char const * name, epc_parser_t * p)
{
    return epc_parser_list_add(list, epc_lookahead(name, p));
}

/**
 * @brief Creates a parser that succeeds if `p` FAILS, and fails if `p` SUCCEEDS and adds it to the list.
 *
 * `p_not` never consumes any input. It acts as a negative lookahead.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to check for non-matching.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_not(char const * name, epc_parser_t * p);

/**
 * @brief Creates a parser that succeeds if `p` FAILS, and fails if `p` SUCCEEDS and adds it to the list.
 *        This is a convenience wrapper for `epc_not()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * `p_not` never consumes any input. It acts as a negative lookahead.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to check for non-matching.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_not_l(epc_parser_list * list, char const * name, epc_parser_t * p)
{
    return epc_parser_list_add(list, epc_not(name, p));
}

/**
 * @brief Creates a parser that always fails with a specified error message and adds it to the list.
 *
 * Useful for explicitly indicating invalid states in a grammar.
 * @param name The name of the parser for debugging/CPT.
 * @param message The error message to report when this parser fails.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_fail(char const * name, char const * message);

/**
 * @brief Creates a parser that always fails with a specified error message and adds it to the list.
 *        This is a convenience wrapper for `epc_fail()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * Useful for explicitly indicating invalid states in a grammar.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param message The error message to report when this parser fails.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_fail_l(epc_parser_list * list, char const * name, char const * message)
{
    return epc_parser_list_add(list, epc_fail(name, message));
}

/**
 * @brief Creates a parser that matches any single character from a specified set and adds it to the list.
 * @param name The name of the parser for debugging/CPT.
 * @param chars_to_match A null-terminated string of characters that are allowed to match.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_one_of(char const * name, char const * chars_to_match);

/**
 * @brief Creates a parser that matches any single character from a specified set and adds it to the list.
 *        This is a convenience wrapper for `epc_one_of()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param chars_to_match A null-terminated string of characters that are allowed to match.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_one_of_l(epc_parser_list * list, char const * name, char const * chars_to_match)
{
    return epc_parser_list_add(list, epc_one_of(name, chars_to_match));
}

/**
 * @brief Creates a parser that matches its child parser, optionally surrounded by whitespace and adds it to the list.
 * The whitespace itself is skipped and not included in the CPT node for the lexeme.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to wrap.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_lexeme(char const * name, epc_parser_t * p);

/**
 * @brief Creates a parser that matches its child parser, optionally surrounded by whitespace and adds it to the list.
 *        This is a convenience wrapper for `epc_lexeme()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * The whitespace itself is skipped and not included in the CPT node for the lexeme.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param p The child parser to wrap.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_lexeme_l(epc_parser_list * list, char const * name, epc_parser_t * p)
{
    return epc_parser_list_add(list, epc_lexeme(name, p));
}

/**
 * @brief Creates a parser that matches one or more `item` parsers,
 *        separated by an `op` parser, applying `op` left-associatively and adds it to the list.
 *        Useful for arithmetic expressions like 1 + 2 - 3.
 * @param name The name of the parser for debugging/CPT.
 * @param item The parser for the individual items.
 * @param op The parser for the operator.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_chainl1(char const * name, epc_parser_t * item, epc_parser_t * op);

/**
 * @brief Creates a parser that matches one or more `item` parsers,
 *        separated by an `op` parser, applying `op` left-associatively and adds it to the list.
 *        This is a convenience wrapper for `epc_chainl1()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *        Useful for arithmetic expressions like 1 + 2 - 3.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param item The parser for the individual items.
 * @param op The parser for the operator.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_chainl1_l(epc_parser_list * list, char const * name, epc_parser_t * item, epc_parser_t * op)
{
    return epc_parser_list_add(list, epc_chainl1(name, item, op));
}

/**
 * @brief Creates a parser that matches one or more `item` parsers,
 *        separated by an `op` parser, applying `op` right-associatively and adds it to the list.
 *        Useful for expressions like 1 ^ 2 ^ 3.
 * @param name The name of the parser for debugging/CPT.
 * @param item The parser for the individual items.
 * @param op The parser for the operator.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_chainr1(char const * name, epc_parser_t * item, epc_parser_t * op);

/**
 * @brief Creates a parser that matches one or more `item` parsers,
 *        separated by an `op` parser, applying `op` right-associatively and adds it to the list.
 *        This is a convenience wrapper for `epc_chainr1()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *        Useful for expressions like 1 ^ 2 ^ 3.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param item The parser for the individual items.
 * @param op The parser for the operator.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_chainr1_l(epc_parser_list * list, char const * name, epc_parser_t * item, epc_parser_t * op)
{
    return epc_parser_list_add(list, epc_chainr1(name, item, op));
}

/**
 * @brief Creates a parser that tries to match one of several alternative parsers.
 *
 * `p_or` tries each parser in the list in order. The first one that succeeds
 * determines the result. If all fail, `p_or` fails.
 *
 * @param name The name of the parser for debugging/CPT.
 * @param count The number of alternative parsers.
 * @param ... A variable argument list of `parser_t*` pointers, one for each alternative.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_or(char const * name, int count, ...);

/**
 * @brief Creates a parser that tries to match one of several alternative parsers.
 *        This is a convenience wrapper for `epc_or()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * `p_or` tries each parser in the list in order. The first one that succeeds
 * determines the result. If all fail, `p_or` fails.
 *
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param count The number of alternative parsers.
 * @param ... A variable argument list of `parser_t*` pointers, one for each alternative.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_or_l(epc_parser_list * list, char const * name, int count, ...);

/**
 * @brief Creates a parser that matches a sequence of parsers in order.
 *
 * `p_and` succeeds if all provided parsers in the sequence succeed,
 * consuming input as each parser matches. If any parser in the sequence fails,
 * `p_and` fails. The CPT node will have children for each successful parser in the sequence.
 *
 * @param name The name of the parser for debugging/CPT.
 * @param count The number of parsers in the sequence.
 * @param ... A variable argument list of `parser_t*` pointers, one for each part of the sequence.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_and(char const * name, int count, ...);

/**
 * @brief Creates a parser that matches a sequence of parsers in order.
 *        This is a convenience wrapper for `epc_and()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * `p_and` succeeds if all provided parsers in the sequence succeed,
 * consuming input as each parser matches. If any parser in the sequence fails,
 * `p_and` fails. The CPT node will have children for each successful parser in the sequence.
 *
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param count The number of parsers in the sequence.
 * @param ... A variable argument list of `parser_t*` pointers, one for each part of the sequence.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_and_l(epc_parser_list * list, char const * name, int count, ...);

/**
 * @brief Creates a parser that attempts to match `parser_to_skip` zero or more times, discarding its results and adds
 * it to the list.
 *
 * This is similar to `p_many`, but it explicitly discards the CPT nodes generated
 * by `parser_to_skip`. It always succeeds, consuming as much input as `parser_to_skip`
 * matches, but generating a single "skip" node with combined length and no children.
 * @param name The name of the parser for debugging/CPT.
 * @param parser_to_skip The parser whose matches should be skipped.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_skip(char const * name, epc_parser_t * parser_to_skip);

/**
 * @brief Creates a parser that attempts to match `parser_to_skip` zero or more times, discarding its results and adds
 * it to the list. This is a convenience wrapper for `epc_skip()` that automatically adds the created parser to the
 * provided `epc_parser_list`.
 *
 * This is similar to `p_many`, but it explicitly discards the CPT nodes generated
 * by `parser_to_skip`. It always succeeds, consuming as much input as `parser_to_skip`
 * matches, but generating a single "skip" node with combined length and no children.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param parser_to_skip The parser whose matches should be skipped.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_skip_l(epc_parser_list * list, char const * name, epc_parser_t * parser_to_skip)
{
    return epc_parser_list_add(list, epc_skip(name, parser_to_skip));
}

/**
 * @brief Creates a parser that matches `parser_to_repeat` one or more times and adds it to the list.
 *
 * This parser requires at least one successful match of the child parser.
 * @param name The name of the parser for debugging/CPT.
 * @param parser_to_repeat The child parser to repeat.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_plus(char const * name, epc_parser_t * parser_to_repeat);

/**
 * @brief Creates a parser that matches `parser_to_repeat` one or more times and adds it to the list.
 *        This is a convenience wrapper for `epc_plus()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * This parser requires at least one successful match of the child parser.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @param parser_to_repeat The child parser to repeat.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_plus_l(epc_parser_list * list, char const * name, epc_parser_t * parser_to_repeat)
{
    return epc_parser_list_add(list, epc_plus(name, parser_to_repeat));
}

/**
 * @brief Creates a parser that matches the end of the input stream and adds it to the list.
 *
 * This parser succeeds only if there are no more characters to consume in the input.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_eoi(char const * name);

/**
 * @brief Creates a parser that matches the end of the input stream and adds it to the list.
 *        This is a convenience wrapper for `epc_eoi()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * This parser succeeds only if there are no more characters to consume in the input.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_eoi_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_eoi(name));
}

/**
 * @brief Creates a parser that matches a C++ style comment (// to end of line or EOF).
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_cpp_comment(char const * name);

/**
 * @brief Creates a parser that matches a C++ style comment (// to end of line or EOF).
 *        This is a convenience wrapper for epc_cpp_comment that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_cpp_comment_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_cpp_comment(name));
}

/**
 * @brief Creates a parser that matches a Bash comment (# to end of line or EOF).
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_bash_comment(char const * name);

/**
 * @brief Creates a parser that matches a Bash style comment (# to end of line or EOF).
 *        This is a convenience wrapper for epc_bash_comment that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_bash_comment_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_bash_comment(name));
}

/**
 * @brief Creates a parser that matches a C-style comment.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_c_comment(char const * name);

/**
 * @brief Creates a parser that matches a C-style comment.
 *        This is a convenience wrapper for epc_c_comment that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 * @param list The parser list to add to.
 * @param name The name of the parser for debugging/CPT.
 * @return A new `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_c_comment_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_c_comment(name));
}

/**
 * @brief Allocates and initializes a new parser object within the grammar's memory context.
 *
 * This function is typically used to create a forward reference to a parser that is needed to
 * overcome circular references to parsers that haven't been defined. The call this function to
 * obtain a base parser, refer to it when constructing other parsers as needed, create the actual
 * parser definition when its dependencies have been created, then finally copy the final parser
 * details to the forward reference by calling by epc_parser_duplicate().
 *
 * @param name An optional name for the parser, primarily for debugging and CPT visualization.
 * @return A pointer to the newly allocated and initialized `parser_t` instance, or NULL on error.
 */
EASY_PC_API epc_parser_t * epc_parser_fwd_decl(char const * name);

/**
 * @brief Allocates and initializes a new parser object within the grammar's memory context.
 *        This is a convenience wrapper for `epc_parser_fwd_decl()` that automatically adds the created
 *        parser to the provided `epc_parser_list`.
 *
 * This function is typically used to create a forward reference to a parser that is needed to
 * overcome circular references to parsers that haven't been defined. The call this function to
 * obtain a base parser, refer to it when constructing other parsers as needed, create the actual
 * parser definition when its dependencies have been created, then finally copy the final parser
 * details to the forward reference by calling by epc_parser_duplicate().
 *
 * @param list The parser list to add to.
 * @param name A string name for the parser, primarily for debugging and CPT visualization.
 * @return A pointer to the newly allocated and initialized `parser_t` instance, or NULL on error.
 */
static inline epc_parser_t *
epc_parser_fwd_decl_l(epc_parser_list * list, char const * name)
{
    return epc_parser_list_add(list, epc_parser_fwd_decl(name));
}

/**
 * @brief Duplicates the contents of a source parser into a destination parser.
 *
 * This function performs a shallow copy of a parser's data. It's particularly
 * useful for forward declarations where a placeholder parser needs to be
 * filled in later with the definition of another parser.
 *
 * @param dst A pointer to the destination `parser_t` to be filled.
 * @param src A pointer to the source `parser_t` whose contents will be copied.
 */
EASY_PC_API void epc_parser_duplicate(epc_parser_t * dst, epc_parser_t const * src);

/**
 * @brief Sets the AST semantic action for a parser.
 *
 * This function configures how the parse result of a specific parser
 * should be transformed or processed during Abstract Syntax Tree (AST)
 * construction.
 *
 * @param p A pointer to the `parser_t` for which to set the AST action.
 * @param action_type An integer identifier specifying the semantic action.
 *                    Concrete action types are defined elsewhere (e.g., `AST_ACTION_CREATE_NUMBER_FROM_CONTENT`).
 */
EASY_PC_API void epc_parser_set_ast_action(epc_parser_t * p, int action_type);

// --- Updated Top-Level API ---
/**
 * @brief Initiates a parsing operation with a given grammar and input string.
 *        This is a convenience wrapper for `epc_parse_input()` that accepts a null-terminated C string.
 *
 * @param top_parser The starting parser for the grammar (e.g., the root rule).
 * @param input_string The null-terminated string to be parsed.
 * @return An `easy_pc_parse_session_t` structure containing the result of the
 *         parsing operation (success CPT or error details) and an internal
 *         context for cleanup.
 *         This session MUST be destroyed with `easy_pc_parse_session_destroy`.
 */
EASY_PC_API epc_parse_session_t
epc_parse_str(epc_parser_t * top_parser, char const * input_string);

/**
 * @brief Initiates a parsing operation with a given grammar and input file stream.
 *
 * This function is similar to `epc_parse_input()`, but it reads from a `FILE*`
 * stream instead of a string. It attempts to match the `top_parser` against
 * the content of the file.
 *
 * @param top_parser The starting parser for the grammar (e.g., the root rule).
 * @param fp A pointer to an open `FILE` stream to be parsed.
 * @return An `easy_pc_parse_session_t` structure containing the result of the
 *         parsing operation (success CPT or error details) and an internal
 *         context for cleanup.
 *         This session MUST be destroyed with `easy_pc_parse_session_destroy`.
 */
EASY_PC_API epc_parse_session_t epc_parse_fp(epc_parser_t * top_parser, FILE * fp);

/**
 * @brief Initiates a parsing operation with a given grammar and input file specified by filename.
 *
 * This function is similar to `epc_parse_input()`, but it reads from a file specified by its name.
 * It attempts to match the `top_parser` against the content of the file.
 *
 * @param top_parser The starting parser for the grammar (e.g., the root rule).
 * @param filename A null-terminated string containing the path to the file to be parsed.
 * @return An `easy_pc_parse_session_t` structure containing the result of the
 *         parsing operation (success CPT or error details) and an internal
 *         context for cleanup.
 *         This session MUST be destroyed with `easy_pc_parse_session_destroy`.
 */
EASY_PC_API epc_parse_session_t epc_parse_file(epc_parser_t * top_parser, char const * filename);

/**
 * @brief Destroys an `easy_pc_parse_session_t` and frees all associated resources.
 *
 * This function is crucial for preventing memory leaks. It frees the CPT
 * (if parsing was successful) or the error details (if parsing failed),
 * along with the internal parser context.
 *
 * @param session A pointer to the `easy_pc_parse_session_t` to be destroyed.
 */
EASY_PC_API void epc_parse_session_destroy(epc_parse_session_t * session);

EASY_PC_API
void epc_parse_session_print_cpt(FILE * fp, epc_parse_session_t const * session);

/**
 * @brief Retrieves the semantically relevant content from a CPT node.
 *
 * This function returns a pointer to the start of the substring within the
 * node's full `content` that is considered semantically relevant, after
 * accounting for any `semantic_start_offset`.
 *
 * @param node A pointer to the `epc_cpt_node_t`.
 * @return A `const char*` pointer to the semantic content.
 */
EASY_PC_API const char * epc_cpt_node_get_semantic_content(epc_cpt_node_t * node);

/**
 * @brief Retrieves the length of the semantically relevant content from a CPT node.
 *
 * This function returns the length of the substring within the node's full
 * `content` that is considered semantically relevant, after accounting for
 * `semantic_start_offset` and `semantic_end_offset`.
 *
 * @param node A pointer to the `epc_cpt_node_t`.
 * @return A `size_t` representing the length of the semantic content.
 */
EASY_PC_API size_t epc_cpt_node_get_semantic_len(epc_cpt_node_t * node);

/**
 * @brief Retrieves the content from a CPT node.
 *
 * This function returns a pointer to the start of the node's full `content`.
 * Usually the same as the semantic content, except for the epc_lexeme parser,
 * which excludes leading/trailing whitespace.
 *
 * @param node A pointer to the `epc_cpt_node_t`.
 * @return A `const char*` pointer to the content.
 */
EASY_PC_API const char * epc_cpt_node_get_content(epc_cpt_node_t * node);

/**
 * @brief Retrieves the length of the content from a CPT node.
 *
 * This function returns the length of the the node's full `content`.
 * Usually the same as the semantic content length, except for the epc_lexeme
 * parser, which excludes leading/trailing whitespace.
 *
 * @param node A pointer to the `epc_cpt_node_t`.
 * @return A `size_t` representing the length of the content.
 */
EASY_PC_API size_t epc_cpt_node_get_len(epc_cpt_node_t * node);

/**
 * @brief Prints a Concrete Parse Tree (CPT) to a dynamically allocated string.
 *
 * This utility function generates a human-readable string representation of
 * the CPT for debugging or visualization purposes. The returned string
 * must be freed by the caller.
 *
 * @param parse_ctx The parser context associated with the CPT.
 * @param node The root `pt_node_t` of the CPT (or any sub-tree) to print.
 * @return A dynamically allocated string containing the CPT representation,
 *         or NULL on allocation failure. The caller is responsible for freeing this string.
 */
EASY_PC_API char * epc_cpt_to_string(epc_parser_ctx_t * parse_ctx, epc_cpt_node_t * node);

/**
 * @brief Frees the supplied list of parsers.
 *
 * This function frees the supplied list of parsers. Call this once the parsers are
 * no longer needed.
 *
 * @param count The number of parsers to follow in the pargument list.
 */
void epc_parsers_free(size_t const count, ...);

#ifdef __cplusplus
}
#endif
