#ifndef SEMANTIC
#define SEMANTIC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tree.h"

#define HASH_SIZE 0x3fff

typedef struct Type_ Type;
typedef struct FieldList_ FieldList;
typedef struct _var_hash_node var_hash_node;
typedef struct _func_type func_type;
typedef struct _func_hash_node func_hash_node;

enum { INT_TYPE = 1, FLOAT_TYPE };
enum {
    DECLARATION,
    DEFINITION = 1,
};

struct Type_ {
    enum { BASIC = 1, ARRAY, STRUCTURE, OptTag } kind;
    union {
        // 基本类型
        int basic;
        // 数组类型信息包括元素类型与数组大小构成
        struct {
            Type *elem;
            int size;
        } array;
        // 结构体类型信息是一个链表
        FieldList *structure;
    } u;
};

struct FieldList_ {
    char *name; // 域的名字
    Type *type; // 域的类型
    FieldList *next;
};

struct _var_hash_node {
    char *name;
    int lineno;
    int depth; //?
    Type *type;
    var_hash_node *next;
    var_hash_node *last;
};

struct _func_type {
    char *name;
    Type *return_type;
    FieldList *args;
    int line;
    bool wheather_defined;
    bool wheather_declared;
};

struct _func_hash_node {
    func_type *func;
    func_hash_node *next;
    func_hash_node *pre; // Do we really need this?
};

#define CHECK(node, correct_name)                                              \
    do {                                                                       \
        if (node == NULL)                                                      \
            printf("Error: %s NULL node at %s:%d\n", correct_name, __FILE__,   \
                   __LINE__);                                                  \
        else {                                                                 \
            node->visited = 1;                                                 \
            if (strcmp(node->name, correct_name) != 0) {                       \
                printf("It is a '%s' Node, not a '%s' Node at %s:%d\n",        \
                       node->name, correct_name, __FILE__, __LINE__);          \
            }                                                                  \
        }                                                                      \
    } while (0)

unsigned int hash_func(char *name);
void semantic_analysis(Node *root);
bool type_eq(Type *a, Type *b);
bool func_eq(func_type *a, func_type *b);
bool field_eq(FieldList *a, FieldList *b);
bool field_eq_args(FieldList *list, Node *args);

static inline Node *get_child(Node *node, int n);
static inline int get_child_num(Node *node);

void add_var(char *name, int line, Type *type);
void add_func(char *name, int line, Type *return_type, FieldList *args,
              bool defined);

void ExtDef(Node *node);
void DefList(Node *node);
Type *Specifier(Node *node);
Type *StructSpecifier(Node *node);
Type *VarDec(Node *node, Type *basic_type);
void FunDec(Node *node, Type *return_type, int definition);
void CompSt(Node *node, Type *type, bool is_func_compst);
void Stmt(Node *node, Type *type);
FieldList *VarList(Node *node, bool definition);
FieldList *ParamDec(Node *node, bool definition);
Type *StructSpecifier(Node *node);
FieldList *Struct_VarDec(Node *node, Type *basic_type);
void Def(Node *node);

void semantic_error(int error_type, int lineno, char *name);

Type *basic_type(int type);
Type *exp_type(Node *node);

bool redefined_in_FL(FieldList *fl, char *name);
Type *func_exp(Node *node);

void traversal(Node *node);
void handle(Node *node);
void finish();
void add_opt_tag(Node *node, Type *type);

// debug
#define RESET "\033[0m"     // 重置所有属性
#define BOLD "\033[1m"      // 粗体
#define UNDERLINE "\033[4m" // 下划线
#define RED "\033[31m"      // 红色
#define GREEN "\033[32m"    // 绿色
#define YELLOW "\033[33m"   // 黄色
#define BLUE "\033[34m"     // 蓝色
#define MAGENTA "\033[35m"  // 洋红色
#define CYAN "\033[36m"     // 青色

#define PRINT_COLOR(color, format, ...)                                        \
    printf(color format RESET, ##__VA_ARGS__)

#define PRINT_FORMAT(format, ...) printf(format, ##__VA_ARGS__)

#define I_AM_HERE printf(BLUE "I am here at %s:%d\n" RESET, __FILE__, __LINE__);

#endif