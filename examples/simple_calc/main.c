
#include "ast_evaluator.h"
#include "grammar.h"
#include "simple_calc_ast_actions.h" // For registry init, free, and action enum

#include "easy_pc/easy_pc.h"
#include "easy_pc/easy_pc_ast.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char ** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return 1;
    }

    char const * input_expr = argv[1];

    variable_t variables[1] = {{
        .name = "x",
        .value = 2.3,
    }};

    variable_t constants[2] = {
        {
            .name = "pi",
            .value = M_PI,
        },
        {
            .name = "e",
            .value = M_E,
        },
    };

    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * formula_parser = create_formula_grammar(list, 1, variables, 2, constants);

    epc_parse_input_t input = {.type = EPC_PARSE_TYPE_STRING, .input_string = input_expr};

    epc_compile_result_t compile_result
        = epc_parse_and_build_ast(formula_parser, input, AST_ACTION_MAX, simple_calc_ast_hook_registry_init, NULL);

    int exit_code = EXIT_FAILURE;

    if (!compile_result.success)
    {
        printf("Failed to compile: `%s`\n", input_expr);
        if (compile_result.parse_error_message)
        {
            printf("Parse Error: %s\n", compile_result.parse_error_message);
        }
        if (compile_result.ast_error_message)
        {
            printf("AST Build Error: %s\n", compile_result.ast_error_message);
        }
    }
    else
    {
        printf("Expression successfully compiled.\n");
        double calculated_result = evaluate_ast(compile_result.ast, variables, 1, constants, 2);

        printf("Result: %lf\n", calculated_result);
        exit_code = EXIT_SUCCESS;
    }

    epc_compile_result_cleanup(&compile_result, ast_node_free, NULL);

    epc_parser_list_free(list);

    return exit_code;
}