#include "CppUTest/TestHarness.h"

#include <iostream>

extern "C" {
    #include "easy_pc_private.h"
    #include <string.h> // For strlen, strcmp
    #include <stdlib.h> // For calloc, free
}

TEST_GROUP(ErrorHandling)
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
};

TEST(ErrorHandling, PCharReportsNullInputError)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(NULL);
    epc_parser_t* p = epc_char(NULL, 'a');
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, NULL);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Input is NULL", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == NULL);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
     // Furthest error should be updated
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PCharReportsEmptyInputError)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("");
    epc_parser_t* p = epc_char(NULL, 'a');
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, "");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("", result.data.error->input_position);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("EOF", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PCharReportsMismatchError)
{
    const char* input_str = "b";
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(input_str);
    epc_parser_t* p = epc_char(NULL, 'a');
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    STRCMP_EQUAL("b", result.data.error->input_position);
    STRCMP_EQUAL("a", result.data.error->expected);
    STRCMP_EQUAL("b", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PStringReportsNullInputError)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(NULL);
    epc_parser_t* p = epc_string(NULL, "abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, NULL);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Input is NULL", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == NULL);
    STRCMP_EQUAL("abc", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PStringReportsTooShortInputError)
{
    const char* input_str = "ab";
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(input_str);
    epc_parser_t* p = epc_string(NULL, "abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("ab", result.data.error->input_position);
    STRCMP_EQUAL("abc", result.data.error->expected);
    STRCMP_EQUAL("ab", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PStringReportsMismatchError)
{
    const char* input_str = "axc";
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(input_str);
    epc_parser_t* p = epc_string(NULL, "abc");
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected string", result.data.error->message);
    STRCMP_EQUAL("axc", result.data.error->input_position);
    STRCMP_EQUAL("abc", result.data.error->expected);
    STRCMP_EQUAL("axc", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PDigitReportsNullInputError)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(NULL);
    epc_parser_t* p = epc_digit(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, NULL);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Input is NULL", result.data.error->message);
    CHECK_TRUE(result.data.error->input_position == NULL);
    STRCMP_EQUAL("digit", result.data.error->expected);
    STRCMP_EQUAL("NULL", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PDigitReportsEmptyInputError)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("");
    epc_parser_t* p = epc_digit(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, "");

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected end of input", result.data.error->message);
    STRCMP_EQUAL("", result.data.error->input_position);
    STRCMP_EQUAL("digit", result.data.error->expected);
    STRCMP_EQUAL("EOF", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, PDigitReportsMismatchError)
{
    const char* input_str = "a";
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(input_str);
    epc_parser_t* p = epc_digit(NULL);
    epc_parse_result_t result = p->parse_fn(p, parse_ctx, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("Unexpected character", result.data.error->message);
    STRCMP_EQUAL("a", result.data.error->input_position);
    STRCMP_EQUAL("digit", result.data.error->expected);
    STRCMP_EQUAL("a", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, POrReportsErrorWhenNoAlternatives)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("abc");
    epc_parser_t* p_or_parser = epc_or(NULL, 0);
    const char* input_str = "abc";

    epc_parse_result_t result = p_or_parser->parse_fn(p_or_parser, parse_ctx, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    STRCMP_EQUAL("No alternatives provided to 'or' parser", result.data.error->message);
    STRCMP_EQUAL(input_str, result.data.error->input_position);
    STRCMP_EQUAL("or_parser", result.data.error->expected); // The parser's name as expected
    STRCMP_EQUAL("N/A", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, POrReportsErrorWhenAllAlternativesFail)
{
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx("abc");
    epc_parser_t* p_x = epc_char(NULL, 'x');
    epc_parser_t* p_y = epc_char(NULL, 'y');
    epc_parser_t* p_or_parser = epc_or(NULL, 2, p_x, p_y);
    const char* input_str = "abc";

    epc_parse_result_t result = p_or_parser->parse_fn(p_or_parser, parse_ctx, input_str);

    CHECK_TRUE(result.is_error);
    CHECK_TRUE(result.data.error != NULL);
    // The furthest error should be from the last attempted alternative 'y' at position 'a'
    STRCMP_EQUAL("No alternative matched", result.data.error->message); // Updated expectation

    STRCMP_EQUAL(input_str, result.data.error->input_position); // Changed to STRCMP_EQUAL
    STRCMP_EQUAL("x or y", result.data.error->expected); // Updated to aggregated expected
    STRCMP_EQUAL("abc", result.data.error->found);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result.data.error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}

TEST(ErrorHandling, FurthestErrorTracking)
{
    const char* input_str = "abcdef";
    epc_parser_ctx_t* parse_ctx = create_transient_parse_ctx(input_str);

    epc_parser_t* p_x = epc_char(NULL, 'x'); // Fails at 'a'
    epc_parser_t* p_def = epc_string(NULL, "def"); // Fails at 'b' after 'd' (too short from current position)
    epc_parser_t* p_or_x_def = epc_or(NULL, 2, p_x, p_def);
    epc_parser_t* p_z = epc_char(NULL, 'z'); // Fails at 'a' as well

    epc_parse_result_t result_x = p_x->parse_fn(p_x, parse_ctx, input_str);
    CHECK_TRUE(result_x.is_error);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    // Furthest error is 'x' at 'a'
    CHECK_TRUE(result_x.data.error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == result_x.data.error->input_position);

    // Now make p_char_g fail after p_char_f succeeded
    epc_parser_t* p_char_f = epc_char(NULL, 'f'); // Parser for 'f'
    epc_parse_result_t res_f = p_char_f->parse_fn(p_char_f, parse_ctx, input_str + 5); // Try 'f' on 'f'
    CHECK_FALSE(res_f.is_error); // Should succeed

    epc_parser_t* p_char_g = epc_char(NULL, 'g'); // Parser for 'g'
    epc_parse_result_t res_g = p_char_g->parse_fn(p_char_g, parse_ctx, input_str + 5); // Try 'g' on 'f'
    CHECK_TRUE(res_g.is_error);

    // Furthest error should be from res_g because it's at input_str + 5
    CHECK_TRUE(res_g.data.error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error != NULL);
    CHECK_TRUE(parse_ctx->furthest_error->input_position == res_g.data.error->input_position);

    STRCMP_EQUAL("g", parse_ctx->furthest_error->expected);
    STRCMP_EQUAL("f", parse_ctx->furthest_error->found);
    STRCMP_EQUAL(input_str + 5, parse_ctx->furthest_error->input_position);

    destroy_transient_parse_ctx(parse_ctx);
}
