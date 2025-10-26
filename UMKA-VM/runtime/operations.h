#pragma once

#include "../model/model.h"
#include <stdexcept>
#include <functional>
#include <unordered_map>

class ArithmeticOperationHandler {
public:
    using OperationFunc = std::function<Entity(const Entity&, const Entity&)>;

    ArithmeticOperationHandler() {
        // Register all arithmetic operations
        operations[0x10] = [](const Entity& a, const Entity& b) { 
            return Entity{std::get<int64_t>(a.value) + std::get<int64_t>(b.value)}; 
        };
        operations[0x11] = [](const Entity& a, const Entity& b) { 
            return Entity{std::get<int64_t>(a.value) - std::get<int64_t>(b.value)}; 
        };
        operations[0x12] = [](const Entity& a, const Entity& b) { 
            return Entity{std::get<int64_t>(a.value) * std::get<int64_t>(b.value)}; 
        };
        operations[0x13] = [](const Entity& a, const Entity& b) { 
            if (std::get<int64_t>(b.value) == 0) throw std::runtime_error("Division by zero");
            return Entity{std::get<int64_t>(a.value) / std::get<int64_t>(b.value)}; 
        };
        operations[0x14] = [](const Entity& a, const Entity& b) { 
            if (std::get<int64_t>(b.value) == 0) throw std::runtime_error("Division by zero");
            return Entity{std::get<int64_t>(a.value) % std::get<int64_t>(b.value)}; 
        };
    }

    Entity perform_operation(uint8_t opcode, const Entity& a, const Entity& b) {
        auto it = operations.find(opcode);
        if (it == operations.end()) {
            throw std::runtime_error("Unsupported arithmetic operation: " + std::to_string(opcode));
        }
        return it->second(a, b);
    }

private:
    std::unordered_map<uint8_t, OperationFunc> operations;
};