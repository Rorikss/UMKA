#include <iostream>

extern FILE* yyin;
extern int yyparse();
extern void execute_program();
extern void print_program_ast();

extern void generate_bytecode();
extern void print_generated_code();

int main(int argc, char** argv) {
  if (argc > 1) {
    yyin = fopen(argv[1], "r");
    if (!yyin) {
      std::cerr << "Cannot open file: " << argv[1] << std::endl;
      return 1;
    }
  } else {
    // читаем из stdin
    yyin = stdin;
  }

  if (yyparse() != 0) {
    std::cerr << "Parsing failed." << std::endl;
    return 1;
  }

  // распечатать AST (для отладки)
  print_program_ast();
  // Выполняем программу, которая была собрана парсером
  execute_program();
  generate_bytecode();
  print_generated_code();


  return 0;
}
