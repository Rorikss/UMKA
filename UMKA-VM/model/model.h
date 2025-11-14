#pragma once

#include <compare>
#include <concepts>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

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

template <typename T, typename U>
concept Comparable = requires(T a, U b) {
    { a <=> b } -> std::convertible_to<std::partial_ordering>;
};

template <typename T, typename U>
concept Equtable = requires(T a, U b) {
    { a == b } -> std::convertible_to<bool>;
};

template <typename T>
concept ConvertableToString = requires(T value) {
    { std::to_string(value) } -> std::same_as<std::string>;
};

struct Entity {
    std::variant<int64_t, double, bool, unit, std::string, std::map<int, Reference<Entity>>> value;

    std::string to_string() const {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (ConvertableToString<T>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, unit>) {
                return "unit";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else {
                throw std::runtime_error("Cannot convert to string");
            }
        }, value);
    }

    friend bool operator==(const Reference<Entity> a, const Reference<Entity> b) {
        if (a.expired() || b.expired()) throw std::runtime_error("Reference is expired");
        auto [own_a, own_b] = std::pair{ a.lock(), b.lock() };
        if (own_a && own_b) return *own_a == *own_b;
        return own_a == own_b;
    }

    std::partial_ordering operator<=>(const Entity& other) const {
        return std::visit([&](auto&& arg1) {
            return std::visit([&](auto&& arg2) -> std::partial_ordering {
                using T1 = std::decay_t<decltype(arg1)>;
                using T2 = std::decay_t<decltype(arg2)>;
                
                if constexpr (Comparable<T1, T2>) {
                    return arg1 <=> arg2;
                }
                if constexpr (Equtable<T1, T2>) {
                    return arg1 == arg2
                    ? std::partial_ordering::equivalent 
                    : std::partial_ordering::unordered;
                }
                return std::partial_ordering::unordered;
            }, other.value);
        }, value);
    }
    
    bool operator==(const Entity& other) const {
        return (*this <=> other) == std::partial_ordering::equivalent;
    }

    bool is_unit() const {
        return std::holds_alternative<unit>(value);
    }
};


struct Command {
    uint8_t code;
    int64_t arg;
};

struct FunctionTableEntry {
    uint64_t id;
    int64_t code_offset;
    int64_t arg_count;
    int64_t local_count;
};

struct StackFrame {
    uint64_t name;
    std::vector<Command>::iterator instruction_ptr;
    std::unordered_map<int64_t, Reference<Entity>> name_resolver = {};
};

Entity make_entity(auto x) { return Entity { .value = x }; }
