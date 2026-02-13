
#include "ast_evaluator.h"
#include "grammar.h"

#include "easy_pc/easy_pc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return 1;
    }

    const char * input_expr = argv[1];

    variable_t variables[1] = {
        {
            .name = "x",
            .value = 2.3,
        }
    };

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

    parse_and_evaluate_result_st result =
        parse_and_evaluate(formula_parser, input_expr, 1, variables, 2, constants);
    if (!result.success)
    {
        printf("Failed to parse and evaluate: `%s`\n", input_expr);
        printf("%s\n", result.message);
    }
    else
    {
        printf("Result: %lf\n", result.value);
    }

    int exit_code = result.success ? EXIT_SUCCESS : EXIT_FAILURE;

    parse_and_evaluate_result_cleanup(&result);

    epc_parser_list_free(list);

    return exit_code;
}