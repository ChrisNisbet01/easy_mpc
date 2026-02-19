# AST Builder Generation Plan

## Goal
Enhance the `gdl_compiler` to automatically generate "bones" for an Abstract Syntax Tree (AST) builder. This removes the boilerplate of stack management and tree traversal while allowing the user to define their own AST node structures and logic.

## Architectural Strategy

### 1. The "Bones" Dispatcher
The compiler will generate a language-specific dispatcher that implements the `epc_cpt_visitor` interface. 
- **Automatic Stack Management:** Handles "Placeholder" nodes to mark boundaries between parent and child nodes.
- **Child Collection:** Automatically gathers all AST nodes produced by children into a `void **children` array before calling the user's action handler.
- **Dynamic Growth:** Uses a dynamically resizing stack to prevent overflows on deeply nested grammars.
- **Tagged Stack Entries:** Distinguishes between boundary markers (placeholders) and user-defined AST nodes to ensure safe stack unwinding.

### 2. The Push API
Action handlers will receive a context (`epc_ast_builder_ctx_t`) providing a `push` function.
- **Flexibility:** A single handler can push 0 items (pruning), 1 item (standard), or multiple items (flattening).
- **Safety:** The dispatcher handles the "pulling" and placeholder verification, keeping the user's "sandbox" clean.

### 3. Hook Interface (Dual-Layer API)
To allow the library to remain language-agnostic while providing a "pretty" interface for the user, the system uses a dual-layer approach:
- **Generic Library API (The "Raw" Layer):** The `easy_pc` library uses a `registry` containing an array of function pointers, indexed by the semantic action enum values. This allows the visitor to perform O(1) lookups without knowing language-specific names.
- **Generated Sugar API (The "Pretty" Layer):** The `gdl_compiler` generates a structure with named fields (e.g., `on_ADD_EXPR`) and a "Packing Function" that maps these named fields into the raw library array. This provides type safety and IDE friendliness for the developer.
- **Incremental Development:** The dispatcher will check for NULL pointers in the registry; unimplemented hooks can provide default pass-through behavior.
- **Memory Safety (Destructor):** The registry includes a specialized `free_node` callback used for automatic stack unwinding.

### 4. Memory Safety & Error Recovery
- **Automatic Unwinding:** If a parse fails or an error is triggered, the builder context will automatically traverse the stack and call the user-provided `free_node` function for every user node on the stack, preventing memory leaks in "half-built" trees.

---

## Task List

### Phase 1: Library Infrastructure (`lib/easy_pc`)
- [ ] **Define `epc_ast_stack_entry_t`**: Create a tagged union/struct to differentiate placeholders from user `void*` nodes.
- [ ] **Define `epc_ast_builder_ctx_t`**: Create the structure to hold the dynamic stack, capacity, user data, and the `free_node` destructor hook.
- [ ] **Implement Dynamic Stack**: Add `epc_ast_push` with `realloc` logic.
- [ ] **Implement Stack Cleanup**: Add `epc_ast_builder_cleanup` which safely unwinds the stack using the destructor hook.

### Phase 2: GDL Compiler Updates (`tools/gdl_compiler`)
- [ ] **Action Analysis**: Update the compiler to extract all unique semantic action names from the GDL AST.
- [ ] **Hook Header Generation**: Create a generator for `<lang>_ast_hooks.h` containing the function pointer structure and the `free_node` prototype.
- [ ] **Dispatcher Generation**: Create a generator for `<lang>_ast_builder.c` that implements the visitor, placeholder logic, and automated child gathering.
- [ ] **Stub Generation**: Create a generator for a template `<lang>_ast_actions.c` containing empty handler functions and a skeleton destructor.

### Phase 3: Integration & Validation
- [ ] **Update `main.c`**: Add command-line flags or logic to trigger AST code generation.
- [ ] **Refactor Arithmetic Example**: Convert the existing manual AST builder in `examples/arithmetic_parser` to use the new generated bones.
- [ ] **Verify Error Recovery**: Create a test case that fails mid-parse to verify that `free_node` is called correctly for all items on the stack.
- [ ] **Documentation**: Update `GDL_Language_Description.md` with instructions on how to use the generated AST builders and management of memory.
