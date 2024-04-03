#ifndef TREE_H
#define TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

enum { HAS_VALUE, NO_VALUE };

typedef struct node {
    int line;
    char *name;
    char *value;
    // for example, name: ID, value: "a"
    struct node *child;
    struct node *sibling; // next
} Node;

struct node *p_empty;

Node *new_node(char *name, int line, Node *child, ...)
{
    // some don't need line number, set it to 0
    Node *node = (Node *)malloc(sizeof(Node));
    assert(node != NULL);
    node->name = (char *)malloc(strlen(name) + 1);
    strcpy(node->name, name);
    node->line = line;
    node->value = NULL;
    // 添加兄弟节点
    assert(child);
    node->child = child;
    Node *temp = node->child;
    va_list ap;
    va_start(ap, child);
    Node *to_add = va_arg(ap, Node *);
    while (to_add) {
        temp->sibling = to_add;
        // printf("add: %s\n", to_add->name);
        to_add = va_arg(ap, Node *);
        if (temp->sibling == p_empty)
            continue;
        ;
        temp = temp->sibling;
    }
    temp->sibling = NULL;
    va_end(ap);

    // va_list ap;
    // va_start(ap, child_num);
    // Node *temp = va_arg(ap, Node *);
    // node->child = temp;
    // for (int i = 1; i < child_num; i++) {
    //     temp->sibling = va_arg(ap, Node *);
    //     if (temp->sibling == NULL) {
    //         continue; // 可能某个子节点为NULL
    //     }
    //     temp = temp->sibling;
    //     // assert(temp != NULL);
    // }
    // va_end(ap);
    return node;
}

Node *new_token(char *name, char *value, int line)
{
    line = 0;
    // printf("new token: %s\n", name);
    // some tokens don't need value, set it to NULL
    Node *node = (Node *)malloc(sizeof(Node));
    assert(node != NULL);
    node->name = (char *)malloc(strlen(name) + 1);
    strcpy(node->name, name);
    node->line = line;
    if (value == NULL) {
        node->value = NULL;
        // return node;
    } else {
        node->value = (char *)malloc(strlen(value) + 1);
        strcpy(node->value, value);
    }
    node->sibling = NULL;
    node->child = NULL;
    return node;
}

void print_tree(Node *root, int level)
{
    if (root == NULL) {
        return;
    }
    // if (strcmp(root->name, "empty") == 0)
    //     goto print;
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    if (root->value == NULL) {
        if (root->line == 0) {
            printf("%s\n", root->name);
        } else {
            printf("%s (%d)\n", root->name, root->line);
        }
    } else {
        if (strcmp(root->name, "INT") == 0) {
            if (root->value[0] != '0') {
                printf("%s: %d\n", root->name, atoi(root->value));
            } else if (root->value[1] == 'x' || root->value[1] == 'X') {
                printf("%s: %d\n", root->name,
                       (int)strtol(root->value, NULL, 16));
            } else {
                printf("%s: %d\n", root->name,
                       (int)strtol(root->value, NULL, 8));
            }
        } else if (strcmp(root->name, "FLOAT") == 0) {
            printf("%s: %f\n", root->name, (float)atof(root->value));
        } else {
            printf("%s: %s\n", root->name, root->value);
        }
    }
print:
    print_tree(root->child, level + 1);
    print_tree(root->sibling, level);
}
struct node empty = {0, "empty", NULL, NULL, NULL};

struct node *p_empty = &empty;

#endif // TREE_H