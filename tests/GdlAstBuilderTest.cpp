#include "CppUTest/TestHarness.h"
#include "easy_pc/easy_pc.h"

extern "C" {
#include "gdl_parser.h"
#include "gdl_compiler_ast_actions.h"
}

#include <string.h>
#include <stdio.h>

TEST_GROUP(GdlAstBuilderTest)
{
    epc_parser_list *parser_list;
    epc_parser_t *gdl_grammar;
    epc_parse_session_t session;
    epc_ast_hook_registry_t * ast_registry = NULL;
    epc_ast_result_t ast_build_result = {0};

    void setup() override
    {
        parser_list = epc_parser_list_create();
        gdl_grammar = create_gdl_parser(parser_list);
        ast_registry = epc_ast_hook_registry_create(GDL_AST_ACTION_MAX);
        gdl_ast_hook_registry_init(ast_registry, NULL); // No specific user_data needed
    }

    void teardown() override
    {
        epc_parse_session_destroy(&session);
        epc_parser_list_free(parser_list);
        epc_ast_hook_registry_free(ast_registry);
    }
};

TEST(GdlAstBuilderTest, SimpleRuleStringLiteral)
{
    const char *gdl_input = "MyRule = \"hello\";";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    // Further assertions can be added here to check the structure of the AST
    // For now, just checking root and no errors
}

TEST(GdlAstBuilderTest, RuleDefinitionWithCharRange)
{
    const char *gdl_input = "MyRangeRule = [a-z];";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MyRangeRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *alternative_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, alternative_node->type);
    CHECK(alternative_node->data.alternative.alternatives.count == 1);

    gdl_ast_list_node_t *alternative_list_node = alternative_node->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node = alternative_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node->type);
    CHECK(sequence_node->data.sequence.elements.count == 1);

    gdl_ast_list_node_t *sequence_list_node = sequence_node->data.sequence.elements.head;
    gdl_ast_node_t *char_range_node = sequence_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_RANGE, char_range_node->type);
    LONGS_EQUAL('a', char_range_node->data.char_range.start_char);
    LONGS_EQUAL('z', char_range_node->data.char_range.end_char);
}

TEST(GdlAstBuilderTest, RuleDefinitionWithSemanticAction)
{
    const char *gdl_input = "MyActionRule = 'a' @my_action;";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MyActionRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *definition_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, definition_node->type);
    CHECK(definition_node->data.alternative.alternatives.count == 1);

    gdl_ast_list_node_t *alternative_list_node = definition_node->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node = alternative_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node->type);
    CHECK(sequence_node->data.sequence.elements.count == 1);

    gdl_ast_list_node_t *sequence_list_node = sequence_node->data.sequence.elements.head;
    gdl_ast_node_t * terminal_node = sequence_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, terminal_node->type);

    gdl_ast_node_t * char_literal_node = terminal_node->data.terminal.expression;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, char_literal_node->type);
    LONGS_EQUAL('a', char_literal_node->data.char_literal.value);

    gdl_ast_node_t *semantic_action_node = rule_def_node->data.rule_def.semantic_action;
    CHECK(semantic_action_node != NULL);
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEMANTIC_ACTION, semantic_action_node->type);
    STRCMP_EQUAL("my_action", semantic_action_node->data.semantic_action.action_name);
}

TEST(GdlAstBuilderTest, RuleDefinitionWithSequence)
{
    const char *gdl_input = "MySeqRule = 'a' 'b';";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MySeqRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *definition_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, definition_node->type);
    CHECK(definition_node->data.alternative.alternatives.count == 1);

    gdl_ast_list_node_t *alternative_list_node = definition_node->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node = alternative_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node->type);
    CHECK(sequence_node->data.sequence.elements.count == 2);

    gdl_ast_list_node_t *first_sequence_element = sequence_node->data.sequence.elements.head;
    gdl_ast_node_t * first_terminal_node = first_sequence_element->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, first_terminal_node->type);

    gdl_ast_node_t *first_char_literal_node = first_terminal_node->data.terminal.expression;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, first_char_literal_node->type);
    LONGS_EQUAL('a', first_char_literal_node->data.char_literal.value);

    gdl_ast_list_node_t *second_sequence_element = first_sequence_element->next;
    gdl_ast_node_t * second_terminal_node = second_sequence_element->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, second_terminal_node->type);

    gdl_ast_node_t *second_char_literal_node = second_terminal_node->data.terminal.expression;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, second_char_literal_node->type);
    LONGS_EQUAL('b', second_char_literal_node->data.char_literal.value);
}

