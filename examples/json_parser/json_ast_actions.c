#include "json_ast_actions.h"
#include "json_ast.h"
#include "semantic_actions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_LIST_INITIAL_CAPACITY 4

static bool
json_list_ensure_capacity(json_list_t * list)
{
    if (list->count < list->capacity)
    {
        return true; // Enough capacity
    }

    size_t new_capacity = list->capacity * 2;

    if (new_capacity == 0) // Handle initial zero capacity
    {
        new_capacity = JSON_LIST_INITIAL_CAPACITY;
    }

    json_node_t ** new_items = realloc(list->items, new_capacity * sizeof(json_node_t *));

    if (new_items == NULL)
    {
        return false; // Reallocation failed
    }

    list->items = new_items;
    list->capacity = new_capacity;

    return true;
}

static json_node_t *
json_node_alloc(json_node_type_t type)
{
    json_node_t * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = type;
    }
    return node;
}

void
json_node_free(void * node_ptr, void * user_data)
{
    json_node_t * node = (json_node_t *)node_ptr;
    if (node == NULL)
    {
        return;
    }

    switch (node->type)
    {
        case JSON_NODE_STRING:
            free(node->data.string);
            break;
        case JSON_NODE_OBJECT:
        case JSON_NODE_ARRAY:
        case JSON_NODE_LIST:
        {
            // Free individual items in the dynamic array
            for (size_t i = 0; i < node->data.list.count; i++)
            {
                json_node_free(node->data.list.items[i], user_data);
            }
            // Free the array itself
            free(node->data.list.items);
            break;
        }
        case JSON_NODE_MEMBER:
            free(node->data.member.key);
            json_node_free(node->data.member.value, user_data);
            break;
        case JSON_NODE_NUMBER:
        case JSON_NODE_BOOLEAN:
        case JSON_NODE_NULL:
            break;
    }
    free(node);
}

static void
ast_list_append(json_list_t * list, json_node_t * item)
{
    if (!json_list_ensure_capacity(list))
    {
        // Handle error: memory reallocation failed
        // For now, just return, will likely lead to further errors
        return;
    }

    list->items[list->count] = item;
    list->count++;
}

static void
free_children(void * * children, int count, void * user_data)
{
    for (int i = 0; i < count; i++)
    {
        if (children[i] != NULL)
        {
            json_node_free((json_node_t *)children[i], user_data);
        }
    }
}

/* --- Semantic Action Callbacks --- */

static void
create_string_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    if (count != 0)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "String action expected 0 children, but got %u\n", count);
        return;
    }

    json_node_t * jnode = json_node_alloc(JSON_NODE_STRING);
    if (!jnode)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate JSON string node");
        return;
    }
    const char * content = epc_cpt_node_get_semantic_content(node);
    size_t len = epc_cpt_node_get_semantic_len(node);

    // Remove quotes if present (epc_between includes them in the matched range)
    if (len >= 2 && content[0] == '"' && content[len - 1] == '"')
    {
        jnode->data.string = strndup(content + 1, len - 2);
    }
    else
    {
        jnode->data.string = strndup(content, len);
    }

    epc_ast_push(ctx, jnode);
}

static void
create_number_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    if (count != 0)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Number action expected 0 children, but got %u\n", count);
        return;
    }

    json_node_t * jnode = json_node_alloc(JSON_NODE_NUMBER);
    if (!jnode)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate JSON number node");
        return;
    }

    const char * content = epc_cpt_node_get_semantic_content(node);
    size_t len = epc_cpt_node_get_semantic_len(node);
    char * endptr;
    char * buf = strndup(content, len);
    if (buf == NULL)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to duplicate number string");
        free(jnode);
        return;
    }
    jnode->data.number = strtod(buf, &endptr);
    free(buf);

    epc_ast_push(ctx, jnode);
}

static void
create_boolean_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    if (count != 0)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Boolean action expected 0 children, but got %u\n", count);
        return;
    }

    json_node_t * jnode = json_node_alloc(JSON_NODE_BOOLEAN);
    if (!jnode)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate JSON boolean node");
        return;
    }
    const char * content = epc_cpt_node_get_semantic_content(node);
    jnode->data.boolean = (strncmp(content, "true", 4) == 0);

    epc_ast_push(ctx, jnode);
}

