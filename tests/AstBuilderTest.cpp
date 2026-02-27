#include "easy_pc_private.h"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>
#include <stdarg.h> // For va_list in epc_ast_builder_set_error
#include <stdio.h>
#include <stdlib.h> // For malloc, free
#include <string.h>

// --- Mock AST Node Definition ---
typedef struct MyNode
{
    char const * type;
    char const * value; // Content from CPT node
    struct MyNode ** children;
    int children_count;
} MyNode_t;

enum MyActions
{
    ACTION_NONE,
    ACTION_PRUNE,
    ACTION_IDENTIFIER,
    ACTION_NUMBER,
    ACTION_ADD_OP,
    ACTION_MULTIPLY_OP,
    ACTION_EXPRESSION,
    ACTION_TERM,
    ACTION_PRIMARY,
    ACTION_ROOT,
    ACTION_PASS_CHILDREN, // For default action test
    MAX_ACTIONS
};

// Custom user data for callbacks
typedef struct
{
    int free_call_count;
    int enter_call_count;
    int action_call_count[MAX_ACTIONS];
    char const * last_enter_tag;
} TestUserData;

// --- Helper for Mock Node Creation and Freeing ---
static MyNode_t *
MyNode_create(char const * type, char const * value, size_t value_len)
{
    MyNode_t * node = (MyNode_t *)calloc(1, sizeof(*node));
    if (node)
    {
        node->type = type; // Static string, no need to strdup
        if (value)
        {
            node->value = strndup(value, value_len); // Duplicate value as it might be from CPT
        }
    }
    return node;
}

static void
MyNode_free(MyNode_t * node, void * user_data)
{
    TestUserData * data = (TestUserData *)user_data;

    if (!node)
    {
        return;
    }
    if (node->value)
    {
        free((void *)node->value);
    }
    if (node->children)
    {
        for (int i = 0; i < node->children_count; ++i)
        {
            MyNode_free(node->children[i], user_data); // Recursive free for children
        }
        free(node->children);
    }

    data->free_call_count++;
    mock().actualCall("free_node_cb").withPointerParameter("node", node);

    free(node);
}

// --- Mock Callbacks for epc_ast_action_cb ---
static void
mock_free_node_cb(void * node, void * user_data)
{
    MyNode_t * my_node = (MyNode_t *)node;
    MyNode_free(my_node, user_data);
}

static void
mock_enter_node_cb(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void * user_data)
{
    TestUserData * data = (TestUserData *)user_data;
    data->enter_call_count++;
    data->last_enter_tag = node->name; // Checking name as per user instructions
    mock().actualCall("enter_node_cb").withStringParameter("name", node->name);
}

// Example semantic action callback: Create an IDENTIFIER node
static void
mock_action_identifier(
    epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data
)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_IDENTIFIER]++;
    mock()
        .actualCall("action_IDENTIFIER")
        .withStringParameter("cpt_name", node->name)
        .withIntParameter("child_count", count);

    MyNode_t * ast_node
        = MyNode_create("IDENTIFIER", epc_cpt_node_get_semantic_content(node), epc_cpt_node_get_semantic_len(node));
    epc_ast_push(ctx, ast_node);
}

// Example semantic action callback: Create a NUMBER node
static void
mock_action_number(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_NUMBER]++;
    mock()
        .actualCall("action_NUMBER")
        .withStringParameter("cpt_name", node->name)
        .withIntParameter("child_count", count);

    MyNode_t * ast_node
        = MyNode_create("NUMBER", epc_cpt_node_get_semantic_content(node), epc_cpt_node_get_semantic_len(node));
    epc_ast_push(ctx, ast_node);
}

// Example semantic action callback: Create an ADD_OP node (binary operator)
static void
mock_action_add_op(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_ADD_OP]++;
    mock()
        .actualCall("action_ADD_OP")
        .withStringParameter("cpt_name", node->name)
        .withIntParameter("child_count", count);

    MyNode_t * ast_node
        = MyNode_create("ADD_OP", epc_cpt_node_get_semantic_content(node), epc_cpt_node_get_semantic_len(node));
    epc_ast_push(ctx, ast_node);
}

