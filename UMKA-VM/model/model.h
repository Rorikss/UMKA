#pragma once

#include <compare>
#include <concepts>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace umka::vm {
template<typename T>
using Reference = std::weak_ptr<T>;
template<typename T>
using Owner = std::shared_ptr<T>;

struct Entity;
using Array = std::vector<Reference<Entity>>;
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
struct ReferencableImpl {
    static constexpr bool value = false;
};

template <typename T>
struct ReferencableImpl<Reference<T>> {
    static constexpr bool value = true;
};

template <typename T>
struct ReferencableImpl<Owner<T>> {
    static constexpr bool value = true;
};

template <typename T>
concept Referencable = ReferencableImpl<T>::value;

template <typename T>
concept ConvertableToString = requires(T value) {
    { std::to_string(value) } -> std::same_as<std::string>;
};

struct Entity {
    std::variant<
        int64_t,
        double,
        bool, 
        unit, 
        std::string, 
        Owner<Array>
    > value;

    std::string to_string() const {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (ConvertableToString<T>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, unit>) {
                return "unit";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_same_v<T, Owner<Array>>) {
                std::stringstream ss;
                ss << "[";
                for (size_t it = 0; const auto& value : *arg) {
                    ss 
                        << it << ": " 
                        << value.lock()->to_string() 
                        << (it + 1 == arg->size() ? "" : ", ")
                    ;
                    ++it;
                }
                ss << "]";
                return ss.str();
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
    int64_t code_offset_end;
    int64_t arg_count;
    int64_t local_count;
    
    FunctionTableEntry() : id(0), code_offset(0), code_offset_end(0), arg_count(0), local_count(0) {}
};

enum BuiltinFunctionIDs : int64_t {
    PRINT_FUN = 9223372036854775807LL,
    LEN_FUN = 9223372036854775806LL,
    GET_FUN = 9223372036854775805LL,
    SET_FUN = 9223372036854775804LL,
    ADD_FUN = 9223372036854775803LL,
    REMOVE_FUN = 9223372036854775802LL,
    WRITE_FUN = 9223372036854775800LL,
    READ_FUN = 9223372036854775799LL,
    ASSERT_FUN = 9223372036854775798LL,
    INPUT_FUN = 9223372036854775797LL,
    RANDOM_FUN = 9223372036854775796LL,
};

enum ConstantType : uint8_t {
    TYPE_INT64 = 0x01,
    TYPE_DOUBLE = 0x02,
    TYPE_STRING = 0x03,
    TYPE_UNIT = 0x04,
};

struct Constant {
    ConstantType type;
    std::vector<uint8_t> data;
};

struct StackFrame {
    uint64_t name;
    std::vector<Command>::const_iterator instruction_ptr;
    std::vector<Command>::const_iterator begin;
    std::vector<Command>::const_iterator end;
    std::unordered_map<int64_t, Reference<Entity>> name_resolver = {};
};

// Virtual method table entry: (class_id, method_id) -> function_id
struct VMethodTableEntry {
    int64_t class_id;
    int64_t method_id;
    int64_t function_id;
};

// Virtual field table entry: (class_id, field_id) -> field_index
struct VFieldTableEntry {
    int64_t class_id;
    int64_t field_id;
    int64_t field_index;
};

Entity make_entity(auto&& x) { return Entity { .value = x }; }
Entity make_array();

struct ReleaseMod {};
struct DebugMod {};

}
