#include "semantic.h"

var_hash_node *var_hash_table[HASH_SIZE + 1] = {NULL};
func_hash_node *func_hash_table[HASH_SIZE + 1] = {NULL};
var_list_stack var_stack[DEPTH];
int current_depth = 0;

void init()
{
    for (int i = 0; i < DEPTH; i++) {
        var_stack[i].head = var_stack[i].tail = NULL;
    }
}

void handle_scope(var_hash_node *node)
{
    var_list_stack *cur = &var_stack[current_depth];
    var_list_node *list_node = malloc(sizeof(var_list_node));
    list_node->next = NULL;
    list_node->node = node;
    if (cur->head == NULL) {
        cur->head = list_node;
        cur->tail = list_node;
    } else {
        cur->tail->next = list_node;
        cur->tail = list_node;
    }
}

void delete_val(var_hash_node *node)
{
    int id = hash_func(node->name);
    if (node->last == NULL) {
        var_hash_table[id] = node->next;
    } else {
        node->last->next = node->next;
        if (node->next) {
            node->next->last = node->last;
        }
    }
    free(node);
}

void delete_stack()
{
    var_list_node *head = var_stack[current_depth].head;
    while (head) {
        delete_val(head->node);
        head = head->next;
    }
    var_stack[current_depth].head = NULL;
    var_stack[current_depth].tail = NULL;
    current_depth--;
}

static inline int get_child_num(Node *node)
{
    int num = 0;
    Node *tmp = node->child;
    while (tmp != NULL) {
        num++;
        tmp = tmp->sibling;
    }
    return num;
}

Node *get_id_node(Node *Vardec)
{
    // CHECK(Vardec, "VarDec");
    assert(strcmp(Vardec->name, "VarDec") == 0 ||
           strcmp(Vardec->name, "Tag") == 0);
    Node *tmp = Vardec;
    while (strcmp(tmp->name, "ID")) {
        tmp = tmp->child;
    }
    return tmp;
}

/*
VarDec → ID
| VarDec LB INT RB
*/
// get type of variable, including array type
Type *get_id_type(Node *node, Type *basic_type)
{
    CHECK(node, "VarDec");
    Type *ans = basic_type;
    while (get_child_num(node) == 4) {
        Type *new_type = malloc(sizeof(Type));
        new_type->kind = ARRAY;
        new_type->u.array.size = atoi(get_child(node, 2)->value);
        new_type->u.array.elem = ans;
        ans = new_type;
        node = node->child;
    }
    return ans;
}

// input: variable name
// output: hash value
unsigned int hash_func(char *name)
{
    unsigned int val = 0;
    for (; *name; ++name) {
        val = (val << 2) + *name;
        unsigned int i = val & ~HASH_SIZE;
        if (i)
            val = (val ^ (i >> 12)) & HASH_SIZE;
    }
    return val;
}

/*
get variable node by name from hash table, if not found, return NULL
input: variable name
output: variable node ptr
type may be different?
 */
var_hash_node *get_var_hash_node(char *key)
{
    int id = hash_func(key);
    var_hash_node *tmp = var_hash_table[id];
    var_hash_node *ans = NULL;
    while (tmp != NULL) {
        if (strcmp(key, tmp->name) == 0) {
            ans = tmp;
        }
        tmp = tmp->next;
    }
    return ans;
}

// get function node by name from hash table, if not found, return NULL
// input: function name
// output: function node ptr
func_hash_node *get_func_hash_node(char *key)
{
    int id = hash_func(key);
    func_hash_node *tmp = func_hash_table[id];
    func_hash_node *ans = NULL;
    while (tmp != NULL) {
        if (strcmp(key, tmp->func->name) == 0) {
            ans = tmp;
            return ans;
        } else {
            tmp = tmp->next;
        }
    }
    return ans;
}

bool type_eq(Type *a, Type *b)
{
    // assert(a != NULL && b != NULL);
    if (a == NULL || b == NULL)
        return false;
    if (a->kind != b->kind)
        return false;
    else {
        switch (a->kind) {
        case BASIC:
            return a->u.basic == b->u.basic;
        case ARRAY:
            return type_eq(a->u.array.elem, b->u.array.elem);
            // size? a[10][2]和 int b[5][3]属于同一类型
        case STRUCTURE: {
            FieldList *fa = a->u.structure;
            FieldList *fb = b->u.structure;
            return strcmp(fa->name, fb->name) == 0; //???? leave a question
        }
        default:
            assert(0);
        }
    }
}

