#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <stdio.h>

extern "C" {
#include "easy_pc_private.h"
#include "gdl_parser.h"
}

TEST_GROUP(GdlParserTest)
{
    epc_parser_t * gdl_parser_root;
    epc_parser_list * parser_list;
    epc_parse_session_t session;

    void setup()
    {
        session = (epc_parse_session_t){0}; // Initialize session to zero
        parser_list = epc_parser_list_create();
        CHECK(parser_list != NULL);
        gdl_parser_root = create_gdl_parser(parser_list);
        CHECK(gdl_parser_root != NULL);
    }

    void print_error_and_fail(epc_parser_error_t const * error)
    {
        char error_msg_buffer[512];
        snprintf(
            error_msg_buffer,
            sizeof(error_msg_buffer),
            "GDL Parsing Error: %s at input position '%.10s' (line %zu, col %zu)"
            " expected: '%s', found: '%s'\n",
            error->message,
            error->input_position,
            error->position.line,
            error->position.col,
            error->expected,
            error->found
        );
        FAIL(error_msg_buffer);
    }

    epc_parse_session_t parse(epc_parser_t * parser, char const * input)
    {
        void * user_ctx = NULL; // No user context for these tests
        session = epc_parse_str(parser, input, user_ctx);
        return session;
    }

    void teardown()
    {
        epc_parse_session_destroy(&session);
        epc_parser_list_free(parser_list);
    }
};

TEST(GdlParserTest, SimpleFailTerminalWithStringShouldParse)
{
    char const * gdl_input = "MyRule = fail(\"failure message\");";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }
}

TEST(GdlParserTest, SimpleFailTerminalWithoutStringShouldntParse)
{
    char const * gdl_input = "MyRule = fail;";
    session = parse(gdl_parser_root, gdl_input);

    CHECK(session.result.is_error);
    STRCMP_CONTAINS("Unexpected character", session.result.data.error->message);
}

// Test parsing a simple identifier rule.
// Example GDL: MyRule = alpha;
TEST(GdlParserTest, ParseSimpleIdentifierRule)
{
    char const * gdl_input = "MyRule = alpha;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    // Get Program node (root)
    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count); // ManyRuleDefinitions and EOI

    // Get ManyRuleDefinitions node
    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count); // Contains one RuleDefinition

    // Get RuleDefinition node
    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(
        5, rule_def_node->children_count
    ); // Identifier, EqualsChar, DefinitionExpression, OptionalSemanticAction, SemicolonChar

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count); // ExpressionTerm and ManyAlternatives

    // Check ExpressionTerm (first child of DefinitionExpression)
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count); // One ExpressionFactor

    // Check ExpressionFactor (first child of ExpressionTerm)
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count); // PrimaryExpression and OptionalRepetition

    // Check PrimaryExpression (first child of ExpressionFactor)
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count); // Terminal

    // Check Terminal (first child of PrimaryExpression)
    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_node->name);
    CHECK_EQUAL(1, terminal_node->children_count); // Keyword

    // Check Keyword (first child of Terminal)
    epc_cpt_node_t * keyword_node = terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(keyword_node), epc_cpt_node_get_semantic_len(keyword_node)
    );

    // Check OptionalRepetition (the second child of ExpressionFactor) - should be empty
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // Check ManyAlternatives (second child of DefinitionExpression) - should be empty
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check OptionalSemanticAction (fourth child of RuleDefinition) - should be empty
    epc_cpt_node_t * optional_semantic_action_node = rule_def_node->children[3];
    STRCMP_EQUAL("OptionalSemanticAction", optional_semantic_action_node->name);
    CHECK_EQUAL(0, optional_semantic_action_node->children_count);

    // Check SemicolonChar (fifth child of RuleDefinition)
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    // Check EOI (second child of Program)
    epc_cpt_node_t * eoi_node = program_node->children[1];
    STRCMP_EQUAL("EOI", eoi_node->name);
    CHECK_EQUAL(0, eoi_node->children_count);

    epc_parse_session_destroy(&session);
}

