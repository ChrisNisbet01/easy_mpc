#pragma once

#include "easy_pc_private.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct child_list_t
{
    size_t count;
    size_t capacity;
    epc_cpt_node_t ** children;
} child_list_t;

// Initializes a child list. Allocates initial capacity.
// Returns true on success, false on failure.
EASY_PC_HIDDEN
bool
child_list_init(child_list_t * list, size_t initial_capacity);

// Appends a child node to the list. Resizes if necessary.
// Returns true on success, false on failure (e.g., allocation failure).
EASY_PC_HIDDEN
bool
child_list_append(child_list_t * list, epc_cpt_node_t * child);

// Releases the memory allocated for the children array, but DOES NOT free the child nodes themselves.
// Sets list->children to NULL and count/capacity to 0.
EASY_PC_HIDDEN
void
child_list_release(child_list_t * list);

// Transfers ownership of the children array from the list to the parent node.
// The child_list is then released (its internal pointer is nulled out),
// preventing double free and ensuring the parent node owns the memory.
EASY_PC_HIDDEN
void
child_list_transfer(child_list_t * list, epc_cpt_node_t * parent);