bool func_eq(func_type *a, func_type *b)
{
    assert(a != NULL && b != NULL);
    if (!type_eq(a->return_type, b->return_type))
        return false;
    FieldList *fa = a->args;
    FieldList *fb = b->args;
    while (fa != NULL && fb != NULL) {
        if (!type_eq(fa->type, fb->type))
            return false;
        fa = fa->next;
        fb = fb->next;
    }
    if (fa != NULL || fb != NULL) //?????
        return false;
    return true;
}

// void handle_var(Node *node)
// {
//     if (strcmp(node->name, "ExtDef") == 0)
//         ExtDef(node);
//     else if (strcmp(node->name, "DefList") == 0)
//         DefList(node);
//     else if (strcmp(node->name, "Exp") == 0)
//         Exp(node);
// }

/*
ExtDef → Specifier ExtDecList SEMI
| Specifier SEMI
| Specifier FunDec CompSt
| Specifier FunDec SEMI
*/
// 一个全局变量、结构体或函数
void ExtDef(Node *node)
{
    CHECK(node, "ExtDef");
    Type *type = Specifier(node->child);

    if (strcmp(get_child(node, 1)->name, "ExtDecList") == 0) {
        Node *ExtDecList_node = get_child(node, 1);
        /*
        ExtDecList → VarDec
        | VarDec COMMA ExtDecList
        */
        while (1) {
            assert(ExtDecList_node != NULL);
            Node *Vardec_node = ExtDecList_node->child;
            VarDec(Vardec_node, type);
            if (get_child_num(ExtDecList_node) > 1) {
                ExtDecList_node = get_child(ExtDecList_node, 2);
            } else
                break;
        }
    } else if (strcmp(get_child(node, 1)->name, "FunDec") == 0) {
        if (strcmp(get_child(node, 2)->name, "SEMI") == 0) {
            FunDec(get_child(node, 1), type, DECLARATION);
        } else if (strcmp(get_child(node, 2)->name, "CompSt") == 0) {
            current_depth++;
            FunDec(get_child(node, 1), type, DEFINITION);
            CompSt(get_child(node, 2), type, true);
        } else
            assert(0);
    }
}
/*
DefList → Def DefList
| p_empty
Def → Specifier DecList SEMI
DecList → Dec
| Dec COMMA DecList
Dec → VarDec
| VarDec ASSIGNOP Exp
*/
void Def(Node *node)
{
    CHECK(node, "Def");
    Type *type = Specifier(get_child(node, 0));
    Node *DecList_node = get_child(node, 1);
    while (1) {
        Node *Dec_node = get_child(DecList_node, 0);
        Node *vardec_node = get_child(Dec_node, 0);
        if (get_child_num(Dec_node) == 1) {
            VarDec(vardec_node, type);
        } else if (get_child_num(Dec_node) == 3) {
            Type *ltype = VarDec(vardec_node, type);
            if (!type_eq(ltype, exp_type(get_child(Dec_node, 2)))) {
                semantic_error(5, get_child(Dec_node, 1)->line, NULL);
            }
        } else
            assert(0);
        if (get_child_num(DecList_node) > 1) {
            DecList_node = get_child(DecList_node, 2);
        } else
            break;
    }
}
/*
DefList → Def DefList
| p_empty
*/
void DefList(Node *node)
{
    assert(node != NULL);
    // CHECK(node, "DefList");
    while (node != p_empty && node != NULL) {
        // assert(get_child_num(node) == 2); //?????
        CHECK(node, "DefList");
        Def(node->child);
        node = get_child(node, 1);
    }
}

FieldList *Struct_VarDec(Node *node, Type *basic_type)
{
    CHECK(node, "VarDec");
    FieldList *field = (FieldList *)malloc(sizeof(FieldList));
    field->type = get_id_type(node, basic_type);
    field->name = get_id_node(node)->value;
    field->next = NULL;
    add_var(field->name, get_id_node(node)->line, field->type);
    return field;
}