TEST(GdlAstBuilderTest, RuleDefinitionWithAlternative)
{
    const char *gdl_input = "MyAltRule = 'a' | 'b';";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MyAltRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *definition_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, definition_node->type);
    CHECK(definition_node->data.alternative.alternatives.count == 2);

    gdl_ast_list_node_t *first_alternative = definition_node->data.alternative.alternatives.head;
    gdl_ast_node_t *first_sequence = first_alternative->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, first_sequence->type);
    CHECK(first_sequence->data.sequence.elements.count == 1);
    gdl_ast_node_t * terminal_node = first_sequence->data.sequence.elements.head->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, terminal_node->type);

    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, terminal_node->data.terminal.expression->type);
    LONGS_EQUAL('a', terminal_node->data.terminal.expression->data.char_literal.value);

    gdl_ast_list_node_t *second_alternative = first_alternative->next;
    gdl_ast_node_t *second_sequence = second_alternative->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, second_sequence->type);
    CHECK(second_sequence->data.sequence.elements.count == 1);
    terminal_node = second_sequence->data.sequence.elements.head->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, terminal_node->type);

    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, terminal_node->data.terminal.expression->type);
    LONGS_EQUAL('b', terminal_node->data.terminal.expression->data.char_literal.value);
}

TEST(GdlAstBuilderTest, RuleDefinitionWithRepetition)
{
    const char *gdl_input = "MyStarRule = 'a'*;";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MyStarRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *definition_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, definition_node->type);
    CHECK(definition_node->data.alternative.alternatives.count == 1);

    gdl_ast_list_node_t *alternative_list_node = definition_node->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node = alternative_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node->type);
    CHECK(sequence_node->data.sequence.elements.count == 1);

    gdl_ast_list_node_t *sequence_list_node = sequence_node->data.sequence.elements.head;
    gdl_ast_node_t *repetition_expression_node = sequence_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_REPETITION_EXPRESSION, repetition_expression_node->type);

    gdl_ast_node_t *terminal_node = repetition_expression_node->data.repetition_expr.expression;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, terminal_node->type);
    gdl_ast_node_t * expression_node = terminal_node->data.terminal.expression;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, expression_node->type);
    LONGS_EQUAL('a', expression_node->data.char_literal.value);

    gdl_ast_node_t *repetition_op_node = repetition_expression_node->data.repetition_expr.repetition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_REPETITION_OPERATOR, repetition_op_node->type);
    LONGS_EQUAL('*', repetition_op_node->data.repetition_op.operator_char);
}

TEST(GdlAstBuilderTest, RuleDefinitionWithComplexOptional)
{
    const char *gdl_input = "MyOptRule = ('a' | 'b')?;";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MyOptRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *definition_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, definition_node->type);
    CHECK(definition_node->data.alternative.alternatives.count == 1);

    gdl_ast_list_node_t *alternative_list_node = definition_node->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node = alternative_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node->type);
    CHECK(sequence_node->data.sequence.elements.count == 1);

    gdl_ast_list_node_t *sequence_list_node = sequence_node->data.sequence.elements.head;
    gdl_ast_node_t *repetition_expression_node = sequence_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_REPETITION_EXPRESSION, repetition_expression_node->type);

    gdl_ast_node_t *expression_node = repetition_expression_node->data.repetition_expr.expression;
    // This expression_node should be the ALTERNATIVE for ('a' | 'b')
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, expression_node->type);
    CHECK(expression_node->data.alternative.alternatives.count == 2);

    gdl_ast_list_node_t *first_alt_list_node_in_expr = expression_node->data.alternative.alternatives.head;
    gdl_ast_node_t *first_sequence_node_in_expr = first_alt_list_node_in_expr->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, first_sequence_node_in_expr->type);
    CHECK(first_sequence_node_in_expr->data.sequence.elements.count == 1);
    gdl_ast_node_t * first_terminal_node = first_sequence_node_in_expr->data.sequence.elements.head->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, first_terminal_node->type);

    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, first_terminal_node->data.terminal.expression->type);
    LONGS_EQUAL('a', first_terminal_node->data.terminal.expression->data.char_literal.value);

    gdl_ast_list_node_t *second_alt_list_node_in_expr = first_alt_list_node_in_expr->next;
    gdl_ast_node_t *second_sequence_node_in_expr = second_alt_list_node_in_expr->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, second_sequence_node_in_expr->type);
    CHECK(second_sequence_node_in_expr->data.sequence.elements.count == 1);
    gdl_ast_node_t * second_terminal_node = second_sequence_node_in_expr->data.sequence.elements.head->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_TERMINAL, second_terminal_node->type);

    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, second_terminal_node->data.terminal.expression->type);
    LONGS_EQUAL('b', second_terminal_node->data.terminal.expression->data.char_literal.value);


    gdl_ast_node_t *repetition_op_node = repetition_expression_node->data.repetition_expr.repetition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_REPETITION_OPERATOR, repetition_op_node->type);
    LONGS_EQUAL('?', repetition_op_node->data.repetition_op.operator_char);
}

