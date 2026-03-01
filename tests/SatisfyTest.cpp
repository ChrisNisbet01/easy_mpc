
#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
#include "easy_pc_private.h"

#include <stdlib.h> // For calloc, free
#include <string.h> // For strlen, strcmp
}

TEST_GROUP(SatisfyTest)
{
    epc_parse_session_t session = {0};
    epc_parse_result_t result;
    epc_parser_list * list = NULL;

    void setup() override
    {
        session = (epc_parse_session_t){0}; // Reset session before each test
        list = epc_parser_list_create();
    }

    epc_parse_result_t parse(epc_parser_t * parser, char const * input)
    {
        void * user_ctx = NULL; // No user context for these tests

        session = epc_parse_str(parser, input, user_ctx);
        return session.result;
    }

    void teardown() override
    {
        epc_parse_session_destroy(&session);
        epc_parser_list_free(list);
    }

    static bool is_char_a(epc_cpt_node_t * node, epc_parser_ctx_t * parse_ctx, void * user_ctx)
    {
        (void)parse_ctx;
        (void)user_ctx;
        return node->len == 1 && node->content[0] == 'a';
    }

    static bool is_length_3(epc_cpt_node_t * node, epc_parser_ctx_t * parse_ctx, void * user_ctx)
    {
        (void)parse_ctx;
        (void)user_ctx;
        return node->len == 3;
    }
};

TEST(SatisfyTest, PSatisfyMatchesCorrectToken)
{
    epc_parser_t * p_any = epc_any(NULL);
    epc_parser_t * p_satisfy = epc_satisfy_l(list, "satisfy_a", p_any, "expected 'a'", is_char_a, NULL);

    result = parse(p_satisfy, "abc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("satisfy", result.data.success->tag);
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(SatisfyTest, PSatisfyFailsOnPredicateFalse)
{
    epc_parser_t * p_any = epc_any(NULL);
    epc_parser_t * p_satisfy = epc_satisfy_l(list, "satisfy_a", p_any, "expected 'a'", is_char_a, NULL);

    result = parse(p_satisfy, "bbc");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Predicate function returned false", result.data.error->message);
    STRCMP_EQUAL("expected 'a'", result.data.error->expected);
    STRCMP_EQUAL("token 'b'", result.data.error->found);
}

TEST(SatisfyTest, PSatisfyFailsWhenTokenParserFails)
{
    epc_parser_t * p_char_b = epc_char(NULL, 'b');
    epc_parser_t * p_satisfy = epc_satisfy_l(list, "satisfy_a", p_char_b, "expected 'a'", is_char_a, NULL);

    result = parse(p_satisfy, "abc");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Failed to match the satisfy token parser", result.data.error->message);
    STRCMP_EQUAL("b", result.data.error->expected);
    STRCMP_EQUAL("a", result.data.error->found);
}

TEST(SatisfyTest, PSatisfyWithComplexToken)
{
    epc_parser_t * p_digit = epc_digit(NULL);
    epc_parser_t * p_three_digits = epc_count(NULL, 3, p_digit);
    epc_parser_t * p_satisfy
        = epc_satisfy_l(list, "three_digits", p_three_digits, "expected 3 digits", is_length_3, NULL);

    // Success case
    result = parse(p_satisfy, "12345");
    CHECK_FALSE(result.is_error);
    STRNCMP_EQUAL("123", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);

    // Failure case (only 2 digits)
    result = parse(p_satisfy, "12");
    CHECK_TRUE(result.is_error);
    STRCMP_EQUAL("Failed to match the satisfy token parser", result.data.error->message);
}

TEST(SatisfyTest, PSatisfyPreservesSemanticOffsets)
{
    epc_parser_t * p_digit = epc_digit(NULL);
    epc_parser_t * p_lexeme = epc_lexeme(NULL, p_digit);
    epc_parser_t * p_satisfy = epc_satisfy_l(list, "satisfy_digit", p_lexeme, "expected digit", is_char_a, NULL);

    // We'll use a predicate that always returns true for this test
    auto always_true = [](epc_cpt_node_t * node, epc_parser_ctx_t * parse_ctx, void * user_ctx) -> bool {
        (void)node;
        (void)parse_ctx;
        (void)user_ctx;
        return true;
    };

    epc_parser_t * p_satisfy_ok = epc_satisfy_l(list, "satisfy_ok", p_lexeme, "ok", always_true, NULL);

    result = parse(p_satisfy_ok, "  1  ");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRNCMP_EQUAL("  1  ", result.data.success->content, 5);
    LONGS_EQUAL(5, result.data.success->len);
    LONGS_EQUAL(2, result.data.success->semantic_start_offset);
    LONGS_EQUAL(2, result.data.success->semantic_end_offset);
}
