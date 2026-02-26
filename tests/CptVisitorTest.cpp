#include "CppUTest/TestHarness.h"

#include <stdio.h>  // For snprintf
#include <stdlib.h> // For calloc, free
#include <string.h> // For strlen, strcmp

extern "C" {
#include "easy_pc_private.h"
}

// Helper struct for the test visitor
typedef struct
{
    char log[512]; // A buffer to log visitor actions
    int log_offset;
    int node_count;
} TestVisitorData;

// Test enter_node callback
static void
test_enter_node(epc_cpt_node_t * node, void * user_data)
{
    TestVisitorData * data = (TestVisitorData *)user_data;
    data->log_offset
        += snprintf(data->log + data->log_offset, sizeof(data->log) - data->log_offset, "ENTER:%s ", node->tag);
    data->node_count++;
}

// Test exit_node callback
static void
test_exit_node(epc_cpt_node_t * node, void * user_data)
{
    TestVisitorData * data = (TestVisitorData *)user_data;
    data->log_offset
        += snprintf(data->log + data->log_offset, sizeof(data->log) - data->log_offset, "EXIT:%s ", node->tag);
}

TEST_GROUP(CptVisitor)
{
    epc_parser_ctx_t * test_parse_ctx = NULL; // Renamed to avoid confusion with grammar_ctx

    void setup() override
    {
        test_parse_ctx = (epc_parser_ctx_t *)calloc(1, sizeof(*test_parse_ctx)); // parser_ctx_t struct itself from heap
        CHECK_TRUE(test_parse_ctx != NULL);
        test_parse_ctx->input_start = "test input string"; // Dummy input for error reporting
        test_parse_ctx->input_len = strlen(test_parse_ctx->input_start);
    }

    void teardown() override
    {
        // Destroy the transient parse context for this test's node allocations
        free(test_parse_ctx);
        test_parse_ctx = NULL;
    }
};

TEST(CptVisitor, VisitsSimpleNode)
{
    epc_cpt_node_t * root = epc_node_alloc(epc_parser_allocate("root"), "ROOT");

    TestVisitorData visitor_data = {0};
    epc_cpt_visitor_t visitor
        = {.enter_node = test_enter_node, .exit_node = test_exit_node, .user_data = &visitor_data};

    epc_cpt_visit_nodes(root, &visitor);

    STRCMP_EQUAL("ENTER:ROOT EXIT:ROOT ", visitor_data.log);
    CHECK_EQUAL(1, visitor_data.node_count);
}

TEST(CptVisitor, VisitsTreeWithChildren)
{
    // Create a simple tree: ROOT -> CHILD1, CHILD2
    epc_cpt_node_t * root = epc_node_alloc(epc_parser_allocate("root"), "ROOT");

    epc_cpt_node_t * child1 = epc_node_alloc(epc_parser_allocate("child1"), "CHILD1");

    epc_cpt_node_t * child2 = epc_node_alloc(epc_parser_allocate("child2"), "CHILD2");

    epc_cpt_node_t * children[2] = {child1, child2};
    root->children = children;
    root->children_count = 2;

    TestVisitorData visitor_data = {0};
    epc_cpt_visitor_t visitor
        = {.enter_node = test_enter_node, .exit_node = test_exit_node, .user_data = &visitor_data};

    epc_cpt_visit_nodes(root, &visitor);

    STRCMP_EQUAL("ENTER:ROOT ENTER:CHILD1 EXIT:CHILD1 ENTER:CHILD2 EXIT:CHILD2 EXIT:ROOT ", visitor_data.log);
    CHECK_EQUAL(3, visitor_data.node_count);
}

TEST(CptVisitor, HandlesNullRoot)
{
    TestVisitorData visitor_data = {0};
    epc_cpt_visitor_t visitor
        = {.enter_node = test_enter_node, .exit_node = test_exit_node, .user_data = &visitor_data};

    epc_cpt_visit_nodes(NULL, &visitor);

    STRCMP_EQUAL("", visitor_data.log); // Nothing should be logged
    CHECK_EQUAL(0, visitor_data.node_count);
}

TEST(CptVisitor, HandlesNullVisitor)
{
    epc_cpt_node_t * root = epc_node_alloc(epc_parser_allocate("root"), "ROOT");

    epc_cpt_visit_nodes(root, NULL); // Should not crash
    // No assertions needed, just checking for no crash/memory issues
}

TEST(CptVisitor, HandlesNullCallbacks)
{
    epc_cpt_node_t * root = epc_node_alloc(epc_parser_allocate("root"), "ROOT");

    TestVisitorData visitor_data = {0};
    epc_cpt_visitor_t visitor_no_enter = {.enter_node = NULL, .exit_node = test_exit_node, .user_data = &visitor_data};
    epc_cpt_visit_nodes(root, &visitor_no_enter);
    STRCMP_EQUAL("EXIT:ROOT ", visitor_data.log);
    CHECK_EQUAL(0, visitor_data.node_count); // enter_node was not called

    visitor_data = (TestVisitorData){0}; // Reset
    epc_cpt_visitor_t visitor_no_exit = {.enter_node = test_enter_node, .exit_node = NULL, .user_data = &visitor_data};
    epc_cpt_visit_nodes(root, &visitor_no_exit);
    STRCMP_EQUAL("ENTER:ROOT ", visitor_data.log);
    CHECK_EQUAL(1, visitor_data.node_count); // enter_node was called
}