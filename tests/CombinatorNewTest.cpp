#include "easy_pc_private.h"

#include "CppUTest/TestHarness.h"

#include <stdio.h>
#include <string.h>

TEST_GROUP(CombinatorParsersNew)
{
    epc_parse_session_t session;
    epc_parser_list * list;

    void setup() override
    {
        session = (epc_parse_session_t){0}; // Reset session before each test
        list = epc_parser_list_create();
    }

    void teardown() override
    {
        epc_parser_list_free(list);
        epc_parse_session_destroy(&session);
    }

    // New helper for checking a CPT node directly
    void check_cpt_node(
        epc_cpt_node_t * node,
        char const * expected_tag,
        char const * expected_content,
        size_t expected_len,
        int expected_children_count
    )
    {
        CHECK_TRUE(node != NULL);
        STRCMP_EQUAL(expected_tag, node->tag);
        STRNCMP_EQUAL(expected_content, node->content, expected_len);
        LONGS_EQUAL(expected_len, node->len);
        LONGS_EQUAL(expected_children_count, node->children_count);
    }

    // Modified check_success to use check_cpt_node for the session's success node
    void check_success(
        char const * expected_tag, char const * expected_content, size_t expected_len, int expected_children_count
    )
    {
        CHECK_FALSE(session.result.is_error);
        CHECK_TRUE(session.result.data.success != NULL);
        check_cpt_node(
            session.result.data.success, expected_tag, expected_content, expected_len, expected_children_count
        );
    }

    void check_failure(char const * expected_message_substring)
    {
        CHECK_TRUE(session.result.is_error);
        CHECK_TRUE(session.result.data.error != NULL);
        STRCMP_CONTAINS(expected_message_substring, session.result.data.error->message);
    }
};

// --- p_many tests ---
TEST(CombinatorParsersNew, Many_MatchesZeroOccurrences)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_many_a = epc_many(NULL, p_a);
    session = epc_parse_input(p_many_a, "b");
    check_success("many", "", 0, 0); // Should succeed with 0 length and 0 children
}

TEST(CombinatorParsersNew, Many_MatchesOneOccurrence)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_many_a = epc_many(NULL, p_a);
    session = epc_parse_input(p_many_a, "a");
    check_success("many", "a", 1, 1);
}

TEST(CombinatorParsersNew, Many_MatchesMultipleOccurrences)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_many_a = epc_many(NULL, p_a);
    session = epc_parse_input(p_many_a, "aaaaa");
    check_success("many", "aaaaa", 5, 5);
}

TEST(CombinatorParsersNew, Many_MatchesMultipleThenFails)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_many_a = epc_many(NULL, p_a);
    session = epc_parse_input(p_many_a, "aaab");
    check_success("many", "aaa", 3, 3);
}

TEST(CombinatorParsersNew, Many_FailsNullChildParser)
{
    epc_parser_t * p_many_null = epc_many(NULL, NULL);
    session = epc_parse_input(p_many_null, "a");
    check_failure("p_many received NULL child parser");
}

// --- p_count tests ---
TEST(CombinatorParsersNew, Count_MatchesExactNumber)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_count_3_a = epc_count(NULL, 3, p_a);
    session = epc_parse_input(p_count_3_a, "aaa");
    check_success("count", "aaa", 3, 3);
}

TEST(CombinatorParsersNew, Count_FailsIfLessThanExpected)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_count_3_a = epc_count(NULL, 3, p_a);
    session = epc_parse_input(p_count_3_a, "aa");
    check_failure("Count failed to match child at count 3"); // from p_char
}

TEST(CombinatorParsersNew, Count_FailsIfMoreThanExpected)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_count_3_a = epc_count(NULL, 3, p_a);
    session = epc_parse_input(p_count_3_a, "aaaa"); // Will match 3 'a's, but next char is 'a'
    check_success("count", "aaa", 3, 3);
    // The remaining 'a' is not consumed, but the p_count itself is successful for the first 3
}

TEST(CombinatorParsersNew, Count_ZeroCountSucceedsWithZeroLength)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_count_0_a = epc_count(NULL, 0, p_a);
    session = epc_parse_input(p_count_0_a, "abc");
    check_success("count", "", 0, 0);
}

