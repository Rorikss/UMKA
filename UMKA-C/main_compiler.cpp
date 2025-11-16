#include <iostream>
#include <string>
#include <cstdio>
#include <vector>

extern FILE* yyin;
extern int yyparse();
class Stmt;
extern std::vector<Stmt*> program_stmts;

// BytecodeGenerator объявляем как extern класс, реализация через CMake
class BytecodeGenerator {
public:
    void generateAll(const std::vector<Stmt*>& stmts);
    void writeToFile(const std::string& path);
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: umka_compiler <source_file.umka>\n";
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
        fclose(yyin);
        return 1;
    }
    fclose(yyin);

    if (program_stmts.empty()) {
        std::cerr << "Error: program has no statements.\n";
        return 1;
    }

    // Генерация байткода
    BytecodeGenerator gen;
    gen.generateAll(program_stmts);

    std::string outPath = std::string(inputPath) + ".bin";
    std::cout << "Generating bytecode → " << outPath << "\n";
    gen.writeToFile(outPath);

    std::cout << "Done.\n";
    return 0;
}
