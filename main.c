#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// bool lex_error = false;
// bool syntax_error = false;
bool error = false;
extern FILE *yyin;
extern int yylex(void);
extern int yyparse();
extern void yyrestart(FILE *);
struct node;
extern void print_tree(struct node *, int);
extern struct node *root;
extern int yydebug;

int main(int argc, char **argv)
{
    if (argc <= 1)
        return 1;
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    yyrestart(f);
    // yydebug = 1;
    yyparse();
    if (!error) {
        print_tree(root, 0);
    }
    return 0;
}
// int main(int argc, char **argv)
// {
//     if (argc > 1) {
//         if (!(yyin = fopen(argv[1], "r"))) {
//             perror(argv[1]);
//             return 1;
//         }
//     }
//     while (yylex() != 0)
//         ;
//     return 0;
// }
