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

# UMKA JIT [tech doc]

JIT (Just-In-Time) компилятор оптимизирует байткод функций во время выполнения программы. Оптимизации применяются асинхронно в отдельном потоке, что позволяет не блокировать выполнение виртуальной машины.

### Архитектура

JIT состоит из трех основных компонентов:

1. **JitManager** - управляет процессом JIT-компиляции:
   - Очередь функций для оптимизации
   - Асинхронная компиляция в отдельном потоке
   - Состояния функций: `NONE`, `QUEUED`, `RUNNING`, `READY`

2. **JitRunner** - выполняет оптимизации над байткодом функции:
   - Применяет последовательность оптимизаций
   - Работает с байткодом функции (диапазон команд от `code_offset` до `code_offset_end`)

3. **JittedFunction** - результат оптимизации:
   ```cpp
   struct JittedFunction {
       std::vector<vm::Command> code;  // оптимизированный байткод
       int64_t arg_count;              // количество аргументов
       int64_t local_count;            // количество локальных переменных
   };
   ```

### Асинхронная компиляция

JitManager работает в отдельном потоке (`worker_loop`):

1. **Запрос оптимизации**: При вызове функции VM вызывает `request_jit(function_id)`
   - Если функция еще не в очереди (`NONE`), она переводится в состояние `QUEUED` и добавляется в очередь
   - Поток-воркер уведомляется через `condition_variable`

2. **Обработка очереди**: Поток-воркер:
   - Ждет уведомления о новых функциях в очереди
   - Берет функцию из очереди и переводит в состояние `RUNNING`
   - Вызывает `JitRunner::optimize_function()` для применения оптимизаций
   - Сохраняет результат в `jit_functions` и переводит в состояние `READY`

3. **Использование оптимизированного кода**: При вызове функции VM проверяет:
   ```cpp
   if (jit_manager->has_jitted(function_id)) {
       auto jitted_func = jit_manager->try_get_jitted(function_id);
       // Используем оптимизированный байткод вместо оригинального
   }
   ```

### Оптимизации

Оптимизации реализованы как классы, наследующие `IOptimize`:
```cpp
struct IOptimize {
    virtual void run(
        std::vector<vm::Command>& code,
        std::vector<vm::Constant>& const_pool,
        std::unordered_map<size_t, vm::FunctionTableEntry>& func_table,
        vm::FunctionTableEntry& meta
    ) = 0;
};
```

Оптимизации применяются последовательно в следующем порядке:
1. ConstantPropagation
2. ConstFolding
3. ConstantPropagation (повторно)
4. DeadCodeElimination

#### ConstFolding (Свертка констант)

Вычисляет выражения с константами на этапе компиляции:

1. Поддерживает бинарные операции: `ADD`, `SUB`, `MUL`, `DIV`, `REM`, сравнения (`LT`, `GT`, `EQ` и др.), логические (`AND`, `OR`)
2. Работает со стеком констант: при встрече `PUSH_CONST` - добавляет значение в стек, при бинарной операции - вычисляет результат
3. Операции, требующие flush стека: `CALL`, `LOAD`, `STORE`, `RETURN`, `JMP`, `BUILD_ARR` и др. - сбрасывают стек констант

Пример:
```
PUSH_CONST 0    → stack: [5]
PUSH_CONST 1    → stack: [5, 3]
ADD             → вычисляет 5+3=8, stack: [8]
```
Преобразуется в:
```
PUSH_CONST 2
```

#### ConstantPropagation (Распространение констант)

Заменяет загрузки переменных на константы, если значение известно:

1. Отслеживает значения локальных переменных в `locals` массиве
2. При `STORE` - сохраняет известное значение переменной
3. При `LOAD` - если значение известно и переменная не используется в переходах (`used_in_jump`), заменяет `LOAD` на `PUSH_CONST`
4. Сбрасывает состояние при барьерах: `JMP`, `CALL`, `RETURN` - значения переменных становятся неизвестными

Особенности:
- Проверяет, что переменная не перезаписывается до следующего использования (`is_written_later`)
- Не заменяет переменные, используемые в условных переходах (`JMP_IF_FALSE`, `JMP_IF_TRUE`)

#### DeadCodeElimination (Удаление мертвого кода)

Удаляет недостижимый и неиспользуемый код:

1. **Анализ достижимости**: Обходит граф потока управления (DFS от начала функции):
   - `JMP` - переход к цели
   - `JMP_IF_FALSE`/`JMP_IF_TRUE` - обе ветки (следующая инструкция и цель перехода)
   - Остальные инструкции - последовательное выполнение

2. **Анализ использования**: Для достижимых инструкций определяет, нужны ли они:
   - Инструкции с побочными эффектами (`STORE`, `RETURN`, `CALL`, `POP`) - всегда нужны
   - Инструкции, результаты которых используются (анализ стека в обратном направлении)
   - Цели переходов - всегда нужны

3. **Пересчет переходов**: После удаления мертвого кода пересчитывает смещения в `JMP` инструкциях

### Интеграция с VM

JitManager создается при инициализации StackMachine:
```cpp
jit_manager(std::make_unique<jit::JitManager>(commands, const_pool, func_table))
```

При вызове функции (`CALL`):
1. Проверяется наличие оптимизированной версии (`has_jitted`)
2. Если есть - используется оптимизированный байткод из `JittedFunction`
3. Если нет - запрашивается оптимизация (`request_jit`) и используется оригинальный байткод
4. При следующем вызове функции может быть уже готова оптимизированная версия

### Потокобезопасность

JitManager использует несколько мьютексов для синхронизации:
- `queue_mutex` - защищает очередь функций
- `state_mutex` - защищает состояния функций (`jit_state`)
- `data_mutex` - защищает результаты оптимизации (`jit_functions`)

Это позволяет VM безопасно проверять наличие оптимизированного кода и запрашивать оптимизацию из разных потоков (если VM будет многопоточной в будущем).

