#ifndef _ASSEMBLY_H_
#define _ASSEMBLY_H_

#include "ir.h"
#include "semantic.h"
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>

#define REG_NUM 32

typedef struct _register *pRegister;
typedef struct _registers *pRegisters;

typedef Operand *pOperand;
typedef InterCode *pInterCode;

typedef struct _register {
    bool isFree;
    char *name;
    pOperand op;
} Register;

typedef struct _registers {
    pRegister regLsit[REG_NUM];
    int lastChangedNo;
} Registers;

typedef enum _regNo {
    ZERO,
    AT,
    V0,
    V1,
    A0,
    A1,
    A2,
    A3,
    T0,
    T1,
    T2,
    T3,
    T4,
    T5,
    T6,
    T7,
    S0,
    S1,
    S2,
    S3,
    S4,
    S5,
    S6,
    S7,
    T8,
    T9,
    K0,
    K1,
    GP,
    SP,
    FP,
    RA,
} RegNo;

extern pRegisters registers;

pRegisters init_registers();
void reset_registers();

pRegister new_register(const char *regName);

void init_assembly_code(FILE *fp);
void inter_to_assem(FILE *fp, CodeList *codelist);
void push(FILE *fp);
void pop(FILE *fp);
void gen_assembly_code(char *filename);

#endif