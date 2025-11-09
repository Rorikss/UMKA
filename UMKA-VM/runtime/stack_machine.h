#pragma once
#include "model/model.h"
#include "runtime/command_parser.h"
#include "operations.h"
#include <compare>
#include <cstdint>
#include <memory>
#include <vector>
#include <stdexcept>

enum OpCode : uint8_t {
    PUSH_CONST = 0x01,
    POP = 0x02,
    STORE = 0x03,
    LOAD = 0x04,
    ADD = 0x10,
    SUB = 0x11,
    MUL = 0x12,
    DIV = 0x13,
    MOD = 0x14,
    NOT = 0x17,
    AND = 0x18,
    OR = 0x19,
    EQ = 0x1A,
    NEQ = 0x1B,
    GT = 0x1C,
    LT = 0x1D,
    GTE = 0x1E,
    LTE = 0x1F,
    JMP = 0x20,
    JMP_IF_FALSE = 0x21,
    JMP_IF_TRUE = 0x22,
    CALL = 0x23,
    BUILD_ARR = 0x30,
    OPCOT = 0x40
};

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

        auto UnaryOperationDecorator = [machine = this](const std::string& op_name, auto f) {
            if (machine->operand_stack.empty()) throw std::runtime_error("Not enough operands for " + op_name);
            Reference<Entity> operand = machine->operand_stack.back();
            machine->operand_stack.pop_back();
            if (operand.expired()) throw std::runtime_error("Operand expired for " + op_name);
            Entity result = unary_applier(*operand.lock(), f);
            machine->create_and_push(result);
        };

        auto CompareOperationDecorator = [BinaryOperationDecoratorWithApplier](auto f) {
            BinaryOperationDecoratorWithApplier(
                "Ordering",
                f,
                [](const Entity& a, const Entity& b, auto f) {
                    return Entity(f(a, b));
                }
            );
        };

        switch (cmd.code) {
            case PUSH_CONST:
                // TODO
                break;
            case POP:
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                operand_stack.pop_back();
                break;
            case STORE:
                // TODO
                break;
            case LOAD:
                // TODO
                break;
            case ADD:
                BinaryOperationDecorator("ADD", [](auto a, auto b) { return a + b; });
                break;
            case SUB:
                BinaryOperationDecorator("SUB", [](auto a, auto b) { return a - b; });
                break;
            case MUL:
                BinaryOperationDecorator("MUL", [](auto a, auto b) { return a * b; });
                break;
            case DIV:
                BinaryOperationDecorator("DIV", [](auto a, auto b) { return a / b; });
                break;
            case MOD: {
                auto f = [](auto a, auto b) { return a % b; };
                BinaryOperationDecoratorWithApplier("REM", f, mod_applier<decltype(f)>);
                break;
            }
            case NOT:
                UnaryOperationDecorator("NOT", [](auto val) { return !val; });
                break;
            case AND:
                BinaryOperationDecorator("AND", [](auto a, auto b) { return a && b; });
                break;
            case OR:
                BinaryOperationDecorator("OR", [](auto a, auto b) { return a || b; });
                break;
            case EQ:
                CompareOperationDecorator([](auto a, auto b) { return a == b; });
                break;
            case NEQ:
                CompareOperationDecorator([](auto a, auto b) { return a != b; });
                break;
            case GT:
                CompareOperationDecorator([](auto a, auto b) { return a > b; });
                break;
            case LT:
                CompareOperationDecorator([](auto a, auto b) { return a < b; });
                break;
            case GTE:
                CompareOperationDecorator([](auto a, auto b) { return a >= b; });
                break;
            case LTE:
                CompareOperationDecorator([](auto a, auto b) { return a <= b; });
                break;
            case JMP:
                // TODO
                break;
            case JMP_IF_FALSE:
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                // TODO
                operand_stack.pop_back();
                break;
            case JMP_IF_TRUE:
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                // TODO
                operand_stack.pop_back();
                break;
            case CALL:
                // TODO
                break;
            case BUILD_ARR:
                // TODO
                break;
            case OPCOT:
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
