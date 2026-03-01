#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
#include "easy_pc_private.h"

#include <stdlib.h> // For calloc, free
#include <string.h> // For strlen, strcmp
}

TEST_GROUP(CombinatorTest)
{
    epc_parse_session_t session = {0};
    epc_parse_result_t result;

    void setup() override
    {
        session = (epc_parse_session_t){0}; // Reset session before each test
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
    }
};

TEST(CombinatorTest, PStarMatchesZero)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_star_a = epc_many(NULL, p_char_a);

    result = parse(p_star_a, "");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("", result.data.success->content, 0);
    LONGS_EQUAL(0, result.data.success->len);
    LONGS_EQUAL(0, result.data.success->children_count);
}

TEST(CombinatorTest, PStarMatchesOne)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_star_a = epc_many(NULL, p_char_a);

    result = parse(p_star_a, "abc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
    LONGS_EQUAL(1, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
}

TEST(CombinatorTest, PStarMatchesMultiple)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_star_a = epc_many(NULL, p_char_a);

    result = parse(p_star_a, "aaabc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);
}

TEST(CombinatorTest, PStarMatchesMultipleThenFails)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_star_a = epc_many(NULL, p_char_a);

    result = parse(p_star_a, "aaabbc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("many", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);
}

TEST(CombinatorTest, PPlusMatchesOne)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_plus_a = epc_plus(NULL, p_char_a);

    result = parse(p_plus_a, "abc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("plus", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
    LONGS_EQUAL(1, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
}

TEST(CombinatorTest, PPlusMatchesMultiple)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_plus_a = epc_plus(NULL, p_char_a);

    result = parse(p_plus_a, "aaabc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("plus", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);
}

TEST(CombinatorTest, PPlusFailsOnZeroMatches)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_plus_a = epc_plus(NULL, p_char_a);

    result = parse(p_plus_a, "bbc");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    STRCMP_EQUAL("bbc", result.data.error->input_position);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("b", result.data.error->found);
}

TEST(CombinatorTest, PPlusMatchesMultipleThenFails)
{
    epc_parser_t * p_char_a = epc_char(NULL, 'a');
    epc_parser_t * p_plus_a = epc_plus(NULL, p_char_a);

    result = parse(p_plus_a, "aaabbc");

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("plus", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("aaa", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("a", result.data.success->children[2]->content, 1);
}
