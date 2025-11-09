#pragma once
#include "model/model.h"
#include "operations.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

#define CHECK_REF(ref) \
    if ((ref).expired()) { \
        throw std::runtime_error("Reference expired at " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
    }

#define CHECK_STACK_EMPTY(op_name) \
    if (operand_stack.empty()) { \
        throw std::runtime_error("Stack underflow at operatione: " + op_name); \
    }

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
    OPCOT = 0x40,
    TO_STRING = 0x60,
    TO_INT = 0x61,
    TO_DOUBLE = 0x62
};

class StackMachine {
public:
    StackMachine(const std::vector<Command>& cmds,
                 const std::vector<Entity>& const_pool,
                 const std::vector<FunctionTableEntry>& func_table)
        : commands(cmds), constant_pool(const_pool), function_table(func_table)
    {
        stack_of_functions.emplace_back(StackFrame{
            .name = 0,
            .instruction_ptr = commands.begin(),
            .name_resolver = {}
        });
    }

    void run() {
        while (!stack_of_functions.empty()) {
            StackFrame& current_frame = stack_of_functions.back();
            if (current_frame.instruction_ptr >= commands.end()) {
                stack_of_functions.pop_back();
                continue;
            }
            
            auto it = current_frame.instruction_ptr;
            ++current_frame.instruction_ptr;
            execute_command(*it, current_frame);
        }
    }

private:
    void execute_command(const Command& cmd, StackFrame& current_frame) {
        auto BinaryOperationDecoratorWithApplier = [machine = this](const std::string& op_name, auto f, auto applier) {
            auto [lhs, rhs] = machine->get_operands_from_stack(op_name);
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
            CHECK_REF(operand);
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
            case PUSH_CONST: {
                int64_t const_index = cmd.arg;
                if (const_index < 0 || const_index >= constant_pool.size()) {
                    throw std::runtime_error("Constant index out of bounds");
                }
                
                Entity constant_entity = constant_pool[const_index];
                create_and_push(constant_entity);
                break;
            }
            case POP:
                CHECK_STACK_EMPTY(std::string("POP"));
                operand_stack.pop_back();
                break;
            case STORE: {
                int64_t var_index = cmd.arg;
                CHECK_STACK_EMPTY(std::string("STORE"));
                Reference<Entity> ref = operand_stack.back();
                operand_stack.pop_back();
                
                if (stack_of_functions.empty()) {
                    throw std::runtime_error("No active stack frame");
                }
                StackFrame& frame = stack_of_functions.back();
                frame.name_resolver[var_index] = ref;
                break;
            }
            case LOAD: {
                int64_t var_index = cmd.arg;
                if (stack_of_functions.empty()) {
                    throw std::runtime_error("No active stack frame");
                }
                StackFrame& frame = stack_of_functions.back();
                if (!frame.name_resolver.contains(var_index)) {
                    throw std::runtime_error("Variable not found");
                }
                operand_stack.push_back(frame.name_resolver[var_index]);
                break;
            }
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
                current_frame.instruction_ptr = commands.begin() + cmd.arg;
                break;
            case JMP_IF_FALSE:
                if (!jump_condition()) {
                    current_frame.instruction_ptr = commands.begin() + cmd.arg;
                }
                break;
            case JMP_IF_TRUE:
                if (jump_condition()) {
                    current_frame.instruction_ptr = commands.begin() + cmd.arg;
                }
                break;
            case CALL: {
                if (function_table.size() <= cmd.arg) {
                    throw std::runtime_error("Function not found: " + std::to_string(cmd.arg));
                }

                auto it = function_table.begin() + cmd.arg;
                StackFrame new_frame;
                new_frame.name = it->id;
                new_frame.instruction_ptr = commands.begin() + it->code_offset;
                
                for (int i = it->arg_count - 1; i >= 0; --i) {
                    if (operand_stack.empty()) {
                        throw std::runtime_error("Not enough arguments for function call");
                    }
                    Reference<Entity> arg_ref = operand_stack.back();
                    operand_stack.pop_back();
                    new_frame.name_resolver[i] = arg_ref;
                }
                
                stack_of_functions.push_back(new_frame);
                break;
            }
            case BUILD_ARR: {
                int64_t count = cmd.arg;
                if (operand_stack.size() < static_cast<size_t>(count)) {
                    throw std::runtime_error("Not enough operands for BUILD_ARR");
                }

                std::map<int, Reference<Entity>> array;
                for (int i = 0; i < count; ++i) {
                    Reference<Entity> ref = operand_stack.back();
                    operand_stack.pop_back();
                    CHECK_REF(ref);
                    array[i] = ref;
                }

                Entity array_entity;
                array_entity.value = array;
                create_and_push(array_entity);
                break;
            }
            case OPCOT:
                // TODO
                break;
            case TO_STRING:
                // todo
                break;
            case TO_INT: {
                auto casted_value = umka_cast<int64_t>(get_operand_from_stack("CAST_TO_INT"));
                create_and_push(make_entity(casted_value));
                break;
            }
            case TO_DOUBLE: {
                auto casted_value = umka_cast<double>(get_operand_from_stack("CAST_TO_DOUBLE"));
                create_and_push(make_entity(casted_value));
                break;
            }
            default:
                throw std::runtime_error("Unknown opcode: " + std::to_string(cmd.code));
        }
    }

    std::pair<Entity, Entity> get_operands_from_stack(const std::string& op_name) {
        CHECK_STACK_EMPTY(op_name);
        Reference<Entity> lhs = operand_stack.back();
        operand_stack.pop_back();
        CHECK_STACK_EMPTY(op_name);
        Reference<Entity> rhs = operand_stack.back();
        operand_stack.pop_back();
        CHECK_REF(lhs);
        CHECK_REF(rhs);
        return {*lhs.lock(), *rhs.lock()};
    }

    Entity get_operand_from_stack(const std::string& op_name) {
        CHECK_STACK_EMPTY(op_name);
        Reference<Entity> operand = operand_stack.back();
        operand_stack.pop_back();
        CHECK_REF(operand);
        return *operand.lock();
    }

    void create_and_push(Entity result) {
        heap.push_back(std::make_shared<Entity>(std::move(result)));
        operand_stack.push_back(heap.back());
    }

    bool jump_condition() {
        CHECK_STACK_EMPTY(std::string("JUMP_CONDITION"));
        Reference<Entity> condition_ref = operand_stack.back();
        operand_stack.pop_back();
        if (condition_ref.expired()) throw std::runtime_error("Condition expired");
        Entity condition = *condition_ref.lock();
        return umka_cast<bool>(condition);
    }

    std::vector<Command> commands;
    std::vector<Entity> constant_pool;
    std::vector<FunctionTableEntry> function_table;
    std::vector<Owner<Entity>> heap = {};
    std::vector<StackFrame> stack_of_functions;
    std::vector<Reference<Entity>> operand_stack;
};

#undef CHECK_STACK_EMPTY
#undef CHECK_REF
