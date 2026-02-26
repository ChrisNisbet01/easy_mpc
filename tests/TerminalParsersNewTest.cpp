#include "easy_pc_private.h"

#include "CppUTest/TestHarness.h"

#include <stdio.h>
#include <string.h>

TEST_GROUP(TerminalParsersNew){
    void setup() override{}

    void teardown() override{}

    void check_success(
        epc_parse_session_t session, char const * expected_tag, char const * expected_content, size_t expected_len
    ){CHECK_FALSE(session.result.is_error);
CHECK_TRUE(session.result.data.success != NULL);
STRCMP_EQUAL(expected_tag, session.result.data.success->tag);
STRNCMP_EQUAL(expected_content, session.result.data.success->content, expected_len);
LONGS_EQUAL(expected_len, session.result.data.success->len);
epc_parse_session_destroy(&session);
}

void
check_failure(epc_parse_session_t session, char const * expected_message_substring)
{
    CHECK_TRUE(session.result.is_error);
    CHECK_TRUE(session.result.data.error != NULL);
    STRCMP_CONTAINS(expected_message_substring, session.result.data.error->message);
    epc_parse_session_destroy(&session);
}
}
;

// --- p_char_range tests ---
TEST(TerminalParsersNew, CharRange_MatchesSingleCharInRange)
{
    epc_parser_t * p = epc_char_range(NULL, 'a', 'z');
    epc_parse_session_t session = epc_parse_input(p, "c");
    check_success(session, "char_range", "c", 1);
}

TEST(TerminalParsersNew, CharRange_MatchesStartOfRange)
{
    epc_parser_t * p = epc_char_range(NULL, 'a', 'z');
    epc_parse_session_t session = epc_parse_input(p, "a");
    check_success(session, "char_range", "a", 1);
}

TEST(TerminalParsersNew, CharRange_MatchesEndOfRange)
{
    epc_parser_t * p = epc_char_range(NULL, 'a', 'z');
    epc_parse_session_t session = epc_parse_input(p, "z");
    check_success(session, "char_range", "z", 1);
}

TEST(TerminalParsersNew, CharRange_FailsCharOutOfRange)
{
    epc_parser_t * p = epc_char_range(NULL, 'a', 'z');
    epc_parse_session_t session = epc_parse_input(p, "A");
    check_failure(session, "Unexpected character");
}

TEST(TerminalParsersNew, CharRange_FailsEmptyInput)
{
    epc_parser_t * p = epc_char_range(NULL, 'a', 'z');
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, CharRange_FailsNullInput)
{
    epc_parser_t * p = epc_char_range(NULL, 'a', 'z');
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

// --- p_any tests ---
TEST(TerminalParsersNew, AnyChar_MatchesAnyChar)
{
    epc_parser_t * p = epc_any(NULL);
    epc_parse_session_t session = epc_parse_input(p, "X");
    check_success(session, "any", "X", 1);
}

TEST(TerminalParsersNew, AnyChar_MatchesSpace)
{
    epc_parser_t * p = epc_any(NULL);
    epc_parse_session_t session = epc_parse_input(p, " ");
    check_success(session, "any", " ", 1);
}

TEST(TerminalParsersNew, AnyChar_MatchesDigit)
{
    epc_parser_t * p = epc_any(NULL);
    epc_parse_session_t session = epc_parse_input(p, "5");
    check_success(session, "any", "5", 1);
}

TEST(TerminalParsersNew, AnyChar_FailsEmptyInput)
{
    epc_parser_t * p = epc_any(NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, AnyChar_FailsNullInput)
{
    epc_parser_t * p = epc_any(NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

// --- p_none_of tests ---
TEST(TerminalParsersNew, NoneOfChars_MatchesCharNotInSet)
{
    epc_parser_t * p = epc_none_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "X");
    check_success(session, "none_of", "X", 1);
}

TEST(TerminalParsersNew, NoneOfChars_MatchesCharNotInSetLongerInput)
{
    epc_parser_t * p = epc_none_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "def");
    check_success(session, "none_of", "d", 1);
}

TEST(TerminalParsersNew, NoneOfChars_FailsCharInSet)
{
    epc_parser_t * p = epc_none_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "b");
    check_failure(session, "Character found in forbidden set");
}

TEST(TerminalParsersNew, NoneOfChars_FailsEmptyInput)
{
    epc_parser_t * p = epc_none_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, NoneOfChars_FailsNullInput)
{
    epc_parser_t * p = epc_none_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

// --- p_int tests ---
TEST(TerminalParsersNew, Int_MatchesPositiveInteger)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, "12345abc");
    check_success(session, "integer", "12345", 5);
}

