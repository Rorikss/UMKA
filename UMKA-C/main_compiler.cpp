#include <iostream>
#include <string>
#include <cstdio>
#include <vector>
#include "BytecodeGenerator.h"

extern FILE* yyin;
extern int yyparse();

// program_stmts определён в parser.cpp (bison) — parser.hpp содержит тип Stmt
// extern std::vector<Stmt*> program_stmts;

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

    BytecodeGenerator gen;
    gen.generateAll(program_stmts);

    std::string outPath = std::string(inputPath) + ".bin";
    std::cout << "Generating bytecode to " << outPath << "\n";
    gen.writeToFile(outPath);

    std::cout << "Done.\n";
    return 0;
}
