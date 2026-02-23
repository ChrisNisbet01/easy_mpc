#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
    #include "easy_pc_private.h"
    #include <string.h> // For strlen, strcmp
    #include <stdlib.h> // For calloc, free
}

TEST_GROUP(CombinatorTest)
{
    void setup() override
    {
    }

    void teardown() override
    {
    }

    // Helper to create a transient parser_ctx_t for test cases
    epc_parser_ctx_t* create_transient_parse_ctx(const char* input_str)
    {
        epc_parser_ctx_t* ctx = (epc_parser_ctx_t*)calloc(1, sizeof(*ctx));
        CHECK_TRUE(ctx != NULL);
        ctx->input_start = input_str;
        ctx->input_len = ctx->input_start != NULL ? strlen(ctx->input_start) : 0;
        return ctx;
    }

    // Helper to destroy a transient parser_ctx_t
    void destroy_transient_parse_ctx(epc_parser_ctx_t* ctx)
    {
        free(ctx); // Free the parser_ctx_t struct itself
    }
};

TEST(CombinatorTest, PStarMatchesZero)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_star_a = epc_many(NULL, p_char_a);

    epc_parse_result_t result = p_star_a->parse_fn(p_star_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    STRCMP_EQUAL("many_parser", result.data.success->name);
    STRNCMP_EQUAL("", result.data.success->content, 0);
    LONGS_EQUAL(0, result.data.success->len);
    LONGS_EQUAL(0, result.data.success->children_count);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PStarMatchesOne)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("abc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_star_a = epc_many(NULL, p_char_a);

    epc_parse_result_t result = p_star_a->parse_fn(p_star_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    STRCMP_EQUAL("many_parser", result.data.success->name);
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
    LONGS_EQUAL(1, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PStarMatchesMultiple)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("aaabc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_star_a = epc_many(NULL, p_char_a);

    epc_parse_result_t result = p_star_a->parse_fn(p_star_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    STRCMP_EQUAL("many_parser", result.data.success->name);
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PStarMatchesMultipleThenFails)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("aaabbc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_star_a = epc_many(NULL, p_char_a);

    epc_parse_result_t result = p_star_a->parse_fn(p_star_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    STRCMP_EQUAL("many_parser", result.data.success->name);
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PPlusMatchesOne)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("abc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_plus_a = epc_plus(NULL, p_char_a);

    epc_parse_result_t result = p_plus_a->parse_fn(p_plus_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("plus", result.data.success->tag);
    STRCMP_EQUAL("plus_parser", result.data.success->name);
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
    LONGS_EQUAL(1, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PPlusMatchesMultiple)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("aaabc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_plus_a = epc_plus(NULL, p_char_a);

    epc_parse_result_t result = p_plus_a->parse_fn(p_plus_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("plus", result.data.success->tag);
    STRCMP_EQUAL("plus_parser", result.data.success->name);
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PPlusFailsOnZeroMatches)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("bbc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_plus_a = epc_plus(NULL, p_char_a);

    epc_parse_result_t result = p_plus_a->parse_fn(p_plus_a, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    STRCMP_EQUAL("bbc", result.data.error->input_position);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("b", result.data.error->found);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(CombinatorTest, PPlusMatchesMultipleThenFails)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("aaabbc");
    epc_parser_t* p_char_a = epc_char(NULL, 'a');
    epc_parser_t* p_plus_a = epc_plus(NULL, p_char_a);

    epc_parse_result_t result = p_plus_a->parse_fn(p_plus_a, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("plus", result.data.success->tag);
    STRCMP_EQUAL("plus_parser", result.data.success->name);
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);

    destroy_transient_parse_ctx(parse_ctx);
}