TEST(TerminalParsersNew, Int_MatchesNegativeInteger)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, "-6789xyz");
    check_success(session, "integer", "-6789", 5);
}

TEST(TerminalParsersNew, Int_MatchesZero)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, "0def");
    check_success(session, "integer", "0", 1);
}

TEST(TerminalParsersNew, Int_FailsOnNonDigitStart)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, "abc");
    check_failure(session, "Expected an integer");
}

TEST(TerminalParsersNew, Int_FailsOnEmptyInput)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, Int_FailsOnNullInput)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

TEST(TerminalParsersNew, Int_FailsOnJustNegativeSign)
{
    epc_parser_t * p = epc_int(NULL);
    epc_parse_session_t session = epc_parse_input(p, "-");
    check_failure(session, "Expected an integer");
}

// --- p_alpha tests ---
TEST(TerminalParsersNew, Alpha_MatchesLowercase)
{
    epc_parser_t * p = epc_alpha(NULL);
    epc_parse_session_t session = epc_parse_input(p, "abc");
    check_success(session, "alpha", "a", 1);
}

TEST(TerminalParsersNew, Alpha_MatchesUppercase)
{
    epc_parser_t * p = epc_alpha(NULL);
    epc_parse_session_t session = epc_parse_input(p, "Xyz");
    check_success(session, "alpha", "X", 1);
}

TEST(TerminalParsersNew, Alpha_FailsOnDigit)
{
    epc_parser_t * p = epc_alpha(NULL);
    epc_parse_session_t session = epc_parse_input(p, "123");
    check_failure(session, "Unexpected character");
}

TEST(TerminalParsersNew, Alpha_FailsOnSymbol)
{
    epc_parser_t * p = epc_alpha(NULL);
    epc_parse_session_t session = epc_parse_input(p, "$$$");
    check_failure(session, "Unexpected character");
}

