%{
#include <stdio.h>
#include <stdbool.h>
#include "syntax.tab.h"
#include "tree.h"
extern bool error;

int lines = 0;//多文件
int yycolumn = 1;

#define YY_USER_ACTION \
    yylloc.first_line=yylloc.last_line=yylineno; \
    yylloc.first_column=yycolumn; \
    yylloc.last_column=yycolumn+yyleng-1; \
    yycolumn+=yyleng;
%}

%option yylineno

/* definition */
/*float识别有问题*/
STRUCT "struct"
IF "if"
ELSE "else"
WHILE "while"
RETURN "return"
TYPE "int"|"float"
ID [_a-zA-Z][_a-zA-Z0-9]*
INT (0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)
FLOAT ([0-9]+)\.([0-9]+)
STRING \"(\\.|[^"])*\"
PLUS \+
MINUS -
STAR \*
DIV \/
AND &&
OR \|\|
DOT \.
NOT !
ASSIGNOP =
SEMI ;
COMMA ,
RELOP >|<|>=|<=|==|!=
/* RELOP [><]=?|==|!= */
/*ligature is bad here */
/* {RELOP} { printf("RELOP: %s\n", yytext); } */

LC \{
RC \}
LP \(
RP \)
LB \[
RB \]
WHITESPACE [ \t]
%%
{STRUCT} { yylval.node=new_token("STRUCT",NULL,yylineno);return STRUCT;}
{IF} { yylval.node=new_token("IF",NULL,yylineno); return IF;}
{ELSE} { yylval.node=new_token("ELSE",NULL,yylineno); return ELSE;}
{WHILE} { yylval.node=new_token("WHILE",NULL,yylineno); return WHILE;}
{RETURN} { yylval.node=new_token("RETURN",NULL,yylineno); return RETURN;}
{TYPE} { yylval.node=new_token("TYPE",yytext,yylineno); return TYPE;}
{ID} { yylval.node=new_token("ID",yytext,yylineno); return ID;}
{INT} { yylval.node=new_token("INT",yytext,yylineno); return INT;}
{FLOAT} { yylval.node=new_token("FLOAT",yytext,yylineno); return FLOAT; }
{PLUS} { yylval.node=new_token("PLUS",NULL,yylineno); return PLUS; }
{MINUS} { yylval.node=new_token("MINUS",NULL,yylineno); return MINUS; }
{STAR} { yylval.node=new_token("STAR",NULL,yylineno); return STAR; }
{DIV} { yylval.node=new_token("DIV",NULL,yylineno); return DIV; }
{AND} { yylval.node=new_token("AND",NULL,yylineno); return AND; }
{OR} { yylval.node=new_token("OR",NULL,yylineno); return OR; }
{ASSIGNOP} { yylval.node=new_token("ASSIGNOP",NULL,yylineno); return ASSIGNOP; }
{SEMI} { yylval.node=new_token("SEMI",NULL,yylineno); return SEMI;}
{COMMA} { yylval.node=new_token("COMMA",NULL,yylineno); return COMMA;}
{LC} { yylval.node=new_token("LC",NULL,yylineno); return LC; }
{RC} { yylval.node=new_token("RC",NULL,yylineno); return RC;}
{LP} { yylval.node=new_token("LP",NULL,yylineno); return LP; }
{RP} { yylval.node=new_token("RP",NULL,yylineno); return RP; }
{LB} { yylval.node=new_token("LB",NULL,yylineno); return LB;}
{RB} { yylval.node=new_token("RB",NULL,yylineno); return RB;}
{DOT} { yylval.node=new_token("DOT",NULL,yylineno); return DOT;}
{NOT} { yylval.node=new_token("NOT",NULL,yylineno); return NOT;}
{STRING} { yylval.node=new_token("STRING",yytext,yylineno);return STRING;}
{RELOP} { yylval.node=new_token("RELOP",yytext,yylineno);return RELOP; } 
\n {yycolumn=1;}
{WHITESPACE} { /* ignore whitespace */ }

0[0-7]*[89][0-9]* {error = true;printf("Error type A at Line %d: invalid octal number: %s\n", yylineno,yytext); return INT;}
0[xX][0-9a-fA-F]*[g-zG-Z][0-9a-fA-Fg-zG-Z]* {error = true; printf("Error type A at Line %d: invalid hexadecimal number: %s\n", yylineno,yytext);return INT; }
({FLOAT}|\.[0-9]+|[0-9]+\.)[eE]+[+-]?[0-9]+ {error = true; printf("Error type A at Line %d: invalid float number: %s\n", yylineno,yytext);return FLOAT; }
({DOT}[0-9]+)+|([0-9]+{DOT})+ {error = true; printf("Error type A at Line %d: invalid float number: %s\n", yylineno,yytext);return FLOAT; }
. {error = true; printf("Error type A at Line %d: Mysterious character \"%s\".\n", yylineno, yytext); }

%%