// ugly!!!!
Type *StructSpecifier(Node *node)
{
    CHECK(node, "StructSpecifier");
    Type *type = malloc(sizeof(Type));
    type->kind = STRUCTURE;
    type->u.structure = NULL;
    FieldList *cur = type->u.structure;
    int child_num = get_child_num(node);
    switch (child_num) {
    case 4: {
        // STRUCT  LC DefList RC
        Node *DefList_node = get_child(node, 2);
        assert(DefList_node);

        while (DefList_node != p_empty && DefList_node != NULL) {
            CHECK(DefList_node, "DefList");
            Node *Def_node = get_child(DefList_node, 0);
            Type *basic_type = Specifier(
                get_child(Def_node, 0)); // Def → Specifier DecList SEMI

            Node *DecList_node = get_child(Def_node, 1);
            while (1) {
                /*
                DecList → Dec
                | Dec COMMA DecList
                Dec → VarDec
                | VarDec ASSIGNOP Exp
                */
                Node *dec = get_child(DecList_node, 0);
                Node *vardec = get_child(dec, 0);
                if (get_child_num(dec) == 1) {
                    FieldList *field = Struct_VarDec(vardec, basic_type);
                    if (redefined_in_FL(type->u.structure, field->name)) {
                        semantic_error(15, get_id_node(vardec)->line,
                                       field->name);
                        free(field);
                    } else {
                        if (cur == NULL) {
                            cur = field;
                            type->u.structure = cur;
                        } else {
                            cur->next = field;
                            cur = cur->next;
                        }
                    }
                } else if (get_child_num(dec) == 3) {
                    FieldList *field = Struct_VarDec(vardec, basic_type);
                    semantic_error(15, get_child(dec, 1)->line, NULL);
                    if (cur == NULL) {
                        cur = field;
                        type->u.structure = cur;
                    } else {
                        cur->next = field;
                        cur = cur->next;
                    }
                } else
                    assert(0);

                if (get_child_num(DecList_node) > 1) {
                    DecList_node = get_child(DecList_node, 2);
                    /*
                    DecList → Dec
                    | Dec COMMA DecList
                    */
                } else {
                    break;
                }
            }
            DefList_node = get_child(DefList_node, 1);
        }
    } break;
    case 5: { // STRUCT OptTag LC DefList RC
        /*
    DefList → Def DefList
    | p_empty
    Def → Specifier DecList SEMI
    DecList → Dec
    | Dec COMMA DecList
    Dec → VarDec
    | VarDec ASSIGNOP Exp
    */

        Node *DefList_node = get_child(node, 3);
        assert(DefList_node);
        while (DefList_node != p_empty && DefList_node != NULL) {
            CHECK(DefList_node, "DefList");
            Node *Def_node = get_child(DefList_node, 0);
            Type *basic_type = Specifier(
                get_child(Def_node, 0)); // Def → Specifier DecList SEMI
            Node *DecList_node = get_child(Def_node, 1);
            while (1) {
                /*
                DecList → Dec
                | Dec COMMA DecList
                Dec → VarDec
                | VarDec ASSIGNOP Exp
                */
                Node *dec = get_child(DecList_node, 0);
                Node *vardec = get_child(dec, 0);
                if (get_child_num(dec) == 1) {
                    FieldList *field = Struct_VarDec(vardec, basic_type);
                    if (redefined_in_FL(type->u.structure, field->name)) {
                        semantic_error(15, get_id_node(vardec)->line,
                                       field->name);
                        free(field);
                    } else {
                        if (cur == NULL) {
                            cur = field;
                            type->u.structure = cur;
                        } else {
                            cur->next = field;
                            cur = cur->next;
                        }
                    }
                } else if (get_child_num(dec) == 3) {
                    semantic_error(15, get_child(dec, 1)->line, NULL);
                    FieldList *field = Struct_VarDec(vardec, basic_type);
                    if (cur == NULL) {
                        cur = field;
                        type->u.structure = cur;
                    } else {
                        cur->next = field;
                        cur = cur->next;
                    }
                } else
                    assert(0);

                if (get_child_num(DecList_node) > 1) {
                    DecList_node = get_child(DecList_node, 2);
                    /*
                    DecList → Dec
                    | Dec COMMA DecList
                    */
                } else {
                    break;
                }
            }
            DefList_node = get_child(DefList_node, 1);
        }
        if (get_child(node, 1) != p_empty) {
            add_opt_tag(get_child(node, 1), type);
        }
    } break;
    case 2: {
        // STRUCT Tag
        char *name = get_child(node, 1)->child->value;
        var_hash_node *var_node = get_var_hash_node(name);
        if (var_node == NULL) {
            semantic_error(17, get_id_node(get_child(node, 1))->line, name);
            return NULL;
        }
        if (var_node->type->kind != STRUCTURE) {
            semantic_error(17, get_id_node(get_child(node, 1))->line, name);
            return NULL;
        }
        type = var_node->type; //???
    } break;
    }
    return type;
}

