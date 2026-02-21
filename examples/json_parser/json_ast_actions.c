#include "json_ast_actions.h"
#include "json_ast.h"
#include "semantic_actions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            json_list_node_t * curr = node->data.list.head;
            while (curr)
            {
                json_list_node_t * next = curr->next;
                json_node_free(curr->item, user_data);
                free(curr);
                curr = next;
            }
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
    json_list_node_t * new_node = calloc(1, sizeof(*new_node));
    if (new_node == NULL)
    {
        return;
    }
    new_node->item = item;
    new_node->next = NULL;

    if (list->tail)
    {
        list->tail->next = new_node;
    }
    else
    {
        list->head = new_node;
    }
    list->tail = new_node;
    list->count++;
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
    (void)children; (void)count; (void)user_data;
    json_node_t * jnode = json_node_alloc(JSON_NODE_STRING);
    if (!jnode)
    {
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
    (void)children; (void)count; (void)user_data;
    json_node_t * jnode = json_node_alloc(JSON_NODE_NUMBER);
    if (!jnode)
    {
        epc_ast_builder_set_error(ctx, "Failed to allocate JSON number node");
        return;
    }
    const char * content = epc_cpt_node_get_semantic_content(node);
    size_t len = epc_cpt_node_get_semantic_len(node);
    char * endptr;
    char * buf = strndup(content, len);
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
    (void)children; (void)count; (void)user_data;
    json_node_t * jnode = json_node_alloc(JSON_NODE_BOOLEAN);
    if (!jnode)
    {
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
    (void)node; (void)children; (void)count; (void)user_data;
    json_node_t * jnode = json_node_alloc(JSON_NODE_NULL);
    if (!jnode)
    {
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
    if (count == 0 || children[0] == NULL)
    {
        // Empty list
        json_node_t * list_node = json_node_alloc(JSON_NODE_LIST);
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
    json_node_t * list_node = NULL;
    for (int i = 0; i < count; i++)
    {
        if (children[i] != NULL && ((json_node_t *)children[i])->type == JSON_NODE_LIST)
        {
            list_node = (json_node_t *)children[i];
            break;
        }
    }

    if (!list_node)
    {
        epc_ast_builder_set_error(ctx, "Failed to find list in JSON array children");
        return;
    }

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
    (void)node; (void)user_data;
    // member: quoted_string, colon, value
    json_node_t * key_node = NULL;
    json_node_t * value_node = NULL;

    for (int i = 0; i < count; i++)
    {
        if (children[i] != NULL)
        {
            if (key_node == NULL) key_node = (json_node_t *)children[i];
            else value_node = (json_node_t *)children[i];
        }
    }

    if (!key_node || !value_node || key_node->type != JSON_NODE_STRING)
    {
        epc_ast_builder_set_error(ctx, "Invalid children for JSON member");
        return;
    }

    json_node_t * member_node = json_node_alloc(JSON_NODE_MEMBER);
    member_node->data.member.key = strdup(key_node->data.string);
    member_node->data.member.value = value_node;

    // Free the temporary key node
    json_node_free(key_node, user_data);

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
    (void)node; (void)user_data;
    // json_object: { , optional_members, }
    json_node_t * list_node = NULL;
    for (int i = 0; i < count; i++)
    {
        if (children[i] != NULL && ((json_node_t *)children[i])->type == JSON_NODE_LIST)
        {
            list_node = (json_node_t *)children[i];
            break;
        }
    }

    if (!list_node)
    {
        epc_ast_builder_set_error(ctx, "Failed to find list in JSON object children");
        return;
    }

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