// Example semantic action callback: Creates a generic EXPRESSION node (binary operation)
static void
mock_action_expression(
    epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data
)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_EXPRESSION]++;
    mock()
        .actualCall("action_EXPRESSION")
        .withStringParameter("cpt_name", node->name)
        .withIntParameter("child_count", count);

    MyNode_t * ast_node = MyNode_create("EXPR", NULL, 0);
    ast_node->children_count = count;
    ast_node->children = (MyNode_t **)calloc(count, sizeof(*ast_node->children));
    for (int i = 0; i < count; ++i)
    {
        ast_node->children[i] = (MyNode_t *)children[i];
    }
    epc_ast_push(ctx, ast_node);
}

// Example semantic action callback: Create a ROOT node
static void
mock_action_root(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_ROOT]++;
    mock().actualCall("action_ROOT").withStringParameter("cpt_name", node->name).withIntParameter("child_count", count);

    MyNode_t * ast_node = MyNode_create("ROOT", NULL, 0);
    ast_node->children_count = count;
    ast_node->children = (MyNode_t **)calloc(count, sizeof(*ast_node->children));
    for (int i = 0; i < count; ++i)
    {
        ast_node->children[i] = (MyNode_t *)children[i];
    }
    epc_ast_push(ctx, ast_node);
}

// Callback that doesn't push anything (pruning)
static void
mock_action_prune(epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_PRUNE]++;
    mock()
        .actualCall("action_PRUNE")
        .withStringParameter("cpt_name", node->name)
        .withIntParameter("child_count", count);

    // Explicitly free children that are "pruned" if they were not already freed
    if (ctx->registry->free_node)
    {
        for (int i = 0; i < count; ++i)
        {
            ctx->registry->free_node(children[i], user_data);
        }
    }
    // No push: this node and its children are pruned from the AST
}

// Callback that pushes multiple children (flattening)
static void
mock_action_pass_children(
    epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data
)
{
    TestUserData * data = (TestUserData *)user_data;
    data->action_call_count[ACTION_PASS_CHILDREN]++;
    mock()
        .actualCall("action_PASS_CHILDREN")
        .withStringParameter("cpt_name", node->name)
        .withIntParameter("child_count", count);

    for (int i = 0; i < count; ++i)
    {
        epc_ast_push(ctx, children[i]);
    }
}

TEST_GROUP(AstBuilderTest)
{
    epc_parser_list * parser_list;
    epc_parser_t * grammar_root;
    epc_parse_session_t session;
    epc_ast_hook_registry_t * registry;
    TestUserData user_data_obj;

    void setup() override
    {
        session = (epc_parse_session_t){0}; // Initialize session to zero
        parser_list = epc_parser_list_create();
        grammar_root = NULL; // Initialized by specific tests

        // Initialize user data for callbacks
        memset(&user_data_obj, 0, sizeof(user_data_obj));

        // Create AST hook registry
        registry = epc_ast_hook_registry_create(MAX_ACTIONS);
        CHECK_TRUE(registry != NULL);

        // Set the free_node callback (essential for cleanup tests)
        epc_ast_hook_registry_set_free_node(registry, mock_free_node_cb);
        epc_ast_hook_registry_set_enter_node(registry, mock_enter_node_cb);

        // Set specific action callbacks
        epc_ast_hook_registry_set_action(registry, ACTION_PRUNE, mock_action_prune);
        epc_ast_hook_registry_set_action(registry, ACTION_IDENTIFIER, mock_action_identifier);
        epc_ast_hook_registry_set_action(registry, ACTION_NUMBER, mock_action_number);
        epc_ast_hook_registry_set_action(registry, ACTION_ADD_OP, mock_action_add_op);
        epc_ast_hook_registry_set_action(registry, ACTION_EXPRESSION, mock_action_expression);
        epc_ast_hook_registry_set_action(registry, ACTION_ROOT, mock_action_root);
        epc_ast_hook_registry_set_action(registry, ACTION_PASS_CHILDREN, mock_action_pass_children);

        mock().strictOrder(); // Ensure call order is as expected
    }

    void teardown() override
    {
        epc_parse_session_destroy(&session);
        epc_parser_list_free(parser_list);
        epc_ast_hook_registry_free(registry);
        mock().checkExpectations();
        mock().clear();
    }
};

