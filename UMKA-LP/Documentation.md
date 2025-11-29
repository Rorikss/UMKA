Лексер (lexer.l) разбивает исходный текст на токены (ключевые слова, идентификаторы, литералы, операторы). 
Парсер (parser.y) строит AST как набор C++-структур (наследников Expr и Stmt) и собирает
в вектор program_stmts. main.cpp вызывает yyparse() и печатает AST через print_program_ast().


### Как работает парсер (кратко)

- Входной символ: program — последовательность statement.
- Парсер строит объекты C++:
  - Стейтменты (Stmt): LetStmt, AssignStmt, ExprStmt, BlockStmt, IfStmt, WhileStmt, ForStmt, ReturnStmt, FunctionDefStmt.
  - Выражения (Expr): IntExpr, DoubleExpr, StringExpr, BoolExpr, IdentExpr, ArrayExpr, CallExpr, UnaryExpr, BinaryExpr.

Выход: каждый разобранный statement добавляется в глобальный program_stmts (через add_program_stmt).

### Выражения (Expr)
- IntExpr { long long v; } целочисленный литерал
- DoubleExpr { double v; } double-литерал
- StringExpr { string s; } строковый литерал
- BoolExpr { bool b; } булевый литерал
- IdentExpr { string name; } идентификатор
- ArrayExpr { vector<Expr*> elems; } литерал массива 
- CallExpr { string name; vector<Expr*> args; } вызов функции 
- UnaryExpr { char op; Expr* rhs; } унарный оператор (!, +, -)
- BinaryExpr { string op; Expr* left; Expr* right; } бинарный оператор

### Операторы Statements (Stmt)
- LetStmt { string name; Expr* expr; }  let IDENT = expression;
- AssignStmt { string name; Expr* expr; }  IDENT = expression;
- ExprStmt { Expr* expr; }  выражение как оператор (например print("hi");)
- BlockStmt { vector<Stmt*> stmts; }  { ... }
- IfStmt { Expr* cond; Stmt* thenb; Stmt* elseb; }
- WhileStmt { Expr* cond; Stmt* body; }
- ForStmt { Stmt* init; Expr* cond; Stmt* post; Stmt* body; }  for (let ...; ...; ...) { ... } (init и post — LetStmt/AssignStmt)
- ReturnStmt { Expr* expr; }
- FunctionDefStmt { string name; vector<string> params; Stmt* body; }

### Win_flex & Win_bizon
- win_flex берет файл lexer.l и генерирует исходник сканера. В нём находится функция yylex() и код, который
разбивает поток символов на токены и заполняет yylval
- win_bison берет файл parser.y и генерирует исходник парсера (parser.cpp) и заголовок (parser.hpp) с реализацией yyparse() с определениями токенов и YYSTYPE
  Генерирует перечисления токенов (enum) и union для yylval, который лексер заполняет.
  Парсер вызывает yylex() для получения токенов; yylex() возвращает код токена и заполняет yylval

### В итоге
1. win_bison генерирует заголовок с токенами и YYSTYPE
2. Лексер (win_flex) генерирует #include "parser.hpp", чтобы знать значения токенов и структуру yylval
3. На этапе выполнения yyparse() (в парсере) вызывается yylex() (сгенерированным лексером)
4. Семантические действия в parser.y используют данные из yylval и создают AST-узлы