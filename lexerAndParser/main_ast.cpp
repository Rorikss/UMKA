#include <iostream>

extern FILE* yyin;
extern int yyparse();
extern void print_program_ast();

int main(int argc, char** argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            std::cerr << "Cannot open file: " << argv[1] << std::endl;
            return 1;
        }
    } else {
        yyin = stdin;
    }

    if (yyparse() != 0) {
        std::cerr << "Parsing failed." << std::endl;
        if (yyin != stdin) fclose(yyin);
        return 1;
    }

    if (yyin != stdin) fclose(yyin);

    // Печать AST
    print_program_ast();
    return 0;
}
