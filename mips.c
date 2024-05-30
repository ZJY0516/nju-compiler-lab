#include "mips.h"
#include "ir.h"
#include "semantic.h"
#include <assert.h>
#include <bits/types/FILE.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *REG_NAME[REG_NUM] = {
    "$0",  "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3", "$t0", "$t1", "$t2",
    "$t3", "$t4", "$t5", "$t6", "$t7", "$s0", "$s1", "$s2", "$s3", "$s4", "$s5",
    "$s6", "$s7", "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"};

pRegisters registers;
extern CodeList *code_list_head;
extern uint32_t var_count;
static FILE *global_fp;
int in_func = 0;
char *curFuncName;

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
int get_func_argc(func_hash_node *func)
{
    int ret = 0;
    FieldList *args = func->func->args;
    while (args) {
        ret++;
        args = args->next;
    }
    return ret;
}

void load(FILE *fp, pRegisters registers, pOperand op, int regNo)
{
    if (op->kind == CONSTANT)
        return;
    fprintf(fp, "  la $gp, global_vars\n");
    fprintf(fp, "  lw %s, %d($gp)            #load %s\n",
            registers->regLsit[regNo]->name, op->u.var_no * 4,
            operand_2_string(op));
}
void store(FILE *fp, pRegisters registers, pOperand op, int regNo)
{
    if (op->kind == CONSTANT)
        return;
    fprintf(fp, "  la $gp, global_vars\n");
    fprintf(fp, "  sw %s, %d($gp)            #store %s\n",
            registers->regLsit[regNo]->name, op->u.var_no * 4,
            operand_2_string(op));
}

pRegisters init_registers()
{
    pRegisters p = (pRegisters)malloc(sizeof(Registers));
    assert(p);
    for (int i = 0; i < REG_NUM; i++) {
        p->regLsit[i] = new_register(REG_NAME[i]);
    }
    p->regLsit[0]->isFree = false; // $0不允许使用
    p->lastChangedNo = 0;
    return p;
}

void reset_registers()
{
    for (int i = 1; i < REG_NUM; i++) {
        registers->regLsit[i]->isFree = true;
    }
}

pRegister new_register(const char *regName)
{
    pRegister p = (pRegister)malloc(sizeof(Register));
    assert(p);
    p->isFree = true;
    p->name = (char *)malloc(strlen(regName) + 1);
    strcpy(p->name, regName);
    return p;
}

void gen_assembly_code(char *filename)
{
    FILE *fp = fopen(filename, "w");
    global_fp = fp;
    registers = init_registers();
    init_assembly_code(fp);
    printf("init success\n");
    CodeList *temp = code_list_head;
    while (temp) {
        inter_to_assem(fp, temp);
        temp = temp->next;
    }
    printf("gen end\n");
}

void init_assembly_code(FILE *fp)
{
    fprintf(fp, ".data\n");
    fprintf(fp, "global_vars: .space %d\n", var_count * 4);
    // 无重复变量名，直接把数组当全局变量声明了
    CodeList *temp = code_list_head;
    while (temp) {
        if (temp->code->kind == IC_DEC)
            fprintf(fp, "%s: .space %d\n", /*temp->code->u.dec.x->u*/
                    operand_2_string(temp->code->u.dec.x),
                    temp->code->u.dec.size);
        temp = temp->next;
    }

    fprintf(fp, "_prompt: .asciiz \"Enter an integer:\"\n");
    fprintf(fp, "_ret: .asciiz \"\\n\"\n");
    fprintf(fp, ".globl main\n");

    // read function
    fprintf(fp, ".text\n");
    fprintf(fp, "read:\n");
    fprintf(fp, "  li $v0, 4\n");
    fprintf(fp, "  la $a0, _prompt\n");
    fprintf(fp, "  syscall\n");
    fprintf(fp, "  li $v0, 5\n");
    fprintf(fp, "  syscall\n");
    fprintf(fp, "  jr $ra\n\n");

    // write function
    fprintf(fp, "write:\n");
    fprintf(fp, "  li $v0, 1\n");
    fprintf(fp, "  syscall\n");
    fprintf(fp, "  li $v0, 4\n");
    fprintf(fp, "  la $a0, _ret\n");
    fprintf(fp, "  syscall\n");
    fprintf(fp, "  move $v0, $0\n");
    fprintf(fp, "  jr $ra\n");
}