// add a struct name to hash table after checking whether it is redefined
void add_opt_tag(Node *node, Type *type)
{
    /*
    STRUCT OptTag LC DefList RC
    OptTag → ID
    | p_empty
    */
    // we ensure node is not NULL
    assert(node != NULL);
    Node *id = node->child;
    char *name = id->value;
    unsigned int hash_val = hash_func(name);
    var_hash_node *var_node = var_hash_table[hash_val];
    while (var_node != NULL) {
        if (strcmp(var_node->name, name) == 0) {
            semantic_error(16, id->line, name);
            return;
        }
        var_node = var_node->next;
    }
    add_var(name, id->line, type);
}

bool redefined_in_FL(FieldList *fl, char *name)
{
    while (fl != NULL) {
        if (strcmp(fl->name, name) == 0)
            return true;
        fl = fl->next;
    }
    return false;
}

void Stmt(Node *node, Type *type)
{
    /*
    Stmt → Exp SEMI
    | CompSt
    | RETURN Exp SEMI
    | IF LP Exp RP Stmt
    | IF LP Exp RP Stmt ELSE Stmt
    | WHILE LP Exp RP Stmt
    */
    CHECK(node, "Stmt");
    assert(node->child);
    if (strcmp(get_child(node, 0)->name, "Exp") == 0) {
        exp_type(get_child(node, 0));
    } else if (strcmp(get_child(node, 0)->name, "CompSt") == 0) {
        CompSt(get_child(node, 0), type, false);
    } else if (strcmp(get_child(node, 0)->name, "RETURN") == 0) {
        if (!type_eq(exp_type(get_child(node, 1)), type)) {
            semantic_error(8, get_child(node, 0)->line, NULL);
        }
    } else if (strcmp(get_child(node, 0)->name, "IF") == 0) {
        if (!type_eq(exp_type(get_child(node, 2)), basic_type(INT_TYPE))) {
            semantic_error(7, get_child(node, 2)->line, NULL);
        }
        Stmt(get_child(node, 4), type);
        if (get_child_num(node) == 7) {
            Stmt(get_child(node, 6), type);
        }
    } else if (strcmp(get_child(node, 0)->name, "WHILE") == 0) {
        if (!type_eq(exp_type(get_child(node, 2)), basic_type(INT_TYPE))) {
            semantic_error(7, get_child(node, 2)->line, NULL);
        }
        Stmt(get_child(node, 4), type);
    } else
        assert(0);
}

void CompSt(Node *node, Type *type, bool is_func_compst)
{
    /*
    CompSt → LC DefList StmtList RC
    StmtList → Stmt StmtList
    | p_empty
    */
    // maybe Deflist and StmtList are empty
    CHECK(node, "CompSt");
    if (!is_func_compst)
        current_depth++;
    if (strcmp(get_child(node, 1)->name, "DefList") == 0) {
        DefList(get_child(node, 1));
        Node *StmtList_node = get_child(node, 2);
        if (strcmp(StmtList_node->name, "RC") == 0) {
            return;
        }
        while (StmtList_node != p_empty && StmtList_node != NULL) {
            Stmt(StmtList_node->child, type);
            StmtList_node = get_child(StmtList_node, 1);
        }
    } else {
        Node *StmtList_node = get_child(node, 1);
        if (StmtList_node == p_empty || StmtList_node == NULL ||
            StmtList_node->child == NULL) {
            return;
        }
        if (strcmp(StmtList_node->name, "RC") == 0) {
            return;
        }
        while (StmtList_node != p_empty && StmtList_node != NULL) {
            Stmt(StmtList_node->child, type);
            StmtList_node = get_child(StmtList_node, 1);
        }
    }
    delete_stack(); //?????
}

