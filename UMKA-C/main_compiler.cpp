#include <iostream>
#include <string>
#include <cstdio>
#include <vector>
#include "runtime/bytecode_generator.h"

extern FILE* yyin;
extern int yyparse();
extern void print_program_ast();

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: umka_compiler <source_file>\n";
        return 1;
    }

    const char* inputPath = argv[1];
    yyin = fopen(inputPath, "r");
    if (!yyin) {
        std::cerr << "Cannot open file: " << inputPath << "\n";
        return 1;
    }

    std::cout << "Parsing: " << inputPath << "\n";
    if (yyparse() != 0) {
        std::cerr << "Parsing failed.\n";
        if (yyin != stdin) fclose(yyin);
        return 1;
    }
    if (yyin != stdin) fclose(yyin);

    if (program_stmts.empty()) {
        std::cerr << "Error: program has no statements.\n";
        return 1;
    }

    print_program_ast();
    BytecodeGenerator gen;
    gen.generate_all(program_stmts);

    std::string outPath = std::string(inputPath) + ".bin";
    std::cout << "Generating bytecode to " << outPath << "\n";
    gen.write_to_file(outPath);

    std::cout << "Done.\n";
    return 0;
}