int get_reg(pOperand op)
{
    for (int i = T0; i <= T2; i++) {
        if (registers->regLsit[i]->isFree) {
            registers->regLsit[i]->isFree = false;
            registers->regLsit[i]->op = op;
            if (op->kind == CONSTANT) {
                fprintf(global_fp, "  li %s, %d\n", registers->regLsit[i]->name,
                        op->u.val);
            } else {
                load(global_fp, registers, op, i);
            }
            return i;
        }
    }
    assert(0);
}

// free t0 t1 t2
void setfree()
{
    for (int i = T0; i <= T2; i++) {
        registers->regLsit[i]->isFree = true;
        registers->regLsit[i]->op = NULL;
    }
}

void inter_to_assem(FILE *fp, CodeList *codelist)
{
    InterCode *interCode = codelist->code;
    int kind = interCode->kind;
    char *name = operand_2_string(interCode->u.op);
    if (kind == IC_LABEL) {
        fprintf(fp, "%s:\n", name);
    } else if (kind == IC_FUNC) {

        reset_registers();

        // main函数单独处理一下，在main里调用函数不算函数嵌套调用
        if (!strcmp(interCode->u.func, "main")) {
            // in_func++; //?????
            curFuncName = NULL;
            fprintf(fp, "\n%s:\n", interCode->u.func);
        } else {
            fprintf(fp, "\nfunc_%s:\n", interCode->u.func);
            // func name may be same as instruction name
            in_func = 1;
            curFuncName = interCode->u.func;

            // 处理形参 IR_PARAM
            func_hash_node *item = get_func_hash_node(interCode->u.func);
            int func_argc = get_func_argc(item);
            int argc = 0;
            CodeList *temp = codelist->next;
            while (temp && temp->code->kind == IC_PARAM) {
                // 前4个参数存到a0 到a3中
                if (argc < 4) {
                    store(fp, registers, temp->code->u.op, A0 + argc);
                } else {
                    // 剩下的要用栈存
                    int regNo = get_reg(temp->code->u.op);
                    fprintf(fp, "  lw %s, %d($fp)\n",
                            registers->regLsit[regNo]->name,
                            (func_argc - 1 - argc) * 4);
                    store(fp, registers, temp->code->u.op, regNo);
                    setfree();
                }
                argc++;
                temp = temp->next;
            }
        }
        setfree();
    } else if (kind == IC_GOTO) {
        fprintf(fp, "  j %s\n", name);
    } else if (kind == IC_RETURN) {
        int RegNo = get_reg(interCode->u.op);
        fprintf(fp, "  move $v0, %s\n", registers->regLsit[RegNo]->name);
        fprintf(fp, "  jr $ra\n");
        setfree();
        in_func = 0;
    } else if (kind == IC_ARG) {
        // 需要在call里处理
    } else if (kind == IC_PARAM) {
        // 需要在function里处理
    } else if (kind == IC_READ) {
        fprintf(fp, "  addi $sp, $sp, -4\n");
        fprintf(fp, "  sw $ra, 0($sp)\n");
        fprintf(fp, "  jal read\n");
        fprintf(fp, "  lw $ra, 0($sp)\n");
        fprintf(fp, "  addi $sp, $sp, 4\n");
        int RegNo = get_reg(interCode->u.op);
        fprintf(fp, "  move %s, $v0\n", registers->regLsit[RegNo]->name);
        store(fp, registers, interCode->u.op, RegNo);
        setfree();
    } else if (kind == IC_WRITE) {
        int RegNo = get_reg(interCode->u.op);
        if (in_func == 0) {
            fprintf(fp, "  move $a0, %s\n", registers->regLsit[RegNo]->name);
            fprintf(fp, "  addi $sp, $sp, -4\n");
            fprintf(fp, "  sw $ra, 0($sp)\n");
            fprintf(fp, "  jal write\n");
            fprintf(fp, "  lw $ra, 0($sp)\n");
            fprintf(fp, "  addi $sp, $sp, 4\n");
        } else {
            // 函数嵌套调用，先将a0压栈 调用结束以后需要恢复a0
            fprintf(fp, "  addi $sp, $sp, -8\n");
            fprintf(fp, "  sw $a0, 0($sp)\n");
            fprintf(fp, "  sw $ra, 4($sp)\n");
            fprintf(fp, "  move $a0, %s\n", registers->regLsit[RegNo]->name);
            fprintf(fp, "  jal write\n");
            fprintf(fp, "  lw $a0, 0($sp)\n");
            fprintf(fp, "  lw $ra, 4($sp)\n");
            fprintf(fp, "  addi $sp, $sp, 8\n");
        }
        setfree();
    } else if (kind == CHANGE_ADDR) {
        if (interCode->u.assign.right->kind == CONSTANT) {
            int leftRegNo = get_reg(interCode->u.assign.left);
            int rightRegNo = T4;
            //*t1 := t2
            fprintf(fp, "  li %s, %d\n", registers->regLsit[rightRegNo]->name,
                    interCode->u.assign.right->u.val);
            fprintf(fp, "  sw %s, 0(%s)\n",
                    registers->regLsit[rightRegNo]->name,
                    registers->regLsit[leftRegNo]->name);
            // store(fp, registers, interCode->u.assign.left, leftRegNo);
            setfree();
        } else {
            Operand *left = interCode->u.assign.left;
            Operand *right = interCode->u.assign.right;

            int leftRegNo = get_reg(left);
            int rightRegNo = get_reg(right);
            fprintf(fp, "  sw %s, 0(%s)\n",
                    registers->regLsit[rightRegNo]->name,
                    registers->regLsit[leftRegNo]->name);
            setfree();
        }
    } else if (kind == IC_ASSIGN) {
        // 右值为立即数，直接放左值寄存器里
        if (interCode->u.assign.right->kind == CONSTANT) {
            int leftRegNo = get_reg(interCode->u.assign.left);
            fprintf(fp, "  li %s, %d\n", registers->regLsit[leftRegNo]->name,
                    interCode->u.assign.right->u.val);
            store(fp, registers, interCode->u.assign.left, leftRegNo);
            setfree();
        } else {
            Operand *left = interCode->u.assign.left;
            Operand *right = interCode->u.assign.right;

            int leftRegNo = get_reg(left);
            int rightRegNo = get_reg(right);
            if (left->kind != ADDRESS && right->kind != ADDRESS) {
                fprintf(fp, "  move %s, %s\n",
                        registers->regLsit[leftRegNo]->name,
                        registers->regLsit[rightRegNo]->name);
            }
            if (left->kind == ADDRESS && right->kind == ADDRESS) {
                fprintf(fp, "  lw %s, 0(%s)\n",
                        registers->regLsit[leftRegNo]->name,
                        registers->regLsit[rightRegNo]->name);
            }
            if (left->kind == ADDRESS) {
                // t13 := &t0
                fprintf(fp, "  la %s, %s\n",
                        registers->regLsit[leftRegNo]->name,
                        operand_2_string(right));
            }
            if (right->kind == ADDRESS && left->kind == VARIABLE) {
                // t12 := *t16
                fprintf(fp, "  lw %s, 0(%s)\n",
                        registers->regLsit[leftRegNo]->name,
                        registers->regLsit[rightRegNo]->name);
            }

            store(fp, registers, left, leftRegNo);
        }
        setfree();
    } else if (kind == IC_CALL) {
        char *called_func_name = interCode->u.call.func;
        func_hash_node *calledFunc = get_func_hash_node(called_func_name);
        int calledFuncArgc = get_func_argc(calledFunc);
        int leftRegNo = get_reg(interCode->u.call.result);
        // 函数调用前的准备
        fprintf(fp, "  addi $sp, $sp, -4\n");
        fprintf(fp, "  sw $ra, 0($sp)\n");
        push(fp);

        // 处理实参 IR_ARG
        CodeList *arg = codelist->prev;
        int argc = 0;
        while (arg && argc < calledFuncArgc) {
            if (arg->code->kind == IC_ARG) {
                int argRegNo = get_reg(arg->code->u.op);
                // 前4个参数直接用寄存器存
                if (argc < 4) {
                    fprintf(fp, "  move %s, %s\n",
                            registers->regLsit[A0 + argc]->name,
                            registers->regLsit[argRegNo]->name);
                    argc++;
                }
                // 4个以后的参数压栈
                else {
                    fprintf(fp, "  addi $sp, $sp, -4\n");
                    fprintf(fp, "  sw %s, 0($sp)\n",
                            registers->regLsit[argRegNo]->name);
                    fprintf(fp, "  move $fp, $sp\n");
                    argc++;
                }
            }
            arg = arg->prev;
            setfree();
        }

        // 函数调用
        if (!strcmp(interCode->u.call.func, "main")) {
            fprintf(fp, "  jal %s\n", interCode->u.call.func);
        } else {
            fprintf(fp, "  jal func_%s\n", interCode->u.call.func);
        }

        pop(fp);
        fprintf(fp, "  lw $ra, 0($sp)\n");
        fprintf(fp, "  addi $sp, $sp, 4\n");
        fprintf(fp, "  move %s, $v0\n", registers->regLsit[leftRegNo]->name);
        store(fp, registers, interCode->u.call.result, leftRegNo);
        setfree();
    } else if (kind == IC_ADD) {
        int resultRegNo = get_reg(interCode->u.binop.result);
        // 常数 常数
        if (interCode->u.binop.op1->kind == CONSTANT &&
            interCode->u.binop.op2->kind == CONSTANT) {
            fprintf(fp, "  li %s, %d\n", registers->regLsit[resultRegNo]->name,
                    interCode->u.binop.op1->u.val +
                        interCode->u.binop.op2->u.val);
        }
        // 变量 常数
        else if (interCode->u.binop.op1->kind != CONSTANT &&
                 interCode->u.binop.op2->kind == CONSTANT) {
            int op1RegNo = get_reg(interCode->u.binop.op1);
            fprintf(fp, "  addi %s, %s, %d\n",
                    registers->regLsit[resultRegNo]->name,
                    registers->regLsit[op1RegNo]->name,
                    interCode->u.binop.op2->u.val);
        }
        // 变量 变量
        else {
            int op1RegNo = get_reg(interCode->u.binop.op1);
            int op2RegNo = get_reg(interCode->u.binop.op2);
            fprintf(fp, "  add %s, %s, %s\n",
                    registers->regLsit[resultRegNo]->name,
                    registers->regLsit[op1RegNo]->name,
                    registers->regLsit[op2RegNo]->name);
        }
        store(fp, registers, interCode->u.binop.result, resultRegNo);
        setfree();
    } else if (kind == IC_MINUS) {
        int resultRegNo = get_reg(interCode->u.binop.result);
        // 常数 常数
        if (interCode->u.binop.op1->kind == CONSTANT &&
            interCode->u.binop.op2->kind == CONSTANT) {
            fprintf(fp, "  li %s, %d\n", registers->regLsit[resultRegNo]->name,
                    interCode->u.binop.op1->u.val -
                        interCode->u.binop.op2->u.val);
        }
        // 变量 常数
        else if (interCode->u.binop.op1->kind != CONSTANT &&
                 interCode->u.binop.op2->kind == CONSTANT) {
            int op1RegNo = get_reg(interCode->u.binop.op1);
            fprintf(fp, "  addi %s, %s, %d\n",
                    registers->regLsit[resultRegNo]->name,
                    registers->regLsit[op1RegNo]->name,
                    -interCode->u.binop.op2->u.val);
        }
        // 变量 变量
        else {
            int op1RegNo = get_reg(interCode->u.binop.op1);
            int op2RegNo = get_reg(interCode->u.binop.op2);
            fprintf(fp, "  sub %s, %s, %s\n",
                    registers->regLsit[resultRegNo]->name,
                    registers->regLsit[op1RegNo]->name,
                    registers->regLsit[op2RegNo]->name);
        }
        store(fp, registers, interCode->u.binop.result, resultRegNo);
        setfree();
    } else if (kind == IC_MUL) {
        int resultRegNo = get_reg(interCode->u.binop.result);
        int op1RegNo, op2RegNo;
        if (interCode->u.binop.op1->kind == CONSTANT) {
            op1RegNo = T4;
            fprintf(fp, "  li %s, %d\n", registers->regLsit[op1RegNo]->name,
                    interCode->u.binop.op1->u.val);
        } else {
            op1RegNo = get_reg(interCode->u.binop.op1);
        }
        if (interCode->u.binop.op2->kind == CONSTANT) {
            op2RegNo = T5;
            fprintf(fp, "  li %s, %d\n", registers->regLsit[op2RegNo]->name,
                    interCode->u.binop.op2->u.val);
        } else {
            op2RegNo = get_reg(interCode->u.binop.op2);
        }
        fprintf(fp, "  mul %s, %s, %s\n", registers->regLsit[resultRegNo]->name,
                registers->regLsit[op1RegNo]->name,
                registers->regLsit[op2RegNo]->name);
        store(fp, registers, interCode->u.binop.result, resultRegNo);
        setfree();
    } else if (kind == IC_DIV) {
        int resultRegNo = get_reg(interCode->u.binop.result);
        int op1RegNo, op2RegNo;
        if (interCode->u.binop.op1->kind == CONSTANT) {
            op1RegNo = T4;
            fprintf(fp, "  li %s, %d\n", registers->regLsit[op1RegNo]->name,
                    interCode->u.binop.op1->u.val);
        } else {
            op1RegNo = get_reg(interCode->u.binop.op1);
        }
        if (interCode->u.binop.op2->kind == CONSTANT) {
            op2RegNo = T5;
            fprintf(fp, "  li %s, %d\n", registers->regLsit[op2RegNo]->name,
                    interCode->u.binop.op2->u.val);
        } else {
            op2RegNo = get_reg(interCode->u.binop.op2);
        }
        fprintf(fp, "  div %s, %s\n", registers->regLsit[op1RegNo]->name,
                registers->regLsit[op2RegNo]->name);
        fprintf(fp, "  mflo %s\n", registers->regLsit[resultRegNo]->name);
        store(fp, registers, interCode->u.binop.result, resultRegNo);
        setfree();
    } else if (kind == IC_DEC) {
    } else if (kind == IC_IF_GOTO) {
        char *relopName = interCode->u.if_goto.relop;
        int xRegNo = get_reg(interCode->u.if_goto.x);
        int yRegNo = get_reg(interCode->u.if_goto.y);
        if (!strcmp(relopName, "=="))
            fprintf(fp, "  beq %s, %s, %s\n", registers->regLsit[xRegNo]->name,
                    registers->regLsit[yRegNo]->name,
                    operand_2_string(interCode->u.if_goto.z));
        else if (!strcmp(relopName, "!="))
            fprintf(fp, "  bne %s, %s, %s\n", registers->regLsit[xRegNo]->name,
                    registers->regLsit[yRegNo]->name,
                    operand_2_string(interCode->u.if_goto.z));
        else if (!strcmp(relopName, ">"))
            fprintf(fp, "  bgt %s, %s, %s\n", registers->regLsit[xRegNo]->name,
                    registers->regLsit[yRegNo]->name,
                    operand_2_string(interCode->u.if_goto.z));
        else if (!strcmp(relopName, "<"))
            fprintf(fp, "  blt %s, %s, %s\n", registers->regLsit[xRegNo]->name,
                    registers->regLsit[yRegNo]->name,
                    operand_2_string(interCode->u.if_goto.z));
        else if (!strcmp(relopName, ">="))
            fprintf(fp, "  bge %s, %s, %s\n", registers->regLsit[xRegNo]->name,
                    registers->regLsit[yRegNo]->name,
                    operand_2_string(interCode->u.if_goto.z));
        else if (!strcmp(relopName, "<="))
            fprintf(fp, "  ble %s, %s, %s\n", registers->regLsit[xRegNo]->name,
                    registers->regLsit[yRegNo]->name,
                    operand_2_string(interCode->u.if_goto.z));
        setfree();
    }
}

