#pragma once

#include "../model/model.h"
#include <exception>

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
