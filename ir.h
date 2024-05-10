#ifndef __IR_H
#define __IR_H
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define INTER_CODE_LENTH 100

typedef struct _Operand {
    enum { VARIABLE, CONSTANT, ADDRESS, LABEL, ARR_STRU } kind;
    union {
        uint32_t var_no;
        uint32_t label_no;
        int32_t val; // only int
    } u;
} Operand;

typedef struct _InterCode {
    enum {
        IC_LABEL,
        IC_FUNC,
        IC_ASSIGN,
        IC_ADD,
        IC_MINUS,
        IC_MUL,
        IC_DIV,
        IC_GET_ADDR,
        IC_GET_VALUE,
        IC_SET_VALUE,
        IC_GOTO,
        IC_IF_GOTO,
        IC_RETURN,
        IC_DEC,
        IC_ARG,
        IC_CALL,
        IC_PARAM,
        IC_READ,
        IC_WRITE,
        CHANGE_ADDR,
    } kind;
    union {
        Operand *op;
        char *func;
        struct {
            Operand *right, *left;
        } assign;
        struct {
            Operand *result, *op1, *op2;
        } binop;
        struct {
            Operand *x, *y, *z;
            char *relop;
        } if_goto;
        struct {
            Operand *x;
            int size;
        } dec;
        struct {
            Operand *result;
            char *func;
        } call;
    } u;
} InterCode;

typedef struct _ArgList ArgList;
struct _ArgList {
    Operand *args;
    ArgList *next;
};

typedef struct _CodeList {
    InterCode *code;
    struct _CodeList *prev, *next;
} CodeList;

void gen_intercode(char *filename);

#endif