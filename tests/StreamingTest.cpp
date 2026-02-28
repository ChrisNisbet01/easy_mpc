#include "easy_pc_private.h"

#include <CppUTest/TestHarness.h>
#include <iostream>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define MAX_FRAGMENTS 10

struct ProducerArgs
{
    int fd;
    char const * fragments[MAX_FRAGMENTS];
    int delays_ms[MAX_FRAGMENTS];
    int fragment_count;
};

static void *
producer_thread(void * arg)
{
    ProducerArgs * args = (ProducerArgs *)arg;

    for (int i = 0; i < args->fragment_count; ++i)
    {
        if (args->delays_ms[i] > 0)
        {
            usleep((useconds_t)args->delays_ms[i] * 1000);
        }
        char const * data = args->fragments[i];
        size_t len = strlen(data);
        if (write(args->fd, data, len) != (ssize_t)len)
        {
            break;
        }
    }

    close(args->fd);
    return NULL;
}

TEST_GROUP(StreamingTest)
{
    pthread_t producer;
    ProducerArgs args;
    int pipe_fds[2];
    bool thread_started;
    epc_parser_list * list;
    epc_parse_session_t session;

    void setup() override
    {
        session = (epc_parse_session_t){0}; // Reset session before each test
        list = epc_parser_list_create();
        thread_started = false;
        pipe_fds[0] = -1;
        pipe_fds[1] = -1;
        if (pipe(pipe_fds) != 0)
        {
            FAIL("Failed to create pipe");
        }
    }

    void teardown() override
    {
        if (thread_started)
        {
            pthread_join(producer, NULL);
        }
        if (pipe_fds[0] != -1)
        {
            close(pipe_fds[0]);
        }
        if (pipe_fds[1] != -1)
        {
            close(pipe_fds[1]);
        }
        epc_parse_session_destroy(&session);
        epc_parser_list_free(list);
    }

    int start_producer(char const * data, int delay_ms = 0)
    {
        args.fd = pipe_fds[1];
        args.fragments[0] = data;
        args.delays_ms[0] = delay_ms;
        args.fragment_count = 1;
        pipe_fds[1] = -1; // Ownership transferred to thread
        thread_started = true;
        pthread_create(&producer, NULL, producer_thread, &args);
        int consumer_fd = pipe_fds[0];
        pipe_fds[0] = -1;

        return consumer_fd;
    }

    int start_producer_fragments(int count, char const * const * fragments, int const * delays_ms)
    {
        args.fd = pipe_fds[1];
        args.fragment_count = count > MAX_FRAGMENTS ? MAX_FRAGMENTS : count;
        for (int i = 0; i < args.fragment_count; ++i)
        {
            args.fragments[i] = fragments[i];
            args.delays_ms[i] = delays_ms[i];
        }
        pipe_fds[1] = -1;
        thread_started = true;
        pthread_create(&producer, NULL, producer_thread, &args);
        int consumer_fd = pipe_fds[0];
        pipe_fds[0] = -1;

        return consumer_fd;
    }

    void check_is_success(epc_parse_result_t result)
    {
        if (result.is_error)
        {
            std::cout << "Parse error: " << result.data.error->message << std::endl;
            std::cout << "Expected: " << (result.data.error->expected ? result.data.error->expected : "unknown")
                      << std::endl;
            std::cout << "Found: " << (result.data.error->found ? result.data.error->found : "unknown") << std::endl;
            FAIL("Expected success but got error");
        }
        CHECK_TRUE(result.data.success != NULL);
    }
};

TEST(StreamingTest, StreamingBasicCharTest)
{
    int fd = start_producer("a", 10);

    epc_parser_t * p = epc_char_l(list, NULL, 'a');
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("a", session.result.data.success->content, 1);
    LONGS_EQUAL(1, session.result.data.success->len);
}

TEST(StreamingTest, StreamingBasicStringTest)
{
    int fd = start_producer("hello", 10);

    epc_parser_t * p = epc_string_l(list, NULL, "hello");
    session = epc_parse_fd(p, fd);
    check_is_success(session.result);
    STRNCMP_EQUAL("hello", session.result.data.success->content, 5);
    LONGS_EQUAL(5, session.result.data.success->len);
}

