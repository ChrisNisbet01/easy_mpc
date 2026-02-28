#include "CppUTest/TestHarness.h"
#include "easy_pc/easy_pc.h"

extern "C" {
#include "gdl_code_generator.h"
#include "gdl_compiler_ast_actions.h"
#include "gdl_parser.h"
}

#include <stdio.h>
#include <stdlib.h> // For system()
#include <string.h>

TEST_GROUP(GeneratedParserTest)
{
    epc_parser_list * parser_list;
    epc_parser_t * gdl_grammar;
    epc_parse_session_t session;
    epc_ast_hook_registry_t * ast_registry = NULL;
    epc_ast_result_t ast_build_result = {0};

    void setup() override
    {
        // Clean up generated files
        system("rm -f simple_test_language.h simple_test_language.c simple_test_language_actions.h");
        parser_list = epc_parser_list_create();
        gdl_grammar = create_gdl_parser(parser_list);
        ast_registry = epc_ast_hook_registry_create(GDL_AST_ACTION_MAX);
        gdl_ast_hook_registry_init(ast_registry, NULL); // No specific user_data needed
    }

    void generate_ast(char const * gdl_input)
    {
        session = epc_parse_str(gdl_grammar, gdl_input);
        CHECK_FALSE(session.result.is_error);
        ast_build_result = epc_ast_build(session.result.data.success, ast_registry, NULL);

        CHECK_FALSE(ast_build_result.has_error);
        CHECK(ast_build_result.ast_root != NULL);
    }

    void teardown() override
    {
        epc_parse_session_destroy(&session);
        epc_parser_list_free(parser_list);
        epc_ast_hook_registry_free(ast_registry);
        // Clean up generated files
        // system("rm -f simple_test_language.h simple_test_language.c simple_test_language_actions.h");
    }
};

TEST(GeneratedParserTest, GeneratesFilesSuccessfully)
{
    // Output directory for generated files in the test environment
    char const * output_dir = ".";
    char const * base_name = "simple_test_language";
    // Create AST for the simple test language
    char const * gdl_input = "Greeting = \"hello\";\n"
                             "World = \"world\";\n"
                             "CharX = 'x';\n"
                             "CharY = 'y';\n"
                             "CharZ = 'z';\n"
                             "SeqGreeting = Greeting World;\n"
                             "SeqChar = CharX CharY CharZ;\n"
                             "SimpleRule = SeqGreeting | SeqChar;\n"
                             "EOI = eoi;\n"
                             "Program = SimpleRule EOI @EPC_AST_SEMANTIC_ACTION_PROGRAM_RULE;\n";

    generate_ast(gdl_input);

    CHECK_TRUE(gdl_generate_c_code((gdl_ast_node_t *)ast_build_result.ast_root, base_name, output_dir));
}

TEST(GeneratedParserTest, GeneratesFilesWithFwdRefSuccessfully)
{
    // Output directory for generated files in the test environment
    char const * output_dir = ".";
    char const * base_name = "simple_fwd_ref_test_language";
    // Create AST for the simple test language
    char const * gdl_input = "Greeting = \"hello\";\n"
                             "World = \"world\";\n"
                             "CharX = 'x';\n"
                             "CharY = 'y';\n"
                             "CharZ = 'z';\n"
                             "SeqGreeting = Greeting World SeqChar;\n"
                             "SeqChar = CharX CharY CharZ;\n"
                             "SimpleRule = SeqGreeting | SeqChar;\n"
                             "EOI = eoi;\n"
                             "Program = SimpleRule EOI @EPC_AST_SEMANTIC_ACTION_PROGRAM_RULE;\n";

    generate_ast(gdl_input);

    CHECK_TRUE(gdl_generate_c_code((gdl_ast_node_t *)ast_build_result.ast_root, base_name, output_dir));
}