// --- Single Parser Single Action Test ---
TEST(AstBuilderTest, SingleParserSingleAction)
{
    epc_parser_t * p_root = epc_int_l(parser_list, "Root");
    epc_parser_set_ast_action(p_root, ACTION_NUMBER);
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "123");
    CHECK_FALSE(session.result.is_error);

    // Expected mock calls
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Root").withIntParameter("child_count", 0);

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * char_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("NUMBER", char_node->type);
    STRCMP_EQUAL("123", char_node->value);
    LONGS_EQUAL(0, char_node->children_count);

    // Cleanup: manually free the root node returned by epc_ast_build
    mock().expectOneCall("free_node_cb").withPointerParameter("node", char_node);
    mock_free_node_cb(char_node, &user_data_obj);

    LONGS_EQUAL(1, user_data_obj.free_call_count);
    LONGS_EQUAL(1, user_data_obj.enter_call_count);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_NUMBER]);
}

// --- Test Case 1: Simple Number and Root ---
TEST(AstBuilderTest, BuildsSimpleNumberAst)
{
    epc_parser_t * p_number = epc_int_l(parser_list, "Number");
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_number);
    epc_parser_set_ast_action(p_number, ACTION_NUMBER);
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "123");
    CHECK_FALSE(session.result.is_error);

    // Expected calls during AST building (reverse order of CPT traversal exit)
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Number");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Number").withIntParameter("child_count", 0);
    mock().expectOneCall("action_ROOT").withStringParameter("cpt_name", "Root").withIntParameter("child_count", 1);

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * root_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("ROOT", root_node->type);
    LONGS_EQUAL(1, root_node->children_count);

    MyNode_t * number_node = root_node->children[0];
    STRCMP_EQUAL("NUMBER", number_node->type);
    STRCMP_EQUAL("123", number_node->value);

    // Cleanup: root_node owns number_node, so we free only root_node
    mock().expectOneCall("free_node_cb").withPointerParameter("node", root_node->children[0]);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", root_node);
    MyNode_free(root_node, &user_data_obj); // Manual free since it's the root result

    LONGS_EQUAL(2, user_data_obj.free_call_count);
    LONGS_EQUAL(2, user_data_obj.enter_call_count);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_NUMBER]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_ROOT]);
}

// --- Test Case 2: Simple Identifier, default action ---
TEST(AstBuilderTest, BuildsSimpleIdentifierAstWithDefaultAction)
{
    epc_parser_t * p_identifier = epc_string_l(parser_list, "Identifier", "abc");
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_identifier);
    epc_parser_set_ast_action(p_identifier, ACTION_IDENTIFIER);
    // p_root has no specific action, should default to pushing its child (identifier)
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "abc");
    CHECK_FALSE(session.result.is_error);

    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Identifier");
    mock()
        .expectOneCall("action_IDENTIFIER")
        .withStringParameter("cpt_name", "Identifier")
        .withIntParameter("child_count", 0);

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * identifier_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("IDENTIFIER", identifier_node->type);
    STRCMP_EQUAL("abc", identifier_node->value);
    LONGS_EQUAL(0, identifier_node->children_count); // Identifier is a leaf node in this mock

    // Cleanup: free the single root node
    mock().expectOneCall("free_node_cb").withPointerParameter("node", identifier_node);
    MyNode_free(identifier_node, &user_data_obj);

    LONGS_EQUAL(1, user_data_obj.free_call_count);
    LONGS_EQUAL(2, user_data_obj.enter_call_count);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_IDENTIFIER]);
}

