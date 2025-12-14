#pragma once

#include "../model/model.h"
#include <exception>
#include <type_traits>
#include <charconv>

namespace umka::vm {
#define if_get_then_apply_op(T, S) \
    if (std::get_if<T>(&a.value) && std::get_if<S>(&b.value)) { \
        return make_entity(op(std::get<T>(a.value), std::get<S>(b.value))); \
    }

template<typename F>
Entity numeric_applier(Entity a, Entity b, F op) {
    if_get_then_apply_op(int64_t, int64_t)
    if_get_then_apply_op(int64_t, double)
    if_get_then_apply_op(int64_t, bool)

    if_get_then_apply_op(bool, int64_t)
    if_get_then_apply_op(bool, double)
    if_get_then_apply_op(bool, bool)

    if_get_then_apply_op(double, int64_t)
    if_get_then_apply_op(double, double)
    if_get_then_apply_op(double, bool)

    throw std::runtime_error("bad cast in f");
}

template<typename F>
Entity mod_applier(Entity a, Entity b, F op) {
    if_get_then_apply_op(int64_t, int64_t)
    if_get_then_apply_op(bool, int64_t)
    if_get_then_apply_op(bool, bool)

    throw std::runtime_error("bad cast in f");
}
#undef if_get_then_apply_op

#define if_get_then_apply_unary_op(T) \
    if (auto* val = std::get_if<T>(&a.value)) { \
        return make_entity(op(*val)); \
    }

template<typename F>
Entity unary_applier(Entity a, F op) {
    if_get_then_apply_unary_op(int64_t)
    if_get_then_apply_unary_op(double)
    if_get_then_apply_unary_op(bool)
    
    throw std::runtime_error("bad cast in unary operation");
}

#undef if_get_then_apply_unary_op

template <typename T>
T umka_cast(Entity a) {
    T new_value = std::visit([a](auto value) -> T {
        if constexpr (std::is_convertible_v<std::decay_t<decltype(value)>, T>) {
            return static_cast<T>(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return a.to_string();
        } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string> ) {
            if constexpr (std::is_same_v<T, int64_t>) {
                return std::stoll(value);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(value);
            } else if constexpr (std::is_same_v<T, bool>) {
                if (value == "true") {
                    return true;
                } else if (value == "false") {
                    return false;
                }
                throw std::runtime_error("Bad cast in umka_cast");
            }
            throw std::runtime_error("Bad cast in umka_cast");
        } else {
            throw std::runtime_error("Bad cast in umka_cast");
        }
    }, a.value);
    return new_value;
}
}
