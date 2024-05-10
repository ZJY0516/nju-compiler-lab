#ifndef TREE_H
#define TREE_H

#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

enum { NO_NEED_PRINT, NEED_PRINT };

typedef struct node {
    int line;
    char *name;
    char *value;
    bool visited;
    bool ir_code_visited;
    bool need_print;
    // for example, name: ID, value: "a"
    struct node *child;
    struct node *sibling; // next
} Node;

Node *new_node(char *name, int line, Node *child, ...);
Node *new_token(char *name, char *value, int line);
void print_tree(Node *root, int level);

extern struct node *p_empty; // i don't understand
#endif // TREE_H