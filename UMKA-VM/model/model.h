#pragma once

#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
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

constexpr static int64_t kMaxI64 = std::numeric_limits<int64_t>::max();
enum BuiltinFunctionIDs : int64_t {
    PRINT_FUN = kMaxI64,
    LEN_FUN = kMaxI64 - 1,
    GET_FUN = kMaxI64 - 2,
    SET_FUN = kMaxI64 - 3,
    ADD_FUN = kMaxI64 - 4,
    REMOVE_FUN = kMaxI64 - 5,
    CONCAT_FUN = kMaxI64 - 6,
    WRITE_FUN = kMaxI64 - 7,
    READ_FUN = kMaxI64 - 8,
    ASSERT_FUN = kMaxI64 - 9,
    INPUT_FUN = kMaxI64 - 10,
    RANDOM_FUN = kMaxI64 - 11,
    POW_FUN = kMaxI64 - 12,
    SQRT_FUN = kMaxI64 - 13,
    MIN_FUN = kMaxI64 - 14,
    MAX_FUN = kMaxI64 - 15,
    SORT_FUN = kMaxI64 - 16,
    SPLIT_FUN = kMaxI64 - 17,
    MAKE_HEAP_FUN = kMaxI64 - 18,
    POP_HEAP_FUN = kMaxI64 - 19,
    PUSH_HEAP_FUN = kMaxI64 - 20,
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