void push(FILE *fp)
{
    fprintf(fp, "  addi $sp, $sp, -72\n");
    for (int i = T0; i <= T9; i++) {
        fprintf(fp, "  sw %s, %d($sp)\n", registers->regLsit[i]->name,
                (i - T0) * 4);
    }
    //把global_vars的所有内容存放到栈上
    fprintf(fp, "  addi $sp, $sp, -%d         #store mem\n", var_count * 4);
    fprintf(fp, "  la $gp, global_vars\n");
    for (int i = 0; i < var_count; i++) {
        fprintf(fp, "  lw $t9, %d($gp)\n", i * 4);
        fprintf(fp, "  sw $t9, %d($sp)\n", i * 4);
    }
}

void pop(FILE *fp)
{
    //把栈上的内容存恢复到global_vars
    fprintf(fp, "  la $gp, global_vars         #load mem\n");
    for (int i = 0; i < var_count; i++) {
        fprintf(fp, "  lw $t9, %d($sp)\n", i * 4);
        fprintf(fp, "  sw $t9, %d($gp)\n", i * 4);
    }
    fprintf(fp, "  addi $sp, $sp, %d\n", var_count * 4);
    for (int i = T0; i <= T9; i++) {
        fprintf(fp, "  lw %s, %d($sp)\n", registers->regLsit[i]->name,
                (i - T0) * 4);
    }
    fprintf(fp, "  addi $sp, $sp, 72\n");
}