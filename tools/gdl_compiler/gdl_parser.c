#include "gdl_ast.h"

#include <easy_pc/easy_pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

epc_parser_t *
create_gdl_parser(epc_parser_list * l)
{
    epc_parser_t * gdl_definition_expression = epc_parser_fwd_decl_l(l, "DefinitionExpression");

    // Define basic terminal parsers, now wrapped in lexeme where appropriate
    epc_parser_t * raw_gdl_alpha_char = epc_alpha_l(l, "RawAlphaChar");
    epc_parser_t * raw_gdl_digit_char = epc_digit_l(l, "RawDigitChar");
    epc_parser_t * raw_gdl_underscore = epc_char_l(l, "RawUnderscore", '_');
    epc_parser_t * raw_gdl_minus_char = epc_char_l(l, "RawMinusChar", '-');
    epc_parser_t * gdl_minus_char = epc_lexeme_l(l, "MinusChar", raw_gdl_minus_char);

    // Define Identifier: (alpha | underscore) (alpha | digit | underscore)*
    epc_parser_t * gdl_identifier_start_char
        = epc_or_l(l, "IdentifierStartChar", 2, raw_gdl_alpha_char, raw_gdl_underscore);
    epc_parser_t * gdl_identifier_cont_char
        = epc_or_l(l, "IdentifierContChar", 3, raw_gdl_alpha_char, raw_gdl_digit_char, raw_gdl_underscore);
    epc_parser_t * gdl_identifier_rest = epc_many_l(l, "IdentifierRest", gdl_identifier_cont_char);

    epc_parser_t * temp_identifier_raw
        = epc_and_l(l, "Identifier_Raw", 2, gdl_identifier_start_char, gdl_identifier_rest);
    epc_parser_t * gdl_identifier = epc_lexeme_l(l, "Identifier", temp_identifier_raw);
    epc_parser_set_ast_action(gdl_identifier, GDL_AST_ACTION_CREATE_IDENTIFIER_REF);

    // Define StringLiteral: '"' (char | '\"' | '\\')* '"'
    epc_parser_t * raw_gdl_string_quote = epc_char_l(l, "RawStringQuote", '"');

    epc_parser_t * gdl_escaped_quote = epc_string_l(l, "EscapedDoubleQuote", "\\\"");
    epc_parser_t * gdl_escaped_backslash_str = epc_string_l(l, "EscapedBackslashStr", "\\\\");
    epc_parser_t * gdl_escaped_newline_str = epc_string_l(l, "EscapedNewlineStr", "\\n");
    epc_parser_t * gdl_escaped_tab_str = epc_string_l(l, "EscapedNewlineStr", "\\t");
    epc_parser_t * gdl_escaped_cr_str = epc_string_l(l, "EscapedNewlineStr", "\\r");
    epc_parser_t * gdl_any_char_except_quote_backslash_newline_tab_cr
        = epc_none_of_l(l, "AnyCharExceptQuoteBackslashNewlineTabCR", "\"\\\n\t\r");
    epc_parser_t * gdl_string_char_option = epc_or_l(
        l,
        "StringCharOption",
        6,
        gdl_escaped_quote,
        gdl_escaped_backslash_str,
        gdl_escaped_newline_str,
        gdl_escaped_tab_str,
        gdl_escaped_cr_str,
        gdl_any_char_except_quote_backslash_newline_tab_cr
    );
    epc_parser_t * gdl_string_content = epc_many_l(l, "StringContent", gdl_string_char_option);

    epc_parser_t * temp_string_literal_raw
        = epc_and_l(l, "StringLiteral_Raw", 3, raw_gdl_string_quote, gdl_string_content, raw_gdl_string_quote);
    epc_parser_t * gdl_string_literal = epc_lexeme_l(l, "StringLiteral", temp_string_literal_raw);
    epc_parser_set_ast_action(gdl_string_literal, GDL_AST_ACTION_CREATE_STRING_LITERAL);

    // Define CharLiteral: "'" (char | '\'') "'"
    epc_parser_t * raw_gdl_char_quote = epc_char_l(l, "RawCharQuote", '\'');

    epc_parser_t * gdl_escaped_single_quote = epc_string_l(l, "EscapedSingleQuote", "\\'");
    epc_parser_t * gdl_escaped_backslash_char = epc_string_l(l, "EscapedBackslashChar", "\\\\");
    epc_parser_t * gdl_escaped_n = epc_string_l(l, "EscapedN", "\\n");
    epc_parser_t * gdl_escaped_t = epc_string_l(l, "EscapedT", "\\t");
    epc_parser_t * gdl_escaped_r = epc_string_l(l, "EscapedR", "\\r");
    epc_parser_t * gdl_any_char_except_single_quote_backslash
        = epc_none_of_l(l, "AnyCharExceptSingleQuoteBackslash", "'\\");

    epc_parser_t * gdl_char_literal_content_element = epc_or_l(
        l,
        "CharLiteralContentElement",
        6,
        gdl_escaped_single_quote,
        gdl_escaped_backslash_char,
        gdl_escaped_n,
        gdl_escaped_t,
        gdl_escaped_r,
        gdl_any_char_except_single_quote_backslash
    );

    epc_parser_t * temp_char_literal_content
        = epc_and_l(l, "CharLiteralContent", 1, gdl_char_literal_content_element); // Must be exactly one
    epc_parser_t * temp_char_literal_raw
        = epc_and_l(l, "CharLiteral_Raw", 3, raw_gdl_char_quote, temp_char_literal_content, raw_gdl_char_quote);
    epc_parser_t * gdl_char_literal = epc_lexeme_l(l, "CharLiteral", temp_char_literal_raw);
    epc_parser_set_ast_action(gdl_char_literal, GDL_AST_ACTION_CREATE_CHAR_LITERAL);

    // Define gdl_raw_char: a single character (possibly escaped) without quotes,
    // to be used in CharRange, oneof, none_of where quotes are not expected.
    // This should match a single char that is NOT a structural char, or an escaped char.
    epc_parser_t * gdl_raw_char_escape_sequence_content = epc_and_l(
        l,
        "RawCharEscapeContent",
        2,
        epc_char_l(l, "EscapeBackslash", '\\'),
        epc_any_l(l, "AnyEscapedChar") // Matches any char after backslash
    );
    epc_parser_t * gdl_raw_char_unreserved
        = epc_none_of_l(l, "RawCharNonStructural", "[]\\;=,()"); // Chars that need escaping or are structural

    epc_parser_t * gdl_raw_char_content_option
        = epc_or_l(l, "RawCharContentOption", 2, gdl_raw_char_escape_sequence_content, gdl_raw_char_unreserved);
    epc_parser_t * gdl_raw_char = epc_lexeme_l(l, "RawChar", gdl_raw_char_content_option); // Lexemize the raw character
    epc_parser_set_ast_action(gdl_raw_char, GDL_AST_ACTION_CREATE_RAW_CHAR_LITERAL);

    // TODO: Create a helper to construct the library parser names into a parser.
    /* Terminal. */
    epc_parser_t * p_char_raw = epc_string_l(l, "char", "char");
    epc_parser_t * p_digit_raw = epc_string_l(l, "digit", "digit");
    epc_parser_t * p_alphanum_raw = epc_string_l(l, "alphanum", "alphanum");
    epc_parser_t * p_alpha_raw = epc_string_l(l, "alpha", "alpha");
    epc_parser_t * p_int_raw = epc_string_l(l, "int", "int");
    epc_parser_t * p_double_raw = epc_string_l(l, "double", "double");
    epc_parser_t * p_double = epc_lexeme_l(l, "double", p_double_raw);
    epc_parser_t * p_space_raw = epc_string_l(l, "space", "space");
    epc_parser_t * p_any_raw = epc_string_l(l, "any", "any");
    epc_parser_t * p_succeed_raw = epc_string_l(l, "succeed", "succeed");
    epc_parser_t * p_hex_digit_raw = epc_string_l(l, "hex_digit", "hex_digit");
    epc_parser_t * p_eoi_raw = epc_string_l(l, "eoi", "eoi");
    epc_parser_t * p_fail_raw = epc_string_l(l, "fail", "fail");
    epc_parser_t * p_fail = epc_lexeme_l(l, "fail", p_fail_raw);
    epc_parser_t * p_cpp_comment_raw = epc_string_l(l, "cpp_comment", "cpp_comment");
    epc_parser_t * p_c_comment_raw = epc_string_l(l, "c_comment", "c_comment");
    epc_parser_t * p_bash_comment_raw = epc_string_l(l, "bash_comment", "bash_comment");

    /* Combinator. */
    epc_parser_t * p_string_raw = epc_string_l(l, "string", "string");
    epc_parser_t * p_char_range_raw = epc_string_l(l, "char_range", "char_range");
    epc_parser_t * p_none_of_raw = epc_string_l(l, "noneof", "noneof");
    epc_parser_t * p_none_of = epc_lexeme_l(l, "noneof", p_none_of_raw);
    epc_parser_t * p_many_raw = epc_string_l(l, "many", "many");
    epc_parser_t * p_count_raw = epc_string_l(l, "count", "count");
    epc_parser_t * p_count = epc_lexeme_l(l, "count", p_count_raw);
    epc_parser_t * p_between_raw = epc_string_l(l, "between", "between");
    epc_parser_t * p_between = epc_lexeme_l(l, "between", p_between_raw);
    epc_parser_t * p_delimited_raw = epc_string_l(l, "delimited", "delimited");
    epc_parser_t * p_delimited = epc_lexeme_l(l, "delimited", p_delimited_raw);
    epc_parser_t * p_optional_raw = epc_string_l(l, "optional", "optional");
    epc_parser_t * p_lookahead_raw = epc_string_l(l, "lookahead", "lookahead");
    epc_parser_t * p_lookahead = epc_lexeme_l(l, "lookahead", p_lookahead_raw);
    epc_parser_t * p_not_raw = epc_string_l(l, "not", "not");
    epc_parser_t * p_not = epc_lexeme_l(l, "not", p_not_raw);
    epc_parser_t * p_one_of_raw = epc_string_l(l, "oneof", "oneof");
    epc_parser_t * p_one_of = epc_lexeme_l(l, "oneof", p_one_of_raw);
    epc_parser_t * p_lexeme_raw = epc_string_l(l, "lexeme", "lexeme");
    epc_parser_t * p_lexeme = epc_lexeme_l(l, "lexeme", p_lexeme_raw);
    epc_parser_t * p_chainl1_raw = epc_string_l(l, "chainl1", "chainl1");
    epc_parser_t * p_chainl1 = epc_lexeme_l(l, "chainl1", p_chainl1_raw);
    epc_parser_t * p_chainr1_raw = epc_string_l(l, "chainr1", "chainr1");
    epc_parser_t * p_chainr1 = epc_lexeme_l(l, "chainr1", p_chainr1_raw);
    epc_parser_t * p_skip_raw = epc_string_l(l, "skip", "skip");
    epc_parser_t * p_skip = epc_lexeme_l(l, "skip", p_skip_raw);

    epc_parser_t * terminal_no_arg_parser = epc_or_l(
        l,
        "TerminalNoArgKeyword",
        14,
        p_char_raw,
        p_digit_raw,
        p_alphanum_raw,
        p_alpha_raw,
        p_int_raw,
        p_double_raw,
        p_space_raw,
        p_any_raw,
        p_succeed_raw,
        p_hex_digit_raw,
        p_cpp_comment_raw,
        p_c_comment_raw,
        p_bash_comment_raw,
        p_eoi_raw
    );
    epc_parser_set_ast_action(terminal_no_arg_parser, GDL_AST_ACTION_CREATE_KEYWORD);

    epc_parser_t * terminal_with_arg_parser = epc_or_l(l, "TerminalWithArgKeyword", 1, p_fail_raw);
    epc_parser_set_ast_action(terminal_with_arg_parser, GDL_AST_ACTION_CREATE_KEYWORD);

    epc_parser_t * terminal_parser_raw
        = epc_or_l(l, "TerminalKeyword_Raw", 2, terminal_no_arg_parser, terminal_with_arg_parser);
    epc_parser_t * terminal_keyword = epc_lexeme_l(l, "TerminalKeyword", terminal_no_arg_parser);

    epc_parser_t * combinator_parser = epc_or_l(
        l,
        "CombinatorKeyword",
        15,
        p_string_raw,
        p_char_range_raw,
        p_none_of_raw,
        p_many_raw,
        p_count_raw,
        p_between_raw,
        p_delimited_raw,
        p_optional_raw,
        p_lookahead_raw,
        p_not_raw,
        p_one_of_raw,
        p_lexeme_raw,
        p_chainl1_raw,
        p_chainr1_raw,
        p_skip_raw
    );
    epc_parser_set_ast_action(combinator_parser, GDL_AST_ACTION_CREATE_KEYWORD);

    epc_parser_t * keyword
        = epc_lexeme_l(l, "Keyword", epc_or_l(l, "Keyword", 2, terminal_parser_raw, combinator_parser));

    // Define Terminal: string_literal | char_literal | keyword | identifier
    // Order matters: keywords should be matched before general identifiers
    epc_parser_t * gdl_not_keyword = epc_not_l(l, "NotKeyword", keyword);
    epc_parser_t * gdl_actual_identifier = epc_and_l(l, "ActualIdentifier", 2, gdl_not_keyword, gdl_identifier);

    // Define CharRange: '[' char_literal '-' char_literal ']'
    epc_parser_t * raw_gdl_lbrack = epc_char_l(l, "RawLBracket", '[');
    epc_parser_t * raw_gdl_rbrack = epc_char_l(l, "RawRBracket", ']');

    epc_parser_t * temp_char_range_raw
        = epc_and_l(l, "CharRange_Raw", 5, raw_gdl_lbrack, gdl_raw_char, gdl_minus_char, gdl_raw_char, raw_gdl_rbrack);
    epc_parser_t * gdl_char_range = epc_lexeme_l(l, "CharRange", temp_char_range_raw); // Lexemize the whole range
    epc_parser_set_ast_action(gdl_char_range, GDL_AST_ACTION_CREATE_CHAR_RANGE);

    // Define RepetitionOperator: '*' | '+' | '?'
    epc_parser_t * raw_gdl_star = epc_char_l(l, "RawStar", '*');
    epc_parser_t * gdl_star = epc_lexeme_l(l, "Star", raw_gdl_star);
    epc_parser_t * raw_gdl_plus_char = epc_char_l(l, "RawPlus", '+');
    epc_parser_t * gdl_plus_char = epc_lexeme_l(l, "Plus", raw_gdl_plus_char);
    epc_parser_t * raw_gdl_question = epc_char_l(l, "RawQuestion", '?');
    epc_parser_t * gdl_question = epc_lexeme_l(l, "Question", raw_gdl_question);

    epc_parser_t * temp_repetition_operator_raw
        = epc_or_l(l, "RepetitionOperator_Raw", 3, gdl_star, gdl_plus_char, gdl_question);
    epc_parser_t * gdl_repetition_operator = epc_lexeme_l(l, "RepetitionOperator", temp_repetition_operator_raw);
    epc_parser_set_ast_action(gdl_repetition_operator, GDL_AST_ACTION_CREATE_REPETITION_OPERATOR);

    // Define SemanticAction: '@' identifier
    epc_parser_t * raw_gdl_at = epc_char_l(l, "RawAtSign", '@');
    epc_parser_t * gdl_at = epc_lexeme_l(l, "AtSign", raw_gdl_at);
    epc_parser_t * gdl_semantic_action = epc_and_l(l, "SemanticAction", 2, gdl_at, gdl_identifier);
    epc_parser_set_ast_action(gdl_semantic_action, GDL_AST_ACTION_CREATE_SEMANTIC_ACTION);
    epc_parser_t * gdl_optional_semantic_action = epc_optional_l(l, "OptionalSemanticAction", gdl_semantic_action);
    epc_parser_set_ast_action(gdl_optional_semantic_action, GDL_AST_ACTION_CREATE_OPTIONAL_SEMANTIC_ACTION);

    // Define NumberLiteral: digit+ (for count() argument, etc.)
    epc_parser_t * temp_number_literal_raw = epc_plus_l(l, "NumberLiteral_Raw", raw_gdl_digit_char);
    epc_parser_t * gdl_number_literal = epc_lexeme_l(l, "NumberLiteral", temp_number_literal_raw);
    epc_parser_set_ast_action(gdl_number_literal, GDL_AST_ACTION_CREATE_NUMBER_LITERAL);

    // Define CombinatorCall arguments and calls
    epc_parser_t * raw_gdl_lparen = epc_char_l(l, "RawLParen", '(');
    epc_parser_t * gdl_lparen = epc_lexeme_l(l, "LParen", raw_gdl_lparen);
    epc_parser_t * raw_gdl_rparen = epc_char_l(l, "RawRParen", ')');
    epc_parser_t * gdl_rparen = epc_lexeme_l(l, "RParen", raw_gdl_rparen);
    epc_parser_t * raw_gdl_comma = epc_char_l(l, "RawComma", ',');
    epc_parser_t * gdl_comma = epc_lexeme_l(l, "Comma", raw_gdl_comma);

    /* Terminal special case. The fail parser is a terminal, but takes an argument (a message string). */
    epc_parser_t * fail_call = epc_and_l(l, "FailCall", 4, p_fail, gdl_lparen, gdl_string_literal, gdl_rparen);
    epc_parser_set_ast_action(fail_call, GDL_AST_ACTION_CREATE_FAIL_CALL);

    epc_parser_t * gdl_terminal = epc_or_l(
        l,
        "Terminal",
        6,
        gdl_string_literal,
        gdl_char_literal,
        terminal_keyword,
        fail_call,
        gdl_actual_identifier,
        p_double
    );
    epc_parser_set_ast_action(gdl_terminal, GDL_AST_ACTION_CREATE_TERMINAL);

    // A generic argument parser for combinators that take expression arguments
    // An argument can be any definition expression
    epc_parser_t * gdl_expression_arg = epc_parser_fwd_decl_l(l, "ExpressionArgFwd");

    /* Function calls (maps to epc_<xxx>_l() parsers) to follow. */

    /* Combinator parsers. */

    // none_of_call: 'none_of' '(' char_literal (',' char_literal)* ')'
    epc_parser_t * none_of_call = epc_and_l(l, "NoneofCall", 4, p_none_of, gdl_lparen, gdl_string_literal, gdl_rparen);
    epc_parser_set_ast_action(none_of_call, GDL_AST_ACTION_CREATE_NONEOF_CALL);

    // count_call: 'count' '(' number_literal ',' definition_expression ')'
    epc_parser_t * count_args = epc_and_l(l, "CountArgs", 3, gdl_number_literal, gdl_comma, gdl_definition_expression);
    epc_parser_t * count_call = epc_and_l(l, "CountCall", 4, p_count, gdl_lparen, count_args, gdl_rparen);
    epc_parser_set_ast_action(count_call, GDL_AST_ACTION_CREATE_COUNT_CALL);

    epc_parser_t * between_args = epc_and_l(
        l, "BetweenArgs", 5, gdl_expression_arg, gdl_comma, gdl_expression_arg, gdl_comma, gdl_expression_arg
    );
    epc_parser_t * between_call = epc_and_l(l, "BetweenCall", 4, p_between, gdl_lparen, between_args, gdl_rparen);
    epc_parser_set_ast_action(between_call, GDL_AST_ACTION_CREATE_BETWEEN_CALL);

    epc_parser_t * delimited_args = epc_and_l(l, "DelimitedArgs", 3, gdl_expression_arg, gdl_comma, gdl_expression_arg);
    epc_parser_t * delimited_call
        = epc_and_l(l, "DelimitedCall", 4, p_delimited, gdl_lparen, delimited_args, gdl_rparen);
    epc_parser_set_ast_action(delimited_call, GDL_AST_ACTION_CREATE_DELIMITED_CALL);

    epc_parser_t * lookahead_call
        = epc_and_l(l, "LookaheadCall", 4, p_lookahead, gdl_lparen, gdl_expression_arg, gdl_rparen);
    epc_parser_set_ast_action(lookahead_call, GDL_AST_ACTION_CREATE_LOOKAHEAD_CALL);

    epc_parser_t * not_call = epc_and_l(l, "NotCall", 4, p_not, gdl_lparen, gdl_expression_arg, gdl_rparen);
    epc_parser_set_ast_action(not_call, GDL_AST_ACTION_CREATE_NOT_CALL);

    // oneof_call: 'oneof' '(' string_literal ')'
    epc_parser_t * oneof_call = epc_and_l(l, "OneofCall", 4, p_one_of, gdl_lparen, gdl_string_literal, gdl_rparen);
    epc_parser_set_ast_action(oneof_call, GDL_AST_ACTION_CREATE_ONEOF_CALL);

    epc_parser_t * lexeme_call = epc_and_l(l, "LexemeCall", 4, p_lexeme, gdl_lparen, gdl_expression_arg, gdl_rparen);
    epc_parser_set_ast_action(lexeme_call, GDL_AST_ACTION_CREATE_LEXEME_CALL);

    epc_parser_t * chain_args = epc_and_l(l, "ChainArgs", 3, gdl_expression_arg, gdl_comma, gdl_expression_arg);

    epc_parser_t * chainl1_call = epc_and_l(l, "ChainL1Call", 4, p_chainl1, gdl_lparen, chain_args, gdl_rparen);
    epc_parser_set_ast_action(chainl1_call, GDL_AST_ACTION_CREATE_CHAINL1_CALL);

    epc_parser_t * chainr1_call = epc_and_l(l, "ChainR1Call", 4, p_chainr1, gdl_lparen, chain_args, gdl_rparen);
    epc_parser_set_ast_action(chainr1_call, GDL_AST_ACTION_CREATE_CHAINR1_CALL);

    epc_parser_t * skip_call = epc_and_l(l, "SkipCall", 4, p_skip, gdl_lparen, gdl_expression_arg, gdl_rparen);
    epc_parser_set_ast_action(skip_call, GDL_AST_ACTION_CREATE_SKIP_CALL);

    epc_parser_t * gdl_combinator_call = epc_or_l(
        l,
        "CombinatorCall",
        12,
        none_of_call,
        count_call,
        between_call,
        delimited_call,
        lookahead_call,
        not_call,
        fail_call,
        oneof_call,
        lexeme_call,
        chainl1_call,
        chainr1_call,
        skip_call
    );

    // PrimaryExpression: terminal | char_range | combinator_call | '(' definition_expression ')'
    epc_parser_t * gdl_parenthesized_expression
        = epc_and_l(l, "ParenthesizedExpression", 3, gdl_lparen, gdl_definition_expression, gdl_rparen);

    epc_parser_t * gdl_primary_expression = epc_or_l(
        l,
        "PrimaryExpression",
        5,
        gdl_combinator_call,
        gdl_terminal,
        gdl_char_range,
        gdl_parenthesized_expression,
        gdl_number_literal
    );

    // ExpressionFactor: primary_expression repetition_operator?
    epc_parser_t * gdl_optional_repetition = epc_optional_l(l, "OptionalRepetition", gdl_repetition_operator);
    epc_parser_set_ast_action(gdl_optional_repetition, GDL_AST_ACTION_CREATE_OPTIONAL);

    epc_parser_t * gdl_expression_factor
        = epc_and_l(l, "ExpressionFactor", 2, gdl_primary_expression, gdl_optional_repetition);
    epc_parser_set_ast_action(gdl_expression_factor, GDL_AST_ACTION_CREATE_EXPRESSION_FACTOR);

    // ExpressionTerm: expression_factor+ (sequence of factors)
    epc_parser_t * gdl_expression_term = epc_plus_l(l, "ExpressionTerm", gdl_expression_factor);
    epc_parser_set_ast_action(gdl_expression_term, GDL_AST_ACTION_CREATE_SEQUENCE);

    // DefinitionExpression: expression_term ('|' expression_term)*
    epc_parser_t * raw_gdl_pipe_char = epc_char_l(l, "RawPipeChar", '|');
    epc_parser_t * gdl_pipe_char = epc_lexeme_l(l, "PipeChar", raw_gdl_pipe_char);

    epc_parser_t * gdl_alternative_part = epc_and_l(l, "AlternativePart", 2, gdl_pipe_char, gdl_expression_term);
    epc_parser_t * gdl_many_alternatives = epc_many_l(l, "ManyAlternatives", gdl_alternative_part);

    epc_parser_t * temp_definition_expression
        = epc_and_l(l, "DefinitionExpression", 2, gdl_expression_term, gdl_many_alternatives);
    epc_parser_set_ast_action(temp_definition_expression, GDL_AST_ACTION_CREATE_ALTERNATIVE);

    epc_parser_duplicate(gdl_definition_expression, temp_definition_expression);
    epc_parser_duplicate(gdl_expression_arg, gdl_definition_expression);

    // RuleDefinition: identifier '=' definition_expression semantic_action? ';'
    epc_parser_t * raw_gdl_equals_char = epc_char_l(l, "RawEqualsChar", '=');
    epc_parser_t * gdl_equals_char = epc_lexeme_l(l, "EqualsChar", raw_gdl_equals_char);
    epc_parser_t * raw_gdl_semicolon_char = epc_char_l(l, "RawSemicolonChar", ';');
    epc_parser_t * gdl_semicolon_char = epc_lexeme_l(l, "SemicolonChar", raw_gdl_semicolon_char);

    epc_parser_t * gdl_rule_definition = epc_and_l(
        l,
        "RuleDefinition",
        5,
        gdl_identifier,
        gdl_equals_char,
        gdl_definition_expression,
        gdl_optional_semantic_action,
        gdl_semicolon_char
    );
    epc_parser_set_ast_action(gdl_rule_definition, GDL_AST_ACTION_CREATE_RULE_DEFINITION);

    // Program: rule_definition+ eoi
    epc_parser_t * gdl_many_rule_definitions = epc_plus_l(l, "ManyRuleDefinitions", gdl_rule_definition);
    epc_parser_set_ast_action(gdl_many_rule_definitions, GDL_AST_ACTION_CREATE_SEQUENCE);

    epc_parser_t * gdl_eoi_parser = epc_eoi_l(l, "EOI");

    epc_parser_t * gdl_program = epc_and_l(l, "Program", 2, gdl_many_rule_definitions, gdl_eoi_parser);
    epc_parser_set_ast_action(gdl_program, GDL_AST_ACTION_CREATE_PROGRAM);

    return gdl_program;
}