// --- Test Case 3: Binary Expression (Number OP Number) ---
TEST(AstBuilderTest, BuildsBinaryExpressionAst)
{
    epc_parser_t * p_num = epc_int_l(parser_list, "Number");
    epc_parser_t * p_plus = epc_char_l(parser_list, "AddOp", '+');
    epc_parser_t * p_expr = epc_and_l(parser_list, "Expression", 3, p_num, p_plus, p_num);
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_expr);

    epc_parser_set_ast_action(p_num, ACTION_NUMBER);
    epc_parser_set_ast_action(p_plus, ACTION_ADD_OP);
    epc_parser_set_ast_action(p_expr, ACTION_EXPRESSION);
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "1+2");
    CHECK_FALSE(session.result.is_error);

    // Expected CPT traversal and action calls
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Expression");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Number");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Number").withIntParameter("child_count", 0);
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "AddOp");
    mock().expectOneCall("action_ADD_OP").withStringParameter("cpt_name", "AddOp").withIntParameter("child_count", 0);
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Number");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Number").withIntParameter("child_count", 0);
    mock()
        .expectOneCall("action_EXPRESSION")
        .withStringParameter("cpt_name", "Expression")
        .withIntParameter("child_count", 3);
    mock().expectOneCall("action_ROOT").withStringParameter("cpt_name", "Root").withIntParameter("child_count", 1);

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * root_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("ROOT", root_node->type);
    LONGS_EQUAL(1, root_node->children_count);

    MyNode_t * expr_node = root_node->children[0];
    STRCMP_EQUAL("EXPR", expr_node->type);
    LONGS_EQUAL(3, expr_node->children_count);

    MyNode_t * num1 = expr_node->children[0];
    MyNode_t * op = expr_node->children[1];
    MyNode_t * num2 = expr_node->children[2];

    STRCMP_EQUAL("NUMBER", num1->type);
    STRCMP_EQUAL("1", num1->value);
    STRCMP_EQUAL("ADD_OP", op->type);
    STRCMP_EQUAL("+", op->value);
    STRCMP_EQUAL("NUMBER", num2->type);
    STRCMP_EQUAL("2", num2->value);

    // Cleanup: manually free the root to trigger recursive free calls
    mock().expectOneCall("free_node_cb").withPointerParameter("node", num1);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", op);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", num2);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", expr_node);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", root_node);
    MyNode_free(root_node, &user_data_obj);

    LONGS_EQUAL(5, user_data_obj.free_call_count);
    LONGS_EQUAL(5, user_data_obj.enter_call_count); // Root, Expr, Num, Op, Num
    LONGS_EQUAL(2, user_data_obj.action_call_count[ACTION_NUMBER]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_ADD_OP]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_EXPRESSION]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_ROOT]);
}

// --- Test Case 5: Error during AST node creation (within action callback) ---
TEST(AstBuilderTest, HandlesErrorDuringActionCallback)
{
    epc_parser_t * p_number = epc_int_l(parser_list, "Number");
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_number);
    epc_parser_set_ast_action(p_number, ACTION_NUMBER);
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    // Replace the NUMBER action to simulate an allocation failure
    epc_ast_hook_registry_set_action(
        registry,
        ACTION_NUMBER,
        [](epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data) {
            // Simulate allocation failure and set error in context
            epc_ast_builder_set_error(ctx, "Simulated allocation failure in action_NUMBER for %s", node->name);
            mock().actualCall("action_NUMBER_fail").withStringParameter("cpt_name", node->name);
            // Do NOT push any node, the error condition will trigger cleanup
        }
    );

    session = epc_parse_input(grammar_root, "123");
    CHECK_FALSE(session.result.is_error);

    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Number");
    mock().expectOneCall("action_NUMBER_fail").withStringParameter("cpt_name", "Number");

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_TRUE(ast_result.has_error);
    STRCMP_CONTAINS("Simulated allocation failure", ast_result.error_message);
    CHECK_TRUE(ast_result.ast_root == NULL);

    // No free_node_cb calls expected if no user AST nodes were successfully pushed
    LONGS_EQUAL(0, user_data_obj.free_call_count);
    LONGS_EQUAL(2, user_data_obj.enter_call_count);
}

// --- Test Case 6: Pruning (action callback returns zero nodes) ---
TEST(AstBuilderTest, PrunesAstNodes)
{
    epc_parser_t * p_keyword = epc_string_l(parser_list, "Keyword", "skipme");
    epc_parser_t * p_number = epc_int_l(parser_list, "Number");
    epc_parser_t * p_seq = epc_and_l(parser_list, "Sequence", 2, p_keyword, p_number);
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_seq);

    epc_parser_set_ast_action(p_keyword, ACTION_PRUNE);
    epc_parser_set_ast_action(p_number, ACTION_NUMBER);
    epc_parser_set_ast_action(p_seq, ACTION_PASS_CHILDREN); // Sequence just passes through its children
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "skipme123");
    CHECK_FALSE(session.result.is_error);

    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Sequence");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Keyword");
    mock().expectOneCall("action_PRUNE").withStringParameter("cpt_name", "Keyword").withIntParameter("child_count", 0);
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Number");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Number").withIntParameter("child_count", 0);
    mock()
        .expectOneCall("action_PASS_CHILDREN")
        .withStringParameter("cpt_name", "Sequence")
        .withIntParameter("child_count", 1); // Only number is left
    mock().expectOneCall("action_ROOT").withStringParameter("cpt_name", "Root").withIntParameter("child_count", 1);

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * root_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("ROOT", root_node->type);
    LONGS_EQUAL(1, root_node->children_count);

    MyNode_t * number_node = root_node->children[0];
    STRCMP_EQUAL("NUMBER", number_node->type);
    STRCMP_EQUAL("123", number_node->value);

    // Cleanup for the remaining nodes
    mock().expectOneCall("free_node_cb").withPointerParameter("node", number_node);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", root_node);
    MyNode_free(root_node, &user_data_obj);

    LONGS_EQUAL(2, user_data_obj.free_call_count);
    LONGS_EQUAL(4, user_data_obj.enter_call_count);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_PRUNE]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_NUMBER]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_PASS_CHILDREN]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_ROOT]);
}

