#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    JSON_NODE_OBJECT,
    JSON_NODE_ARRAY,
    JSON_NODE_STRING,
    JSON_NODE_NUMBER,
    JSON_NODE_BOOLEAN,
    JSON_NODE_NULL,
    JSON_NODE_MEMBER, // Internal node for object key-value pairs
    JSON_NODE_LIST,   // Internal node for list of elements or members
} json_node_type_t;

typedef struct json_node_t json_node_t;

typedef struct json_list_node_t {
    json_node_t * item;
    struct json_list_node_t * next;
} json_list_node_t;

typedef struct {
    json_list_node_t * head;
    json_list_node_t * tail;
    int count;
} json_list_t;

typedef struct {
    char * key;
    json_node_t * value;
} json_member_t;

struct json_node_t {
    json_node_type_t type;
    union {
        json_list_t list;       // Used by OBJECT and ARRAY
        char * string;          // Used by STRING
        double number;          // Used by NUMBER
        bool boolean;           // Used by BOOLEAN
        json_member_t member;   // Used by MEMBER
    } data;
};

/* Function to free a JSON AST node and its children */
void
json_node_free(void * node, void * user_data);