// input: node and basic type.
// for example,int a[10][2] and int is basic type
// output: type of variable, array or basic type
Type *VarDec(Node *node, Type *basic_type)
{
    CHECK(node, "VarDec");
    Node *id = get_id_node(node);
    Type *type = get_id_type(node, basic_type);
    add_var(id->value, id->line, type);
    return type;
}

// ParamDec → Specifier VarDec
// do we really need name?
FieldList *ParamDec(Node *node, bool definition)
{
    CHECK(node, "ParamDec");
    FieldList *field = (FieldList *)malloc(sizeof(FieldList));
    Type *type = Specifier(get_child(node, 0));
    field->type = get_id_type(get_child(node, 1), type);
    field->next = NULL;
    field->name = get_id_node(get_child(node, 1))->value;
    if (definition)
        add_var(field->name, get_id_node(get_child(node, 1))->line,
                field->type);
    return field;
}
/*
VarList → ParamDec COMMA VarList
| ParamDec
*/
// for example foo(int x, float y[10])
// varlist = int x, float y[10]
FieldList *VarList(Node *node, bool definition)
{
    CHECK(node, "VarList");
    FieldList *field = (FieldList *)malloc(sizeof(FieldList));
    field = ParamDec(get_child(node, 0), definition);
    if (get_child_num(node) == 3) {
        field->next = VarList(get_child(node, 2), definition);
    } else {
        field->next = NULL;
    }
    return field;
}

void FunDec(Node *node, Type *return_type, int definition)
{
    /*
    ExtDef:
    Specifier FunDec SEMI // declaration, definition == 0
    | Specifier FunDec CompSt //definition, definition == 1

    FunDec → ID LP VarList RP
    | ID LP RP
    */
    CHECK(node, "FunDec");
    char *name = get_child(node, 0)->value;
    FieldList *args = NULL;
    int line = get_child(node, 0)->line;
    if (get_child_num(node) == 4) {
        args = VarList(get_child(node, 2), definition);
    }
    func_hash_node *func_node = get_func_hash_node(name);
    if (func_node == NULL) {
        add_func(name, line, return_type, args, definition);
    } else {
        if (type_eq(return_type, func_node->func->return_type) &&
            field_eq(args, func_node->func->args)) {
            if (definition == 1) {
                if (func_node->func->wheather_defined == true) { // redefine
                    semantic_error(4, line, name);
                } else {
                    assert(func_node->func->wheather_defined == false);
                    func_node->func->wheather_defined = true;
                }
            } else {
                // nothing
            }
        } else {
            if (definition == 1) {
                if (func_node->func->wheather_defined == true) {
                    semantic_error(4, line, name);
                } else {
                    semantic_error(19, line, name);
                }
            } else {
                semantic_error(19, line, name);
            }
        }
    }
}
Type *__int_type = NULL;
Type *__float_type = NULL;

// retun a Type pointer to int type or float type.
// global variable __int_type and __float_type are used to store the
// pointer, for saving memory.
inline Type *basic_type(int type)
{
    if (type == INT_TYPE) {
        if (__int_type == NULL) {
            __int_type = (Type *)malloc(sizeof(Type));
            __int_type->kind = BASIC;
            __int_type->u.basic = INT_TYPE;
        }
        return __int_type;
    } else if (type == FLOAT_TYPE) {
        if (__float_type == NULL) {
            __float_type = (Type *)malloc(sizeof(Type));
            __float_type->kind = BASIC;
            __float_type->u.basic = FLOAT_TYPE;
        }
        return __float_type;
    } else {
        assert(0);
    }
}

/*
Specifier → TYPE
| StructSpecifier
StructSpecifier → STRUCT OptTag LC DefList RC
| STRUCT Tag
OptTag → ID
| \episilon
Tag → ID
*/
// return type of variable
Type *Specifier(Node *node)
{
    if (strcmp(node->child->name, "TYPE") == 0) {
        Type *type = NULL;
        if (strcmp(node->child->value, "int") == 0) {
            type = basic_type(INT_TYPE);
        } else if (strcmp(node->child->value, "float") == 0) {
            type = basic_type(FLOAT_TYPE);
        } else {
            assert(0);
        }
        return type;
    } else if (strcmp(node->child->name, "StructSpecifier") == 0) {
        return StructSpecifier(node->child);
    } else {
        assert(0);
    }
}