// --- Test Case 7: AST Stack dynamic growth ---
TEST(AstBuilderTest, AstStackGrowsDynamically)
{
    // Create a deeply nested structure (e.g., A = (B); B = (C); C = (D); ...)
    // CPT traversal will push many placeholders
    epc_parser_t * p_char_a = epc_char_l(parser_list, "A", 'a');
    epc_parser_t * p_expr_fwd = epc_parser_fwd_decl_l(parser_list, "ExprFwd");
    epc_parser_t * p_paren_expr = epc_between_l(
        parser_list,
        "ParenExpr",
        epc_char_l(parser_list, "LParen", '('),
        p_expr_fwd,
        epc_char_l(parser_list, "RParen", ')')
    );
    epc_parser_t * p_expr_alt = epc_or_l(parser_list, "ExprAlt", 2, p_char_a, p_paren_expr);
    epc_parser_duplicate(p_expr_fwd, p_expr_alt);

    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_expr_fwd);

    epc_parser_set_ast_action(p_char_a, ACTION_IDENTIFIER);
    epc_parser_set_ast_action(p_paren_expr, ACTION_EXPRESSION);
    epc_parser_set_ast_action(p_expr_alt, ACTION_PASS_CHILDREN); // Flatten (A) or Expr
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    // Input:
    // "((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((a))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))"
    // This creates a CPT depth equal to the number of opening parentheses (64 in this case) + 2 (Root, A)
    // Each PUSH_PLACEHOLDER will increment the stack depth
    int const depth = 64;
    char input[200]; /* Must be at least as large as 2 * depth + 1. */
    int offset = 0;
    for (int i = 0; i < depth; ++i)
    {
        input[offset++] = '(';
    }
    input[offset++] = 'a';
    for (int i = 0; i < depth; ++i)
    {
        input[offset++] = ')';
    }
    input[offset] = '\0';

    session = epc_parse_input(grammar_root, input);
    CHECK_FALSE(session.result.is_error);

    mock().ignoreOtherCalls(); // Ignore enter_node_cb calls that are not explicitly expected for this dynamic test.
    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * root_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("ROOT", root_node->type);
    LONGS_EQUAL(1, root_node->children_count);

    MyNode_t * current_node = root_node->children[0];
    for (int i = 0; i < depth; ++i)
    {
        STRCMP_EQUAL("EXPR", current_node->type);
        LONGS_EQUAL(1, current_node->children_count);
        current_node = current_node->children[0];
    }
    STRCMP_EQUAL("IDENTIFIER", current_node->type);
    STRCMP_EQUAL("a", current_node->value);

    mock().disable();
    // Cleanup will free all the nested nodes
    MyNode_free(root_node, &user_data_obj);

    LONGS_EQUAL(depth + 2, user_data_obj.free_call_count);
    LONGS_EQUAL(2 * depth + 3, user_data_obj.enter_call_count);
}

