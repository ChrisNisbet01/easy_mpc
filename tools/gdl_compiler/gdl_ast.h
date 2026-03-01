#pragma once

#include <easy_pc/easy_pc.h> // This includes the definition of EPC_AST_ACTION_USER_DEFINED
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Define GDL-specific AST semantic actions
typedef enum epc_ast_user_defined_action_gdl
{
    GDL_AST_ACTION_CREATE_PROGRAM,
    GDL_AST_ACTION_CREATE_RULE_DEFINITION,
    GDL_AST_ACTION_CREATE_IDENTIFIER_REF,
    GDL_AST_ACTION_CREATE_STRING_LITERAL,
    GDL_AST_ACTION_CREATE_CHAR_LITERAL,
    GDL_AST_ACTION_CREATE_NUMBER_LITERAL,
    GDL_AST_ACTION_CREATE_CHAR_RANGE,
    GDL_AST_ACTION_CREATE_REPETITION_OPERATOR,
    GDL_AST_ACTION_CREATE_SEMANTIC_ACTION,
    GDL_AST_ACTION_CREATE_OPTIONAL_SEMANTIC_ACTION,
    GDL_AST_ACTION_CREATE_ONEOF_CALL,
    GDL_AST_ACTION_CREATE_NONEOF_CALL,
    GDL_AST_ACTION_CREATE_COUNT_CALL,
    GDL_AST_ACTION_CREATE_BETWEEN_CALL,
    GDL_AST_ACTION_CREATE_DELIMITED_CALL,
    GDL_AST_ACTION_CREATE_LOOKAHEAD_CALL,
    GDL_AST_ACTION_CREATE_NOT_CALL,
    GDL_AST_ACTION_CREATE_LEXEME_CALL,
    GDL_AST_ACTION_CREATE_SKIP_CALL,
    GDL_AST_ACTION_CREATE_CHAINL1_CALL,
    GDL_AST_ACTION_CREATE_CHAINR1_CALL,
    GDL_AST_ACTION_CREATE_SEQUENCE,
    GDL_AST_ACTION_CREATE_ALTERNATIVE,
    GDL_AST_ACTION_CREATE_OPTIONAL,
    GDL_AST_ACTION_CREATE_EXPRESSION_FACTOR,
    GDL_AST_ACTION_CREATE_RAW_CHAR_LITERAL,
    GDL_AST_ACTION_CREATE_KEYWORD,
    GDL_AST_ACTION_CREATE_TERMINAL,
    GDL_AST_ACTION_CREATE_FAIL_CALL,
    GDL_AST_ACTION_CREATE_SATISFY_CALL,
    GDL_AST_ACTION_CREATE_WRAP_CALL,
    GDL_AST_ACTION_MAX,
} epc_ast_user_defined_action_gdl;

// GDL AST Node Types
typedef enum
{
    GDL_AST_NODE_TYPE_PLACEHOLDER, // New placeholder node type
    GDL_AST_NODE_TYPE_PROGRAM,
    GDL_AST_NODE_TYPE_RULE_DEFINITION,
    GDL_AST_NODE_TYPE_IDENTIFIER_REF,
    GDL_AST_NODE_TYPE_STRING_LITERAL,
    GDL_AST_NODE_TYPE_CHAR_LITERAL,
    GDL_AST_NODE_TYPE_NUMBER_LITERAL,
    GDL_AST_NODE_TYPE_CHAR_RANGE,
    GDL_AST_NODE_TYPE_REPETITION_OPERATOR,
    GDL_AST_NODE_TYPE_SEMANTIC_ACTION,
    GDL_AST_NODE_TYPE_RAW_CHAR_LITERAL,
    GDL_AST_NODE_TYPE_REPETITION_EXPRESSION, // For expression_factor with repetition
    // New types for structured terminal handling
    GDL_AST_NODE_TYPE_KEYWORD,
    GDL_AST_NODE_TYPE_TERMINAL,
    // Combinator Calls (base types)
    GDL_AST_NODE_TYPE_COMBINATOR_ONEOF,
    GDL_AST_NODE_TYPE_COMBINATOR_NONEOF,
    GDL_AST_NODE_TYPE_COMBINATOR_COUNT,
    GDL_AST_NODE_TYPE_COMBINATOR_BETWEEN,
    GDL_AST_NODE_TYPE_COMBINATOR_DELIMITED,
    GDL_AST_NODE_TYPE_COMBINATOR_LOOKAHEAD,
    GDL_AST_NODE_TYPE_COMBINATOR_NOT,
    GDL_AST_NODE_TYPE_COMBINATOR_LEXEME,
    GDL_AST_NODE_TYPE_COMBINATOR_SKIP,
    GDL_AST_NODE_TYPE_COMBINATOR_CHAINL1,
    GDL_AST_NODE_TYPE_COMBINATOR_CHAINR1,
    GDL_AST_NODE_TYPE_FAIL_CALL,
    GDL_AST_NODE_TYPE_SEQUENCE,
    GDL_AST_NODE_TYPE_ALTERNATIVE,
    GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION,
    GDL_AST_NODE_TYPE_ARGUMENT_LIST,
    GDL_AST_NODE_TYPE_SATISFY_CALL,
    GDL_AST_NODE_TYPE_WRAP_CALL,
} gdl_ast_node_type_t;