// add variable to hash table
// two different variable may have same hash value, so we use a linked list
void add_var(char *name, int line, Type *type)
{
    //????????
    unsigned int hash_val = hash_func(name);
    var_hash_node *var_node = var_hash_table[hash_val];
    while (var_node != NULL) {
        // if (strcmp(var_node->name, name) == 0) {
        //     semantic_error(3, line, name);
        //     return;
        // }
        if (strcmp(var_node->name, name) == 0) {
            if (var_node->type->kind == STRUCTURE) {
                semantic_error(3, line, name);
            }
            if (var_node->depth == current_depth) {
                semantic_error(3, line, name);
                return;
            }
        }
        var_node = var_node->next;
    }
    var_node = (var_hash_node *)malloc(sizeof(var_hash_node));
    var_node->name = name;
    var_node->lineno = line;
    var_node->type = type;
    var_node->depth = current_depth;
    var_node->next = var_hash_table[hash_val];
    var_hash_table[hash_val] = var_node;
    handle_scope(var_node);
}

// add function to hash table
// TODO: how to handle declare and define
// one function can be declared more than once, but can only be defined once
void add_func(char *name, int line, Type *return_type, FieldList *args,
              bool defined)
{
    int hash_value = hash_func(name);
    func_hash_node *func_node = malloc(sizeof(func_hash_node));
    func_node->func = malloc(sizeof(func_type));
    assert(func_node->func != NULL);

    func_node->func->name = (char *)malloc(strlen(name) + 1);
    strcpy(func_node->func->name, name);
    func_node->func->return_type = return_type;
    func_node->func->args = args;
    func_node->func->line = line;
    func_node->func->wheather_defined = defined;  //????
    func_node->func->wheather_declared = defined; //????

    if (func_hash_table[hash_value] == NULL) {
        func_hash_table[hash_value] = func_node;
        func_node->pre = NULL;
    } else {
        func_hash_node *temp = func_hash_table[hash_value];
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = func_node;
        func_node->pre = temp;
    }
}

// return child of node
// get_child(node, 0) == node->child
// get_child(node, 1) == node->child->sibling
static inline Node *get_child(Node *node, int n)
{
    assert(node != NULL);
    Node *child = node->child;
    while (n-- && child != NULL)
        child = child->sibling;
    return child;
}

