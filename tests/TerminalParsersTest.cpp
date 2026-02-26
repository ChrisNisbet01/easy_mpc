#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
#include "easy_pc_private.h"

#include <string.h> // For strlen, strcmp
}

TEST_GROUP(TerminalParsers)
{
    epc_parser_ctx_t * parse_ctx;

    void setup() override
    {
        // Setup transient parse context for parse_fn calls
        // This simulates the internal context creation in epc_parse_input()
        parse_ctx = (epc_parser_ctx_t *)calloc(1, sizeof(*parse_ctx)); // parser_ctx_t struct itself from heap
        CHECK_TRUE(parse_ctx != NULL);
        assign_input("test_input");
    }

    void assign_input(char const * input)
    {
        parse_ctx->input_start = input;
        parse_ctx->input_len = input != NULL ? strlen(input) : 0;
    }

    void teardown() override
    {
        // Free the parser_ctx_t struct itself
        free(parse_ctx);
        parse_ctx = NULL;
    }
};

TEST(TerminalParsers, PCharMatchesCorrectCharacter)
{
    epc_parser_t * p = epc_char(NULL, 'a');
    assign_input("abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("char", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, PCharDoesNotMatchIncorrectCharacter)
{
    epc_parser_t * p = epc_char(NULL, 'b');
    assign_input("abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PCharFailsOnEmptyInput)
{
    epc_parser_t * p = epc_char(NULL, 'a');
    assign_input("");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PCharFailsOnNullInput)
{
    epc_parser_t * p = epc_char(NULL, 'a');
    assign_input(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PStringMatchesCorrectString)
{
    epc_parser_t * p = epc_string(NULL, "hello");
    assign_input("hello world");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("string", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("hello", result.data.success->content, 5);
    LONGS_EQUAL(5, result.data.success->len);
}

TEST(TerminalParsers, PStringDoesNotMatchIncorrectString)
{
    epc_parser_t * p = epc_string(NULL, "world");
    assign_input("hello world");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PStringFailsWhenInputTooShort)
{
    epc_parser_t * p = epc_string(NULL, "hello");
    assign_input("hell");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PStringFailsOnEmptyInput)
{
    epc_parser_t * p = epc_string(NULL, "hello");
    assign_input("");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PStringFailsOnNullInput)
{
    epc_parser_t * p = epc_string(NULL, "hello");
    assign_input(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PDigitMatchesCorrectDigit)
{
    epc_parser_t * p = epc_digit(NULL);
    assign_input("123");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("digit", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("1", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, PDigitDoesNotMatchNonDigit)
{
    epc_parser_t * p = epc_digit(NULL);
    assign_input("abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PDigitFailsOnEmptyInput)
{
    epc_parser_t * p = epc_digit(NULL);
    assign_input("");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PDigitFailsOnNullInput)
{
    epc_parser_t * p = epc_digit(NULL);
    assign_input(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, POrMatchesFirstAlternative)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_b = epc_char(NULL, 'b');
    epc_parser_t * p_or_parser = epc_or(NULL, 2, p_a, p_b);
    assign_input("abc");

    epc_parse_result_t result = p_or_parser->parse_fn(p_or_parser, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("or", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("a", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, POrMatchesLaterAlternative)
{
    epc_parser_t * p_a = epc_char(NULL, 'x'); // Will fail
    epc_parser_t * p_b = epc_char(NULL, 'b'); // Will succeed
    epc_parser_t * p_or_parser = epc_or(NULL, 2, p_a, p_b);
    assign_input("bca");

    epc_parse_result_t result = p_or_parser->parse_fn(p_or_parser, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("or", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("b", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, POrFailsWhenAllAlternativesFail)
{
    epc_parser_t * p_a = epc_char(NULL, 'x');
    epc_parser_t * p_b = epc_char(NULL, 'y');
    epc_parser_t * p_or_parser = epc_or(NULL, 2, p_a, p_b);

    assign_input("abc");
    epc_parse_result_t result = p_or_parser->parse_fn(p_or_parser, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, POrFailsWithEmptyAlternativesList)
{
    epc_parser_t * p_or_parser = epc_or(NULL, 0);

    assign_input("abc");
    epc_parse_result_t result = p_or_parser->parse_fn(p_or_parser, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(TerminalParsers, PAndMatchesSequenceOfParsers)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_b = epc_char(NULL, 'b');
    epc_parser_t * p_c = epc_char(NULL, 'c');
    epc_parser_t * p_and_parser = epc_and(NULL, 3, p_a, p_b, p_c);

    assign_input("abcde");
    epc_parse_result_t result = p_and_parser->parse_fn(p_and_parser, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("and", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("abc", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
    LONGS_EQUAL(3, result.data.success->children_count);
    CHECK_TRUE(result.data.success->children[0] != NULL);
    CHECK_TRUE(result.data.success->children[1] != NULL);
    CHECK_TRUE(result.data.success->children[2] != NULL);
    STRNCMP_EQUAL("a", result.data.success->children[0]->content, 1);
    STRNCMP_EQUAL("b", result.data.success->children[1]->content, 1);
    STRNCMP_EQUAL("c", result.data.success->children[2]->content, 1);
}

TEST(TerminalParsers, PAndFailsIfFirstChildFails)
{
    epc_parser_t * p_x = epc_char(NULL, 'x'); // Will fail
    epc_parser_t * p_b = epc_char(NULL, 'b');
    epc_parser_t * p_c = epc_char(NULL, 'c');
    epc_parser_t * p_and_parser = epc_and(NULL, 3, p_x, p_b, p_c);
    char const * input_str = "abc";
    assign_input(input_str);

    epc_parse_result_t result = p_and_parser->parse_fn(p_and_parser, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == input_str); // Error at 'a'
    STRCMP_EQUAL("x", result.data.error->expected);
    STRCMP_EQUAL("a", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);
}

TEST(TerminalParsers, PAndFailsIfMiddleChildFails)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_x = epc_char(NULL, 'x'); // Will fail
    epc_parser_t * p_c = epc_char(NULL, 'c');
    epc_parser_t * p_and_parser = epc_and(NULL, 3, p_a, p_x, p_c);
    char const * input_str = "abc";

    assign_input(input_str);
    epc_parse_result_t result = p_and_parser->parse_fn(p_and_parser, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == (input_str + 1)); // Error at 'b'
    STRCMP_EQUAL("x", result.data.error->expected);
    STRCMP_EQUAL("b", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);
}

TEST(TerminalParsers, PAndFailsWithEmptySequenceList)
{
    epc_parser_t * p_and_parser = epc_and(NULL, 0);
    char const * input_str = "abc";

    assign_input(input_str);
    epc_parse_result_t result = p_and_parser->parse_fn(p_and_parser, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("No parsers in 'and' sequence", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == input_str);
    STRCMP_EQUAL("and", result.data.error->expected);
    STRCMP_EQUAL("N/A", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);
}

TEST(TerminalParsers, PSpaceMatchesSpace)
{
    epc_parser_t * p = epc_space(NULL);
    assign_input(" abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("space", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL(" ", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, PSpaceMatchesTab)
{
    epc_parser_t * p = epc_space(NULL);
    assign_input("\tabc");

    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("space", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("\t", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, PSpaceMatchesNewline)
{
    epc_parser_t * p = epc_space(NULL);
    assign_input("\nabc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("space", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("\n", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(TerminalParsers, PSpaceDoesNotMatchNonWhitespace)
{
    epc_parser_t * p = epc_space(NULL);
    char const * input_str = "abc";
    assign_input(input_str);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == input_str);
    STRCMP_EQUAL("whitespace", result.data.error->expected);
    STRCMP_EQUAL("a", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);
}

TEST(TerminalParsers, PSpaceFailsOnEmptyInput)
{
    epc_parser_t * p = epc_space(NULL);
    assign_input("");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("", result.data.error->input_position);
    STRCMP_EQUAL("whitespace", result.data.error->expected);
    STRCMP_EQUAL("EOF", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);
}

TEST(TerminalParsers, PSkipSkipsMultipleSpaces)
{
    epc_parser_t * p_s = epc_space(NULL);
    epc_parser_t * p = epc_skip(NULL, p_s);
    char const * input_str = "   abc";

    assign_input(input_str);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("skip", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    CHECK_TRUE(result.data.success->content == input_str);
    LONGS_EQUAL(3, result.data.success->len); // Skipped 3 spaces
}

TEST(TerminalParsers, PSkipSkipsZeroSpaces)
{
    epc_parser_t * p_s = epc_space(NULL);
    epc_parser_t * p = epc_skip(NULL, p_s);
    char const * input_str = "abc";

    assign_input(input_str);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("skip", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    CHECK_TRUE(result.data.success->content == input_str);
    LONGS_EQUAL(0, result.data.success->len); // Skipped 0 spaces
}

TEST(TerminalParsers, PSkipSkipsMixedWhitespace)
{
    epc_parser_t * p_s = epc_space(NULL);
    epc_parser_t * p = epc_skip(NULL, p_s);
    char const * input_str = " \t\n\r abc"; // Space, tab, newline, carriage return

    assign_input(input_str);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("skip", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    CHECK_TRUE(result.data.success->content == input_str);
    LONGS_EQUAL(5, result.data.success->len); // Skipped 5 chars
}

TEST(TerminalParsers, PSkipHandlesNullChildParser)
{
    epc_parser_t * p = epc_skip(NULL, NULL);
    char const * input_str = "abc";

    assign_input(input_str);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("p_skip received NULL child parser", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == input_str);
    STRCMP_EQUAL("skip", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
}

// NEW TEST GROUP FOR P_DOUBLE
TEST_GROUP(DoubleParser)
{
    epc_parser_ctx_t * parse_ctx = NULL;

    void setup() override
    {
        parse_ctx = (epc_parser_ctx_t *)calloc(1, sizeof(*parse_ctx));
        CHECK_TRUE(parse_ctx != NULL);
        assign_input("test_input");
    }

    void assign_input(char const * input)
    {
        parse_ctx->input_start = input;
        parse_ctx->input_len = input != NULL ? strlen(input) : 0;
    }

    void teardown() override
    {
        free(parse_ctx);
        parse_ctx = NULL;
    }
};

// Valid Double Tests
TEST(DoubleParser, PDoubleMatchesInteger)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("123abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("123", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesSimpleDecimal)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("123.45xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("123.45", result.data.success->content, 6);
    LONGS_EQUAL(6, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesLeadingDecimal)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input(".45xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL(".45", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesTrailingDecimal)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("123.xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("123.", result.data.success->content, 4);
    LONGS_EQUAL(4, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesPositiveSign)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("+123.45xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("+123.45", result.data.success->content, 7);
    LONGS_EQUAL(7, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesNegativeSign)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("-123xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("-123", result.data.success->content, 4);
    LONGS_EQUAL(4, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesExponentPositive)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("1.23e5xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("1.23e5", result.data.success->content, 6);
    LONGS_EQUAL(6, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesExponentNegative)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("1.23E-5xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("1.23E-5", result.data.success->content, 7);
    LONGS_EQUAL(7, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesExponentWithSign)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("-1e+2xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("-1e+2", result.data.success->content, 5);
    LONGS_EQUAL(5, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesZero)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("0xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("0", result.data.success->content, 1);
    LONGS_EQUAL(1, result.data.success->len);
}

TEST(DoubleParser, PDoubleMatchesZeroDecimal)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("0.0xyz");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_FALSE(result.is_error);
    CHECK_TRUE(result.data.success != NULL);
    STRCMP_EQUAL("double", result.data.success->tag);
    POINTERS_EQUAL(result.data.success->name, NULL); // Name should be NULL since we didn't set it
    STRNCMP_EQUAL("0.0", result.data.success->content, 3);
    LONGS_EQUAL(3, result.data.success->len);
}

// Invalid Double Tests
TEST(DoubleParser, PDoubleFailsOnNonNumeric)
{
    epc_parser_t * p = epc_double(NULL);
    char const * input = "abc";
    assign_input(input);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Expected a double", result.data.error->message);
    STRNCMP_EQUAL("a", result.data.error->found, 1);
    POINTERS_EQUAL(parse_ctx->furthest_error->input_position, input);
}

TEST(DoubleParser, PDoubleFailsOnEmptyInput)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("EOF", result.data.error->found);
}

TEST(DoubleParser, PDoubleFailsOnNullInput)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
}

TEST(DoubleParser, PDoubleFailsOnJustDecimalPoint)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input(".");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Expected a double", result.data.error->message);
    STRNCMP_EQUAL(".", result.data.error->found, 1);
}

TEST(DoubleParser, PDoubleFailsOnJustSign)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("+");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Expected a double", result.data.error->message);
    STRNCMP_EQUAL("+", result.data.error->found, 1);
}

TEST(DoubleParser, PDoubleFailsOnSignDecimal)
{
    epc_parser_t * p = epc_double(NULL);
    assign_input("+.");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, 0);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Expected a double", result.data.error->message);
    STRNCMP_EQUAL("+", result.data.error->found, 1); // Only '+' is reported as found
}