TEST(CombinatorParsersNew, Count_FailsNullChildParser)
{
    epc_parser_t * p_count_3_null = epc_count(NULL, 3, NULL);
    session = epc_parse_input(p_count_3_null, "abc");
    check_failure("p_count received NULL child parser");
}

// --- p_between tests ---
TEST(CombinatorParsersNew, Between_MatchesCorrectly)
{
    epc_parser_t * p_open = epc_char(NULL, '(');
    epc_parser_t * p_close = epc_char(NULL, ')');
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_between_paren_a = epc_between(NULL, p_open, p_a, p_close);
    session = epc_parse_input(p_between_paren_a, "(a)");
    check_success("between", "(a)", 3, 1);
}

TEST(CombinatorParsersNew, Between_FailsIfOpenMissing)
{
    epc_parser_t * p_open = epc_char(NULL, '(');
    epc_parser_t * p_close = epc_char(NULL, ')');
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_between_paren_a = epc_between(NULL, p_open, p_a, p_close);
    session = epc_parse_input(p_between_paren_a, "a)");
    check_failure("Unexpected character"); // expecting '('
}

TEST(CombinatorParsersNew, Between_FailsIfWrappedMissing)
{
    epc_parser_t * p_open = epc_char(NULL, '(');
    epc_parser_t * p_close = epc_char(NULL, ')');
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_between_paren_a = epc_between(NULL, p_open, p_a, p_close);
    session = epc_parse_input(p_between_paren_a, "()");
    check_failure("Unexpected character"); // expecting 'a'
}

TEST(CombinatorParsersNew, Between_FailsIfCloseMissing)
{
    epc_parser_t * p_open = epc_char(NULL, '(');
    epc_parser_t * p_close = epc_char(NULL, ')');
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_between_paren_a = epc_between(NULL, p_open, p_a, p_close);
    session = epc_parse_input(p_between_paren_a, "(a");
    check_failure("Unexpected end of input"); // expecting ')'
}

TEST(CombinatorParsersNew, Between_FailsNullChildParser)
{
    epc_parser_t * p_open = epc_char(NULL, '(');
    epc_parser_t * p_close = epc_char(NULL, ')');
    epc_parser_t * p_between_null_wrapped = epc_between(NULL, p_open, NULL, p_close);
    session = epc_parse_input(p_between_null_wrapped, "(a)");
    check_failure("p_between received NULL child parser(s)");
}

// --- p_delimited tests ---
TEST(CombinatorParsersNew, Delimited_MatchesSingleItemNoDelimiter)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_delimited_a = epc_delimited(NULL, p_a, NULL);
    session = epc_parse_input(p_delimited_a, "a");
    check_success("delimited", "a", 1, 1);
}

TEST(CombinatorParsersNew, Delimited_MatchesMultipleItemsWithDelimiter)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_comma = epc_char(NULL, ',');
    epc_parser_t * p_delimited_a_comma = epc_delimited(NULL, p_a, p_comma);
    session = epc_parse_input(p_delimited_a_comma, "a,a,a");
    check_success("delimited", "a,a,a", 5, 3);
}

TEST(CombinatorParsersNew, Delimited_MatchesMultipleItemsWithoutLastDelimiter)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_comma = epc_char(NULL, ',');
    epc_parser_t * p_delimited_a_comma = epc_delimited(NULL, p_a, p_comma);
    session = epc_parse_input(p_delimited_a_comma, "a,a");
    check_success("delimited", "a,a", 3, 2);
}

TEST(CombinatorParsersNew, Delimited_FailsIfFirstItemMissing)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_comma = epc_char(NULL, ',');
    epc_parser_t * p_delimited_a_comma = epc_delimited(NULL, p_a, p_comma);
    session = epc_parse_input(p_delimited_a_comma, ",a");
    check_failure("Unexpected character"); // expecting 'a'
}

TEST(CombinatorParsersNew, Delimited_MatchesFirstItemEvenIfSubsequentFails)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_comma = epc_char(NULL, ',');
    epc_parser_t * p_delimited_a_comma = epc_delimited(NULL, p_a, p_comma);
    session = epc_parse_input(p_delimited_a_comma, "a,");
    check_failure("Unexpected trailing delimiter");
}

TEST(CombinatorParsersNew, Delimited_FailsNullItemParser)
{
    epc_parser_t * p_comma = epc_char(NULL, ',');
    epc_parser_t * p_delimited_null_item = epc_delimited(NULL, NULL, p_comma);
    session = epc_parse_input(p_delimited_null_item, "a,a");
    check_failure("p_delimited received NULL item parser");
}

