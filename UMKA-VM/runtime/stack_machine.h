#pragma once
#include "../model/model.h"
#include "../model/command_parser.h"
#include "operations.h"
#include <functional>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <stdexcept>

class StackMachine {
public:
    StackMachine(const std::vector<Command>& cmds) : commands(cmds) {} 


    void run() {
        size_t program_counter = 0;
        while (program_counter < commands.size()) {
            execute_command(commands[program_counter]);
            program_counter++;
        }
    }

private:
    void execute_command(const Command& cmd) {
        auto BinaryOperationDecoratorWithApplier = [machine = this](const std::string& op_name, auto f, auto applier) {
            if (machine->operand_stack.size() < 2) throw std::runtime_error("Not enough operands for " + op_name);
            auto [lhs, rhs] = machine->get_operands_from_stack();
            Entity result = applier(lhs, rhs, f);
            machine->create_and_push(result);
        };

        auto BinaryOperationDecorator = [BinaryOperationDecoratorWithApplier](const std::string& op_name, auto f) {
            BinaryOperationDecoratorWithApplier(op_name, f, numeric_applier<decltype(f)>);
        };

        switch (cmd.code) {
            case 0x01: // PUSH_CONST
                // TODO
                break;
            case 0x02: // POP
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                operand_stack.pop_back();
                break;
            case 0x03: // STORE
                // TODO
                break;
            case 0x04: // LOAD
                // TODO
                break;
            case 0x10: // ADD
                BinaryOperationDecorator("ADD", [](auto a, auto b) { return a + b; });
                break;
            case 0x11: // SUB
                BinaryOperationDecorator("SUB", [](auto a, auto b) { return a - b; });
                break;
            case 0x12: // MUL
                BinaryOperationDecorator("MUL", [](auto a, auto b) { return a * b; });
                break;
            case 0x13: // DIV
                BinaryOperationDecorator("DIV", [](auto a, auto b) { return a / b; });
                break;
            case 0x14: // REM
                auto f = [](auto a, auto b) { return a % b; };
                BinaryOperationDecoratorWithApplier("REM", f, mod_applier<decltype(f)>);
                break;
            case 0x17: // NOT
                // TODO
                break;
            case 0x18: // AND
                BinaryOperationDecorator("AND", [](auto a, auto b) { return a && b; });
                break;
            case 0x19: // OR 
                BinaryOperationDecorator("OR", [](auto a, auto b) { return a || b; });
                break;
            case 0x1A: // EQ
                // todo
                break;
            case 0x1B: // NEQ
                // todo
                break;
            case 0x1C: // GT
                // todo
                break;
            case 0x1D: // LT
                // todo
                break;
            case 0x1E: // GTE
                // todo
                break;
            case 0x1F: // LTE
                // todo
                break;
            case 0x20: // JMP
                // TODO
                break;
            case 0x21: // JMP_IF_FALSE
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                // TODO
                operand_stack.pop_back();
                break;
            case 0x22: // JMP_IF_TRUE
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                // TODO
                operand_stack.pop_back();
                break;
            case 0x23: // CALL
                // TODO
                break;
            case 0x30: // BUILD_ARR
                // TODO
                break;
            case 0x40: // OPCOT
                // TODO
                break;
            default:
                throw std::runtime_error("Unknown opcode: " + std::to_string(cmd.code));
        }
    }

    std::pair<Entity, Entity> get_operands_from_stack() {
        Reference<Entity> lhs = operand_stack.back();
        operand_stack.pop_back();
        Reference<Entity> rhs = operand_stack.back();
        operand_stack.pop_back();
        if (lhs.expired() || rhs.expired()) throw std::runtime_error("Oops.. Garbage Collector destroyed data :(");
        return {*lhs.lock(), *rhs.lock()};
    }

    void create_and_push(Entity result) {
        heap.push_back(std::make_shared<Entity>(std::move(result)));
        operand_stack.push_back(heap.back());
    }

    std::vector<Command> commands;
    std::vector<Owner<Entity>> heap = {};
    std::vector<StackFrame> stack_of_functions = {};
    std::vector<Reference<Entity>> operand_stack;
};
