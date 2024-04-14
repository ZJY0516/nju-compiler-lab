%{
#include <stdio.h>
#include <stdbool.h>
#include "lex.yy.c"
#include "tree.h"
int yylex(void);
void yyerror (char  *s);
struct node *root;
extern bool error;
%}

%locations
%union {
/*int type_int;
float type_float;
double type_double;*/
struct node *node;
 }
%token <node>INT
%token <node>FLOAT
%token <node>PLUS MINUS STAR DIV DOT
%token <node>LP RP LC RC LB RB
%token <node>ASSIGNOP RELOP
%token <node>ID
%token <node>IF ELSE WHILE FOR RETURN
%token <node>AND OR NOT
%token <node>SEMI COMMA
%token <node>STRUCT TYPE STRING

/*%type <type_int> input*/

%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT
%left DOT
%left LB RB
%left LP RP

%type <node> Program ExtDefList ExtDef ExtDecList 
%type <node> Specifier StructSpecifier OptTag Tag
%type <node> VarDec FunDec VarList ParamDec 
%type <node> CompSt StmtList Stmt DefList Def DecList Dec Exp Args

%%

/* 不能只声明函数 */
/* 变量必须在语句块的开头定义 */
/* 不能初始化全局变量 */
/* 不能在函数内部定义结构体？ */
/* int a[2][2*3]不行 */

/* High-level Definitions */
Program: ExtDefList{root=new_node("Program", @$.first_line, $1,NULL); }
       ;
ExtDefList: ExtDef ExtDefList{$$ = new_node("ExtDefList", @$.first_line, $1, $2,NULL);}
           | /* empty */{$$ = p_empty;}
           ;
ExtDef: Specifier ExtDecList SEMI{$$ = new_node("ExtDef", @$.first_line, $1, $2,$3,NULL);}
      | Specifier SEMI{$$ = new_node("ExtDef", @$.first_line, $1, $2,NULL);}
      | Specifier FunDec CompSt{$$ = new_node("ExtDef", @$.first_line, $1, $2,$3,NULL);}/*不能只有函数声明？*/
      | Specifier FunDec SEMI{$$ = new_node("ExtDef", @$.first_line, $1,$2,$3,NULL);}
      | error SEMI{ error = true; }
      ;
ExtDecList: VarDec{$$ = new_node("ExtDecList", @$.first_line, $1,NULL);}
          | VarDec COMMA ExtDecList{$$ = new_node("ExtDecList", @$.first_line, $1,$2,$3,NULL);}
          ;

/* Specifiers */
Specifier: TYPE{$$ = new_node("Specifier", @$.first_line, $1,NULL); }
         | StructSpecifier{$$ = new_node("Specifier", @$.first_line, $1,NULL);}
         ;
StructSpecifier: STRUCT OptTag LC DefList RC{$$ = new_node("StructSpecifier", @$.first_line, $1, $2,$3,$4,$5,NULL);}
               | STRUCT Tag{$$ = new_node("StructSpecifier", @$.first_line, $1, $2,NULL);}
               ;
OptTag: ID{$$ = new_node("OptTag", @$.first_line, $1,NULL);}
      |/* empty */{$$ = p_empty;}
      ;
Tag: ID{$$ = new_node("Tag", @$.first_line, $1,NULL);}

/* Declarators */
VarDec: ID{$$ = new_node("VarDec", @$.first_line, $1,NULL);}
      | VarDec LB INT RB{$$ = new_node("VarDec", @$.first_line, $1,$2,$3,$4,NULL);}
      ;
FunDec: ID LP VarList RP{$$ = new_node("FunDec", @$.first_line, $1,$2,$3,$4,NULL);}
      | ID LP RP{$$ = new_node("FunDec", @$.first_line, $1,$2,$3,NULL);}
      | error RP{ error = true; }
      ;
VarList: ParamDec{$$ = new_node("VarList", @$.first_line, $1,NULL);}
       | ParamDec COMMA VarList{$$ = new_node("VarList", @$.first_line, $1,$2,$3,NULL);}
       ;
