# UMKA Lexer & Parser [tech doc]
Лексер (lexer.l) разбивает исходный текст на токены (ключевые слова, идентификаторы, литералы, операторы). 
Парсер (parser.y) строит AST как набор C++-структур (наследников Expr и Stmt) и собирает
в вектор program_stmts. main.cpp вызывает yyparse() и печатает AST через print_program_ast().


### Как работает парсер (кратко)

- Входной символ: program — последовательность statement.
- Парсер строит объекты C++:
  - Стейтменты (Stmt): LetStmt, AssignStmt, MemberAssignStmt, ExprStmt, BlockStmt, IfStmt, WhileStmt, ForStmt, ReturnStmt, FunctionDefStmt, ClassDefStmt, MethodDefStmt.
  - Выражения (Expr): IntExpr, DoubleExpr, StringExpr, BoolExpr, UnitExpr, IdentExpr, ArrayExpr, CallExpr, UnaryExpr, BinaryExpr, MemberAccessExpr, MethodCallExpr.

Выход: каждый разобранный statement добавляется в глобальный program_stmts (через add_program_stmt).

### Выражения (Expr)
- `IntExpr { long long v; }` целочисленный литерал
- `DoubleExpr { double v; }` double-литерал
- `StringExpr { string s; }` строковый литерал
- `BoolExpr { bool b; }` булевый литерал
- `UnitExpr { }` литерал типа unit
- `IdentExpr { string name; }` идентификатор
- `ArrayExpr { vector<Expr*> elems; }` литерал массива 
- `CallExpr { string name; vector<Expr*> args; }` вызов функции 
- `UnaryExpr { char op; Expr* rhs; }` унарный оператор (!, +, -)
- `BinaryExpr { string op; Expr* left; Expr* right; }` бинарный оператор
- `MemberAccessExpr { string object_name; string field; }` доступ к полю объекта (object:field)
- `MethodCallExpr { string object_name; string method_name; vector<Expr*> args; }`  вызов метода объекта (object$method(...))

### Операторы Statements (Stmt)
- `LetStmt { string name; Expr* expr; }  let IDENT = expression;`
- `AssignStmt { string name; Expr* expr; }  IDENT = expression;`
- `MemberAssignStmt { string object_name; string field; Expr* expr; } IDENT : IDENT = expression;` (присваивание полю объекта)
- `ExprStmt { Expr* expr; }  выражение как оператор (например print("hi");)`
- `BlockStmt { vector<Stmt*> stmts; }  { ... }`
- `IfStmt { Expr* cond; Stmt* thenb; Stmt* elseb; }`
- `WhileStmt { Expr* cond; Stmt* body; }`
- `ForStmt { Stmt* init; Expr* cond; Stmt* post; Stmt* body; }  for (let ...; ...; ...) { ... }` (init и post - LetStmt/AssignStmt)
- `ReturnStmt { Expr* expr; }`
- `FunctionDefStmt { string name; vector<string> params; string ret_type; Stmt* body; } fun IDENT(...) -> return_type { ... }`
- `ClassDefStmt { string name; vector<Stmt*> fields; } class IDENT { ... }` (fields - только LetStmt)
- `MethodDefStmt { string class_name; string method_name; vector<string> params; string ret_type; Stmt* body; } method IDENT IDENT(...) -> return_type { ... }`

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

# UMKA Virtual Machine [tech doc]

### Сущности
Переменная представляется как
```cpp
struct Entity {
    std::variant<int64_t, double, bool, unit, std::string, std::map<int, Reference<Entity>>> value;
};
```
где std::map<int, Reference<Entity>>> value -- наше представление массива

1. Сравнение сущностей:
```cpp
std::partial_ordering operator<=>(const Entity& other) const {
    return std::visit([&](auto&& arg1) {
        return std::visit([&](auto&& arg2) -> std::partial_ordering {
            // Сравнение разных типов
        }, other.value);
    }, value);
}
```

2. Преобразование типов:
```cpp
std::string to_string() const {
    return std::visit([](auto&& arg) -> std::string {
        // Обработка разных типов
    }, value);
}
```

Также определены 2 типа ссылок на объект: 
```cpp
# владение обхектом на куче
template<typename T>
using Owner = std::shared_ptr<T>;

# ссылка на объект на куче
template<typename T>
using Reference = std::weak_ptr<T>;
```


### Память
Куча представлена как:
```cpp
std::vector<Owner<Entity>> heap = {};
```
Все объекты выделяются на куче. 

