# Implementation Plan: JSON AST Builder

## 1. Define AST Structure
- Create `examples/json_parser/json_ast.h` to define the `json_node_t` structure and supported JSON types:
    - `JSON_NODE_OBJECT`
    - `JSON_NODE_ARRAY`
    - `JSON_NODE_STRING`
    - `JSON_NODE_NUMBER`
    - `JSON_NODE_BOOLEAN`
    - `JSON_NODE_NULL`
- Include structures for handling lists (arrays and object members).

## 2. Implement AST Actions
- Create `examples/json_parser/json_ast_actions.h` and `examples/json_parser/json_ast_actions.c`.
- Implement `json_node_alloc` and `json_node_free` helper functions.
- Implement the following semantic action callbacks:
    - `create_string_action`: Extracts the string content.
    - `create_number_action`: Parses the double value.
    - `create_boolean_action`: Handles "true" and "false".
    - `create_null_action`: Creates a null node.
    - `create_value_action`: Passes through the choice.
    - `create_array_elements_action`: Collects elements into a list.
    - `create_optional_array_elements_action`: Handles empty or non-empty lists.
    - `create_array_action`: Wraps the list in an array node.
    - `create_member_action`: Creates a key-value pair node.
    - `create_object_elements_action`: Collects members into a list.
    - `create_optional_object_elements_action`: Handles empty or non-empty member lists.
    - `create_object_action`: Wraps the list in an object node.
- Implement `json_ast_hook_registry_init` to register these actions.

## 3. Update Main Application
- Modify `examples/json_parser/main.c`:
    - Include `json_ast.h`, `json_ast_actions.h`, and `easy_pc/easy_pc_ast.h`.
    - Replace `epc_parse_input` with `epc_parse_and_build_ast`.
    - Update error handling to print `ast_error_message`.
    - Add a function to print the resulting AST for verification.
    - Ensure `epc_compile_result_cleanup` is called with `json_node_free`.

## 4. Update Build System
- Modify `examples/json_parser/CMakeLists.txt` to include `json_ast_actions.c`.

## 5. Verification
- Compile the updated example.
- Test with various JSON inputs:
    - `null`
    - `true`, `false`
    - `123.45`
    - `"string"`
    - `[]`, `[1, 2, 3]`
    - `{}`, `{"key": "value"}`
    - Complex nested structures.