ParamDec: Specifier VarDec{$$ = new_node("ParamDec", @$.first_line, $1,$2,NULL);}
        ;

/* Statements */
CompSt: LC DefList StmtList RC{$$ = new_node("CompSt", @$.first_line, $1,$2,$3,$4,NULL);}
      | error RC{ error = true; }
      ;
StmtList: Stmt StmtList{$$ = new_node("StmtList", @$.first_line, $1,$2,NULL);}
        | /* empty */{$$ = p_empty;}
        ;
Stmt: Exp SEMI{$$ = new_node("Stmt", @$.first_line, $1,$2,NULL);}
    | CompSt{$$ = new_node("Stmt", @$.first_line, $1,NULL);}
    | RETURN Exp SEMI{$$ = new_node("Stmt", @$.first_line, $1,$2,$3,NULL);}
    | IF LP Exp RP Stmt{$$ = new_node("Stmt", @$.first_line, $1,$2,$3,$4,$5,NULL);}
    | IF LP Exp RP Stmt ELSE Stmt{$$ = new_node("Stmt", @$.first_line, $1,$2,$3,$4,$5,$6,$7,NULL);}
    | WHILE LP Exp RP Stmt{$$ = new_node("Stmt", @$.first_line, $1,$2,$3,$4,$5,NULL);}
    | WHILE LP error RP Stmt{error = true;}
    | WHILE error RP Stmt{error = true;}
    | WHILE LP error Stmt{error = true;}
    | error SEMI{error = true;}
    | error Stmt{ error = true; }
    | Exp error {error = true;}
    | Exp error SEMI{error = true;}
    ;

/* Local Definitions */
DefList: Def DefList {$$ = new_node("DefList", @$.first_line, $1,$2,NULL);}
       | /* empty */{$$ = p_empty;}
       ;
Def : Specifier DecList SEMI{$$ = new_node("Def", @$.first_line, $1,$2,$3,NULL);}
      | error SEMI{ error = true; }
     ;

DecList: Dec{$$ = new_node("DecList", @$.first_line, $1,NULL);}
      | Dec COMMA DecList{$$ = new_node("DecList", @$.first_line, $1,$2,$3,NULL);}
      ;

Dec: VarDec{$$ = new_node("Dec", @$.first_line, $1,NULL);}
   | VarDec ASSIGNOP Exp{$$ = new_node("Dec", @$.first_line, $1,$2,$3,NULL);}
   ;

/* Expressions */
Exp: Exp ASSIGNOP Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp AND Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp OR Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp RELOP Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp PLUS Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp MINUS Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp STAR Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp DIV Exp {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | LP Exp RP {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | MINUS Exp {$$ = new_node("Exp", @$.first_line, $1,$2,NULL);}
   | NOT Exp {$$ = new_node("Exp", @$.first_line, $1,$2,NULL);}
   | ID LP Args RP {$$ = new_node("Exp", @$.first_line, $1,$2,$3,$4,NULL);}
   | ID LP RP {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | Exp LB Exp RB {$$ = new_node("Exp", @$.first_line, $1,$2,$3,$4,NULL);}
   | Exp DOT ID {$$ = new_node("Exp", @$.first_line, $1,$2,$3,NULL);}
   | ID {$$ = new_node("Exp", @$.first_line, $1,NULL);}
   | INT {$$ = new_node("Exp", @$.first_line, $1,NULL);}
   | FLOAT {$$ = new_node("Exp", @$.first_line, $1,NULL);}
   | LB error{ error = true;}
   | error RP{ error = true;}
   | error{error = true;}
   ;
Args: Exp{$$ = new_node("Args", @$.first_line, $1,NULL);}
    | Exp COMMA Args{$$ = new_node("Args", @$.first_line, $1,$2,$3,NULL);}
    ;

%%

/* int main()
{ return yyparse();} */
void yyerror (char  *s) {
      fprintf(stdout, "Error type B at Line %d: %s\n", yylineno,s);
}
