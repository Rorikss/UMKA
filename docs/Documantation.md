# UMKA Virtual Maschine

### Сущности
Переменная представляется как
```cpp
struct Entity {
    std::variant<int64_t, double, bool, unit, std::string, std::map<int, Reference<Entity>>> value;
};
```
где std::map<int, Reference<Entity>>> value -- наше представление массива

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
struct StackFrame {
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