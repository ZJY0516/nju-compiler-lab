#include "ir.h"
#include "semantic.h"
#include "tree.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IR_CHECK(node, correct_name)                                           \
    do {                                                                       \
        if (node == NULL)                                                      \
            printf("Error: %s NULL node at %s:%d\n", correct_name, __FILE__,   \
                   __LINE__);                                                  \
        else {                                                                 \
            node->ir_code_visited = 1;                                         \
            if (strcmp(node->name, correct_name) != 0) {                       \
                printf("It is a '%s' Node, not a '%s' Node at %s:%d\n",        \
                       node->name, correct_name, __FILE__, __LINE__);          \
            }                                                                  \
        }                                                                      \
    } while (0)

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

static void traversal(Node *node);
static void handle(Node *node);
static inline Operand *new_tmp(int kind);
static inline InterCode *new_inter_code(int kind);
static inline CodeList *new_label_codelist(Operand *op);
static inline CodeList *new_CodeList(InterCode *code);
static CodeList *splice(CodeList *code1, CodeList *code2);
static char *inter_code_2_string(InterCode *code);
static char *operand_2_string(Operand *op);
static void write_file(char *filename);
static Operand *lookup(Node *ID);
static CodeList *ExtDef(Node *node);
static CodeList *FunDec(Node *node);
static CodeList *CompSt(Node *node);
static CodeList *DefList(Node *node);
static CodeList *Def(Node *node);
static CodeList *Dec(Node *node);
static CodeList *StmtList(Node *node);
static CodeList *Exp(Node *node, Operand *place);
static CodeList *Stmt(Node *node);
static CodeList *translate_Cond(Node *node, Operand *label_true,
                                Operand *label_false);

uint32_t var_count = 0;
uint32_t label_count = 0;
CodeList *code_list_head = NULL;
CodeList *code_list_tail = NULL;

size_t my_sizeof(Type *type)
{
    if (type->kind == BASIC) {
        return 4;
    } else if (type->kind == ARRAY) {
        return type->u.array.size * my_sizeof(type->u.array.elem);
    } else if (type->kind == STRUCTURE) {
        size_t ans = 0;
        FieldList *tmp = type->u.structure;
        while (tmp != NULL) {
            ans += my_sizeof(tmp->type);
            tmp = tmp->next;
        }
        return ans;
    }
    assert(0);
}

size_t get_array_size(Node *Exp)
{
    assert(strcmp(get_child(Exp, 1)->name, "LB") == 0);
    Type *type = exp_type(Exp->child);
    return my_sizeof(type->u.array.elem);
}

CodeList *translate_Args(Node *Args, ArgList **arg_list)
{
    IR_CHECK(Args, "Args");
    Operand *t1 = new_tmp(VARIABLE);

    if (exp_type(Args->child)->kind != BASIC) {
        t1->kind = ADDRESS;
    }
    CodeList *code1 = Exp(Args->child, t1);
    ArgList *new_arg = malloc(sizeof(ArgList));
    new_arg->args = t1;
    new_arg->next = *arg_list;
    *arg_list = new_arg;
    int child_num = get_child_num(Args);
    if (child_num == 1) {
        return code1;
    } else {
        CodeList *code2 = translate_Args(get_child(Args, 2), arg_list);
        return splice(code1, code2);
    }
}

static int get_struct_offset(Type *s, char *id)
{
    assert(s->kind == STRUCTURE);
    int ans = 0;
    FieldList *fl = s->u.structure;
    while (strcmp(fl->name, id) != 0) {
        ans += my_sizeof(fl->type);
        fl = fl->next;
    }
    return ans;
}

static Type *get_id_type(Node *ID)
{
    IR_CHECK(ID, "ID");
    var_hash_node *tmp = get_var_hash_node(ID->value);
    assert(tmp != NULL);
    return tmp->type;
}

static CodeList *new_goto_code(Operand *op)
{
    assert(op->kind == LABEL);
    InterCode *code = new_inter_code(IC_GOTO);
    code->u.op = op;
    return new_CodeList(code);
}

static CodeList *new_plus_code(Operand *result, Operand *op1, Operand *op2)
{
    InterCode *code = new_inter_code(IC_ADD);
    code->u.binop.result = result;
    code->u.binop.op1 = op1;
    code->u.binop.op2 = op2;
    return new_CodeList(code);
}