Type *exp_type(Node *node)
{
    CHECK(node, "Exp");
    int n = get_child_num(node);
    switch (n) {
    case 1: {
        Node *child = get_child(node, 0);
        if (strcmp(child->name, "ID") == 0) {
            var_hash_node *var_node = get_var_hash_node(child->value);
            // var_hash_node *backup = var_node;
            // for (int i = 0; i < DEPTH; i++) {
            //     var_node = backup;
            //     while (var_node != NULL) {
            //         if (strcmp(var_node->name, child->value) == 0 &&
            //             current_depth == var_node->depth + i) {
            //             return var_node->type;
            //         }
            //         var_node = var_node->next;
            //     }
            // }
            if (var_node == NULL) {
                semantic_error(1, child->line, child->value);
                return NULL;
            }
            return var_node->type;
        } else if (strcmp(child->name, "INT") == 0) {
            return basic_type(INT_TYPE);
        } else if (strcmp(child->name, "FLOAT") == 0) {
            return basic_type(FLOAT_TYPE);
        } else
            assert(0);
    } break;
    case 2: {
        Node *child = get_child(node, 0);
        if (strcmp(child->name, "MINUS") == 0) {
            return exp_type(get_child(node, 1));
        } else if (strcmp(child->name, "NOT") == 0) {
            if (type_eq(exp_type(get_child(node, 1)), basic_type(INT_TYPE))) {
                return basic_type(INT_TYPE);
            } else {
                semantic_error(7, child->line, NULL);
                return NULL;
            }
        } else
            assert(0);
    } break;
    case 3: {
        if (strcmp(get_child(node, 0)->name, "Exp") == 0 &&
            strcmp(get_child(node, 2)->name, "Exp") == 0) {
            Type *type1 = exp_type(get_child(node, 0));
            Type *type2 = exp_type(get_child(node, 2));
            // assert(type1 != NULL && type2 != NULL);
            if (type1 == NULL || type2 == NULL)
                return type1 ? type1 : type2; //???
            char *op = get_child(node, 1)->name;
            if (strcmp(op, "PLUS") == 0 || strcmp(op, "MINUS") == 0 ||
                strcmp(op, "STAR") == 0 || strcmp(op, "DIV") == 0) {
                if (type_eq(type1, type2) && type1->kind == BASIC) {
                    // basic type?
                    return type1;
                } else {
                    semantic_error(7, get_child(node, 1)->line, NULL);
                    return NULL;
                }
            } else if (strcmp(op, "ASSIGNOP") == 0) {
                Node *l_exp = get_child(node, 0);
                if (get_child_num(l_exp) == 1 &&
                        strcmp(get_child(l_exp, 0)->name, "ID") == 0 ||
                    get_child_num(l_exp) == 3 &&
                        strcmp(get_child(l_exp, 1)->name, "DOT") == 0 ||
                    get_child_num(l_exp) == 4 &&
                        strcmp(get_child(l_exp, 1)->name, "LB") == 0) {
                    if (type_eq(type1, type2)) {
                        return type1; // should return type1 or type2?
                    } else {
                        semantic_error(5, node->line, NULL);
                        return NULL;
                    }
                } else {
                    semantic_error(6, node->line, NULL);
                    return NULL;
                }
            } else if (strcmp(op, "AND") == 0 || strcmp(op, "OR") == 0) {
                if (type_eq(type1, type2) && type1->u.basic == INT_TYPE) {
                    return type1;
                } else {
                    semantic_error(7, get_child(node, 1)->line, NULL);
                    return NULL;
                }
            } else if (strcmp(op, "RELOP") == 0) {
                if (type_eq(type1, type2)) {
                    return basic_type(INT_TYPE);
                } else {
                    semantic_error(7, get_child(node, 1)->line, NULL);
                    return NULL;
                }
            } else {
                assert(0);
            }
        } else if (strcmp(get_child(node, 0)->name, "LP") == 0) {
            assert(strcmp(get_child(node, 2)->name, "RP") == 0);
            return exp_type(get_child(node, 1));
        } else if (strcmp(get_child(node, 1)->name, "DOT") == 0) {
            // Exp DOT ID
            Type *structure_type = exp_type(get_child(node, 0));
            if (structure_type == NULL || structure_type->kind != STRUCTURE) {
                semantic_error(13, get_child(node, 1)->line, NULL);
                return NULL;
            } else {
                FieldList *field = structure_type->u.structure;
                while (field != NULL) {
                    if (strcmp(field->name, get_child(node, 2)->value) == 0) {
                        return field->type;
                    }
                    field = field->next;
                }
                semantic_error(14, get_child(node, 2)->line,
                               get_child(node, 2)->value);
                return NULL;
            }
        } else if (strcmp(get_child(node, 0)->name, "ID") == 0) {
            return func_exp(node);
        }
        break;
    }
    case 4: {
        if (strcmp(get_child(node, 0)->name, "ID") == 0) {
            // ID LP RP
            return func_exp(node);
        } else if (strcmp(get_child(node, 0)->name, "Exp") == 0) {
            // Exp LB Exp RB
            Type *array_type = exp_type(get_child(node, 0));
            if (array_type == NULL || array_type->kind != ARRAY) {
                semantic_error(10, get_child(node, 0)->line,
                               get_child(node, 0)->name);
                return NULL;
            } else {
                if (type_eq(exp_type(get_child(node, 2)),
                            basic_type(INT_TYPE))) {
                    return array_type->u.array.elem;
                } else {
                    semantic_error(12, get_child(node, 2)->line,
                                   get_child(node, 0)->name);
                    return NULL;
                }
            }
        } else {
            assert(0);
        }
        break;
    }
    default:
        return NULL;
        break;
    }
    return NULL;
}

Type *func_exp(Node *node)
{
    CHECK(node, "Exp");
    CHECK(get_child(node, 1), "LP");
    char *func_name = get_child(node, 0)->value;
    func_hash_node *func_node = get_func_hash_node(func_name);
    if (func_node == NULL) {
        if (get_var_hash_node(func_name) == NULL) {
            semantic_error(2, get_child(node, 0)->line, func_name);
        } else {
            semantic_error(11, get_child(node, 0)->line, func_name);
        }
        return NULL;
    } else {
        if (get_child_num(node) == 3) {
            // ID LP RP
            if (func_node->func->args == NULL) {
                return func_node->func->return_type;
            } else {
                semantic_error(9, get_child(node, 1)->line, func_name);
                return NULL;
            }
        } else {
            // ID LP Args RP
            Node *args = get_child(node, 2);
            if (!field_eq_args(func_node->func->args, args)) {
                semantic_error(9, get_child(node, 1)->line, func_name);
                return NULL;
            } else {
                return func_node->func->return_type;
            }
        }
    }
}

