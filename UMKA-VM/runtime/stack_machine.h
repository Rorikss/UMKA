#pragma once
#include "model/model.h"
#include "operations.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <stdexcept>

#define CHECK_REF(ref) \
    if ((ref).expired()) { \
        throw std::runtime_error("Reference expired at " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
    }

#define CHECK_STACK_EMPTY() \
    if (operand_stack.empty()) { \
        throw std::runtime_error("Stack underflow at " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
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
    OPCOT = 0x40
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
                CHECK_STACK_EMPTY();
                operand_stack.pop_back();
                break;
            case STORE: {
                int64_t var_index = cmd.arg;
                if (operand_stack.empty()) {
                    throw std::runtime_error("Stack underflow for STORE");
                }
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
                auto it = frame.name_resolver.find(var_index);
                if (it == frame.name_resolver.end()) {
                    throw std::runtime_error("Variable not found");
                }
                operand_stack.push_back(it->second);
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
                int64_t func_id = cmd.arg;
                auto it = std::find_if(function_table.begin(), function_table.end(),
                    [func_id](const FunctionTableEntry& entry) { return entry.id == func_id; });
                
                if (it == function_table.end()) {
                    throw std::runtime_error("Function not found: " + std::to_string(func_id));
                }
                
                StackFrame new_frame;
                new_frame.name = it->id;
                new_frame.instruction_ptr = commands.begin() + it->code_offset;
                
                for (int i = it->arg_count - 1; i >= 0; i--) {
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

    bool jump_condition() {
        CHECK_STACK_EMPTY();
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
