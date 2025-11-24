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