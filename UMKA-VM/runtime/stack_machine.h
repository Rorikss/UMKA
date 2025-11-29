#pragma once
#include "model/model.h"
#include "../parser/command_parser.h"
#include "operations.h"
#include "standart_funcs.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

#define CHECK_REF(ref) \
    if ((ref).expired()) { \
        throw std::runtime_error("Reference expired at " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
    }

#define CHECK_STACK_EMPTY(op_name) \
    if (operand_stack.empty()) { \
        throw std::runtime_error("Stack underflow at operation: " + op_name); \
    }

struct ReleaseMod {};
struct DebugMod {};

class StackMachine {
public:
    StackMachine(const auto& parser)
        : commands(parser.get_commands()),
          const_pool(parser.get_const_pool()),
          func_table(parser.get_func_table())
    {
        stack_of_functions.emplace_back(StackFrame{
            .name = 0,
            .instruction_ptr = commands.begin(),
            .name_resolver = {}
        });
    }

    using debugger_t = std::function<void(Command, std::string)>;
    template<typename Tag = ReleaseMod>
    void run(debugger_t debugger = [](auto, auto){}) {
        while (!stack_of_functions.empty()) {
            StackFrame& current_frame = stack_of_functions.back();
            if (current_frame.instruction_ptr >= commands.end()) {
                stack_of_functions.pop_back();
                continue;
            }
            
            auto it = current_frame.instruction_ptr;
            ++current_frame.instruction_ptr;
            if constexpr (std::is_same_v<Tag, DebugMod>) {
                auto entity = stack_lookup();
                debugger(*it, entity.has_value()
                    ? entity.value().to_string() 
                    : "EMPTY STACK"
                );
            }
            execute_command(*it, current_frame);
        }
    }

private:
    Entity parse_constant(const Constant& constant) {
        switch (constant.type) {
            case TYPE_INT64: {
                int64_t value;
                std::memcpy(&value, constant.data.data(), sizeof(int64_t));
                return make_entity(value);
            }
            case TYPE_DOUBLE: {
                double value;
                std::memcpy(&value, constant.data.data(), sizeof(double));
                return make_entity(value);
            }
            case TYPE_STRING: {
                std::string value(constant.data.begin(), constant.data.end());
                return make_entity(value);
            }
            default:
                throw std::runtime_error("Unknown constant type");
        }
    }

    void execute_command(const Command& cmd, StackFrame& current_frame) {
        auto call_void_proc = [this](auto proc) {
            auto arg = get_operand_from_stack("CALL PROC");
            proc(arg);
            create_and_push(make_entity(unit{}));
        };

        auto call_value_proc = [this](auto proc) {
            auto arg = get_operand_from_stack("CALL PROC");
            create_and_push(make_entity(proc(arg)));
        };

        auto BinaryOperationDecoratorWithApplier = [machine = this](const std::string& op_name, auto f, auto applier) {
            auto [lhs, rhs] = machine->get_operands_from_stack(op_name);
            Entity result = applier(lhs, rhs, f);
            machine->create_and_push(result);
        };

        auto BinaryOperationDecorator = [BinaryOperationDecoratorWithApplier](const std::string& op_name, auto f) {
            BinaryOperationDecoratorWithApplier(op_name, f, numeric_applier<decltype(f)>);
        };

        auto UnaryOperationDecorator = [machine = this](const std::string& op_name, auto f) {
            auto operand = machine->get_operand_from_stack(op_name);
            Entity result = unary_applier(operand, f);
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
                if (const_index < 0 || const_index >= const_pool.size()) {
                    throw std::runtime_error("Constant index out of bounds");
                }
                
                Entity constant_entity = parse_constant(const_pool[const_index]);
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
                switch (cmd.arg) {
                    case PRINT: call_void_proc([this](auto arg) { print(arg); }); break;
                    case LEN: call_value_proc([this](auto arg) { return len(arg); }); break;
                    case GET: {
                        auto idx = umka_cast<int64_t>(get_operand_from_stack("CALL GET"));
                        auto arr = get_operand_from_stack("CALL GET");
                        create_and_push(*get(arr, idx).lock());
                        break;
                    }
                    case SET: {
                        auto val = stack_pop();
                        auto idx = umka_cast<int64_t>(get_operand_from_stack("CALL SET"));
                        auto arr = get_operand_from_stack("CALL SET");
                        call_void_proc([&](auto) { set(arr, idx, val); });
                        break;
                    }
                    case ADD: {
                        auto val = stack_pop(); // тут сделана функция которая возвращает именно ссылку с вершины стека
                        auto arr = get_operand_from_stack("CALL ADD"); // вот тут мы вроде возвращаем &. но хотим ли мы именно так? правильно ли это? ссылка ли это вообще???? потому что это по идее ссылочный тип? а как это обеспечить? + хотим ли мы операторы по ссылке возвращать? или рил сделать 2 разных методв: по ссылке которая реф и просто ентити???
                        call_void_proc([&](auto) { add_elem(arr, val); });
                        break;
                    }
                    case REMOVE: {
                        auto idx = umka_cast<int64_t>(get_operand_from_stack("CALL REMOVE"));
                        auto arr = get_operand_from_stack("CALL REMOVE");
                        call_void_proc([&](auto) { remove(arr, idx); });
                        break;
                    }
                    case WRITE: {
                        auto content = get_operand_from_stack("CALL WRITE");
                        auto filename = get_operand_from_stack("CALL WRITE");
                        call_void_proc([&](auto) { write(filename.to_string(), content); });
                        break;
                    }
                    case READ: {
                        auto filename = get_operand_from_stack("CALL READ");
                        std::vector<std::string> lines = read(filename.to_string());
    
                        std::map<int, Reference<Entity>> array;
                        
                        for (size_t i = 0; i < lines.size(); ++i) {
                            Owner<Entity> line_entity = std::make_shared<Entity>(make_entity(lines[i]));
                            heap.push_back(line_entity);
                            array[i] = line_entity;
                        }
                    
                        Entity array_entity = make_entity(array);
                        create_and_push(array_entity);
                        break;
                    }
                    default: {
                        if (func_table.size() <= cmd.arg) {
                            throw std::runtime_error("Function not found: " + std::to_string(cmd.arg));
                        }

                        const FunctionTableEntry& entry = func_table[cmd.arg];
                        StackFrame new_frame;
                        new_frame.name = entry.id;
                        new_frame.instruction_ptr = commands.begin() + entry.code_offset;
                        
                        for (int64_t i = entry.arg_count - 1; i >= 0; --i) {
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
                }
                break;
            }
            case RETURN: {
                Reference<Entity> return_value;
                if (!operand_stack.empty()) {
                    return_value = operand_stack.back();
                    operand_stack.pop_back();
                    CHECK_REF(return_value);
                }
                
                if (stack_of_functions.empty()) {
                    throw std::runtime_error("No frame to return from");
                }
                stack_of_functions.pop_back();
                
                if (!return_value.expired() && !stack_of_functions.empty()) {
                    operand_stack.push_back(return_value);
                }
                break;
            }
            case BUILD_ARR: {
                int64_t count = cmd.arg;
                if (operand_stack.size() < static_cast<size_t>(count)) {
                    throw std::runtime_error("Not enough operands for BUILD_ARR");
                }

                std::map<int, Reference<Entity>> array;
                for (size_t i = 0; i < count; ++i) {
                    Reference<Entity> ref = operand_stack.back();
                    operand_stack.pop_back();
                    CHECK_REF(ref);
                    array[i] = ref;
                }

                Entity array_entity = make_entity(array);
                create_and_push(array_entity);
                break;
            }
            case OPCOT: {
                auto operand = get_operand_from_stack("OPCOT");
                create_and_push(make_entity(operand.is_unit()));
                break;
            }
            case TO_STRING: {
                auto operand = get_operand_from_stack("TO_STRING");
                create_and_push(make_entity(operand.to_string()));
                break;
            }
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
        Reference<Entity> rhs = operand_stack.back();
        operand_stack.pop_back();
        CHECK_STACK_EMPTY(op_name);
        Reference<Entity> lhs = operand_stack.back();
        operand_stack.pop_back();
        CHECK_REF(lhs);
        CHECK_REF(rhs);
        return {*lhs.lock(), *rhs.lock()};
    }

    Entity& get_operand_from_stack(const std::string& op_name) { // возможно нет 
        CHECK_STACK_EMPTY(op_name);
        Reference<Entity> operand = operand_stack.back();
        operand_stack.pop_back();
        CHECK_REF(operand);
        return *operand.lock();
    }

    Reference<Entity> stack_pop() {
        CHECK_STACK_EMPTY(std::string("STACK_POP"));
        Reference<Entity> operand = operand_stack.back();
        operand_stack.pop_back();
        return operand;
    }

    std::optional<Entity> stack_lookup() {
        if (operand_stack.empty()) {
            return std::nullopt;
        }
        return *operand_stack.back().lock();
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
    std::vector<Constant> const_pool;
    std::vector<FunctionTableEntry> func_table;
    std::vector<Owner<Entity>> heap = {};
    std::vector<StackFrame> stack_of_functions;
    std::vector<Reference<Entity>> operand_stack;
};

#undef CHECK_STACK_EMPTY
#undef CHECK_REF
