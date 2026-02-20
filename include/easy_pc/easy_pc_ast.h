#pragma once

#include <easy_pc/easy_pc.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Forward declarations of internal context types
typedef struct epc_ast_builder_ctx_t epc_ast_builder_ctx_t;

/**
 * @brief Callback for user-defined semantic actions during AST construction.
 *
 * @param ctx The AST builder context, used for pushing new AST nodes.
 * @param node The CPT node associated with the action, providing parse details.
 * @param children An array of AST nodes produced by the children of this CPT node.
 *        Note that action callbacks must consume these child nodes either by freeing
 *        them or attaching them to a new node that is pushed back onto the stack.
 * @param count The number of children in the `children` array.
 * @param user_data User-defined data pointer, passed through from `epc_ast_build`.
 */
typedef void (*epc_ast_action_cb)(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
);

/**
 * @brief Callback for entering a CPT node during AST construction.
 *        Optional, used for context management (e.g., symbol tables).
 *
 * @param ctx The AST builder context.
 * @param node The CPT node being entered.
 * @param user_data User-defined data pointer.
 */
typedef void (*epc_ast_enter_cb)(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * user_data
);

/**
 * @brief Callback for freeing a user-defined AST node.
 *        This callback is essential for memory safety and error recovery.
 *        It must be provided by the user if `epc_ast_push` is used to store
 *        dynamically allocated nodes.
 *
 * @param node The user-defined AST node to free.
 * @param user_data User-defined data pointer.
 */
typedef void (*epc_ast_node_free_cb)(void * node, void * user_data);

/**
 * @brief Registry for AST semantic action hooks and builder configuration.
 *        This structure holds an array of callbacks for each semantic action,
 *        along with special `free_node` and `enter_node` callbacks.
 */
typedef struct epc_ast_hook_registry_t epc_ast_hook_registry_t;

/**
 * @brief Creates a new AST hook registry.
 *        The `action_count` determines the size of the internal callback array.
 *
 * @param action_count The maximum value of any semantic action index that will be used.
 * @return A new `epc_ast_hook_registry_t` instance, or NULL on error.
 */
EASY_PC_API epc_ast_hook_registry_t *
epc_ast_hook_registry_create(int action_count);

/**
 * @brief Frees an AST hook registry and its associated resources.
 *
 * @param registry The registry to free.
 */
EASY_PC_API void
epc_ast_hook_registry_free(epc_ast_hook_registry_t * registry);

/**
 * @brief Sets a semantic action callback in the registry.
 *
 * @param registry The registry to update.
 * @param action_index The index of the action (from the semantic action enum).
 * @param cb The callback function to be called for this action.
 */
EASY_PC_API void
epc_ast_hook_registry_set_action(
    epc_ast_hook_registry_t * registry,
    int action_index,
    epc_ast_action_cb cb
);

/**
 * @brief Sets the node destructor callback in the registry.
 *        This callback is invoked by the AST builder to free user-defined
 *        AST nodes when they are no longer needed (e.g., during error recovery).
 *
 * @param registry The registry to update.
 * @param cb The destructor callback function.
 */
EASY_PC_API void
epc_ast_hook_registry_set_free_node(
    epc_ast_hook_registry_t * registry,
    epc_ast_node_free_cb cb
);

/**
 * @brief Sets the global enter-node callback in the registry.
 *        This callback is called every time the AST builder enters a CPT node.
 *
 * @param registry The registry to update.
 * @param cb The callback function.
 */
EASY_PC_API void
epc_ast_hook_registry_set_enter_node(
    epc_ast_hook_registry_t * registry,
    epc_ast_enter_cb cb
);

EASY_PC_API void
epc_ast_builder_set_error(
    epc_ast_builder_ctx_t * ctx,
    const char * format,
    ...
);

/**
 * @brief Pushes a user-defined AST node onto the builder's internal stack.
 *        This function is to be called from within user-defined semantic action callbacks.
 *
 * @param ctx The AST builder context.
 * @param node The user-defined AST node (a `void*` pointer) to push.
 */
EASY_PC_API void
epc_ast_push(epc_ast_builder_ctx_t * ctx, void * node);

/**
 * @brief Represents the result of an AST construction operation.
 */
typedef struct
{
    void * ast_root;         /**< @brief The root of the constructed AST. NULL if building failed or pruned. */
    bool has_error;          /**< @brief Flag indicating if an error occurred during construction. */
    char error_message[512]; /**< @brief Detailed error message on failure. */
} epc_ast_result_t;

/**
 * @brief Constructs an AST from a Concrete Parse Tree (CPT).
 *
 * This function traverses the CPT, calling appropriate semantic action callbacks
 * defined in the provided `registry` to build the Abstract Syntax Tree.
 *
 * @param root The root of the CPT.
 * @param registry The registry of semantic action hooks.
 * @param user_data Optional user data to be passed to all callbacks.
 * @return An `epc_ast_result_t` containing the result of the AST build.
 */
EASY_PC_API epc_ast_result_t
epc_ast_build(
    epc_cpt_node_t * root,
    epc_ast_hook_registry_t * registry,
    void * user_data
);

#ifdef __cplusplus
}
#endif
