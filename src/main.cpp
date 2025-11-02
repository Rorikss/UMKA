#include <iostream>

extern FILE* yyin;
extern int yyparse();
extern void execute_program(); // <- объявляем функцию из parser.y

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

  // Выполняем программу, которая была собрана парсером
  execute_program();

  return 0;
}
