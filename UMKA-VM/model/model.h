#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <variant>
#include <string>
#include <unordered_map>

template<typename T>
using Reference = std::weak_ptr<T>;
template<typename T>
using Owner = std::shared_ptr<T>;

using unit = std::monostate;

#define for_all_types(X) \
    X(int) \
    X(double) \
    X(bool) \
    X(std::string)
    
#define for_all_numeric(X) \
    X(int) \
    X(double) \
    X(bool)

struct Entity {
    std::variant<int64_t, double, bool, unit, std::string, std::map<int, Reference<Entity>>> value;
};

struct Command {
    uint8_t code;
    int64_t arg;
};

struct StackFrame {
    uint64_t name;
    size_t instruction_index = 0;
    std::unordered_map<int64_t, Reference<Entity>> name_resolver = {};
};

Entity make_entity(auto x) { return Entity { .value = x }; }