bool field_eq(FieldList *a, FieldList *b)
{
    while (a != NULL && b != NULL) {
        if (!type_eq(a->type, b->type))
            return false;
        a = a->next;
        b = b->next;
    }
    if (a != NULL || b != NULL) {
        return false;
    }
    return true;
}

bool field_eq_args(FieldList *list, Node *args)
{
    CHECK(args, "Args");
    while (args != NULL && list != NULL) {
        if (!type_eq(list->type, exp_type(get_child(args, 0))))
            return 0;
        else {
            list = list->next;
            args = get_child(args, 2);
        }
    }
    if (args != NULL || list != NULL)
        return 0;
    else
        return 1;
}

void semantic_error(int error_type, int lineno, char *name)
{
    char msg[100] = "\0";
    if (error_type == 1)
        sprintf(msg, "Undefined variable \"%s\".", name);
    else if (error_type == 2)
        sprintf(msg, "Undefined function \"%s\".", name);
    else if (error_type == 3)
        sprintf(msg, "Redefined variable \"%s\".", name);
    else if (error_type == 4)
        sprintf(msg, "Redefined function \"%s\".", name);
    else if (error_type == 5)
        sprintf(msg, "Type mismatched for assignment.");
    else if (error_type == 6)
        sprintf(msg, "The left-hand side of an assignment must be a variable.");
    else if (error_type == 7)
        sprintf(msg, "Type mismatched for operands.");
    else if (error_type == 8)
        sprintf(msg, "Type mismatched for return.");
    else if (error_type == 9)
        sprintf(msg,
                "Function \"%s\" is not applicable for arguments followed.",
                name);
    else if (error_type == 10)
        sprintf(msg, "\"%s\" is not an array.", name);
    else if (error_type == 11)
        sprintf(msg, "\"%s\" is not a function.", name);
    else if (error_type == 12)
        sprintf(msg, "\"%s\" is not an integer.", name);
    else if (error_type == 13)
        sprintf(msg, "Illegal use of \".\".");
    else if (error_type == 14)
        sprintf(msg, "Non-existent field \"%s\".", name);
    else if (error_type == 15) { // two different error report format
        if (name == NULL)
            sprintf(msg, "Field cannot be initialized.");
        else
            sprintf(msg, "Redefined field \"%s\".", name);
    } else if (error_type == 16)
        sprintf(msg, "Duplicated name \"%s\".", name);
    else if (error_type == 17)
        sprintf(msg, "Undefined structure \"%s\".", name);
    else if (error_type == 18)
        sprintf(msg, "Undefined function \"%s\".", name);
    else if (error_type == 19)
        sprintf(msg, "Inconsistent declaration of function \"%s\".", name);
    else
        printf("Unknown error_type\n");
    printf("Error type %d at Line %d: %s\n", error_type, lineno, msg);
}

void semantic_analysis(Node *root)
{
    init();
    traversal(root);
    finish();
}

void handle(Node *node)
{
    if (strcmp(node->name, "ExtDef") == 0)
        ExtDef(node);
    else if (strcmp(node->name, "DefList") == 0)
        DefList(node);
    else if (strcmp(node->name, "Exp") == 0)
        exp_type(node);
}

void traversal(Node *node)
{
    if (node == p_empty)
        return;
    if (node->visited)
        return;
    handle(node);
    int n = get_child_num(node);
    for (int i = 0; i < n; i++) {
        if (get_child(node, i) != NULL || get_child(node, i) != p_empty) {
            traversal(get_child(node, i));
        }
    }
}

void finish()
{
    for (int i = 0; i < HASH_SIZE + 1; i++) {
        if (func_hash_table[i] != NULL) {
            func_hash_node *tmp = func_hash_table[i];
            while (tmp != NULL) {
                if (tmp->func->wheather_defined == false)
                    semantic_error(18, tmp->func->line, tmp->func->name);
                func_hash_node *next = tmp->next;
                free(tmp);
                tmp = next;
            }
        }
    }
}