#if 0
TEST(GdlAstBuilderTest, RuleDefinitionWithOptionalAbsent)
{
    const char *gdl_input = "MyOptAbsentRule = optional('a');";
    session = epc_parse_input(gdl_grammar, gdl_input);

    CHECK_FALSE(session.result.is_error);
    ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

    CHECK_FALSE(ast_build_result.has_error);
    CHECK(ast_build_result.ast_root != NULL);
    gdl_ast_node_t * ast_root = (gdl_ast_node_t *)ast_build_result.ast_root;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_PROGRAM, ast_root->type);

    gdl_ast_node_t *program_node = (gdl_ast_node_t *)ast_build_result.ast_root;
    CHECK(program_node->data.program.rules.count == 1);

    gdl_ast_list_node_t *rule_list_node = program_node->data.program.rules.head;
    gdl_ast_node_t *rule_def_node = rule_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_RULE_DEFINITION, rule_def_node->type);
    STRCMP_EQUAL("MyOptAbsentRule", rule_def_node->data.rule_def.name);

    gdl_ast_node_t *definition_node = rule_def_node->data.rule_def.definition;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, definition_node->type);
    CHECK(definition_node->data.alternative.alternatives.count == 1);
    gdl_ast_list_node_t *alternative_list_node = definition_node->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node = alternative_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node->type);
    CHECK(sequence_node->data.sequence.elements.count == 1);

    gdl_ast_list_node_t *sequence_list_node = sequence_node->data.sequence.elements.head;
    gdl_ast_node_t *optional_expression_node = sequence_list_node->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_OPTIONAL_EXPRESSION, optional_expression_node->type);

    gdl_ast_node_t * optional_expr_content = optional_expression_node->data.optional.expr;
    CHECK(optional_expr_content != NULL); // Expecting 'a' here
    LONGS_EQUAL(GDL_AST_NODE_TYPE_ALTERNATIVE, optional_expr_content->type);
    CHECK(optional_expr_content->data.alternative.alternatives.count == 1);

    gdl_ast_list_node_t *alternative_list_node_in_optional = optional_expr_content->data.alternative.alternatives.head;
    gdl_ast_node_t *sequence_node_in_optional = alternative_list_node_in_optional->item;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_SEQUENCE, sequence_node_in_optional->type);
    CHECK(sequence_node_in_optional->data.sequence.elements.count == 1);

    gdl_ast_list_node_t *sequence_list_node_in_optional = sequence_node_in_optional->data.sequence.elements.head;
    gdl_ast_node_t * terminal_node = sequence_list_node_in_optional->item;
    gdl_ast_node_t * char_literal_node_in_optional = terminal_node->data.terminal.expression;
    LONGS_EQUAL(GDL_AST_NODE_TYPE_CHAR_LITERAL, char_literal_node_in_optional->type);
    LONGS_EQUAL('a', char_literal_node_in_optional->data.char_literal.value);
}
#endif

