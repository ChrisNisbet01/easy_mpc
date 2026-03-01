#include "easy_pc_private.h"

#include "CppUTest/TestHarness.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    int entry_calls;
    int exit_calls;
    bool should_fail;
    void * last_user_ctx;
    epc_parser_ctx_t * last_parse_ctx;
} wrap_test_ctx_t;

static void
on_entry(epc_parser_t * parser, epc_parser_ctx_t * ctx, void * user_ctx)
{
    (void)parser;
    (void)ctx;
    wrap_test_ctx_t * tctx = (wrap_test_ctx_t *)user_ctx;
    tctx->entry_calls++;
    tctx->last_parse_ctx = ctx;
}

static bool
on_exit(epc_parse_result_t result, epc_parser_ctx_t * ctx, void * user_ctx)
{
    (void)result;
    (void)ctx;
    wrap_test_ctx_t * tctx = (wrap_test_ctx_t *)user_ctx;
    tctx->exit_calls++;
    tctx->last_parse_ctx = ctx;
    return !tctx->should_fail;
}

TEST_GROUP(WrapTest)
{
    epc_parse_session_t session;
    epc_parser_list * list;
    wrap_test_ctx_t tctx;

    void setup() override
    {
        session = (epc_parse_session_t){0};
        list = epc_parser_list_create();
        memset(&tctx, 0, sizeof(tctx));
    }

    void teardown() override
    {
        epc_parser_list_free(list);
        epc_parse_session_destroy(&session);
    }

    void check_success(
        char const * expected_tag, char const * expected_content, size_t expected_len, int expected_children_count
    )
    {
        CHECK_FALSE(session.result.is_error);
        CHECK_TRUE(session.result.data.success != NULL);
        STRCMP_EQUAL(expected_tag, session.result.data.success->tag);
        STRNCMP_EQUAL(expected_content, session.result.data.success->content, expected_len);
        LONGS_EQUAL(expected_len, session.result.data.success->len);
        LONGS_EQUAL(expected_children_count, session.result.data.success->children_count);
    }

    void check_failure(char const * expected_message_substring)
    {
        CHECK_TRUE(session.result.is_error);
        CHECK_TRUE(session.result.data.error != NULL);
        STRCMP_CONTAINS(expected_message_substring, session.result.data.error->message);
    }
};

TEST(WrapTest, Wrap_CallsEntryAndExitOnSuccess)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_wrap_callbacks_t callbacks = {on_entry, on_exit};
    epc_parser_t * p_wrap = epc_wrap_l(list, "my_wrap", p_a, callbacks, &tctx);

    session = epc_parse_str(p_wrap, "a", &tctx);

    check_success("char", "a", 1, 0);
    LONGS_EQUAL(1, tctx.entry_calls);
    LONGS_EQUAL(1, tctx.exit_calls);
    POINTERS_EQUAL(session.internal_parse_ctx, tctx.last_parse_ctx);
    POINTERS_EQUAL(&tctx, parse_ctx_get_user_ctx(tctx.last_parse_ctx));
}

TEST(WrapTest, Wrap_CallsEntryAndExitOnFailure)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_wrap_callbacks_t callbacks = {on_entry, on_exit};
    epc_parser_t * p_wrap = epc_wrap_l(list, "my_wrap", p_a, callbacks, &tctx);

    session = epc_parse_str(p_wrap, "b", &tctx);

    check_failure("Unexpected character");
    LONGS_EQUAL(1, tctx.entry_calls);
    LONGS_EQUAL(1, tctx.exit_calls);
}

TEST(WrapTest, Wrap_ExitHandlerCanOverrideSuccessToFailure)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_wrap_callbacks_t callbacks = {on_entry, on_exit};
    epc_parser_t * p_wrap = epc_wrap_l(list, "my_wrap", p_a, callbacks, &tctx);

    tctx.should_fail = true;
    session = epc_parse_str(p_wrap, "a", &tctx);

    check_failure("on_exit callback indicated failure");
    LONGS_EQUAL(1, tctx.entry_calls);
    LONGS_EQUAL(1, tctx.exit_calls);
}

TEST(WrapTest, Wrap_HandlesNullCallbacks)
{
    epc_parser_t * p_a = epc_char(NULL, 'a');
    epc_wrap_callbacks_t callbacks = {NULL, NULL};
    epc_parser_t * p_wrap = epc_wrap_l(list, "my_wrap", p_a, callbacks, NULL);

    session = epc_parse_str(p_wrap, "a", NULL);

    check_success("char", "a", 1, 0);
}
