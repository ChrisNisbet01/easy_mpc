#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
    #include "easy_pc_private.h"
    #include <string.h> // For strlen, strcmp
    #include <stdlib.h> // For calloc, free
}

TEST_GROUP(CptPrinter)
{
    void setup() override
    {
    }

    void teardown() override
    {
    }

    // Helper to create a transient parser_ctx_t for test cases
    epc_parser_ctx_t* create_transient_parse_ctx(const char* input_str) {
        epc_parser_ctx_t* ctx = (epc_parser_ctx_t*)calloc(1, sizeof(*ctx));
        CHECK_TRUE(ctx != NULL);
        ctx->input_start = input_str;
        return ctx;
    }

    // Helper to destroy a transient parser_ctx_t
    void destroy_transient_parse_ctx(epc_parser_ctx_t* ctx) {
        free(ctx); // Free the parser_ctx_t struct itself
    }

    // Helper to parse and print
    char* parse_and_print(epc_parser_t* parser, const char* input_str) {
        epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(input_str);
        CHECK_TRUE(parse_ctx != NULL);

        epc_parse_result_t result = parser->parse_fn(parser, parse_ctx, input_str);
        if (result.is_error) {
            std::cerr << "Parse Error: " << (result.data.error ? result.data.error->message : "Unknown error") << " at '"
                      << (result.data.error && result.data.error->input_position ? result.data.error->input_position : "NULL")
                      << "', expected '" << (result.data.error && result.data.error->expected ? result.data.error->expected : "N/A")
                      << "', found '" << (result.data.error && result.data.error->found ? result.data.error->found : "N/A")
                      << "'" << std::endl;
            destroy_transient_parse_ctx(parse_ctx);
            return NULL;
        }
        char* printed_cpt = epc_cpt_to_string(result.data.success);

        char* copied_cpt = NULL;
        if (printed_cpt != NULL)
        {
            copied_cpt = strdup(printed_cpt);
        }

        destroy_transient_parse_ctx(parse_ctx);
        return copied_cpt; // Caller is responsible for free(copied_cpt)
    }
};

TEST(CptPrinter, PrintsSingleCharNode)
{
    epc_parser_t* p_a = epc_char(NULL, 'a');
    char* printed_cpt = parse_and_print(p_a, "abc");
    CHECK_TRUE(printed_cpt != NULL);

    const char* expected_output =
        "<char> (char_parser) 'a' (len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsSimpleAndNode)
{
    epc_parser_t* p_a = epc_char(NULL, 'a');
    epc_parser_t* p_b = epc_char(NULL, 'b');
    epc_parser_t* p_c = epc_char(NULL, 'c');
    epc_parser_t* p_and_parser = epc_and(NULL, 3, p_a, p_b, p_c);

    char* printed_cpt = parse_and_print(p_and_parser, "abcde");
    CHECK_TRUE(printed_cpt != NULL);

    const char* expected_output =
        "<and> (and_parser) 'abc' (len=3)\n"
        "    <char> (char_parser) 'a' (len=1)\n"
        "    <char> (char_parser) 'b' (len=1)\n"
        "    <char> (char_parser) 'c' (len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsAndNodeWithNestedOr)
{
    epc_parser_t* p_digit_p = epc_digit(NULL);
    epc_parser_t* p_plus = epc_char(NULL, '+');
    epc_parser_t* p_minus = epc_char(NULL, '-');
    epc_parser_t* p_op = epc_or(NULL, 2, p_plus, p_minus);
    epc_parser_t* p_expr = epc_and(NULL, 3, p_digit_p, p_op, p_digit_p);

    char* printed_cpt = parse_and_print(p_expr, "1+2");
    CHECK_TRUE(printed_cpt != NULL);

    const char* expected_output =
        "<and> (and_parser) '1+2' (len=3)\n"
        "    <digit> (digit_parser) '1' (len=1)\n"
        "    <or> (or_parser) '+' (len=1)\n"
        "        <char> (char_parser) '+' (len=1)\n"
        "    <digit> (digit_parser) '2' (len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsSingleSkipNodeWithTwoSpaces)
{
    epc_parser_t* p_s = epc_space(NULL);
    epc_parser_t* p = epc_skip(NULL, p_s);
    char* printed_cpt = parse_and_print(p, "  abc");
    CHECK_TRUE(printed_cpt != NULL);

    const char* expected_output =
        "<skip> (skip_parser) '  ' (len=2)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}

TEST(CptPrinter, PrintsSingleSkipNodeWithSingleSpace)
{
    epc_parser_t* p_s = epc_space(NULL);
    epc_parser_t* p = epc_skip(NULL, p_s);
    char* printed_cpt = parse_and_print(p, " abc");
    CHECK_TRUE(printed_cpt != NULL);

    const char* expected_output =
        "<skip> (skip_parser) ' ' (len=1)\n";
    STRCMP_EQUAL(expected_output, printed_cpt);

    free(printed_cpt);
}