#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
#include "easy_pc_private.h"

#include <stdlib.h> // For calloc, free
#include <string.h> // For strlen, strcmp
}

TEST_GROUP(CptPrinter)
{
    epc_parse_session_t session = {0};

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

    // Helper to parse and print
    char * parse_and_print(epc_parser_t * parser, char const * input_str)
    {
        epc_parse_result_t result = parse(parser, input_str);

        if (result.is_error)
        {
            std::cerr << "Parse Error: " << (result.data.error ? result.data.error->message : "Unknown error")
                      << " at '"
                      << (result.data.error && result.data.error->input_position ? result.data.error->input_position
                                                                                 : "NULL")
                      << "', expected '"
                      << (result.data.error && result.data.error->expected ? result.data.error->expected : "N/A")
                      << "', found '"
                      << (result.data.error && result.data.error->found ? result.data.error->found : "N/A") << "'"
                      << std::endl;
            return NULL;
        }
        char * printed_cpt = epc_cpt_to_string(session.internal_parse_ctx, result.data.success);

        char * copied_cpt = NULL;
        if (printed_cpt != NULL)
        {
            copied_cpt = strdup(printed_cpt);
        }

        return copied_cpt; // Caller is responsible for free(copied_cpt)
    }
};

TEST(CptPrinter, PrintsSingleCharNode)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    char * printed_cpt = parse_and_print(p_a, "abc");
    CHECK_TRUE(printed_cpt != NULL);

    char const * expected_output = "<char> (char) 'a' (line=0, col=0, len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsSimpleAndNode)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_b = epc_char(NULL, 'b');
    epc_parser_t * p_c = epc_char(NULL, 'c');
    epc_parser_t * p_and_parser = epc_and(NULL, 3, p_a, p_b, p_c);

    char * printed_cpt = parse_and_print(p_and_parser, "abcde");
    CHECK_TRUE(printed_cpt != NULL);

    char const * expected_output = "<and> (and) 'abc' (line=0, col=0, len=3)\n"
                                   "    <char> (char) 'a' (line=0, col=0, len=1)\n"
                                   "    <char> (char) 'b' (line=0, col=1, len=1)\n"
                                   "    <char> (char) 'c' (line=0, col=2, len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsAndNodeWithNestedOr)
{
    epc_parser_t * p_digit_p = epc_digit(NULL);
    epc_parser_t * p_plus = epc_char(NULL, '+');
    epc_parser_t * p_minus = epc_char(NULL, '-');
    epc_parser_t * p_op = epc_or(NULL, 2, p_plus, p_minus);
    epc_parser_t * p_expr = epc_and(NULL, 3, p_digit_p, p_op, p_digit_p);

    char * printed_cpt = parse_and_print(p_expr, "1+2");
    CHECK_TRUE(printed_cpt != NULL);

    char const * expected_output = "<and> (and) '1+2' (line=0, col=0, len=3)\n"
                                   "    <digit> (digit) '1' (line=0, col=0, len=1)\n"
                                   "    <or> (or) '+' (line=0, col=1, len=1)\n"
                                   "        <char> (char) '+' (line=0, col=1, len=1)\n"
                                   "    <digit> (digit) '2' (line=0, col=2, len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsSingleSkipNodeWithTwoSpaces)
{
    epc_parser_t * p_s = epc_space(NULL);
    epc_parser_t * p = epc_skip(NULL, p_s);
    char * printed_cpt = parse_and_print(p, "  abc");
    CHECK_TRUE(printed_cpt != NULL);

    char const * expected_output = "<skip> (skip) '  ' (line=0, col=0, len=2)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsSingleSkipNodeWithSingleSpace)
{
    epc_parser_t * p_s = epc_space(NULL);
    epc_parser_t * p = epc_skip(NULL, p_s);
    char * printed_cpt = parse_and_print(p, " abc");
    CHECK_TRUE(printed_cpt != NULL);

    char const * expected_output = "<skip> (skip) ' ' (line=0, col=0, len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}