TEST(StreamingTest, StreamingIntTest)
{
    int fd = start_producer("12345 ", 10); // Note the space to terminate parsing

    epc_parser_t * p = epc_int_l(list, NULL);
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("12345", session.result.data.success->content, 5);
    LONGS_EQUAL(5, session.result.data.success->len);
}

TEST(StreamingTest, StreamingDoubleTest)
{
    int fd = start_producer("3.14159 ", 10); // Note the space to terminate parsing

    epc_parser_t * p = epc_double_l(list, NULL);
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("3.14159", session.result.data.success->content, 7);
    LONGS_EQUAL(7, session.result.data.success->len);
}

TEST(StreamingTest, StreamingScientificDoubleTest)
{
    int fd = start_producer("1.23e-4 ", 10);

    epc_parser_t * p = epc_double_l(list, NULL);
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("1.23e-4", session.result.data.success->content, 7);
    LONGS_EQUAL(7, session.result.data.success->len);
}

TEST(StreamingTest, StreamingAmbiguousDoubleTest)
{
    char const * frags[] = {"123", "e-4 ", " "};
    int delays[] = {0, 50, 0};
    int fd = start_producer_fragments(2, frags, delays);

    epc_parser_t * p = epc_double_l(list, NULL);
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("123e-4", session.result.data.success->content, 6);
    LONGS_EQUAL(6, session.result.data.success->len);
}

TEST(StreamingTest, StreamingSequenceTest)
{
    char const * frags[] = {"123", " ", "456", " ", "789 "};
    int delays[] = {10, 10, 10, 10, 10};
    int fd = start_producer_fragments(5, frags, delays);

    epc_parser_t * p_int = epc_int_l(list, NULL);
    epc_parser_t * p_space = epc_space_l(list, NULL);
    epc_parser_t * p_seq = epc_and_l(list, NULL, 5, p_int, p_space, p_int, p_space, p_int);

    session = epc_parse_fd(p_seq, fd);

    check_is_success(session.result);
    LONGS_EQUAL(5, session.result.data.success->children_count);
    STRNCMP_EQUAL("123 456 789", session.result.data.success->content, 11);
    LONGS_EQUAL(11, session.result.data.success->len);
}

TEST(StreamingTest, StreamingEOFErrorTest)
{
    int fd = start_producer("hello", 10); // producer closes fd after "hello"

    epc_parser_t * p = epc_string_l(list, NULL, "hello world");
    session = epc_parse_fd(p, fd);

    CHECK_TRUE(session.result.is_error);
    STRCMP_EQUAL("Unexpected end of input", session.result.data.error->message);
}

TEST(StreamingTest, StreamingLexemeTest)
{
    char const * frags[] = {"  ", "// comment\n", "  ", "123", "  ", "// another comment\n", "  "};
    int delays[] = {10, 10, 10, 10, 10, 10, 10};
    int fd = start_producer_fragments(7, frags, delays);

    epc_parser_t * p_int = epc_int_l(list, NULL);
    epc_parser_t * p_lex = epc_lexeme_l(list, NULL, p_int);

    session = epc_parse_fd(p_lex, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("123", epc_cpt_node_get_semantic_content(session.result.data.success), 3);
    LONGS_EQUAL(3, epc_cpt_node_get_semantic_len(session.result.data.success));
}

TEST(StreamingTest, StreamingCppCommentTest)
{
    char const * frags[] = {"//", " first part\n", "next line"};
    int delays[] = {10, 10, 10};
    int fd = start_producer_fragments(3, frags, delays);

    epc_parser_t * p = epc_cpp_comment_l(list, NULL);
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("// first part\n", session.result.data.success->content, 14);
    LONGS_EQUAL(14, session.result.data.success->len);
}

TEST(StreamingTest, StreamingCCommentTest)
{
    char const * frags[] = {"/*", " first part", " second part", "*/", " tail"};
    int delays[] = {10, 10, 10, 10, 10};
    int fd = start_producer_fragments(5, frags, delays);

    epc_parser_t * p = epc_c_comment_l(list, NULL);
    session = epc_parse_fd(p, fd);

    check_is_success(session.result);
    STRNCMP_EQUAL("/* first part second part*/", session.result.data.success->content, 27);
    LONGS_EQUAL(27, session.result.data.success->len);
}