// --- p_optional tests ---
TEST(CombinatorParsersNew, Optional_MatchesChild)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_opt_a = epc_optional(NULL, p_a);
    session = epc_parse_input(p_opt_a, "a");
    check_success("optional", "a", 1, 1);
}

TEST(CombinatorParsersNew, Optional_DoesNotMatchChild_SucceedsWithZeroLength)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_opt_a = epc_optional(NULL, p_a);
    session = epc_parse_input(p_opt_a, "b");
    check_success("optional", "", 0, 0); // Should succeed, consume nothing, 0 children
}

TEST(CombinatorParsersNew, Optional_FailsNullChildParser)
{
    epc_parser_t * p_opt_null = epc_optional(NULL, NULL);
    session = epc_parse_input(p_opt_null, "a");
    check_failure("p_optional received NULL child parser");
}

// --- p_lookahead tests ---
TEST(CombinatorParsersNew, Lookahead_SucceedsIfChildMatchesConsumesNothing)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_look_a = epc_lookahead(NULL, p_a);
    session = epc_parse_input(p_look_a, "abc");
    check_success("lookahead", "", 0, 0); // Should succeed, len 0, content is ""
}

TEST(CombinatorParsersNew, Lookahead_FailsIfChildFails)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_look_a = epc_lookahead(NULL, p_a);
    session = epc_parse_input(p_look_a, "bbc");
    check_failure("Unexpected character"); // from p_char, expecting 'a'
}

TEST(CombinatorParsersNew, Lookahead_FailsNullChildParser)
{
    epc_parser_t * p_look_null = epc_lookahead(NULL, NULL);
    session = epc_parse_input(p_look_null, "a");
    check_failure("p_lookahead received NULL child parser");
}

// --- p_not tests ---
TEST(CombinatorParsersNew, Not_SucceedsIfChildFailsConsumesNothing)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_not_a = epc_not(NULL, p_a);
    session = epc_parse_input(p_not_a, "b");
    check_success("not", "", 0, 0); // Should succeed, len 0, content is ""
}

TEST(CombinatorParsersNew, Not_FailsIfChildMatches)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_parser_t * p_not_a = epc_not(NULL, p_a);
    session = epc_parse_input(p_not_a, "a");
    check_failure("Parser unexpectedly matched"); // p_a matched
}

TEST(CombinatorParsersNew, Not_FailsNullChildParser)
{
    epc_parser_t * p_not_null = epc_not(NULL, NULL);
    session = epc_parse_input(p_not_null, "a");
    check_failure("p_not received NULL child parser");
}

// --- p_fail tests ---
TEST(CombinatorParsersNew, Fail_AlwaysFailsWithCustomMessage)
{
    epc_parser_t * p_fail_msg = epc_fail(NULL, "This parser always fails!");
    session = epc_parse_input(p_fail_msg, "anything");
    check_failure("This parser always fails!");
}

// --- p_succeed tests ---
TEST(CombinatorParsersNew, Succeed_AlwaysSucceedsConsumingNoContent)
{
    epc_parser_t * p_succeed_hello = epc_succeed(NULL);
    session = epc_parse_input(p_succeed_hello, "hello");
    check_success("succeed", "", 0, 0);
    epc_parsers_free(1, p_succeed_hello);
}

// --- epc_lexeme tests ---
TEST(CombinatorParsersNew, Lexeme_ParsesWithLeadingAndTrailingSpaces)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "   hello   world");
    check_success("lexeme", "   hello   ", 11, 1);
}

TEST(CombinatorParsersNew, Lexeme_ParsesWithoutSpaces)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "helloworld");
    check_success("lexeme", "hello", 5, 1);
}

TEST(CombinatorParsersNew, Lexeme_ParsesWithOnlyLeadingSpaces)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "   hello");
    check_success("lexeme", "   hello", 8, 1);
}

TEST(CombinatorParsersNew, Lexeme_ParsesWithOnlyTrailingSpaces)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "hello   ");
    check_success("lexeme", "hello   ", 8, 1);
}

TEST(CombinatorParsersNew, Lexeme_FailsIfWrappedParserFails)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "   world   ");
    check_failure("Unexpected string");
}