static CodeList *new_assign_code(Operand *l, Operand *r)
{
    InterCode *code = new_inter_code(IC_ASSIGN);
    code->u.assign.left = l;
    code->u.assign.right = r;
    return new_CodeList(code);
}

static inline Operand *new_constant(int val)
{
    Operand *op = (Operand *)malloc(sizeof(Operand));
    op->kind = CONSTANT;
    op->u.val = val;
    return op;
}

static inline Operand *new_tmp(int kind)
{
    Operand *tmp = (Operand *)malloc(sizeof(Operand));
    tmp->kind = kind;
    if (kind == LABEL) {
        tmp->u.label_no = label_count++;
    } else {
        tmp->u.var_no = var_count++;
    }
    return tmp;
}

static inline InterCode *new_inter_code(int kind)
{
    InterCode *code = (InterCode *)malloc(sizeof(InterCode));
    code->kind = kind;
    return code;
}

static inline CodeList *new_label_codelist(Operand *op)
{
    InterCode *code = new_inter_code(IC_LABEL);
    code->u.op = op;
    return new_CodeList(code);
}

static inline CodeList *new_CodeList(InterCode *code)
{
    CodeList *node = (CodeList *)malloc(sizeof(CodeList));
    node->code = code;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

// return a list of intercode: code1 + code2
static CodeList *splice(CodeList *code1, CodeList *code2)
{
    if (code1 == NULL)
        return code2;
    else {
        CodeList *tmp = code1;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = code2;
        if (code2 != NULL) {
            code2->prev = tmp;
        }
    }
    return code1;
}

// i don't understand
static Operand *lookup(Node *ID)
{
    IR_CHECK(ID, "ID");
    var_hash_node *var_node = get_var_hash_node(ID->value);
    assert(var_node);
    if (var_node->op == NULL) {
        var_node->op = new_tmp(VARIABLE);
    }
    return var_node->op;
}

static CodeList *translate_Cond(Node *node, Operand *label_true,
                                Operand *label_false)
{
    int child_num = get_child_num(node);
    if (strcmp(get_child(node, 0)->name, "NOT") == 0) {
        return translate_Cond(get_child(node, 1), label_false, label_true);
    }
    switch (child_num) {
    case 2:
        assert(strcmp(get_child(node, 0)->name, "NOT") == 0);
        return translate_Cond(get_child(node, 1), label_false, label_true);
        break;
    case 3:
        if (strcmp(get_child(node, 1)->name, "AND") == 0) {
            Operand *label1 = new_tmp(LABEL);
            CodeList *code1 =
                translate_Cond(get_child(node, 0), label1, label_false);
            CodeList *code2 =
                translate_Cond(get_child(node, 2), label_true, label_false);
            CodeList *label_code = new_label_codelist(label1);
            return splice(code1, splice(label_code, code2));
        } else if (strcmp(get_child(node, 1)->name, "OR") == 0) {
            Operand *label1 = new_tmp(LABEL);
            CodeList *code1 =
                translate_Cond(get_child(node, 0), label_true, label1);
            CodeList *code2 =
                translate_Cond(get_child(node, 2), label_true, label_false);
            CodeList *label_code = new_label_codelist(label1);
            return splice(code1, splice(label_code, code2));
        } else if (strcmp(get_child(node, 1)->name, "RELOP") == 0) {
            Operand *t1 = new_tmp(VARIABLE);
            Operand *t2 = new_tmp(VARIABLE);
            CodeList *code1 = Exp(get_child(node, 0), t1);
            CodeList *code2 = Exp(get_child(node, 2), t2);
            InterCode *code3 = new_inter_code(IC_IF_GOTO);
            code3->u.if_goto.relop = get_child(node, 1)->value;
            code3->u.if_goto.x = t1;
            code3->u.if_goto.y = t2;
            code3->u.if_goto.z = label_true;
            CodeList *code3_list = new_CodeList(code3);
            CodeList *code4_list = new_goto_code(label_false);
            return splice(code1, splice(code2, splice(code3_list, code4_list)));
        } else if (strcmp(get_child(node, 0)->name, "LP") == 0 &&
                   strcmp(get_child(node, 1)->name, "Exp") == 0 &&
                   strcmp(get_child(node, 2)->name, "RP") == 0) {
            return translate_Cond(get_child(node, 1), label_true, label_false);
        } else {
            // printf("name: %s %s %s \n", get_child(node, 0)->name,
            //        get_child(node, 1)->name, get_child(node, 2)->name);
            // assert(0);
        }
    }
    Operand *t = new_tmp(VARIABLE);
    CodeList *code1 = Exp(node, t);
    InterCode *code2 = new_inter_code(IC_IF_GOTO);
    code2->u.if_goto.x = t;
    code2->u.if_goto.y = new_constant(0);
    code2->u.if_goto.z = label_true;
    code2->u.if_goto.relop = "!=";
    CodeList *code2_list = new_CodeList(code2);
    InterCode *code3 = new_inter_code(IC_GOTO);
    code3->u.op = label_false;
    CodeList *code3_list = new_CodeList(code3);
    return splice(code1, splice(code2_list, code3_list));
}

static CodeList *StmtList(Node *node)
{
    IR_CHECK(node, "StmtList");
    CodeList *code = NULL;
    while (node != p_empty && node) {
        Node *stmt = get_child(node, 0);
        if (strcmp(stmt->name, "Stmt") == 0) {
            CodeList *code2 = Stmt(stmt);
            code = splice(code, code2);
        }
        node = get_child(node, 1);
    }
    return code;
}

static CodeList *Stmt(Node *node)
{
    IR_CHECK(node, "Stmt");
    int child_num = get_child_num(node);
    switch (child_num) {
    case 1:
        return CompSt(node->child);
    case 2:
        return Exp(get_child(node, 0), NULL);
    case 3: // RETURN EXP SEMI
        assert(strcmp(get_child(node, 0)->name, "RETURN") == 0);
        Operand *op = new_tmp(VARIABLE);
        InterCode *code = new_inter_code(IC_RETURN);
        code->u.op = op;
        CodeList *exp = Exp(get_child(node, 1), op);
        CodeList *code2 = new_CodeList(code);
        return splice(exp, code2);
    case 5:
        if (strcmp(get_child(node, 0)->name, "IF") == 0) {
            Operand *label1 = new_tmp(LABEL);
            Operand *label2 = new_tmp(LABEL);
            CodeList *code1 =
                translate_Cond(get_child(node, 2), label1, label2);
            CodeList *code2 = Stmt(get_child(node, 4));
            CodeList *label1_code = new_label_codelist(label1);
            CodeList *label2_code = new_label_codelist(label2);
            return splice(code1,
                          splice(label1_code, splice(code2, label2_code)));
        } else if (strcmp(get_child(node, 0)->name, "WHILE") == 0) {
            Operand *label1 = new_tmp(LABEL);
            Operand *label2 = new_tmp(LABEL);
            Operand *label3 = new_tmp(LABEL);
            CodeList *code1 =
                translate_Cond(get_child(node, 2), label2, label3);
            CodeList *code2 = Stmt(get_child(node, 4));
            CodeList *label1_code = new_label_codelist(label1);
            CodeList *label2_code = new_label_codelist(label2);
            CodeList *label3_code = new_label_codelist(label3);
            CodeList *goto_code = new_goto_code(label1);
            CodeList *tmp1 = splice(label1_code, splice(code1, label2_code));
            CodeList *tmp2 = splice(code2, splice(goto_code, label3_code));
            return splice(tmp1, tmp2);
        } else {
            assert(0);
        }
    case 7: {
        Operand *l1 = new_tmp(LABEL);
        Operand *l2 = new_tmp(LABEL);
        Operand *l3 = new_tmp(LABEL);
        CodeList *code1 = translate_Cond(get_child(node, 2), l1, l2);
        CodeList *code2 = Stmt(get_child(node, 4));
        CodeList *code3 = Stmt(get_child(node, 6));
        CodeList *code_label1 = new_label_codelist(l1);
        CodeList *code_goto = new_goto_code(l3);
        CodeList *code_label2 = new_label_codelist(l2);
        CodeList *code_label3 = new_label_codelist(l3);
        CodeList *tmp1 = splice(code1, splice(code_label1, code2));
        CodeList *tmp2 =
            splice(splice(code_goto, code_label2), splice(code3, code_label3));
        return splice(tmp1, tmp2);
    }
    default:
        assert(0);
    }
    return NULL;
}
static CodeList *ExtDef(Node *node)
{
    IR_CHECK(node, "ExtDef");
    int n = get_child_num(node);
    if (strcmp(get_child(node, 1)->name, "FunDec") == 0) {
        assert(n == 3);
        CodeList *code1 = FunDec(get_child(node, 1));
        CodeList *code2 = CompSt(get_child(node, 2));
        return splice(code1, code2);
        assert(0);
    }
    return NULL;
}

static CodeList *FunDec(Node *node)
{
    IR_CHECK(node, "FunDec");
    func_hash_node *func_node = get_func_hash_node(get_child(node, 0)->value);

    assert(func_node);

    FieldList *para = func_node->func->args;
    InterCode *func_code = new_inter_code(IC_FUNC);
    func_code->u.func = get_child(node, 0)->value;

    CodeList *code_list = new_CodeList(func_code);
    while (para) {
        Operand *op = NULL;
        if (para->type->kind == BASIC) {
            op = new_tmp(VARIABLE);
        } else if (para->type->kind == STRUCTURE) {
            op = new_tmp(ADDRESS);
        } else {
            printf(
                "Cannot translate: Code contains variables of "
                "multi-dimensional array type or parameters of array type\n");
            exit(0);
        }
        var_hash_node *var_node = get_var_hash_node(para->name);
        var_node->op = op;
        InterCode *inter_code = new_inter_code(IC_PARAM);
        inter_code->u.op = op;
        code_list = splice(code_list, new_CodeList(inter_code));
        para = para->next;
    }
    return code_list;
}

static CodeList *CompSt(Node *node)
{
    /*
    CompSt → LC DefList StmtList RC
    StmtList → Stmt StmtList
    | p_empty
    */
    // maybe Deflist and StmtList are empty
    IR_CHECK(node, "CompSt");
    CodeList *DefList_code_list = NULL;
    CodeList *StmtList_code_list = NULL;
    if (strcmp(get_child(node, 1)->name, "DefList") == 0) {
        DefList_code_list = DefList(get_child(node, 1));
        StmtList_code_list = StmtList(get_child(node, 2));
    } else {
        assert(strcmp(get_child(node, 1)->name, "StmtList") == 0);
        StmtList_code_list = StmtList(get_child(node, 1));
    }
    return splice(DefList_code_list, StmtList_code_list);
}

static CodeList *DefList(Node *node)
{
    IR_CHECK(node, "DefList");
    CodeList *result = NULL;
    // while (node != p_empty && node) {
    //     Node *def = get_child(node, 0);
    //     result = splice(result, Def(def));
    //     node = get_child(node, 1);
    // }
    // return result;
    while (node != p_empty && node) {
        Node *def = get_child(node, 0);
        result = splice(result, Def(def));
        node = get_child(node, 1);
    }
    return result;
}

static CodeList *Def(Node *node)
{
    /*
    Def → Specifier DecList SEMI
    DecList → Dec
    | Dec COMMA DecList
    Dec → VarDec
    | VarDec ASSIGNOP Exp
    */
    IR_CHECK(node, "Def");
    CodeList *result = NULL;

    Node *declist = get_child(node, 1);
    while (declist != p_empty && declist) {
        Node *dec = get_child(declist, 0);
        result = splice(result, Dec(dec));
        if (get_child_num(declist) > 1 &&
            strcmp(get_child(declist, 2)->name, "DecList") == 0) {
            declist = get_child(declist, 2);
        } else {
            break;
        }
    }
    return result;
}

static CodeList *Dec(Node *node)
{
    IR_CHECK(node, "Dec");
    Node *VarDec = get_child(node, 0);
    if (get_child(VarDec, 1)) {
        if (strcmp(get_child(VarDec, 1)->name, "LB") == 0 &&
            get_child_num(get_child(VarDec, 0)) > 1) {
            printf("Cannot translate: Code contains variables of "
                   "multi-dimensional array type or parameters of array "
                   "type.\n");
            exit(0);
        }
    }
    if (get_child_num(node) > 1) {
        if (strcmp(get_child(VarDec, 0)->name, "ID") == 0) {
            Operand *op = lookup(get_child(VarDec, 0));
            return Exp(get_child(node, 2), op);
        }
    } else {
        Node *ID = get_id_node(VarDec);
        assert(strcmp(ID->name, "ID") == 0);
        var_hash_node *tmp = get_var_hash_node(ID->value);
        if (tmp->type->kind == ARRAY || tmp->type->kind == STRUCTURE) {
            Operand *op = new_tmp(ARR_STRU);
            int size = my_sizeof(tmp->type);
            InterCode *code = new_inter_code(IC_DEC);
            code->u.dec.size = size;
            code->u.dec.x = op;
            tmp->op = op;
            return new_CodeList(code);
        }
    }
    return NULL;
}

static CodeList *Exp(Node *node, Operand *place)
{
    // assert(place);
    IR_CHECK(node, "Exp");
    int child_num = get_child_num(node);

    if (child_num >= 2 && (strcmp(get_child(node, 0)->name, "NOT") == 0 ||
                           strcmp(get_child(node, 1)->name, "RELOP") == 0 ||
                           strcmp(get_child(node, 1)->name, "AND") == 0 ||
                           strcmp(get_child(node, 1)->name, "OR") == 0)) {
        Operand *label1 = new_tmp(LABEL);
        Operand *label2 = new_tmp(LABEL);
        CodeList *code0 = NULL, *code1 = NULL, *code2 = NULL, *code3 = NULL;
        InterCode *inter_code1 = NULL, *inter_code2 = NULL;
        if (place) {
            inter_code1 = new_inter_code(IC_ASSIGN);
            inter_code1->u.assign.left = place;
            inter_code1->u.assign.right = new_constant(0);
            code0 = new_CodeList(inter_code1);
        }
        code1 = translate_Cond(node, label1, label2);
        code2 = new_label_codelist(label1);
        if (place) {
            inter_code2 = new_inter_code(IC_ASSIGN);
            inter_code2->u.assign.left = place;
            inter_code2->u.assign.right = new_constant(1);
            code2 = splice(code2, new_CodeList(inter_code2));
        }
        code3 = new_label_codelist(label2);
        return splice(code0, splice(code1, splice(code2, code3)));
    }

    switch (child_num) {
    case 1:
        if (place) {
            if (strcmp(get_child(node, 0)->name, "ID") == 0) {
                Operand *op = lookup(get_child(node, 0));
                InterCode *code = new_inter_code(IC_ASSIGN);
                code->u.assign.left = place;
                code->u.assign.right = op;
                return new_CodeList(code);
            } else if (strcmp(get_child(node, 0)->name, "INT") == 0) {
                InterCode *code = new_inter_code(IC_ASSIGN);
                code->u.assign.left = place;
                code->u.assign.right =
                    new_constant(atoi(get_child(node, 0)->value));
                return new_CodeList(code);
            }
        } else {
            return NULL;
        }
        break;
    case 2:
        if (strcmp(get_child(node, 0)->name, "MINUS") == 0) {
            Operand *t = new_tmp(VARIABLE);
            CodeList *code1 = Exp(get_child(node, 1), t);
            CodeList *code2 = NULL;
            if (place) {
                InterCode *code = new_inter_code(IC_MINUS);
                code->u.binop.result = place;
                code->u.binop.op1 = new_constant(0);
                code->u.binop.op2 = t;
                code2 = new_CodeList(code);
            }
            return splice(code1, code2);
        } else if (strcmp(get_child(node, 0)->name, "NOT") == 0) {
            assert(0);
        } else {
            assert(0);
        }
        break;
    case 3:
        if (strcmp(get_child(node, 1)->name, "PLUS") == 0 ||
            strcmp(get_child(node, 1)->name, "MINUS") == 0 ||
            strcmp(get_child(node, 1)->name, "STAR") == 0 ||
            strcmp(get_child(node, 1)->name, "DIV") == 0) {
            Operand *t1 = new_tmp(VARIABLE);
            Operand *t2 = new_tmp(VARIABLE);
            if (place) {
                InterCode *code = malloc(sizeof(InterCode));
                code->u.binop.result = place;
                code->u.binop.op1 = t1;
                code->u.binop.op2 = t2;
                if (strcmp(get_child(node, 1)->name, "PLUS") == 0) {
                    code->kind = IC_ADD;
                } else if (strcmp(get_child(node, 1)->name, "MINUS") == 0) {
                    code->kind = IC_MINUS;
                } else if (strcmp(get_child(node, 1)->name, "STAR") == 0) {
                    code->kind = IC_MUL;
                } else if (strcmp(get_child(node, 1)->name, "DIV") == 0) {
                    code->kind = IC_DIV;
                }
                return splice(
                    Exp(get_child(node, 0), t1),
                    splice(Exp(get_child(node, 2), t2), new_CodeList(code)));
            }
        } else if (strcmp(get_child(node, 1)->name, "ASSIGNOP") == 0) {
            if (get_child_num(get_child(node, 0)) == 1) {
                Operand *op = lookup(get_child(get_child(node, 0), 0));
                Operand *t1 = new_tmp(VARIABLE);
                CodeList *code1 = Exp(get_child(node, 2), t1);
                CodeList *code2 = NULL;
                InterCode *code = NULL;
                if (op->kind == ARR_STRU) {
                    Operand *addr1 = new_tmp(ADDRESS);
                    Operand *addr2 = new_tmp(ADDRESS);
                    CodeList *codex = new_assign_code(addr1, op);
                    CodeList *codey = new_assign_code(addr2, t1);
                    code2 = splice(code2, splice(codex, codey));
                    Node *id = node->child->child;
                    Type *type = get_id_type(id);
                    int n = my_sizeof(type);
                    for (int i = 0; i < n; i += 4) {
                        Operand *addrA = new_tmp(ADDRESS);
                        Operand *addrB = new_tmp(ADDRESS);
                        CodeList *c1 =
                            new_plus_code(addrA, addr1, new_constant(i));
                        CodeList *c2 =
                            new_plus_code(addrB, addr2, new_constant(i));
                        code2 = splice(code2, splice(c1, c2));
                        Operand *tmp = new_tmp(VARIABLE);
                        CodeList *c3 = new_assign_code(tmp, addrB);
                        CodeList *c4 = new_assign_code(addrA, tmp);
                        c4->code->kind = CHANGE_ADDR;
                        code2 = splice(code2, splice(c3, c4));
                    }
                } else {
                    code = new_inter_code(IC_ASSIGN);
                    code->u.assign.left = op;
                    code->u.assign.right = t1;
                    code2 = new_CodeList(code);
                }
                if (place) {
                    code = new_inter_code(IC_ASSIGN);
                    code->u.assign.left = place;
                    code->u.assign.right = op;
                    code2 = splice(code2, new_CodeList(code));
                }
                return splice(code1, code2);
            } else {
                Operand *t1 = new_tmp(ADDRESS);
                Operand *t2 = new_tmp(VARIABLE);
                CodeList *code1 = Exp(get_child(node, 0), t1);
                CodeList *code2 = Exp(get_child(node, 2), t2);
                InterCode *code = new_inter_code(CHANGE_ADDR);
                code->u.assign.left = t1;
                code->u.assign.right = t2;
                CodeList *code3 = new_CodeList(code);
                if (place) {
                    code = new_inter_code(IC_ASSIGN);
                    code->u.assign.left = place;
                    code->u.assign.right = t2;
                    code3 = splice(code3, new_CodeList(code));
                }
                return splice(code1, splice(code2, code3));
            }
        } else if (strcmp(get_child(node, 0)->name, "LP") == 0) {
            return Exp(get_child(node, 1), place);
        } else if (strcmp(get_child(node, 1)->name, "LP") == 0) {
            char *name = get_child(node, 0)->value;
            if (strcmp(name, "read") == 0) {
                if (place == NULL) {
                    return NULL;
                }
                InterCode *code = new_inter_code(IC_READ);
                code->u.op = place;
                return new_CodeList(code);
            } else {
                InterCode *code = new_inter_code(IC_CALL);
                if (place) {
                    code->u.call.result = place;
                } else {
                    code->u.call.result = new_tmp(VARIABLE);
                }
                code->u.call.func = get_child(node, 0)->value;
                return new_CodeList(code);
            }
        } else if (strcmp(get_child(node, 1)->name, "DOT") == 0) {
            if (place == NULL) {
                return NULL;
            }
            Operand *t1 = new_tmp(ADDRESS);
            CodeList *code1 = Exp(get_child(node, 0), t1);
            int offset = get_struct_offset(exp_type(get_child(node, 0)),
                                           get_child(node, 2)->value); //???
            InterCode *code = new_inter_code(IC_ADD);
            code->u.binop.result = place;
            code->u.binop.op1 = t1;
            code->u.binop.op2 = new_constant(offset);
            CodeList *code2 = new_CodeList(code);
            if (place->kind == VARIABLE) {
                Operand *t2 = new_tmp(ADDRESS);
                code->u.binop.result = t2;
                InterCode *code3 = new_inter_code(IC_ASSIGN);
                code3->u.assign.left = place;
                code3->u.assign.right = t2;
                code2 = splice(code2, new_CodeList(code3));
            }
            return splice(code1, code2); //???
        }
        break;
    case 4:
        if (strcmp(get_child(node, 1)->name, "LP") == 0) {
            ArgList *arg = NULL;
            char *func_name = get_child(node, 0)->value;
            CodeList *code1 = translate_Args(get_child(node, 2), &arg);
            if (strcmp(func_name, "write") == 0) {
                InterCode *code = new_inter_code(IC_WRITE);
                code->u.op = arg->args;
                CodeList *code2 = new_CodeList(code);
                CodeList *code3 = NULL;
                if (place) {
                    code = new_inter_code(IC_ASSIGN);
                    code->u.assign.left = place;
                    code->u.assign.right = new_constant(0);
                    code3 = new_CodeList(code);
                }
                return splice(code1, splice(code2, code3));
            } else {
                CodeList *code2 = NULL;
                while (arg) {
                    InterCode *code = new_inter_code(IC_ARG);
                    code->u.op = arg->args;
                    code2 = splice(code2, new_CodeList(code));
                    arg = arg->next;
                }
                InterCode *code = new_inter_code(IC_CALL);
                if (place) {
                    code->u.call.result = place;
                } else {
                    code->u.call.result = new_tmp(VARIABLE);
                }
                code->u.call.func = func_name;
                return splice(code1, splice(code2, new_CodeList(code)));
            }
        } else if (strcmp(get_child(node, 1)->name, "LB") == 0) {
            if (get_child_num(get_child(node, 0)) > 1 &&
                strcmp(get_child(get_child(node, 0), 1)->name, "LB") == 0) {
                printf("Cannot translate: Code contains variables of "
                       "multi-dimensional array type or parameters of array "
                       "type.\n");
                printf("line: %d\n", __LINE__);
                exit(0);
            }
            Operand *t1 = new_tmp(ADDRESS);
            Operand *t2 = new_tmp(VARIABLE);
            Operand *t3 = new_tmp(VARIABLE);
            CodeList *code1 = Exp(get_child(node, 0), t1);
            CodeList *code2 = Exp(get_child(node, 2), t2);
            InterCode *code = new_inter_code(IC_MUL);
            code->u.binop.result = t3;
            code->u.binop.op1 = t2;
            code->u.binop.op2 = new_constant(get_array_size(node));
            CodeList *code3 = new_CodeList(code);
            CodeList *code4 = NULL;
            if (place) {
                code = new_inter_code(IC_ADD);
                code->u.binop.result = place;
                code->u.binop.op1 = t1;
                code->u.binop.op2 = t3;
                code4 = new_CodeList(code);
                if (place->kind == VARIABLE) {
                    Operand *t4 = new_tmp(ADDRESS);
                    code->u.binop.result = t4;
                    InterCode *code5 = new_inter_code(IC_ASSIGN);
                    code5->u.assign.left = place;
                    code5->u.assign.right = t4;
                    code4 = splice(code4, new_CodeList(code5));
                }
            }
            return splice(splice(code1, code2), splice(code3, code4));
        }
    }
    return NULL;
}

static void traversal(Node *node)
{
    if (node == NULL)
        return;
    if (node->ir_code_visited) {
        return;
    }
    handle(node);
    int n = get_child_num(node);
    for (int i = 0; i < n; i++) {
        if (get_child(node, i) != p_empty) {
            traversal(get_child(node, i));
        }
    }
}

void insert_code(CodeList *code)
{
    // translate_CodeList(code);
    if (code == NULL) {
        return;
    }
    if (code_list_head == NULL) {
        code_list_head = code;
        CodeList *tmp = code;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        code_list_tail = tmp;
    } else {
        code->prev = code_list_tail;
        code_list_tail->next = code;
        CodeList *tmp = code;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        code_list_tail = tmp;
    }
}

static void handle(Node *node)
{
    if (strcmp(node->name, "ExtDef") == 0 &&
        strcmp(node->child->child->name, "StructSpecifier") != 0) {
        insert_code(ExtDef(node));
    }
}

static char *inter_code_2_string(InterCode *code)
{
    char *str = (char *)malloc(INTER_CODE_LENTH);
    memset(str, 0, INTER_CODE_LENTH);
    switch (code->kind) {
    case IC_LABEL:
        sprintf(str, "LABEL %s :", operand_2_string(code->u.op));
        break;
    case IC_FUNC:
        sprintf(str, "FUNCTION %s :", code->u.func);
        break;
    case IC_ADD:
        assert(code->u.binop.result);
        sprintf(str, "%s := %s + %s", operand_2_string(code->u.binop.result),
                operand_2_string(code->u.binop.op1),
                operand_2_string(code->u.binop.op2));
        break;
    case IC_MINUS:
        assert(code->u.binop.result);
        sprintf(str, "%s := %s - %s", operand_2_string(code->u.binop.result),
                operand_2_string(code->u.binop.op1),
                operand_2_string(code->u.binop.op2));
        break;
    case IC_MUL:
        assert(code->u.binop.result);
        sprintf(str, "%s := %s * %s", operand_2_string(code->u.binop.result),
                operand_2_string(code->u.binop.op1),
                operand_2_string(code->u.binop.op2));
        break;
    case IC_DIV:
        assert(code->u.binop.result);
        sprintf(str, "%s := %s / %s", operand_2_string(code->u.binop.result),
                operand_2_string(code->u.binop.op1),
                operand_2_string(code->u.binop.op2));
        break;
    case IC_GOTO:
        sprintf(str, "GOTO %s", operand_2_string(code->u.op));
        break;
    case IC_IF_GOTO:
        sprintf(str, "IF %s %s %s GOTO %s", operand_2_string(code->u.if_goto.x),
                code->u.if_goto.relop, operand_2_string(code->u.if_goto.y),
                operand_2_string(code->u.if_goto.z));
        break;
    case IC_RETURN:
        sprintf(str, "RETURN %s", operand_2_string(code->u.op));
        break;
    case IC_DEC:
        sprintf(str, "DEC %s %d", operand_2_string(code->u.dec.x),
                code->u.dec.size);
        break;
    case IC_ARG:
        sprintf(str, "ARG %s", operand_2_string(code->u.op));
        break;
    case IC_CALL:
        sprintf(str, "%s := CALL %s", operand_2_string(code->u.call.result),
                code->u.call.func);
        break;
    case IC_PARAM:
        sprintf(str, "PARAM %s", operand_2_string(code->u.op));
        break;
    case IC_READ:
        sprintf(str, "READ %s", operand_2_string(code->u.op));
        break;
    case IC_WRITE:
        sprintf(str, "WRITE %s", operand_2_string(code->u.op));
        break;
    case IC_ASSIGN: {
        Operand *left = code->u.assign.left;
        Operand *right = code->u.assign.right;
        if (left) {
            char *l = operand_2_string(left);
            char *r = operand_2_string(right);
            if (left->kind == ADDRESS && right->kind == ADDRESS) {
                sprintf(str, "%s := %s", l, r);
                // assert(0);
            } else if (left->kind == ADDRESS) {
                sprintf(str, "%s := &%s", l, r);
            } else if (right->kind == ADDRESS) {
                sprintf(str, "%s := *%s", l, r);
            } else {
                sprintf(str, "%s := %s", l, r);
            }
        }
    } break;
    case CHANGE_ADDR:
        sprintf(str, "*%s := %s", operand_2_string(code->u.assign.left),
                operand_2_string(code->u.assign.right));
        break;
    default:
        assert(0);
    }
    return str;
}

static char *operand_2_string(Operand *op)
{
#define LENGTH 64
    char str[LENGTH] = {0};
    switch (op->kind) {
    case CONSTANT:
        snprintf(str, LENGTH, "#%d", op->u.val);
        break;
    case LABEL:
        snprintf(str, LENGTH, "l%" PRIu32, op->u.label_no);
        break;
    default:
        snprintf(str, LENGTH, "t%" PRIu32, op->u.var_no);
        break;
    }
    char *ret = malloc(sizeof(char) * strlen(str) + 1);
    strcpy(ret, str);
    return ret;
}

static void write_file(char *filename)
{
    FILE *fp = NULL;
    if (filename)
        fp = fopen(filename, "w");
    else {
        fp = fopen("output.ir", "w");
    }
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    CodeList *tmp = code_list_head;
    while (tmp) {
        fprintf(fp, "%s\n", inter_code_2_string(tmp->code));
        tmp = tmp->next;
    }
    fclose(fp);
}

extern Node *root;

void gen_intercode(char *filename)
{
    traversal(root);
    write_file(filename);
}