TEST(TerminalParsersNew, Alpha_FailsOnEmptyInput)
{
    epc_parser_t * p = epc_alpha(NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, Alpha_FailsOnNullInput)
{
    epc_parser_t * p = epc_alpha(NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

// --- p_alphanum tests ---
TEST(TerminalParsersNew, Alphanum_MatchesLowercase)
{
    epc_parser_t * p = epc_alphanum(NULL);
    epc_parse_session_t session = epc_parse_input(p, "abc");
    check_success(session, "alphanum", "a", 1);
}

TEST(TerminalParsersNew, Alphanum_MatchesUppercase)
{
    epc_parser_t * p = epc_alphanum(NULL);
    epc_parse_session_t session = epc_parse_input(p, "Xyz");
    check_success(session, "alphanum", "X", 1);
}

TEST(TerminalParsersNew, Alphanum_MatchesDigit)
{
    epc_parser_t * p = epc_alphanum(NULL);
    epc_parse_session_t session = epc_parse_input(p, "123");
    check_success(session, "alphanum", "1", 1);
}

TEST(TerminalParsersNew, Alphanum_FailsOnSymbol)
{
    epc_parser_t * p = epc_alphanum(NULL);
    epc_parse_session_t session = epc_parse_input(p, "$$$");
    check_failure(session, "Unexpected character");
}

TEST(TerminalParsersNew, Alphanum_FailsOnEmptyInput)
{
    epc_parser_t * p = epc_alphanum(NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, Alphanum_FailsOnNullInput)
{
    epc_parser_t * p = epc_alphanum(NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

// --- epc_hex_digit tests ---
TEST(TerminalParsersNew, HexDigit_MatchesDigit)
{
    epc_parser_t * p = epc_hex_digit(NULL);
    epc_parse_session_t session = epc_parse_input(p, "5abc");
    check_success(session, "hex_digit", "5", 1);
}

TEST(TerminalParsersNew, HexDigit_MatchesLowercaseAlpha)
{
    epc_parser_t * p = epc_hex_digit(NULL);
    epc_parse_session_t session = epc_parse_input(p, "cdef");
    check_success(session, "hex_digit", "c", 1);
}

TEST(TerminalParsersNew, HexDigit_MatchesUppercaseAlpha)
{
    epc_parser_t * p = epc_hex_digit(NULL);
    epc_parse_session_t session = epc_parse_input(p, "ABCE");
    check_success(session, "hex_digit", "A", 1);
}

TEST(TerminalParsersNew, HexDigit_FailsOnNonHexChar)
{
    epc_parser_t * p = epc_hex_digit(NULL);
    epc_parse_session_t session = epc_parse_input(p, "GHI");
    check_failure(session, "Unexpected character");
}

TEST(TerminalParsersNew, HexDigit_FailsOnEmptyInput)
{
    epc_parser_t * p = epc_hex_digit(NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, HexDigit_FailsOnNullInput)
{
    epc_parser_t * p = epc_hex_digit(NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

// --- epc_one_of tests ---
TEST(TerminalParsersNew, OneOf_MatchesCharInSet)
{
    epc_parser_t * p = epc_one_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "bdef");
    check_success(session, "one_of", "b", 1);
}

TEST(TerminalParsersNew, OneOf_MatchesFirstCharInSet)
{
    epc_parser_t * p = epc_one_of(NULL, "123");
    epc_parse_session_t session = epc_parse_input(p, "1xyz");
    check_success(session, "one_of", "1", 1);
}

TEST(TerminalParsersNew, OneOf_MatchesLastCharInSet)
{
    epc_parser_t * p = epc_one_of(NULL, "xyz");
    epc_parse_session_t session = epc_parse_input(p, "zabc");
    check_success(session, "one_of", "z", 1);
}

TEST(TerminalParsersNew, OneOf_FailsCharNotInSet)
{
    epc_parser_t * p = epc_one_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "dxyz");
    check_failure(session, "Character not found in set");
}

TEST(TerminalParsersNew, OneOf_FailsEmptyInput)
{
    epc_parser_t * p = epc_one_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
}

TEST(TerminalParsersNew, OneOf_FailsNullInput)
{
    epc_parser_t * p = epc_one_of(NULL, "abc");
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
}

TEST(TerminalParsersNew, OneOf_FailsWithEmptySet)
{
    epc_parser_t * p = epc_one_of(NULL, "");
    epc_parse_session_t session = epc_parse_input(p, "a");
    check_failure(session, "Character not found in set");
}

// --- epc_cpp_comment tests ---
TEST(TerminalParsersNew, CppComment_MatchesSimpleComment)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "// A simple comment\nNext line");
    check_success(session, "cpp_comment", "// A simple comment\n", 20);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CppComment_MatchesCommentAtEOF)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "// Comment at EOF");
    check_success(session, "cpp_comment", "// Comment at EOF", 17);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CppComment_MatchesEmptyComment)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "//\nNext line");
    check_success(session, "cpp_comment", "//\n", 3);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CppComment_FailsOnNoDoubleSlash)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "A regular line\n");
    check_failure(session, "Expected '//'");
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CppComment_FailsOnSingleSlash)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/ A single slash comment\n");
    check_failure(session, "Expected '//'");
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CppComment_FailsOnEmptyInput)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Unexpected end of input");
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CppComment_FailsOnNullInput)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
    epc_parser_list_free(list);
}

// --- epc_c_comment tests ---
TEST(TerminalParsersNew, CComment_MatchesSimpleComment)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/* This is a C comment */ Next code");
    check_success(session, "c_comment", "/* This is a C comment */", 25);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_MatchesMultiLineComment)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/* Multi\nline\ncomment */ After");
    check_success(session, "c_comment", "/* Multi\nline\ncomment */", 24);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_MatchesCommentWithStarsInside)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/* Comment * with * stars */ End");
    check_success(session, "c_comment", "/* Comment * with * stars */", 28);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_MatchesCommentAtEOF)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/* Comment at EOF */");
    check_success(session, "c_comment", "/* Comment at EOF */", 20);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_MatchesEmptyComment)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/**/Something else");
    check_success(session, "c_comment", "/**/", 4);
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_FailsOnUnterminatedComment)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "/* Unterminated comment");
    check_failure(session, "Unterminated C-style comment");
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_FailsOnNoStartDelimiter)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "Not a comment */");
    check_failure(session, "Expected '/*'");
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_FailsOnEmptyInput)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, "");
    check_failure(session, "Expected '/*'");
    epc_parser_list_free(list);
}

TEST(TerminalParsersNew, CComment_FailsOnNullInput)
{
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * p = epc_c_comment_l(list, NULL);
    epc_parse_session_t session = epc_parse_input(p, NULL);
    check_failure(session, "Input string is NULL");
    epc_parser_list_free(list);
}