Фунции у нас определяются стек фреймами
```cpp
struct StackFrame {Переменная представляется как
```cpp
struct Entity {
    std::variant<int64_t, double, bool, unit, std::string, std::map<int, Reference<Entity>>> value;
};
```
где std::map<int, Reference<Entity>>> value -- наше представление массива

Также определены 2 типа ссылок на объект: 
```cpp
# владение обхе
    uint64_t name;
    size_t instruction_index = 0;
    std::unordered_map<int64_t, Reference<Entity>> name_resolver = {};
};
```
- имя функции

    - если это просто код из файла, то он оборачивается компилятором в условный мейн
    - в ВМ передается мапа имен функций и их номеров, поэтому тут имя - число
    - есть указатель на текущую инструкцию 
    - есть нейм резолвер переменных. Тут он просто по имени хранит ссылку на обхект на хипе


Сборщик мусора должен обходить весь `name_resolver` функции и удалять из `heap` все недостижимые из `name_resolver`'а объекты. Для полного удаления использовать `shrink_to_fit`

### Стековая машина 
Для вычислений используется стековая машина.
Арифметические операции реализованы в файле `operations.h`

### Реализация операций

#### Обработка команд (stack_machine.h)
Команды выполняются в методе `execute_command` через switch-case. Для операций используются декораторы:

1. Бинарные операции (ADD, SUB и др.):
```cpp
auto BinaryOperationDecorator = [](const std::string& op_name, auto f) {
    BinaryOperationDecoratorWithApplier(op_name, f, numeric_applier<decltype(f)>);
};
```

2. Унарные операции (NOT):
```cpp
auto UnaryOperationDecorator = [](const std::string& op_name, auto f) {
    auto operand = get_operand_from_stack(op_name);
    Entity result = unary_applier(operand, f);
    create_and_push(result);
};
```

Чтобы уменьшить дубляж кода используем различные декораторы, куда передаем лямбды, как операторы.

#### Работа с памятью
1. Создание объекта:
```cpp
void create_and_push(Entity result) {
    heap.push_back(std::make_shared<Entity>(std::move(result)));
    operand_stack.push_back(heap.back());
}
```

2. Работа с переменными:
```cpp
// STORE - сохранение в name_resolver
frame.name_resolver[var_index] = ref;

// LOAD - загрузка из name_resolver
operand_stack.push_back(frame.name_resolver[var_index]);
```

#### Особенности реализации
Проверки реализованы через макросы:
1. Проверки ссылок:
```cpp
#define CHECK_REF(ref) \
    if ((ref).expired()) { \
        throw std::runtime_error("Reference expired"); \
    }
```

2. Проверки стека:
```cpp
#define CHECK_STACK_EMPTY(op_name) \
    if (operand_stack.empty()) { \
        throw std::runtime_error("Stack underflow"); \
    }
```

### Парсер команд (command_parser.h)
Чтение аргументов команд:
```cpp
if (has_operand(opcode) && index + sizeof(int64_t) <= bytecode.size()) {
    std::memcpy(&arg, &bytecode[index], sizeof(int64_t));
    index += sizeof(int64_t);
}
```

## Описание Garbage Collector

### Используется алгоритм Mark and sweep
1. Mark (Пометка): Начинаем с "корней" и помечаем все объекты, до которых можно добраться. Эти объекты - "живые".

2. Sweep (Очистка): Проходим по всей куче и освобождаем память, занятую непомеченными объектами.

### Когда запускаем
1. У нас есть счетчик общего обьема выделенной памяти (`bytes_allocated`). Он увеличивается с каждой операцией, которая выделяет память
   - для обычного объекта: `sizeof(Entity)`
   - для строк: `sizeof(Entity) + str.capacity()`
   - для массива `sizeof(Entity) + arr->size() * sizeof(std::pair<size_t, Reference<Entity>>)`
2. У нас есть лимит (`GC_THRESHOLD = total_available_ram_bytes * GC_PERCENT;`). GC_PERCENT = 1%
   GC запускается, когда разница между текущим объемом выделенной памяти и объемом после последней очистки превышает порог: `(bytes_allocated - after_last_clean) > GC_THRESHOLD`
### Stop the world  
На время работы GC выполнение кода ВМ останавливается. После того как GC завершит свою работу и он освободил достаточно памяти, то ВМ сможет продолжить работу


### Mark
Цель: пометить все достижимые (живые) обьекты
1. Для каждого обьекта в куче добавим поле is_marked (`std::unordered_map<const Entity*, bool> marked_objects`)
2. Перед началом пометки все флаги `false` (сброшены). Помечаем все `false`
3. Необходимы корни (Roots). С них и начинаем помечать
   - стек операндов. Все `Reference<Entity>` в стеке операндов (`operand_stack`)
   - `stackFrame = function frame` 
   - стек вызовов виртуальной машины. Сборщик мусора должен обходить весь `name_resolver` каждой функции в стеке вызовов

4. Рекурсивно обходим граф обьектов.
    - Если указатель на обьект в куче еще не был помечен - помечаем его  
    - Особый случай: массивы (`std::map<int, Reference<Entity>>`), нужно рекурсивно обойти все элементы массива

Таким образом, мы обойдем весь граф достижимых обьектов и они будут помечены как достижимые (`is_marked = true`)

### Sweep
Цель: проходим по всей куче и освобождаем, что было не отмечено как достижимый обьект
1. Иду по списку всех обьектов (`heap`)
2. Если обьект отмечен (`is_marked = true`) - он живой (его не трогаем).
3. Если обьект не отмечен (`is_marked = false`) - он мертв (его трогаем). Удаляем из списка всех обьектов, освобождаем память, уменьшаем счетчик `bytes_allocate`
4. Если очистка не помогла освободить столько памяти, чтобы новый обьект аллоцировался - выкидываем ошибку OutOfMemory
5. Делаем `heap.shrink_to_fit()`