static void
create_null_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node;

    if (count != 0)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Null action expected 0 children, but got %u\n", count);
        return;
    }

    json_node_t * jnode = json_node_alloc(JSON_NODE_NULL);
    if (!jnode)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate JSON null node");
        return;
    }

    epc_ast_push(ctx, jnode);
}

static void
create_list_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node; (void)user_data;
    json_node_t * list_node = json_node_alloc(JSON_NODE_LIST);
    if (!list_node)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate JSON list node");
        return;
    }

    // Children are passed in the order they appear in the grammar
    for (int i = 0; i < count; i++)
    {
        if (children[i] != NULL)
        {
            ast_list_append(&list_node->data.list, (json_node_t *)children[i]);
        }
    }

    epc_ast_push(ctx, list_node);
}

static void
create_optional_list_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node; (void)user_data;

    if (count == 0)
    {
        // Empty list
        json_node_t * list_node = json_node_alloc(JSON_NODE_LIST);
        if (list_node == NULL)
        {
            epc_ast_builder_set_error(ctx, "Failed to allocate JSON list node for empty optional list");
            return;
        }
        epc_ast_push(ctx, list_node);
    }
    else
    {
        // Passthrough the list created by CREATE_ARRAY_ELEMENTS or CREATE_OBJECT_ELEMENTS
        epc_ast_push(ctx, children[0]);
    }
}

static void
create_array_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node; (void)user_data;
    // json_array: [ , optional_elements, ]

    if (count != 1 || ((json_node_t *)children[0])->type != JSON_NODE_LIST)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Array action expected a LIST type node, but found an unexpected type");
        return;
    }

    json_node_t * list_node = (json_node_t *)children[0];

    list_node->type = JSON_NODE_ARRAY;
    epc_ast_push(ctx, list_node);
}

static void
create_member_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node;
    // member: quoted_string, colon, value

    if (count != 2)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "JSON member expected 2 children, but got %u\n", count);
        return;
    }
    json_node_t * key_node = (json_node_t *)children[0];
    json_node_t * value_node = (json_node_t *)children[1];

    json_node_t * member_node = json_node_alloc(JSON_NODE_MEMBER);
    if (member_node == NULL)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Failed to allocate Member node");
        return;
    }

    member_node->data.member.key = key_node->data.string;
    key_node->data.string = NULL;
    member_node->data.member.value = value_node;

    json_node_free(key_node, user_data);
    /* No need to free the value node as it is attached to the member node. */

    epc_ast_push(ctx, member_node);
}

static void
create_object_action(
    epc_ast_builder_ctx_t * ctx,
    epc_cpt_node_t * node,
    void * * children,
    int count,
    void * user_data
)
{
    (void)node;

    if (count != 1 || ((json_node_t *)children[0])->type != JSON_NODE_LIST)
    {
        free_children(children, count, user_data);
        epc_ast_builder_set_error(ctx, "Object action expected 1 child of type LIST, but received %u children or an invalid type", count);
        return;
    }

    // json_object: { , optional_members, }
    json_node_t * list_node = (json_node_t *)children[0];

    list_node->type = JSON_NODE_OBJECT;

    epc_ast_push(ctx, list_node);
}

void
json_ast_hook_registry_init(epc_ast_hook_registry_t * registry)
{
    epc_ast_hook_registry_set_free_node(registry, json_node_free);

    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_STRING, create_string_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_NUMBER, create_number_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_BOOLEAN, create_boolean_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_NULL, create_null_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_ARRAY_ELEMENTS, create_list_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_OPTIONAL_ARRAY_ELEMENTS, create_optional_list_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_ARRAY, create_array_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_MEMBER, create_member_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_OBJECT_ELEMENTS, create_list_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_OPTIONAL_OBJECT_ELEMENTS, create_optional_list_action);
    epc_ast_hook_registry_set_action(registry, JSON_ACTION_CREATE_OBJECT, create_object_action);
}
