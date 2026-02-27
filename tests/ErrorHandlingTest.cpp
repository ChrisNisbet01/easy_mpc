#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
#include "easy_pc_private.h"

#include <stdlib.h> // For calloc, free
#include <string.h> // For strlen, strcmp
}

TEST_GROUP(ErrorHandling)
{
    epc_parse_session_t session = {0};
    epc_parse_result_t result;

    void setup() override
    {
        session = (epc_parse_session_t){0}; // Reset session before each test
    }

    epc_parse_result_t parse(epc_parser_t * parser, char const * input)
    {
        session = epc_parse_input(parser, input);
        return session.result;
    }

    void teardown() override
    {
        epc_parse_session_destroy(&session);
    }
};

TEST(ErrorHandling, PCharReportsNullInputError)
{
    epc_parser_t * p = epc_char(NULL, 'a');
    result = parse(p, NULL);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    CHECK_TRUE(result.data.error->input_position == NULL);
    STRCMP_EQUAL("non-NULL input string", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
}

TEST(ErrorHandling, PCharReportsEmptyInputError)
{
    epc_parser_t * p = epc_char(NULL, 'a');
    result = parse(p, "");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("", result.data.error->input_position);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("EOF", result.data.error->found);
}

TEST(ErrorHandling, PCharReportsMismatchError)
{
    char const * input_str = "b";
    epc_parser_t * p = epc_char(NULL, 'a');
    result = parse(p, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    STRCMP_EQUAL("b", result.data.error->input_position);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("b", result.data.error->found);
}

TEST(ErrorHandling, PStringReportsNullInputError)
{
    epc_parser_t * p = epc_string(NULL, "abc");
    result = parse(p, NULL);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Input string is NULL", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == NULL);
    STRCMP_EQUAL("non-NULL input string", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
}

TEST(ErrorHandling, PStringReportsTooShortInputError)
{
    char const * input_str = "ab";
    epc_parser_t * p = epc_string(NULL, "abc");
    result = parse(p, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("ab", result.data.error->input_position);
    STRCMP_EQUAL("abc", result.data.error->expected);
    STRCMP_EQUAL("ab", result.data.error->found);
}

TEST(ErrorHandling, PStringReportsMismatchError)
{
    char const * input_str = "axc";
    epc_parser_t * p = epc_string(NULL, "abc");
    result = parse(p, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected string", result.data.error->message);
    STRCMP_EQUAL("axc", result.data.error->input_position);
    STRCMP_EQUAL("abc", result.data.error->expected);
    STRCMP_EQUAL("axc", result.data.error->found);
}

TEST(ErrorHandling, PDigitReportsNullInputError)
{
    epc_parser_t * p = epc_digit(NULL);
    result = parse(p, NULL);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Input string is NULL", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == NULL);
    STRCMP_EQUAL("non-NULL input string", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
}

TEST(ErrorHandling, PDigitReportsEmptyInputError)
{
    epc_parser_t * p = epc_digit(NULL);
    result = parse(p, "");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("", result.data.error->input_position);
    STRCMP_EQUAL("digit", result.data.error->expected);
    STRCMP_EQUAL("EOF", result.data.error->found);
}

TEST(ErrorHandling, PDigitReportsMismatchError)
{
    char const * input_str = "a";
    epc_parser_t * p = epc_digit(NULL);
    result = parse(p, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    STRCMP_EQUAL("a", result.data.error->input_position);
    STRCMP_EQUAL("digit", result.data.error->expected);
    STRCMP_EQUAL("a", result.data.error->found);
}

TEST(ErrorHandling, POrReportsErrorWhenNoAlternatives)
{
    char const * input_str = "abc";
    epc_parser_t * p_or_parser = epc_or(NULL, 0);

    result = parse(p_or_parser, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("No alternatives provided to 'or' parser", result.data.error->message);
    STRCMP_EQUAL(input_str, result.data.error->input_position);
    STRCMP_EQUAL("or", result.data.error->expected); // The parser's name as expected
    STRCMP_EQUAL("N/A", result.data.error->found);
}

TEST(ErrorHandling, POrReportsErrorWhenAllAlternativesFail)
{
    char const * input_str = "abc";
    epc_parser_t * p_x = epc_char(NULL, 'x');
    epc_parser_t * p_y = epc_char(NULL, 'y');
    epc_parser_t * p_or_parser = epc_or(NULL, 2, p_x, p_y);

    result = parse(p_or_parser, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    // The furthest error should be from the last attempted alternative 'y' at position 'a'
    STRCMP_EQUAL("No alternative matched", result.data.error->message); // Updated expectation

    STRCMP_EQUAL(input_str, result.data.error->input_position);
    STRCMP_EQUAL("x or y", result.data.error->expected);
    STRCMP_EQUAL("abc", result.data.error->found);
}