// Test parsing a simple string literal rule.
// Example GDL: MyStringRule = \"hello world\";
TEST(GdlParserTest, ParseSimpleStringLiteralRule)
{
    char const * gdl_input = "MyStringRule = \"hello world\";";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyStringRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to StringLiteral
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    epc_cpt_node_t * string_literal_node = terminal_node->children[0];

    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    STRCMP_EQUAL("Terminal", terminal_node->name);
    STRCMP_EQUAL("StringLiteral", string_literal_node->name);
    STRNCMP_EQUAL(
        "\"hello world\"",
        epc_cpt_node_get_semantic_content(string_literal_node),
        epc_cpt_node_get_semantic_len(string_literal_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple char literal rule.
// Example GDL: MyCharRule = 'c';
TEST(GdlParserTest, ParseSimpleCharLiteralRule)
{
    char const * gdl_input = "MyCharRule = 'c';";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyCharRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to CharLiteral
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    epc_cpt_node_t * char_literal_node = terminal_node->children[0];

    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    STRCMP_EQUAL("Terminal", terminal_node->name);
    STRCMP_EQUAL("CharLiteral", char_literal_node->name);
    STRNCMP_EQUAL(
        "'c'", epc_cpt_node_get_semantic_content(char_literal_node), epc_cpt_node_get_semantic_len(char_literal_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple char range rule.
// Example GDL: MyRangeRule = [a-z];
TEST(GdlParserTest, ParseSimpleCharRangeRule)
{
    char const * gdl_input = "MyRangeRule = [a-z];";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyRangeRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to CharRange
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    epc_cpt_node_t * char_range_node = primary_expr_node->children[0]; // CharRange is a PrimaryExpression directly

    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    STRCMP_EQUAL("CharRange", char_range_node->name);
    STRNCMP_EQUAL(
        "[a-z]", epc_cpt_node_get_semantic_content(char_range_node), epc_cpt_node_get_semantic_len(char_range_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple sequence rule (implicit AND)
// Example GDL: MySeqRule = alpha digit;
TEST(GdlParserTest, ParseSimpleSequenceRule)
{
    char const * gdl_input = "MySeqRule = alpha digit;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MySeqRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path for sequence
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0]; // This is the sequence (alpha digit)

    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(2, expr_term_node->children_count); // Expecting two ExpressionFactors: one for alpha, one for digit

    // Check first ExpressionFactor (alpha)
    epc_cpt_node_t * alpha_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", alpha_factor_node->name);
    CHECK_EQUAL(2, alpha_factor_node->children_count); // PrimaryExpression and OptionalRepetition

    epc_cpt_node_t * alpha_primary_node = alpha_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", alpha_primary_node->name);
    CHECK_EQUAL(1, alpha_primary_node->children_count);

    epc_cpt_node_t * alpha_terminal_node = alpha_primary_node->children[0];
    STRCMP_EQUAL("Terminal", alpha_terminal_node->name);
    CHECK_EQUAL(1, alpha_terminal_node->children_count);

    epc_cpt_node_t * alpha_keyword_node = alpha_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", alpha_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha",
        epc_cpt_node_get_semantic_content(alpha_keyword_node),
        epc_cpt_node_get_semantic_len(alpha_keyword_node)
    );

    // Check second ExpressionFactor (digit)
    epc_cpt_node_t * digit_factor_node = expr_term_node->children[1];
    STRCMP_EQUAL("ExpressionFactor", digit_factor_node->name);
    CHECK_EQUAL(2, digit_factor_node->children_count); // PrimaryExpression and OptionalRepetition

    epc_cpt_node_t * digit_primary_node = digit_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", digit_primary_node->name);
    CHECK_EQUAL(1, digit_primary_node->children_count);

    epc_cpt_node_t * digit_terminal_node = digit_primary_node->children[0];
    STRCMP_EQUAL("Terminal", digit_terminal_node->name);
    CHECK_EQUAL(1, digit_terminal_node->children_count);

    epc_cpt_node_t * digit_keyword_node = digit_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", digit_keyword_node->name);
    STRNCMP_EQUAL(
        "digit",
        epc_cpt_node_get_semantic_content(digit_keyword_node),
        epc_cpt_node_get_semantic_len(digit_keyword_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple choice rule (implicit OR)
// Example GDL: MyChoiceRule = alpha | digit;
TEST(GdlParserTest, ParseSimpleChoiceRule)
{
    char const * gdl_input = "MyChoiceRule = alpha | digit;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyChoiceRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    // First ExpressionTerm (for 'alpha')
    epc_cpt_node_t * expr_term_alpha_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_alpha_node->name);
    CHECK_EQUAL(1, expr_term_alpha_node->children_count);

    epc_cpt_node_t * expr_factor_alpha_node = expr_term_alpha_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_alpha_node->name);
    CHECK_EQUAL(2, expr_factor_alpha_node->children_count);

    epc_cpt_node_t * primary_expr_alpha_node = expr_factor_alpha_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_alpha_node->name);
    CHECK_EQUAL(1, primary_expr_alpha_node->children_count);

    epc_cpt_node_t * terminal_alpha_node = primary_expr_alpha_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_alpha_node->name);
    CHECK_EQUAL(1, terminal_alpha_node->children_count);

    epc_cpt_node_t * keyword_alpha_node = terminal_alpha_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_alpha_node->name);
    STRNCMP_EQUAL(
        "alpha",
        epc_cpt_node_get_semantic_content(keyword_alpha_node),
        epc_cpt_node_get_semantic_len(keyword_alpha_node)
    );

    // ManyAlternatives (for '| digit')
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(1, many_alternatives_node->children_count); // One alternative part

    epc_cpt_node_t * alternative_part_node = many_alternatives_node->children[0];
    STRCMP_EQUAL("AlternativePart", alternative_part_node->name);
    CHECK_EQUAL(2, alternative_part_node->children_count); // PipeChar and ExpressionTerm

    epc_cpt_node_t * pipe_char_node = alternative_part_node->children[0];
    STRCMP_EQUAL("PipeChar", pipe_char_node->name);
    STRNCMP_EQUAL(
        "|", epc_cpt_node_get_semantic_content(pipe_char_node), epc_cpt_node_get_semantic_len(pipe_char_node)
    );

    // Second ExpressionTerm (for 'digit')
    epc_cpt_node_t * expr_term_digit_node = alternative_part_node->children[1];
    STRCMP_EQUAL("ExpressionTerm", expr_term_digit_node->name);
    CHECK_EQUAL(1, expr_term_digit_node->children_count);

    epc_cpt_node_t * expr_factor_digit_node = expr_term_digit_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_digit_node->name);
    CHECK_EQUAL(2, expr_factor_digit_node->children_count);

    epc_cpt_node_t * primary_expr_digit_node = expr_factor_digit_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_digit_node->name);
    CHECK_EQUAL(1, primary_expr_digit_node->children_count);

    epc_cpt_node_t * terminal_digit_node = primary_expr_digit_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_digit_node->name);
    CHECK_EQUAL(1, terminal_digit_node->children_count);

    epc_cpt_node_t * keyword_digit_node = terminal_digit_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_digit_node->name);
    STRNCMP_EQUAL(
        "digit",
        epc_cpt_node_get_semantic_content(keyword_digit_node),
        epc_cpt_node_get_semantic_len(keyword_digit_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple optional rule

// Example GDL: MyOptionalRule = alpha?;

TEST(GdlParserTest, ParseSimpleOptionalRule)
{
    char const * gdl_input = "MyOptionalRule = alpha?;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;

    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];

    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyOptionalRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count); // ExpressionTerm and ManyAlternatives

    // ExpressionTerm (for 'alpha?')
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    // ExpressionFactor (for 'alpha?')
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count); // PrimaryExpression and OptionalRepetition

    // PrimaryExpression (for 'alpha')
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_node->name);
    CHECK_EQUAL(1, terminal_node->children_count);

    epc_cpt_node_t * keyword_node = terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(keyword_node), epc_cpt_node_get_semantic_len(keyword_node)
    );

    // OptionalRepetition (for '?')
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(1, optional_repetition_node->children_count);

    epc_cpt_node_t * repetition_operator_node = optional_repetition_node->children[0];
    STRCMP_EQUAL("RepetitionOperator", repetition_operator_node->name);
    CHECK_EQUAL(1, repetition_operator_node->children_count);

    epc_cpt_node_t * repetition_operator_raw_node = repetition_operator_node->children[0];
    STRCMP_EQUAL("RepetitionOperator_Raw", repetition_operator_raw_node->name);
    CHECK_EQUAL(1, repetition_operator_raw_node->children_count);

    epc_cpt_node_t * question_node = repetition_operator_raw_node->children[0];
    STRCMP_EQUAL("Question", question_node->name);
    CHECK_EQUAL(1, question_node->children_count);

    epc_cpt_node_t * raw_question_node = question_node->children[0];
    STRCMP_EQUAL("RawQuestion", raw_question_node->name);
    STRNCMP_EQUAL(
        "?", epc_cpt_node_get_semantic_content(raw_question_node), epc_cpt_node_get_semantic_len(raw_question_node)
    );

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple plus rule (one or more)
// Example GDL: MyPlusRule = alpha+;
TEST(GdlParserTest, ParseSimplePlusRule)
{
    char const * gdl_input = "MyPlusRule = alpha+;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyPlusRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count); // ExpressionTerm and ManyAlternatives

    // ExpressionTerm (for 'alpha+')
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    // ExpressionFactor (for 'alpha+')
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count); // PrimaryExpression and OptionalRepetition

    // PrimaryExpression (for 'alpha')
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_node->name);
    CHECK_EQUAL(1, terminal_node->children_count);

    epc_cpt_node_t * keyword_node = terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(keyword_node), epc_cpt_node_get_semantic_len(keyword_node)
    );

    // OptionalRepetition (for '+')
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(1, optional_repetition_node->children_count);

    epc_cpt_node_t * repetition_operator_node = optional_repetition_node->children[0];
    STRCMP_EQUAL("RepetitionOperator", repetition_operator_node->name);
    CHECK_EQUAL(1, repetition_operator_node->children_count);

    epc_cpt_node_t * repetition_operator_raw_node = repetition_operator_node->children[0];
    STRCMP_EQUAL("RepetitionOperator_Raw", repetition_operator_raw_node->name);
    CHECK_EQUAL(1, repetition_operator_raw_node->children_count);

    epc_cpt_node_t * plus_node = repetition_operator_raw_node->children[0];
    STRCMP_EQUAL("Plus", plus_node->name);
    CHECK_EQUAL(1, plus_node->children_count);

    epc_cpt_node_t * raw_plus_node = plus_node->children[0];
    STRCMP_EQUAL("RawPlus", raw_plus_node->name);
    STRNCMP_EQUAL("+", epc_cpt_node_get_semantic_content(raw_plus_node), epc_cpt_node_get_semantic_len(raw_plus_node));

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a simple star rule (zero or more)
// Example GDL: MyStarRule = alpha*;
TEST(GdlParserTest, ParseSimpleStarRule)
{
    char const * gdl_input = "MyStarRule = alpha*;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyStarRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count); // ExpressionTerm and ManyAlternatives

    // ExpressionTerm (for 'alpha*')
    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    // ExpressionFactor (for 'alpha*')
    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count); // PrimaryExpression and OptionalRepetition

    // PrimaryExpression (for 'alpha')
    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_node->name);
    CHECK_EQUAL(1, terminal_node->children_count);

    epc_cpt_node_t * keyword_node = terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(keyword_node), epc_cpt_node_get_semantic_len(keyword_node)
    );

    // OptionalRepetition (for '*')
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(1, optional_repetition_node->children_count);

    epc_cpt_node_t * repetition_operator_node = optional_repetition_node->children[0];
    STRCMP_EQUAL("RepetitionOperator", repetition_operator_node->name);
    CHECK_EQUAL(1, repetition_operator_node->children_count);

    epc_cpt_node_t * repetition_operator_raw_node = repetition_operator_node->children[0];
    STRCMP_EQUAL("RepetitionOperator_Raw", repetition_operator_raw_node->name);
    CHECK_EQUAL(1, repetition_operator_raw_node->children_count);

    epc_cpt_node_t * star_node = repetition_operator_raw_node->children[0];
    STRCMP_EQUAL("Star", star_node->name);
    CHECK_EQUAL(1, star_node->children_count);

    epc_cpt_node_t * raw_star_node = star_node->children[0];
    STRCMP_EQUAL("RawStar", raw_star_node->name);
    STRNCMP_EQUAL("*", epc_cpt_node_get_semantic_content(raw_star_node), epc_cpt_node_get_semantic_len(raw_star_node));

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a oneof combinator call
// Example GDL: MyOneofRule = oneof('a', 'b', 'c');
TEST(GdlParserTest, ParseOneofCallRule)
{
    char const * gdl_input = "MyOneofRule = oneof(\"abc\");";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyOneofRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to OneofCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * oneof_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("OneofCall", oneof_call_node->name);
    CHECK_EQUAL(4, oneof_call_node->children_count); // KW_oneof, LParen, OneofArgs, RParen

    // Check KW_oneof
    epc_cpt_node_t * kw_oneof_node = oneof_call_node->children[0];
    STRCMP_EQUAL("oneof", kw_oneof_node->name);
    STRNCMP_EQUAL(
        "oneof", epc_cpt_node_get_semantic_content(kw_oneof_node), epc_cpt_node_get_semantic_len(kw_oneof_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = oneof_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    epc_cpt_node_t * abc_node = oneof_call_node->children[2];
    STRCMP_EQUAL("StringLiteral", abc_node->name);
    STRNCMP_EQUAL("\"abc\"", epc_cpt_node_get_semantic_content(abc_node), epc_cpt_node_get_semantic_len(abc_node));

    // Check RParen
    epc_cpt_node_t * rparen_node = oneof_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a none_of combinator call
// Example GDL: MyNoneofRule = none_of("xyz");
TEST(GdlParserTest, ParseNoneofCallRule)
{
    char const * gdl_input = "MyNoneofRule = noneof(\"xyz\");";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyNoneofRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to NoneofCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * noneof_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("NoneofCall", noneof_call_node->name);
    CHECK_EQUAL(4, noneof_call_node->children_count); // KW_none_of, LParen, NoneofArgs, RParen

    // Check KW_none_of
    epc_cpt_node_t * kw_noneof_node = noneof_call_node->children[0];
    STRCMP_EQUAL("noneof", kw_noneof_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "noneof", epc_cpt_node_get_semantic_content(kw_noneof_node), epc_cpt_node_get_semantic_len(kw_noneof_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = noneof_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // "xyz"
    epc_cpt_node_t * xyz_node = noneof_call_node->children[2];
    STRCMP_EQUAL("StringLiteral", xyz_node->name);
    STRNCMP_EQUAL("\"xyz\"", epc_cpt_node_get_semantic_content(xyz_node), epc_cpt_node_get_semantic_len(xyz_node));

    // Check RParen
    epc_cpt_node_t * rparen_node = noneof_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a count combinator call
// Example GDL: MyCountRule = count(5, alpha);
TEST(GdlParserTest, ParseCountCallRule)
{
    char const * gdl_input = "MyCountRule = count(5, alpha);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyCountRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to CountCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * count_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("CountCall", count_call_node->name);
    CHECK_EQUAL(4, count_call_node->children_count); // KW_count, LParen, CountArgs, RParen

    // Check KW_count
    epc_cpt_node_t * kw_count_node = count_call_node->children[0];
    STRCMP_EQUAL("count", kw_count_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "count", epc_cpt_node_get_semantic_content(kw_count_node), epc_cpt_node_get_semantic_len(kw_count_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = count_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check CountArgs
    epc_cpt_node_t * count_args_node = count_call_node->children[2];
    STRCMP_EQUAL("CountArgs", count_args_node->name);
    CHECK_EQUAL(3, count_args_node->children_count); // NumberLiteral, Comma, DefinitionExpression

    // NumberLiteral
    epc_cpt_node_t * number_literal_node = count_args_node->children[0];
    STRCMP_EQUAL("NumberLiteral", number_literal_node->name);
    STRNCMP_EQUAL(
        "5", epc_cpt_node_get_semantic_content(number_literal_node), epc_cpt_node_get_semantic_len(number_literal_node)
    );

    // Comma
    epc_cpt_node_t * comma_node = count_args_node->children[1];
    STRCMP_EQUAL("Comma", comma_node->name);
    STRNCMP_EQUAL(",", epc_cpt_node_get_semantic_content(comma_node), epc_cpt_node_get_semantic_len(comma_node));

    // DefinitionExpression (for 'alpha')
    epc_cpt_node_t * def_expr_arg_node = count_args_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_arg_node->name);
    CHECK_EQUAL(2, def_expr_arg_node->children_count);

    epc_cpt_node_t * arg_expr_term_node = def_expr_arg_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", arg_expr_term_node->name);
    CHECK_EQUAL(1, arg_expr_term_node->children_count);

    epc_cpt_node_t * arg_expr_factor_node = arg_expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", arg_expr_factor_node->name);
    CHECK_EQUAL(2, arg_expr_factor_node->children_count);

    epc_cpt_node_t * arg_primary_expr_node = arg_expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", arg_primary_expr_node->name);
    CHECK_EQUAL(1, arg_primary_expr_node->children_count);

    epc_cpt_node_t * arg_terminal_node = arg_primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", arg_terminal_node->name);
    CHECK_EQUAL(1, arg_terminal_node->children_count);

    epc_cpt_node_t * arg_keyword_node = arg_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg_keyword_node), epc_cpt_node_get_semantic_len(arg_keyword_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = count_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a between combinator call
// Example GDL: MyBetweenRule = between('[', alpha, ']');
TEST(GdlParserTest, ParseBetweenCallRule)
{
    char const * gdl_input = "MyBetweenRule = between('[', alpha, ']');";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyBetweenRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to BetweenCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * between_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("BetweenCall", between_call_node->name);
    CHECK_EQUAL(4, between_call_node->children_count); // KW_between, LParen, BetweenArgs, RParen

    // Check KW_between
    epc_cpt_node_t * kw_between_node = between_call_node->children[0];
    STRCMP_EQUAL("between", kw_between_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "between", epc_cpt_node_get_semantic_content(kw_between_node), epc_cpt_node_get_semantic_len(kw_between_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = between_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check BetweenArgs
    epc_cpt_node_t * between_args_node = between_call_node->children[2];
    STRCMP_EQUAL("BetweenArgs", between_args_node->name);
    CHECK_EQUAL(5, between_args_node->children_count); // 3 DefinitionExpressions, 2 Commas

    // First arg: '['
    epc_cpt_node_t * arg1_def_expr_node = between_args_node->children[0];
    STRCMP_EQUAL("DefinitionExpression", arg1_def_expr_node->name);
    epc_cpt_node_t * arg1_expr_term_node = arg1_def_expr_node->children[0];
    epc_cpt_node_t * arg1_expr_factor_node = arg1_expr_term_node->children[0];
    epc_cpt_node_t * arg1_primary_expr_node = arg1_expr_factor_node->children[0];
    epc_cpt_node_t * arg1_terminal_node = arg1_primary_expr_node->children[0];
    epc_cpt_node_t * arg1_char_literal_node = arg1_terminal_node->children[0];
    STRCMP_EQUAL("CharLiteral", arg1_char_literal_node->name);
    STRNCMP_EQUAL(
        "'['",
        epc_cpt_node_get_semantic_content(arg1_char_literal_node),
        epc_cpt_node_get_semantic_len(arg1_char_literal_node)
    );

    // First Comma
    epc_cpt_node_t * comma1_node = between_args_node->children[1];
    STRCMP_EQUAL("Comma", comma1_node->name);
    STRNCMP_EQUAL(",", epc_cpt_node_get_semantic_content(comma1_node), epc_cpt_node_get_semantic_len(comma1_node));

    // Second arg: alpha
    epc_cpt_node_t * arg2_def_expr_node = between_args_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg2_def_expr_node->name);
    epc_cpt_node_t * arg2_expr_term_node = arg2_def_expr_node->children[0];
    epc_cpt_node_t * arg2_expr_factor_node = arg2_expr_term_node->children[0];
    epc_cpt_node_t * arg2_primary_expr_node = arg2_expr_factor_node->children[0];
    epc_cpt_node_t * arg2_terminal_node = arg2_primary_expr_node->children[0];
    epc_cpt_node_t * arg2_keyword_node = arg2_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg2_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg2_keyword_node), epc_cpt_node_get_semantic_len(arg2_keyword_node)
    );

    // Second Comma
    epc_cpt_node_t * comma2_node = between_args_node->children[3];
    STRCMP_EQUAL("Comma", comma2_node->name);
    STRNCMP_EQUAL(",", epc_cpt_node_get_semantic_content(comma2_node), epc_cpt_node_get_semantic_len(comma2_node));

    // Third arg: ']'
    epc_cpt_node_t * arg3_def_expr_node = between_args_node->children[4];
    STRCMP_EQUAL("DefinitionExpression", arg3_def_expr_node->name);
    epc_cpt_node_t * arg3_expr_term_node = arg3_def_expr_node->children[0];
    epc_cpt_node_t * arg3_expr_factor_node = arg3_expr_term_node->children[0];
    epc_cpt_node_t * arg3_primary_expr_node = arg3_expr_factor_node->children[0];
    epc_cpt_node_t * arg3_terminal_node = arg3_primary_expr_node->children[0];
    epc_cpt_node_t * arg3_char_literal_node = arg3_terminal_node->children[0];
    STRCMP_EQUAL("CharLiteral", arg3_char_literal_node->name);
    STRNCMP_EQUAL(
        "']'",
        epc_cpt_node_get_semantic_content(arg3_char_literal_node),
        epc_cpt_node_get_semantic_len(arg3_char_literal_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = between_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a delimited combinator call
// Example GDL: MyDelimitedRule = delimited('(', alpha);
TEST(GdlParserTest, ParseDelimitedCallRule)
{
    char const * gdl_input = "MyDelimitedRule = delimited('(', alpha);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyDelimitedRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to DelimitedCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * delimited_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("DelimitedCall", delimited_call_node->name);
    CHECK_EQUAL(4, delimited_call_node->children_count); // KW_delimited, LParen, DelimitedArgs, RParen

    // Check KW_delimited
    epc_cpt_node_t * kw_delimited_node = delimited_call_node->children[0];
    STRCMP_EQUAL("delimited", kw_delimited_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "delimited",
        epc_cpt_node_get_semantic_content(kw_delimited_node),
        epc_cpt_node_get_semantic_len(kw_delimited_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = delimited_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check DelimitedArgs
    epc_cpt_node_t * delimited_args_node = delimited_call_node->children[2];
    STRCMP_EQUAL("DelimitedArgs", delimited_args_node->name);
    CHECK_EQUAL(3, delimited_args_node->children_count); // 2 DefinitionExpressions, 1 Comma

    // First arg: '('
    epc_cpt_node_t * arg1_def_expr_node = delimited_args_node->children[0];
    STRCMP_EQUAL("DefinitionExpression", arg1_def_expr_node->name);
    epc_cpt_node_t * arg1_expr_term_node = arg1_def_expr_node->children[0];
    epc_cpt_node_t * arg1_expr_factor_node = arg1_expr_term_node->children[0];
    epc_cpt_node_t * arg1_primary_expr_node = arg1_expr_factor_node->children[0];
    epc_cpt_node_t * arg1_terminal_node = arg1_primary_expr_node->children[0];
    epc_cpt_node_t * arg1_char_literal_node = arg1_terminal_node->children[0];
    STRCMP_EQUAL("CharLiteral", arg1_char_literal_node->name);
    STRNCMP_EQUAL(
        "'('",
        epc_cpt_node_get_semantic_content(arg1_char_literal_node),
        epc_cpt_node_get_semantic_len(arg1_char_literal_node)
    );

    // Comma
    epc_cpt_node_t * comma_node = delimited_args_node->children[1];
    STRCMP_EQUAL("Comma", comma_node->name);
    STRNCMP_EQUAL(",", epc_cpt_node_get_semantic_content(comma_node), epc_cpt_node_get_semantic_len(comma_node));

    // Second arg: alpha
    epc_cpt_node_t * arg2_def_expr_node = delimited_args_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg2_def_expr_node->name);
    epc_cpt_node_t * arg2_expr_term_node = arg2_def_expr_node->children[0];
    epc_cpt_node_t * arg2_expr_factor_node = arg2_expr_term_node->children[0];
    epc_cpt_node_t * arg2_primary_expr_node = arg2_expr_factor_node->children[0];
    epc_cpt_node_t * arg2_terminal_node = arg2_primary_expr_node->children[0];
    epc_cpt_node_t * arg2_keyword_node = arg2_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg2_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg2_keyword_node), epc_cpt_node_get_semantic_len(arg2_keyword_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = delimited_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a lookahead combinator call
// Example GDL: MyLookaheadRule = lookahead(alpha);
TEST(GdlParserTest, ParseLookaheadCallRule)
{
    char const * gdl_input = "MyLookaheadRule = lookahead(alpha);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyLookaheadRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to LookaheadCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * lookahead_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("LookaheadCall", lookahead_call_node->name);
    CHECK_EQUAL(4, lookahead_call_node->children_count); // KW_lookahead, LParen, DefinitionExpression, RParen

    // Check KW_lookahead
    epc_cpt_node_t * kw_lookahead_node = lookahead_call_node->children[0];
    STRCMP_EQUAL("lookahead", kw_lookahead_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "lookahead",
        epc_cpt_node_get_semantic_content(kw_lookahead_node),
        epc_cpt_node_get_semantic_len(kw_lookahead_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = lookahead_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check DefinitionExpression (arg)
    epc_cpt_node_t * arg_def_expr_node = lookahead_call_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg_def_expr_node->name);
    CHECK_EQUAL(2, arg_def_expr_node->children_count);

    epc_cpt_node_t * arg_expr_term_node = arg_def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", arg_expr_term_node->name);
    CHECK_EQUAL(1, arg_expr_term_node->children_count);

    epc_cpt_node_t * arg_expr_factor_node = arg_expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", arg_expr_factor_node->name);
    CHECK_EQUAL(2, arg_expr_factor_node->children_count);

    epc_cpt_node_t * arg_primary_expr_node = arg_expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", arg_primary_expr_node->name);
    CHECK_EQUAL(1, arg_primary_expr_node->children_count);

    epc_cpt_node_t * arg_terminal_node = arg_primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", arg_terminal_node->name);
    CHECK_EQUAL(1, arg_terminal_node->children_count);

    epc_cpt_node_t * arg_keyword_node = arg_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg_keyword_node), epc_cpt_node_get_semantic_len(arg_keyword_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = lookahead_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a not combinator call
// Example GDL: MyNotRule = not(alpha);
TEST(GdlParserTest, ParseNotCallRule)
{
    char const * gdl_input = "MyNotRule = not(alpha);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyNotRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to NotCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * not_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("NotCall", not_call_node->name);
    CHECK_EQUAL(4, not_call_node->children_count); // KW_not, LParen, DefinitionExpression, RParen

    // Check KW_not
    epc_cpt_node_t * kw_not_node = not_call_node->children[0];
    STRCMP_EQUAL("not", kw_not_node->name); // Name matches keyword string
    STRNCMP_EQUAL("not", epc_cpt_node_get_semantic_content(kw_not_node), epc_cpt_node_get_semantic_len(kw_not_node));

    // Check LParen
    epc_cpt_node_t * lparen_node = not_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check DefinitionExpression (arg)
    epc_cpt_node_t * arg_def_expr_node = not_call_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg_def_expr_node->name);
    CHECK_EQUAL(2, arg_def_expr_node->children_count);

    epc_cpt_node_t * arg_expr_term_node = arg_def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", arg_expr_term_node->name);
    CHECK_EQUAL(1, arg_expr_term_node->children_count);

    epc_cpt_node_t * arg_expr_factor_node = arg_expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", arg_expr_factor_node->name);
    CHECK_EQUAL(2, arg_expr_factor_node->children_count);

    epc_cpt_node_t * arg_primary_expr_node = arg_expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", arg_primary_expr_node->name);
    CHECK_EQUAL(1, arg_primary_expr_node->children_count);

    epc_cpt_node_t * arg_terminal_node = arg_primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", arg_terminal_node->name);
    CHECK_EQUAL(1, arg_terminal_node->children_count);

    epc_cpt_node_t * arg_keyword_node = arg_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg_keyword_node), epc_cpt_node_get_semantic_len(arg_keyword_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = not_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a lexeme combinator call
// Example GDL: MyLexemeRule = lexeme(alpha);
TEST(GdlParserTest, ParseLexemeCallRule)
{
    char const * gdl_input = "MyLexemeRule = lexeme(alpha);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyLexemeRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to LexemeCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * lexeme_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("LexemeCall", lexeme_call_node->name);
    CHECK_EQUAL(4, lexeme_call_node->children_count); // KW_lexeme, LParen, DefinitionExpression, RParen

    // Check KW_lexeme
    epc_cpt_node_t * kw_lexeme_node = lexeme_call_node->children[0];
    STRCMP_EQUAL("lexeme", kw_lexeme_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "lexeme", epc_cpt_node_get_semantic_content(kw_lexeme_node), epc_cpt_node_get_semantic_len(kw_lexeme_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = lexeme_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check DefinitionExpression (arg)
    epc_cpt_node_t * arg_def_expr_node = lexeme_call_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg_def_expr_node->name);
    CHECK_EQUAL(2, arg_def_expr_node->children_count);

    epc_cpt_node_t * arg_expr_term_node = arg_def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", arg_expr_term_node->name);
    CHECK_EQUAL(1, arg_expr_term_node->children_count);

    epc_cpt_node_t * arg_expr_factor_node = arg_expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", arg_expr_factor_node->name);
    CHECK_EQUAL(2, arg_expr_factor_node->children_count);

    epc_cpt_node_t * arg_primary_expr_node = arg_expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", arg_primary_expr_node->name);
    CHECK_EQUAL(1, arg_primary_expr_node->children_count);

    epc_cpt_node_t * arg_terminal_node = arg_primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", arg_terminal_node->name);
    CHECK_EQUAL(1, arg_terminal_node->children_count);

    epc_cpt_node_t * arg_keyword_node = arg_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg_keyword_node), epc_cpt_node_get_semantic_len(arg_keyword_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = lexeme_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a skip combinator call
// Example GDL: MySkipRule = skip(alpha);
TEST(GdlParserTest, ParseSkipCallRule)
{
    char const * gdl_input = "MySkipRule = skip(alpha);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MySkipRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to SkipCall
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * skip_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("SkipCall", skip_call_node->name);
    CHECK_EQUAL(4, skip_call_node->children_count); // KW_skip, LParen, DefinitionExpression, RParen

    // Check KW_skip
    epc_cpt_node_t * kw_skip_node = skip_call_node->children[0];
    STRCMP_EQUAL("skip", kw_skip_node->name); // Name matches keyword string
    STRNCMP_EQUAL("skip", epc_cpt_node_get_semantic_content(kw_skip_node), epc_cpt_node_get_semantic_len(kw_skip_node));

    // Check LParen
    epc_cpt_node_t * lparen_node = skip_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check DefinitionExpression (arg)
    epc_cpt_node_t * arg_def_expr_node = skip_call_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg_def_expr_node->name);
    CHECK_EQUAL(2, arg_def_expr_node->children_count);

    epc_cpt_node_t * arg_expr_term_node = arg_def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", arg_expr_term_node->name);
    CHECK_EQUAL(1, arg_expr_term_node->children_count);

    epc_cpt_node_t * arg_expr_factor_node = arg_expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", arg_expr_factor_node->name);
    CHECK_EQUAL(2, arg_expr_factor_node->children_count);

    epc_cpt_node_t * arg_primary_expr_node = arg_expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", arg_primary_expr_node->name);
    CHECK_EQUAL(1, arg_primary_expr_node->children_count);

    epc_cpt_node_t * arg_terminal_node = arg_primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", arg_terminal_node->name);
    CHECK_EQUAL(1, arg_terminal_node->children_count);

    epc_cpt_node_t * arg_keyword_node = arg_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg_keyword_node), epc_cpt_node_get_semantic_len(arg_keyword_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = skip_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a chainl1 combinator call
// Example GDL: MyChainl1Rule = chainl1(alpha, '+');
TEST(GdlParserTest, ParseChainl1CallRule)
{
    char const * gdl_input = "MyChainl1Rule = chainl1(alpha, '+');";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyChainl1Rule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to ChainL1Call
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * chainl1_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("ChainL1Call", chainl1_call_node->name);
    CHECK_EQUAL(4, chainl1_call_node->children_count); // KW_chainl1, LParen, ChainArgs, RParen

    // Check KW_chainl1
    epc_cpt_node_t * kw_chainl1_node = chainl1_call_node->children[0];
    STRCMP_EQUAL("chainl1", kw_chainl1_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "chainl1", epc_cpt_node_get_semantic_content(kw_chainl1_node), epc_cpt_node_get_semantic_len(kw_chainl1_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = chainl1_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check ChainArgs
    epc_cpt_node_t * chain_args_node = chainl1_call_node->children[2];
    STRCMP_EQUAL("ChainArgs", chain_args_node->name);
    CHECK_EQUAL(3, chain_args_node->children_count); // arg1, comma, arg2

    // Arg 1: alpha
    epc_cpt_node_t * arg1_def_expr_node = chain_args_node->children[0];
    STRCMP_EQUAL("DefinitionExpression", arg1_def_expr_node->name);
    epc_cpt_node_t * arg1_expr_term_node = arg1_def_expr_node->children[0];
    epc_cpt_node_t * arg1_expr_factor_node = arg1_expr_term_node->children[0];
    epc_cpt_node_t * arg1_primary_expr_node = arg1_expr_factor_node->children[0];
    epc_cpt_node_t * arg1_terminal_node = arg1_primary_expr_node->children[0];
    epc_cpt_node_t * arg1_keyword_node = arg1_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg1_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg1_keyword_node), epc_cpt_node_get_semantic_len(arg1_keyword_node)
    );

    // Comma
    epc_cpt_node_t * comma_node = chain_args_node->children[1];
    STRCMP_EQUAL("Comma", comma_node->name);
    STRNCMP_EQUAL(",", epc_cpt_node_get_semantic_content(comma_node), epc_cpt_node_get_semantic_len(comma_node));

    // Arg 2: '+'
    epc_cpt_node_t * arg2_def_expr_node = chain_args_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg2_def_expr_node->name);
    epc_cpt_node_t * arg2_expr_term_node = arg2_def_expr_node->children[0];
    epc_cpt_node_t * arg2_expr_factor_node = arg2_expr_term_node->children[0];
    epc_cpt_node_t * arg2_primary_expr_node = arg2_expr_factor_node->children[0];
    epc_cpt_node_t * arg2_terminal_node = arg2_primary_expr_node->children[0];
    epc_cpt_node_t * arg2_char_literal_node = arg2_terminal_node->children[0];
    STRCMP_EQUAL("CharLiteral", arg2_char_literal_node->name);
    STRNCMP_EQUAL(
        "'+'",
        epc_cpt_node_get_semantic_content(arg2_char_literal_node),
        epc_cpt_node_get_semantic_len(arg2_char_literal_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = chainl1_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a chainr1 combinator call
// Example GDL: MyChainr1Rule = chainr1(alpha, '-');
TEST(GdlParserTest, ParseChainr1CallRule)
{
    char const * gdl_input = "MyChainr1Rule = chainr1(alpha, '-');";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyChainr1Rule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to ChainR1Call
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * combinator_call_node = primary_expr_node->children[0];
    STRCMP_EQUAL("CombinatorCall", combinator_call_node->name);
    CHECK_EQUAL(1, combinator_call_node->children_count);

    epc_cpt_node_t * chainr1_call_node = combinator_call_node->children[0];
    STRCMP_EQUAL("ChainR1Call", chainr1_call_node->name);
    CHECK_EQUAL(4, chainr1_call_node->children_count); // KW_chainr1, LParen, ChainArgs, RParen

    // Check KW_chainr1
    epc_cpt_node_t * kw_chainr1_node = chainr1_call_node->children[0];
    STRCMP_EQUAL("chainr1", kw_chainr1_node->name); // Name matches keyword string
    STRNCMP_EQUAL(
        "chainr1", epc_cpt_node_get_semantic_content(kw_chainr1_node), epc_cpt_node_get_semantic_len(kw_chainr1_node)
    );

    // Check LParen
    epc_cpt_node_t * lparen_node = chainr1_call_node->children[1];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    // Check ChainArgs
    epc_cpt_node_t * chain_args_node = chainr1_call_node->children[2];
    STRCMP_EQUAL("ChainArgs", chain_args_node->name);
    CHECK_EQUAL(3, chain_args_node->children_count); // arg1, comma, arg2

    // Arg 1: alpha
    epc_cpt_node_t * arg1_def_expr_node = chain_args_node->children[0];
    STRCMP_EQUAL("DefinitionExpression", arg1_def_expr_node->name);
    epc_cpt_node_t * arg1_expr_term_node = arg1_def_expr_node->children[0];
    epc_cpt_node_t * arg1_expr_factor_node = arg1_expr_term_node->children[0];
    epc_cpt_node_t * arg1_primary_expr_node = arg1_expr_factor_node->children[0];
    epc_cpt_node_t * arg1_terminal_node = arg1_primary_expr_node->children[0];
    epc_cpt_node_t * arg1_keyword_node = arg1_terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", arg1_keyword_node->name);
    STRNCMP_EQUAL(
        "alpha", epc_cpt_node_get_semantic_content(arg1_keyword_node), epc_cpt_node_get_semantic_len(arg1_keyword_node)
    );

    // Comma
    epc_cpt_node_t * comma_node = chain_args_node->children[1];
    STRCMP_EQUAL("Comma", comma_node->name);
    STRNCMP_EQUAL(",", epc_cpt_node_get_semantic_content(comma_node), epc_cpt_node_get_semantic_len(comma_node));

    // Arg 2: '-'
    epc_cpt_node_t * arg2_def_expr_node = chain_args_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", arg2_def_expr_node->name);
    epc_cpt_node_t * arg2_expr_term_node = arg2_def_expr_node->children[0];
    epc_cpt_node_t * arg2_expr_factor_node = arg2_expr_term_node->children[0];
    epc_cpt_node_t * arg2_primary_expr_node = arg2_expr_factor_node->children[0];
    epc_cpt_node_t * arg2_terminal_node = arg2_primary_expr_node->children[0];
    epc_cpt_node_t * arg2_char_literal_node = arg2_terminal_node->children[0];
    STRCMP_EQUAL("CharLiteral", arg2_char_literal_node->name);
    STRNCMP_EQUAL(
        "'-'",
        epc_cpt_node_get_semantic_content(arg2_char_literal_node),
        epc_cpt_node_get_semantic_len(arg2_char_literal_node)
    );

    // Check RParen
    epc_cpt_node_t * rparen_node = chainr1_call_node->children[3];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a semantic action
// Example GDL: MyRule = alpha @my_action;
TEST(GdlParserTest, ParseSemanticActionRule)
{
    char const * gdl_input = "MyRule = alpha @my_action;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier (MyRule)
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyRule", epc_cpt_node_get_semantic_content(identifier_node), epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression (alpha)
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);

    // Check OptionalSemanticAction (for @my_action)
    epc_cpt_node_t * optional_semantic_action_node = rule_def_node->children[3];
    STRCMP_EQUAL("OptionalSemanticAction", optional_semantic_action_node->name);
    CHECK_EQUAL(1, optional_semantic_action_node->children_count);

    epc_cpt_node_t * semantic_action_node = optional_semantic_action_node->children[0];
    STRCMP_EQUAL("SemanticAction", semantic_action_node->name);
    CHECK_EQUAL(2, semantic_action_node->children_count); // AtSign and Identifier

    epc_cpt_node_t * at_sign_node = semantic_action_node->children[0];
    STRCMP_EQUAL("AtSign", at_sign_node->name);
    STRNCMP_EQUAL("@", epc_cpt_node_get_semantic_content(at_sign_node), epc_cpt_node_get_semantic_len(at_sign_node));

    epc_cpt_node_t * action_identifier_node = semantic_action_node->children[1];
    STRCMP_EQUAL("Identifier", action_identifier_node->name);
    STRNCMP_EQUAL(
        "my_action",
        epc_cpt_node_get_semantic_content(action_identifier_node),
        epc_cpt_node_get_semantic_len(action_identifier_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a number literal.
// Example GDL: MyNumberRule = 123;
TEST(GdlParserTest, ParseNumberLiteralRule)
{
    char const * gdl_input = "MyNumberRule = 123;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyNumberRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to NumberLiteral
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * number_literal_node = primary_expr_node->children[0];
    STRCMP_EQUAL("NumberLiteral", number_literal_node->name);
    STRNCMP_EQUAL(
        "123",
        epc_cpt_node_get_semantic_content(number_literal_node),
        epc_cpt_node_get_semantic_len(number_literal_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a parenthesized expression explicitly.
// Example GDL: MyParenthesizedRule = (alpha | digit);
TEST(GdlParserTest, ParseParenthesizedExpressionRule)
{
    char const * gdl_input = "MyParenthesizedRule = (alpha | digit);";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyParenthesizedRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to ParenthesizedExpression
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * parenthesized_expr_node = primary_expr_node->children[0];
    STRCMP_EQUAL("ParenthesizedExpression", parenthesized_expr_node->name);
    CHECK_EQUAL(3, parenthesized_expr_node->children_count); // LParen, DefinitionExpression, RParen

    epc_cpt_node_t * lparen_node = parenthesized_expr_node->children[0];
    STRCMP_EQUAL("LParen", lparen_node->name);
    STRNCMP_EQUAL("(", epc_cpt_node_get_semantic_content(lparen_node), epc_cpt_node_get_semantic_len(lparen_node));

    epc_cpt_node_t * inner_def_expr_node = parenthesized_expr_node->children[1];
    STRCMP_EQUAL("DefinitionExpression", inner_def_expr_node->name);
    CHECK_EQUAL(2, inner_def_expr_node->children_count); // Should contain (alpha | digit) structure

    epc_cpt_node_t * rparen_node = parenthesized_expr_node->children[2];
    STRCMP_EQUAL("RParen", rparen_node->name);
    STRNCMP_EQUAL(")", epc_cpt_node_get_semantic_content(rparen_node), epc_cpt_node_get_semantic_len(rparen_node));

    // Check the contents of the inner DefinitionExpression (alpha | digit)
    epc_cpt_node_t * inner_expr_term_alpha_node = inner_def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", inner_expr_term_alpha_node->name);
    epc_cpt_node_t * inner_keyword_alpha_node
        = inner_expr_term_alpha_node->children[0]->children[0]->children[0]->children[0];
    STRCMP_EQUAL("TerminalKeyword", inner_keyword_alpha_node->name);
    STRNCMP_EQUAL(
        "alpha",
        epc_cpt_node_get_semantic_content(inner_keyword_alpha_node),
        epc_cpt_node_get_semantic_len(inner_keyword_alpha_node)
    );

    epc_cpt_node_t * inner_many_alternatives_node = inner_def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", inner_many_alternatives_node->name);
    CHECK_EQUAL(1, inner_many_alternatives_node->children_count);

    epc_cpt_node_t * inner_alternative_part_node = inner_many_alternatives_node->children[0];
    STRCMP_EQUAL("AlternativePart", inner_alternative_part_node->name);
    epc_cpt_node_t * inner_keyword_digit_node
        = inner_alternative_part_node->children[1]->children[0]->children[0]->children[0]->children[0];
    STRCMP_EQUAL("TerminalKeyword", inner_keyword_digit_node->name);
    STRNCMP_EQUAL(
        "digit",
        epc_cpt_node_get_semantic_content(inner_keyword_digit_node),
        epc_cpt_node_get_semantic_len(inner_keyword_digit_node)
    );

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    epc_parse_session_destroy(&session);
}

// Test parsing a double keyword rule.
// Example GDL: MyDoubleRule = double;
TEST(GdlParserTest, ParseDoubleKeywordRule)
{
    char const * gdl_input = "MyDoubleRule = double;";
    session = parse(gdl_parser_root, gdl_input);

    if (session.result.is_error)
    {
        print_error_and_fail(session.result.data.error);
    }

    epc_cpt_node_t * program_node = session.result.data.success;
    STRCMP_EQUAL("Program", program_node->name);
    CHECK_EQUAL(2, program_node->children_count);

    epc_cpt_node_t * many_rule_defs_node = program_node->children[0];
    STRCMP_EQUAL("ManyRuleDefinitions", many_rule_defs_node->name);
    CHECK_EQUAL(1, many_rule_defs_node->children_count);

    epc_cpt_node_t * rule_def_node = many_rule_defs_node->children[0];
    STRCMP_EQUAL("RuleDefinition", rule_def_node->name);
    CHECK_EQUAL(5, rule_def_node->children_count);

    // Check Identifier
    epc_cpt_node_t * identifier_node = rule_def_node->children[0];
    STRCMP_EQUAL("Identifier", identifier_node->name);
    STRNCMP_EQUAL(
        "MyDoubleRule",
        epc_cpt_node_get_semantic_content(identifier_node),
        epc_cpt_node_get_semantic_len(identifier_node)
    );

    // Check EqualsChar
    epc_cpt_node_t * equals_node = rule_def_node->children[1];
    STRCMP_EQUAL("EqualsChar", equals_node->name);
    STRNCMP_EQUAL("=", epc_cpt_node_get_semantic_content(equals_node), epc_cpt_node_get_semantic_len(equals_node));

    // Check DefinitionExpression path to Keyword (double)
    epc_cpt_node_t * def_expr_node = rule_def_node->children[2];
    STRCMP_EQUAL("DefinitionExpression", def_expr_node->name);
    CHECK_EQUAL(2, def_expr_node->children_count);

    epc_cpt_node_t * expr_term_node = def_expr_node->children[0];
    STRCMP_EQUAL("ExpressionTerm", expr_term_node->name);
    CHECK_EQUAL(1, expr_term_node->children_count);

    epc_cpt_node_t * expr_factor_node = expr_term_node->children[0];
    STRCMP_EQUAL("ExpressionFactor", expr_factor_node->name);
    CHECK_EQUAL(2, expr_factor_node->children_count);

    epc_cpt_node_t * primary_expr_node = expr_factor_node->children[0];
    STRCMP_EQUAL("PrimaryExpression", primary_expr_node->name);
    CHECK_EQUAL(1, primary_expr_node->children_count);

    epc_cpt_node_t * terminal_node = primary_expr_node->children[0];
    STRCMP_EQUAL("Terminal", terminal_node->name);
    CHECK_EQUAL(1, terminal_node->children_count);

    epc_cpt_node_t * keyword_node = terminal_node->children[0];
    STRCMP_EQUAL("TerminalKeyword", keyword_node->name);
    STRNCMP_EQUAL(
        "double", epc_cpt_node_get_semantic_content(keyword_node), epc_cpt_node_get_semantic_len(keyword_node)
    );

    // Check OptionalRepetition (empty)
    epc_cpt_node_t * optional_repetition_node = expr_factor_node->children[1];
    STRCMP_EQUAL("OptionalRepetition", optional_repetition_node->name);
    CHECK_EQUAL(0, optional_repetition_node->children_count);

    // Check ManyAlternatives (empty)
    epc_cpt_node_t * many_alternatives_node = def_expr_node->children[1];
    STRCMP_EQUAL("ManyAlternatives", many_alternatives_node->name);
    CHECK_EQUAL(0, many_alternatives_node->children_count);

    // Check OptionalSemanticAction (empty)
    epc_cpt_node_t * optional_semantic_action_node = rule_def_node->children[3];
    STRCMP_EQUAL("OptionalSemanticAction", optional_semantic_action_node->name);
    CHECK_EQUAL(0, optional_semantic_action_node->children_count);

    // Check SemicolonChar
    epc_cpt_node_t * semicolon_node = rule_def_node->children[4];
    STRCMP_EQUAL("SemicolonChar", semicolon_node->name);
    STRNCMP_EQUAL(
        ";", epc_cpt_node_get_semantic_content(semicolon_node), epc_cpt_node_get_semantic_len(semicolon_node)
    );

    // Check EOI
    epc_cpt_node_t * eoi_node = program_node->children[1];
    STRCMP_EQUAL("EOI", eoi_node->name);
    CHECK_EQUAL(0, eoi_node->children_count);

    epc_parse_session_destroy(&session);
}