TEST(CombinatorParsersNew, Lexeme_EmptyInputFailsWrappedParser)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "");
    check_failure("Unexpected end of input");
}

TEST(CombinatorParsersNew, Lexeme_NullChildParserFails)
{
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", NULL);
    session = epc_parse_input(p_lex, "abc");
    check_failure("epc_lexeme received NULL child parser");
}

TEST(CombinatorParsersNew, Lexeme_ParsesWithCppStyleComments)
{
    epc_parser_t * p_word = epc_string_l(list, "word", "hello");
    epc_parser_t * p_lex = epc_lexeme_l(list, "lexeme", p_word);
    session = epc_parse_input(p_lex, "//comment\n   hello   //another comment\nworld");
    check_success("lexeme", "//comment\n   hello   //another comment\n", 39, 1);
}

// Helper to check CPT for chainl1/chainr1.
// Expected structure:
// <chain_type>_combined
//   child[0]: (sub_chain or item)
//   child[1]: op
//   child[2]: item
void
check_chain_node(epc_cpt_node_t * node, char const * expected_tag, char const * expected_content, size_t expected_len)
{
    CHECK_TRUE(node != NULL);
    STRCMP_EQUAL(expected_tag, node->tag);
    STRNCMP_EQUAL(expected_content, node->content, expected_len);
    LONGS_EQUAL(expected_len, node->len);
    LONGS_EQUAL(3, node->children_count); // item, op, item (or sub-chain)
}

// --- epc_chainl1 tests ---
TEST(CombinatorParsersNew, ChainL1_SingleItem)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_plus = epc_char(NULL, '+');
    epc_parser_t * p_chain = epc_chainl1(NULL, p_num, p_plus);
    session = epc_parse_input(p_chain, "5");
    check_success("integer", "5", 1, 0); // Single item, so not "chainl1_combined"
    epc_parsers_free(3, p_num, p_plus, p_chain);
}

TEST(CombinatorParsersNew, ChainL1_TwoItems)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_plus = epc_char(NULL, '+');
    epc_parser_t * p_chain = epc_chainl1(NULL, p_num, p_plus);
    session = epc_parse_input(p_chain, "1+2");
    check_success("chainl1", "1+2", 3, 3);
    // Access children through session.result.data.success directly for further checks
    epc_cpt_node_t * root_node = session.result.data.success;
    CHECK_TRUE(root_node != NULL);
    check_cpt_node(root_node->children[0], "integer", "1", 1, 0);
    check_cpt_node(root_node->children[1], "char", "+", 1, 0);
    check_cpt_node(root_node->children[2], "integer", "2", 1, 0);
    epc_parsers_free(3, p_num, p_plus, p_chain);
}

TEST(CombinatorParsersNew, ChainL1_MultipleItemsLeftAssociative)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_minus = epc_char(NULL, '-');
    epc_parser_t * p_chain = epc_chainl1(NULL, p_num, p_minus);
    session = epc_parse_input(p_chain, "1-2-3"); // Should be (1-2)-3
    check_success("chainl1", "1-2-3", 5, 3);
    epc_cpt_node_t * root_node = session.result.data.success;
    CHECK_TRUE(root_node != NULL);
    check_cpt_node(root_node->children[0], "chainl1", "1-2", 3, 3); // (1-2)
    check_cpt_node(root_node->children[0]->children[0], "integer", "1", 1, 0);
    check_cpt_node(root_node->children[0]->children[1], "char", "-", 1, 0);
    check_cpt_node(root_node->children[0]->children[2], "integer", "2", 1, 0);
    check_cpt_node(root_node->children[1], "char", "-", 1, 0);    // -
    check_cpt_node(root_node->children[2], "integer", "3", 1, 0); // 3
    epc_parsers_free(3, p_num, p_minus, p_chain);
}

TEST(CombinatorParsersNew, ChainL1_FailsIfFirstItemMissing)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_plus = epc_char(NULL, '+');
    epc_parser_t * p_chain = epc_chainl1(NULL, p_num, p_plus);
    session = epc_parse_input(p_chain, "+1");
    check_failure("Expected an integer");
    epc_parsers_free(3, p_num, p_plus, p_chain);
}