// Forward declaration for gdl_ast_node_t
typedef struct gdl_ast_node_t gdl_ast_node_t;

// Linked list for children/arguments
typedef struct gdl_ast_list_node_t
{
    gdl_ast_node_t * item;
    struct gdl_ast_list_node_t * next;
} gdl_ast_list_node_t;

typedef struct
{
    gdl_ast_list_node_t * head;
    gdl_ast_list_node_t * tail;
    int count;
} gdl_ast_list_t;

// Specific data structures for each AST node type
typedef struct
{
    gdl_ast_list_t rules; // A list of rule definitions
} gdl_ast_program_t;

typedef struct
{
    char const * name;
    gdl_ast_node_t * definition;
    gdl_ast_node_t * semantic_action; // Optional
} gdl_ast_rule_definition_t;

typedef struct
{
    char const * name;
} gdl_ast_identifier_ref_t;

typedef struct
{
    char const * value;
} gdl_ast_string_literal_t;

typedef struct
{
    char * value;
} gdl_ast_char_literal_t;

typedef struct
{
    long long value; // Or double, depending on GDL spec
} gdl_ast_number_literal_t;

typedef struct
{
    char start_char;
    char end_char;
} gdl_ast_char_range_t;

typedef struct
{
    char operator_char; // '*', '+', '?'
} gdl_ast_repetition_operator_t;

typedef struct
{
    char const * action_name;
} gdl_ast_semantic_action_t;

typedef struct
{
    gdl_ast_node_t * expression;
    gdl_ast_node_t * repetition; // GDL_AST_NODE_TYPE_REPETITION_OPERATOR
} gdl_ast_repetition_expression_t;

typedef struct
{
    char const * name; // e.g., "eoi", "digit", "alpha"
} gdl_ast_keyword_t;

typedef struct
{
    gdl_ast_node_t * expression; // The actual literal, keyword, or identifier_ref
} gdl_ast_terminal_t;

typedef struct
{
    char const * args;
} gdl_ast_combinator_none_or_one_of_t;

typedef struct
{
    gdl_ast_node_t * count_node; // GDL_AST_NODE_TYPE_NUMBER_LITERAL
    gdl_ast_node_t * expression;
} gdl_ast_combinator_count_t;

typedef struct
{
    gdl_ast_node_t * open_expr;
    gdl_ast_node_t * content_expr;
    gdl_ast_node_t * close_expr;
} gdl_ast_combinator_between_t;

typedef struct
{
    gdl_ast_node_t * item_expr;
    gdl_ast_node_t * delimiter_expr; // Optional
} gdl_ast_combinator_delimited_t;

typedef struct
{
    gdl_ast_node_t * expr;
} gdl_ast_combinator_unary_t; // For lookahead, not, lexeme, skip, optional

typedef struct
{
    gdl_ast_node_t * item_expr;
    gdl_ast_node_t * op_expr;
} gdl_ast_combinator_chain_t; // For chainl1, chainr1

typedef struct
{
    gdl_ast_list_t elements; // List of gdl_ast_node_t
} gdl_ast_sequence_t;

typedef struct
{
    gdl_ast_list_t alternatives; // List of gdl_ast_node_t
} gdl_ast_alternative_t;

typedef struct
{
    gdl_ast_node_t * expr;
} gdl_ast_optional_expression_t;

typedef struct
{
    char value;
} gdl_ast_raw_char_literal_t;

typedef struct
{
    gdl_ast_node_t * expr;
    char const * message;
    char const * predicate_name;
    char const * parser_data_name;
} gdl_ast_satisfy_call_t;

typedef struct
{
    gdl_ast_node_t * expr;
    char const * callbacks_name;
    char const * parser_data_name;
} gdl_ast_wrap_call_t;

// Main GDL AST Node structure
struct gdl_ast_node_t
{
    gdl_ast_node_type_t type;
    union
    {
        gdl_ast_program_t program;
        gdl_ast_rule_definition_t rule_def;
        gdl_ast_identifier_ref_t identifier_ref;
        gdl_ast_string_literal_t string_literal;
        gdl_ast_char_literal_t char_literal;
        gdl_ast_raw_char_literal_t raw_char_literal;
        gdl_ast_number_literal_t number_literal;
        gdl_ast_char_range_t char_range;
        gdl_ast_repetition_operator_t repetition_op;
        gdl_ast_semantic_action_t semantic_action;
        gdl_ast_repetition_expression_t repetition_expr;
        gdl_ast_keyword_t keyword;
        gdl_ast_terminal_t terminal;

        gdl_ast_combinator_none_or_one_of_t none_or_one_of_call;
        gdl_ast_combinator_count_t count_call;
        gdl_ast_combinator_between_t between_call;
        gdl_ast_combinator_delimited_t delimited_call;
        gdl_ast_combinator_unary_t unary_combinator_call;
        gdl_ast_combinator_chain_t chain_combinator_call;
        gdl_ast_sequence_t sequence;
        gdl_ast_alternative_t alternative;
        gdl_ast_optional_expression_t optional;
        gdl_ast_list_t argument_list;
        gdl_ast_satisfy_call_t satisfy_call;
        gdl_ast_wrap_call_t wrap_call;
    } data;
};

// Functions to create AST nodes
void gdl_ast_node_free(void * node, void * user_data);

#ifdef __cplusplus
}
#endif
