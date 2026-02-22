#include "child_list.h"

#include <stdlib.h>

EASY_PC_HIDDEN
bool
child_list_init(child_list_t * list, size_t initial_capacity)
{
    if (list == NULL)
    {
        return false;
    }
    list->count = 0;
    list->capacity = initial_capacity > 0 ? initial_capacity : 4; // Default initial capacity
    list->children = calloc(list->capacity, sizeof(epc_cpt_node_t *));
    return list->children != NULL;
}

// Appends a child node to the list. Resizes if necessary.
// Returns true on success, false on failure (e.g., allocation failure).
EASY_PC_HIDDEN
bool
child_list_append(child_list_t * list, epc_cpt_node_t * child)
{
    if (list == NULL || child == NULL)
    {
        return false;
    }

    if (list->count == list->capacity)
    {
        size_t new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        epc_cpt_node_t ** new_children = realloc(list->children, new_capacity * sizeof(*new_children));
        if (new_children == NULL)
        {
            // Allocation failed, do not add child. The list remains in its current state.
            return false;
        }
        list->children = new_children;
        list->capacity = new_capacity;
    }

    list->children[list->count++] = child;
    return true;
}

// Releases the memory allocated for the children array, and ALSO frees the child nodes themselves.
// Sets list->children to NULL and count/capacity to 0.
EASY_PC_HIDDEN
void
child_list_release(child_list_t * list)
{
    if (list == NULL)
    {
        return;
    }
    for (size_t i = 0; i < list->count; i++)
    {
        epc_node_free(list->children[i]);
    }
    free(list->children);
    list->children = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Transfers ownership of the children array from the list to the parent node.
// The child_list is then released (its internal pointer is nulled out),
// preventing double free and ensuring the parent node owns the memory.
EASY_PC_HIDDEN
void
child_list_transfer(child_list_t * list, epc_cpt_node_t * parent)
{
    if (list == NULL)
    {
        return;
    }

    if (parent == NULL)
    {
        child_list_release(list);
        return;
    }

    // Assign the dynamically allocated array and its metadata to the parent node.
    parent->children = list->children;
    parent->children_count = list->count;

    // Prevent double free by nulling out the list's pointer.
    // The parent node now owns the memory.
    list->children = NULL;
    list->count = 0;
    list->capacity = 0;
}