// --- Test Case 8: Default Action (children are pushed back onto stack) ---
TEST(AstBuilderTest, DefaultActionPushesChildrenBack)
{
    // CPT: Root -> Seq (no action) -> Num1 (ACTION_NUMBER), Num2 (ACTION_NUMBER)
    epc_parser_t * p_num1 = epc_digit_l(parser_list, "Num1");
    epc_parser_t * p_num2 = epc_digit_l(parser_list, "Num2");
    epc_parser_t * p_seq = epc_and_l(parser_list, "Sequence", 2, p_num1, p_num2);
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_seq);

    epc_parser_set_ast_action(p_num1, ACTION_NUMBER);
    epc_parser_set_ast_action(p_num2, ACTION_NUMBER);
    // p_seq has no explicit action, should default to passing its children back
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "12");
    CHECK_FALSE(session.result.is_error);

    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Root");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Sequence");
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Num1");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Num1").withIntParameter("child_count", 0);
    mock().expectOneCall("enter_node_cb").withStringParameter("name", "Num2");
    mock().expectOneCall("action_NUMBER").withStringParameter("cpt_name", "Num2").withIntParameter("child_count", 0);
    // No explicit action_PASS_CHILDREN for Sequence
    mock()
        .expectOneCall("action_ROOT")
        .withStringParameter("cpt_name", "Root")
        .withIntParameter("child_count", 2); // Root gets both numbers

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_FALSE(ast_result.has_error);
    CHECK_TRUE(ast_result.ast_root != NULL);

    MyNode_t * root_node = (MyNode_t *)ast_result.ast_root;
    STRCMP_EQUAL("ROOT", root_node->type);
    LONGS_EQUAL(2, root_node->children_count); // Expect 2 children directly under root

    MyNode_t * num1 = root_node->children[0];
    MyNode_t * num2 = root_node->children[1];
    STRCMP_EQUAL("NUMBER", num1->type);
    STRCMP_EQUAL("1", num1->value);
    STRCMP_EQUAL("NUMBER", num2->type);
    STRCMP_EQUAL("2", num2->value);

    // Cleanup
    mock().expectOneCall("free_node_cb").withPointerParameter("node", num1);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", num2);
    mock().expectOneCall("free_node_cb").withPointerParameter("node", root_node);
    MyNode_free(root_node, &user_data_obj);

    LONGS_EQUAL(3, user_data_obj.free_call_count);
    LONGS_EQUAL(4, user_data_obj.enter_call_count);
    LONGS_EQUAL(2, user_data_obj.action_call_count[ACTION_NUMBER]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_ROOT]);
}

// --- Test Case 9: Error recovery for partially built AST ---
TEST(AstBuilderTest, ErrorRecoveryFreesPartialAst)
{
    epc_parser_t * p_num = epc_int_l(parser_list, "Number");
    epc_parser_t * p_plus = epc_char_l(parser_list, "AddOp", '+');
    epc_parser_t * p_expr = epc_and_l(parser_list, "Expression", 3, p_num, p_plus, p_num);
    epc_parser_t * p_root = epc_or_l(parser_list, "Root", 1, p_expr);

    epc_parser_set_ast_action(p_num, ACTION_NUMBER);
    epc_parser_set_ast_action(p_plus, ACTION_ADD_OP);
    epc_parser_set_ast_action(p_expr, ACTION_EXPRESSION);
    // Simulate error during expression action

    epc_ast_hook_registry_set_action(
        registry,
        ACTION_EXPRESSION,
        [](epc_ast_builder_ctx_t * ctx, epc_cpt_node_t * node, void ** children, int count, void * user_data) {
            // Free children (they were allocated, but this callback fails to build a parent)

            TestUserData * data = (TestUserData *)user_data;
            data->action_call_count[ACTION_EXPRESSION]++;
            if (ctx->registry->free_node)
            {
                for (int i = 0; i < count; ++i)
                {
                    ctx->registry->free_node(children[i], user_data);
                }
            }
            epc_ast_builder_set_error(ctx, "Simulated error in Expression action");
        }
    );
    epc_parser_set_ast_action(p_root, ACTION_ROOT);
    grammar_root = p_root;

    session = epc_parse_input(grammar_root, "1+2");
    CHECK_FALSE(session.result.is_error);

    /*
     * Couldn't find a way to get mock expectations working with freed nodes
     * when pointer values aren't know ahead of time, so just not bothering.
     */
    mock().disable();

    epc_ast_result_t ast_result = epc_ast_build(session.result.data.success, registry, &user_data_obj);
    CHECK_TRUE(ast_result.has_error);
    STRCMP_CONTAINS("Simulated error in Expression action", ast_result.error_message);
    CHECK_TRUE(ast_result.ast_root == NULL);

    // Expect 3 free calls from the custom action, and 0 from cleanup as nothing was pushed to the stack
    LONGS_EQUAL(3, user_data_obj.free_call_count);
    LONGS_EQUAL(5, user_data_obj.enter_call_count);
    LONGS_EQUAL(2, user_data_obj.action_call_count[ACTION_NUMBER]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_ADD_OP]);
    LONGS_EQUAL(1, user_data_obj.action_call_count[ACTION_EXPRESSION]); // The one that causes the error
}
