#pragma once

#include <easy_pc/easy_pc.h>
#include <easy_pc/easy_pc_ast.h> // Include the new AST header
#include <stdarg.h>
#include <stdbool.h>

// The Parse Tree Node
/**
 * @brief Represents a node in the Concrete Parse Tree (CPT).
 *
 * Each `pt_node_t` stores information about a successfully parsed segment
 * of the input, including its type (tag), the actual content matched,
 * its length, and its hierarchical relationship to other nodes (children).
 */
struct epc_cpt_node_t
{
    char const * tag;     /**< @brief A string tag identifying the type of this node (e.g., "char", "string", "and"). */
    char const * name;    /**< @brief The name assigned to the parser that generated this node, for
                           *    debugging/identification.
                           */
    char const * content; /**< @brief A pointer to the start of the matched substring in the original input (or within
                             the parser itself in the case of epc_succeed()). */
    size_t len;           /**< @brief The length of the matched substring. */
    size_t semantic_start_offset; /**< @brief Offset from `content` to the start of the semantically relevant part. */
    size_t semantic_end_offset;   /**< @brief Length from the end of `content` to exclude from the semantically relevant
                                     part. */
    epc_cpt_node_t ** children;   /**< @brief An array of pointers to child `pt_node_t`s, representing sub-matches. */
    int children_count;           /**< @brief The number of children in the `children` array. */
    epc_ast_semantic_action_t ast_config; /**< @brief A copy of the ast action assigned to the associated parser that
                                           *    created the node.
                                           */
};

// Internal types for AST builder stack management
typedef enum
{
    EPC_AST_ITEM_PLACEHOLDER,
    EPC_AST_ITEM_USER_NODE
} epc_ast_item_type_t;

typedef struct
{
    epc_ast_item_type_t type;
    void * ptr; // Points to user's AST node or is NULL for a placeholder
} epc_ast_stack_entry_t;

struct epc_ast_builder_ctx_t
{
    epc_ast_stack_entry_t * stack;
    int top;      // Number of items currently on stack
    int capacity; // Current allocated capacity of the stack
    epc_ast_hook_registry_t * registry;
    void * user_data;
    bool has_error;
    char error_message[512];
};

typedef struct parse_get_input_result_t
{
    char const * next_input;
    size_t available;
    bool is_eof;
} parse_get_input_result_t;

EASY_PC_HIDDEN
parse_get_input_result_t parse_ctx_get_input_at_offset(epc_parser_ctx_t * ctx, size_t input_offset, size_t count);

static inline char const *
parse_ctx_get_input_start(epc_parser_ctx_t * ctx)
{
    parse_get_input_result_t input_result = parse_ctx_get_input_at_offset(ctx, 0, 0);
    return input_result.next_input;
}

EASY_PC_HIDDEN
size_t parse_ctx_get_input_len(epc_parser_ctx_t * const ctx);

EASY_PC_HIDDEN
ATTR_NONNULL(1)
size_t parse_ctx_get_offset_from_input(epc_parser_ctx_t * ctx, char const * input_position);

EASY_PC_HIDDEN
ATTR_NONNULL(1)
epc_parser_error_t * parse_ctx_get_furthest_error(epc_parser_ctx_t const * ctx);

EASY_PC_HIDDEN
ATTR_NONNULL(1)
void parser_ctx_set_furthest_error(epc_parser_ctx_t * ctx, epc_parser_error_t ** replacement);

// Structure for user-managed parser list
struct epc_parser_list
{
    epc_parser_t ** parsers;
    size_t count;
    size_t capacity;
};

// Structure to hold a list of parsers (e.g., for combinators like p_or)
typedef struct parser_list_t
{
    epc_parser_t ** parsers;
    int count;
} parser_list_t;

typedef struct
{
    char start;
    char end;
} char_range_data_t;

typedef struct
{
    int count;
    epc_parser_t * parser;
} count_data_t;

typedef struct
{
    epc_parser_t * open;
    epc_parser_t * parser;
    epc_parser_t * close;
} between_data_t;

typedef struct
{
    epc_parser_t * item;
    epc_parser_t * delimiter;
} delimited_data_t;

typedef struct
{
    epc_parser_t * parser;
    bool consume_comments;
} lexeme_data_t;

typedef enum parser_data_type_t
{
    PARSER_DATA_TYPE_NONE,
    PARSER_DATA_TYPE_PARSER,
    PARSER_DATA_TYPE_STRING,
    PARSER_DATA_TYPE_PARSER_LIST,
    PARSER_DATA_TYPE_CHAR_RANGE,
    PARSER_DATA_TYPE_COUNT,
    PARSER_DATA_TYPE_BETWEEN,
    PARSER_DATA_TYPE_DELIMITED,
    PARSER_DATA_TYPE_LEXEME,
} parser_data_type_t;

typedef struct parser_data_type_st
{
    parser_data_type_t data_type;
    union
    {
        epc_parser_t * parser;
        char const * string;
        parser_list_t * parser_list;
        char_range_data_t range;
        count_data_t count;
        between_data_t between;
        delimited_data_t delimited;
        lexeme_data_t lexeme;
    };
} parser_data_type_st;

typedef epc_parse_result_t (*parse_fn_t)(struct epc_parser_t * self, epc_parser_ctx_t * ctx, size_t input_offset);

struct epc_parser_t
{
    parse_fn_t parse_fn;

    // Parser-specific data
    parser_data_type_st data;

    char const * name; /**< @brief Must be freed when parser is destroyed. */
    char const * tag;  /**< @brief Unique tag for each parser type. Must _not_ be freed when the parser is destroyed. */
    char const * expected_value; /**< @brief Optional string to use in preference to name or tag when reporting parse
                                  * errors. Must _not_ be freed when the parser is destroyed.
                                  */

    epc_ast_semantic_action_t ast_config;
};

struct epc_ast_hook_registry_t
{
    epc_ast_action_cb * callbacks;  /**< @brief Array of semantic action callbacks, indexed by action value. */
    int action_count;               /**< @brief The number of possible semantic actions. */
    epc_ast_node_free_cb free_node; /**< @brief Callback to free a user-defined AST node. */
    epc_ast_enter_cb enter_node;    /**< @brief Callback for entering a CPT node. */
};

EASY_PC_HIDDEN
epc_parse_result_t
epc_unparsed_error_result(size_t input_offset, char const * message, char const * expected, char const * found);

void epc_parser_result_cleanup(epc_parse_result_t * result);

ATTR_NONNULL(1, 2)
EASY_PC_API
epc_cpt_node_t * epc_node_alloc(epc_parser_t * parser, char const * const tag);

EASY_PC_HIDDEN
void epc_node_free(epc_cpt_node_t * node);

EASY_PC_HIDDEN
char const * epc_node_id(epc_cpt_node_t const * node);

EASY_PC_HIDDEN
void epc_parser_error_free(epc_parser_error_t * error);

EASY_PC_HIDDEN
epc_parser_error_t * parser_furthest_error_copy(epc_parser_ctx_t * ctx);

void epc_parser_free(epc_parser_t * parser);

EASY_PC_HIDDEN
epc_line_col_t epc_calculate_line_and_column(epc_parser_ctx_t * ctx, size_t offset);

EASY_PC_HIDDEN
char const * epc_parser_get_name(epc_parser_t const * p);