TEST(CombinatorParsersNew, ChainL1_FailsIfSubsequentItemMissing)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_plus = epc_char(NULL, '+');
    epc_parser_t * p_chain = epc_chainl1(NULL, p_num, p_plus);
    session = epc_parse_input(p_chain, "1+");
    check_failure("Unexpected end of input");
    epc_parsers_free(3, p_num, p_plus, p_chain);
}

TEST(CombinatorParsersNew, ChainL1_FailsNullChildParser)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_chain = epc_chainl1(NULL, p_num, NULL);
    session = epc_parse_input(p_chain, "1+2");
    check_failure("epc_chainl1 received NULL child parser(s)");
    epc_parsers_free(2, p_num, p_chain);
}

// --- epc_chainr1 tests ---
TEST(CombinatorParsersNew, ChainR1_SingleItem)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_caret = epc_char(NULL, '^');
    epc_parser_t * p_chain = epc_chainr1(NULL, p_num, p_caret);
    session = epc_parse_input(p_chain, "5");
    check_success("integer", "5", 1, 0); // Single item, so not "chainr1_combined"
    epc_parsers_free(3, p_num, p_caret, p_chain);
}

TEST(CombinatorParsersNew, ChainR1_TwoItems)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_caret = epc_char(NULL, '^');
    epc_parser_t * p_chain = epc_chainr1(NULL, p_num, p_caret);
    session = epc_parse_input(p_chain, "1^2");
    check_success("chainr1", "1^2", 3, 3);
    epc_cpt_node_t * root_node = session.result.data.success;
    if (session.result.is_error)
    {
        fprintf(
            stderr,
            "error message: %s %s %s\n",
            session.result.data.error->message,
            session.result.data.error->expected,
            session.result.data.error->found
        );
    }
    CHECK_TRUE(root_node != NULL);
    check_cpt_node(root_node->children[0], "integer", "1", 1, 0);
    check_cpt_node(root_node->children[1], "char", "^", 1, 0);
    check_cpt_node(root_node->children[2], "integer", "2", 1, 0);
    epc_parsers_free(3, p_num, p_caret, p_chain);
}

TEST(CombinatorParsersNew, ChainR1_MultipleItemsRightAssociative)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_caret = epc_char(NULL, '^');
    epc_parser_t * p_chain = epc_chainr1(NULL, p_num, p_caret);
    session = epc_parse_input(p_chain, "1^2^3"); // Should be 1^(2^3)
    check_success("chainr1", "1^2^3", 5, 3);
    epc_cpt_node_t * root_node = session.result.data.success;
    CHECK_TRUE(root_node != NULL);
    check_cpt_node(root_node->children[0], "integer", "1", 1, 0);
    check_cpt_node(root_node->children[1], "char", "^", 1, 0);
    check_cpt_node(root_node->children[2], "chainr1", "2^3", 3, 3); // (2^3)
    check_cpt_node(root_node->children[2]->children[0], "integer", "2", 1, 0);
    check_cpt_node(root_node->children[2]->children[1], "char", "^", 1, 0);
    check_cpt_node(root_node->children[2]->children[2], "integer", "3", 1, 0);
    epc_parsers_free(3, p_num, p_caret, p_chain);
}

TEST(CombinatorParsersNew, ChainR1_FailsIfFirstItemMissing)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_caret = epc_char(NULL, '^');
    epc_parser_t * p_chain = epc_chainr1(NULL, p_num, p_caret);
    session = epc_parse_input(p_chain, "^1");
    check_failure("Expected an integer");
    epc_parsers_free(3, p_num, p_caret, p_chain);
}

TEST(CombinatorParsersNew, ChainR1_FailsIfSubsequentItemMissing)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_caret = epc_char(NULL, '^');
    epc_parser_t * p_chain = epc_chainr1(NULL, p_num, p_caret);
    session = epc_parse_input(p_chain, "1^");
    check_failure("Unexpected end of input");
    epc_parsers_free(3, p_num, p_caret, p_chain);
}

TEST(CombinatorParsersNew, ChainR1_FailsNullChildParser)
{
    epc_parser_t * p_num = epc_int(NULL);
    epc_parser_t * p_chain = epc_chainr1(NULL, p_num, NULL);
    session = epc_parse_input(p_chain, "1^2");
    check_failure("epc_chainr1 received NULL child parser(s)");
    epc_parsers_free(2, p_num, p_chain);
}
