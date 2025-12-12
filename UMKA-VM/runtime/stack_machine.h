#pragma once

#include "../garbage_collector/garbage_collector.h"
#include "../parser/command_parser.h"
#include "model/model.h"
#include "operations.h"
#include "profiler.h"
#include "standart_funcs.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace umka::vm {
#define CHECK_REF(ref) \
    if ((ref).expired()) { \
        throw std::runtime_error("Reference expired at " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
    }

#define CHECK_STACK_EMPTY(op_name) \
    if (operand_stack.empty()) { \
        throw std::runtime_error("Stack underflow at operation: " + op_name); \
    }


template<typename Tag = ReleaseMod>
class StackMachine {
public:
    StackMachine(const auto& parser)
        : commands(parser.get_commands()),
          const_pool(parser.get_const_pool()),
          func_table(parser.get_func_table()),
          vmethod_table(parser.get_vmethod_table()),
          vfield_table(parser.get_vfield_table()),
          profiler(std::make_unique<Profiler>(func_table, commands)),
          garbage_collector()
    {
        // Build lookup maps for fast dispatch
        for (const auto& entry : vmethod_table) {
            vmethod_map[{entry.class_id, entry.method_id}] = entry.function_id;
        }
        for (const auto& entry : vfield_table) {
            vfield_map[{entry.class_id, entry.field_id}] = entry.field_index;
        }
        stack_of_functions.emplace_back(StackFrame{
            .name = 0,
            .instruction_ptr = commands.begin(),
            .name_resolver = {}
        });
    }

    using debugger_t = std::function<void(Command, std::string)>;
    void run(debugger_t debugger = [](auto, auto){}) {
        if constexpr (std::is_same_v<Tag, DebugMod>) {
            printDebugParsedInfo();
        }
        
        while (!stack_of_functions.empty()) {
            StackFrame& current_frame = stack_of_functions.back();
            if (current_frame.instruction_ptr >= commands.end()) {
                stack_of_functions.pop_back();
                continue;
            }
            
            auto it = current_frame.instruction_ptr;
            size_t current_offset = std::distance(commands.begin(), it);
            ++current_frame.instruction_ptr;
            
            if constexpr (std::is_same_v<Tag, DebugMod>) {
                auto entity = stack_lookup();
                debugger(*it, entity.has_value()
                    ? entity.value().to_string()
                    : "EMPTY STACK"
                );
            }
            execute_command(*it, current_frame, current_offset);
        }
    }

    Profiler* get_profiler() { return profiler.get(); }
    
private:
    size_t get_current_function() const {
        if (!stack_of_functions.empty()) {
            const StackFrame& frame = stack_of_functions.back();
            return frame.name;
        }
        return 0;
    }

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
            case TYPE_UNIT:
                return make_entity(unit{});
            default:
                throw std::runtime_error("Unknown constant type");
        }
    }

    void execute_command(const Command& cmd, StackFrame& current_frame, size_t current_offset) {
        auto call_void_proc = [this](auto proc) {
            auto arg = get_operand_from_stack("CALL PROC");
            proc(arg);
            create_and_push(make_entity(unit{}));
        };

        auto call_value_proc = [this](auto proc) {
            auto arg = get_operand_from_stack("CALL PROC");
            create_and_push(make_entity(proc(arg)));
        };

        auto call_function = [this](int64_t function_id, const std::string& error_context) {
            if (func_table.size() <= function_id) {
                throw std::runtime_error("Function not found: " + std::to_string(function_id));
            }

            const FunctionTableEntry& entry = func_table[function_id];
            if (entry.code_offset < 0 || entry.code_offset >= commands.size() ||
                entry.code_offset_end < 0 || entry.code_offset_end > commands.size() ||
                entry.code_offset >= entry.code_offset_end) {
                throw std::runtime_error("Invalid function code range");
            }

            profiler->increment_function_call(function_id);

            StackFrame new_frame;
            new_frame.name = entry.id;
            new_frame.instruction_ptr = commands.begin() + entry.code_offset;
            
            for (int64_t i = entry.arg_count - 1; i >= 0; --i) {
                if (operand_stack.empty()) {
                    throw std::runtime_error("Not enough arguments for " + error_context);
                }
                Reference<Entity> arg_ref = operand_stack.back();
                operand_stack.pop_back();
                new_frame.name_resolver[i] = arg_ref;
            }
            
            stack_of_functions.push_back(new_frame);
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
            case REM: {
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
                profiler->record_backward_jump(current_offset, cmd.arg, get_current_function());
                current_frame.instruction_ptr += cmd.arg;
                break;
            case JMP_IF_FALSE:
                if (!jump_condition()) {
                    profiler->record_backward_jump(current_offset, cmd.arg, get_current_function());
                    current_frame.instruction_ptr += cmd.arg;
                }
                break;
            case JMP_IF_TRUE:
                if (jump_condition()) {
                    profiler->record_backward_jump(current_offset, cmd.arg, get_current_function());
                    current_frame.instruction_ptr += cmd.arg;
                }
                break;
            case CALL: {
                switch (cmd.arg) {
                    case PRINT_FUN: call_void_proc([this](auto arg) { print(arg); }); break;
                    case LEN_FUN: call_value_proc([this](auto arg) { return len(arg); }); break;
                    case GET_FUN: {
                        auto idx = umka_cast<int64_t>(get_operand_from_stack("CALL GET"));
                        auto arr = get_operand_from_stack("CALL GET");
                        create_and_push(*get(arr, idx).lock());
                        break;
                    }
                    case SET_FUN: {
                        auto val = stack_pop();
                        auto idx = umka_cast<int64_t>(get_operand_from_stack("CALL SET"));
                        call_void_proc([&](auto arr) { set(arr, idx, val); });
                        break;
                    }
                    case ADD_FUN: {
                        auto val = stack_pop();
                        call_void_proc([&](auto arr) { add_elem(arr, val); });
                        break;
                    }
                    case REMOVE_FUN: {
                        auto idx = umka_cast<int64_t>(get_operand_from_stack("CALL REMOVE"));
                        call_void_proc([&](auto arr) { remove(arr, idx); });
                        break;
                    }
                    case WRITE_FUN: {
                        auto content = get_operand_from_stack("CALL WRITE");
                        call_void_proc([&](auto filename) { write(filename.to_string(), content); });
                        break;
                    }
                    case READ_FUN: {
                        auto filename = get_operand_from_stack("CALL READ");
                        std::vector<std::string> lines = read(filename.to_string());
     
                        Entity array_entity = make_array();
                        Array& array = *std::get<Owner<Array>>(array_entity.value); 
                        for (size_t i = 0; i < lines.size(); ++i) {
                            Owner<Entity> line_entity = create(make_entity(lines[i]));
                            array[i] = line_entity;
                        }

                        create_and_push(array_entity);
                        break;
                    }
                    case ASSERT_FUN: {
                        call_void_proc([this](auto arg) { umka_assert(arg); });
                        break;
                    }
                    case INPUT_FUN: {
                        auto input_value = input();
                        create_and_push(make_entity(input_value));
                        break;
                    }
                    case RANDOM_FUN: {
                        create_and_push(make_entity(random()));
                        break;
                    }
                    default: {
                        call_function(cmd.arg, "function call");
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

                Entity array_entity = make_array();
                Array& array = *std::get<Owner<Array>>(array_entity.value);
                array.resize(count);
                for (int64_t i = count - 1; i >= 0; --i) {
                    Reference<Entity> ref = operand_stack.back();
                    operand_stack.pop_back();
                    CHECK_REF(ref);
                    array[i] = ref;
                }

                create_and_push(std::move(array_entity));
                break;
            }
            case OPCOT: {
                auto [lhs, rhs] = get_operands_from_stack("OPCOT");
                create_and_push(lhs.is_unit() ? rhs : lhs);
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
            case CALL_METHOD: {
                int64_t method_id = cmd.arg;
                
                Entity obj = *operand_stack.back().lock();
                auto arr = std::get<Owner<Array>>(obj.value);
                
                Reference<Entity> class_id_ref = (*arr)[0];
                CHECK_REF(class_id_ref);
                int64_t class_id = umka_cast<int64_t>(*class_id_ref.lock());
                
                auto key = std::make_pair(class_id, method_id);
                auto it = vmethod_map.find(key);
                if (it == vmethod_map.end()) {
                    throw std::runtime_error("CALL_METHOD: method not found for class_id=" +
                                           std::to_string(class_id) + ", method_id=" + std::to_string(method_id));
                }
                
                int64_t function_id = it->second;
                
                call_function(function_id, "method call");
                break;
            }
            case GET_FIELD: {
                int64_t field_id = cmd.arg;
                
                Entity obj = get_operand_from_stack("GET_FIELD");
                Owner<Array>& arr = std::get<Owner<Array>>(obj.value);
            
                Reference<Entity> class_id_ref = (*arr)[0];
                CHECK_REF(class_id_ref);
                int64_t class_id = umka_cast<int64_t>(*class_id_ref.lock());

                auto key = std::make_pair(class_id, field_id);
                auto it = vfield_map.find(key);
                if (it == vfield_map.end()) {
                    throw std::runtime_error("GET_FIELD: field not found for class_id=" +
                                           std::to_string(class_id) + ", field_id=" + std::to_string(field_id));
                }
                
                int64_t field_index = it->second;
                
                Reference<Entity> field_ref = get(obj, field_index);
                operand_stack.push_back(field_ref);
                break;
            }
            default:
                throw std::runtime_error("Unknown opcode: " + std::to_string(cmd.code) + " at " + std::to_string(current_offset));
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

    Owner<Entity> create(Entity result) {
        size_t entity_size = GarbageCollector<Tag>::calculate_entity_size(result);
        
        if (garbage_collector.should_collect()) {
            garbage_collector.collect(heap, operand_stack, stack_of_functions);
            if (garbage_collector.should_collect()) {
                throw std::runtime_error("OutOfMemory: Garbage collection did not free enough memory");
            }
        }
        heap.emplace_back(std::make_shared<Entity>(std::move(result)));
        garbage_collector.add_allocated_bytes(entity_size);
        return heap.back();
    }

    void create_and_push(Entity result) {
        operand_stack.push_back(create(std::move(result)));
    }

    bool jump_condition() {
        CHECK_STACK_EMPTY(std::string("JUMP_CONDITION"));
        Reference<Entity> condition_ref = operand_stack.back();
        operand_stack.pop_back();
        if (condition_ref.expired()) throw std::runtime_error("Condition expired");
        Entity condition = *condition_ref.lock();
        return umka_cast<bool>(condition);
    }

    void printDebugParsedInfo() {
        auto funcs = std::vector(func_table.begin(), func_table.end());
        std::cout << "Functions:\n";
        for (auto [id, f] : funcs) {
            std::cout << id << " " << f.id << ' '
            << '[' << f.code_offset << ", " << f.code_offset_end << "] "
            << "\n";
        }
        
        std::cout << "\nVirtual Method Table:\n";
        for (const auto& entry : vmethod_table) {
            std::cout << "class_id=" << entry.class_id
                     << ", method_id=" << entry.method_id
                     << " -> function_id=" << entry.function_id << "\n";
        }
        
        std::cout << "\nVirtual Field Table:\n";
        for (const auto& entry : vfield_table) {
            std::cout << "class_id=" << entry.class_id
                     << ", field_id=" << entry.field_id
                     << " -> field_index=" << entry.field_index << "\n";
        }
        
        std::cout << "\nConsts\n";
        for (int id = 0; id < (int)const_pool.size(); ++id) {
            std::cout << id << " " << parse_constant(const_pool[id]).to_string() << "\n";
        }
        std::cout << "\nCommands:\n";
        for (int i = 0; i < commands.size(); ++i) {
            std::cout << i << " " << (long long)(commands[i].code) << " " << (long long)(commands[i].arg) << "\n";
        }
        std::cout << "\n";
    }

    std::vector<Command> commands;
    std::vector<Constant> const_pool;
    std::unordered_map<size_t, FunctionTableEntry> func_table;
    std::vector<VMethodTableEntry> vmethod_table;
    std::vector<VFieldTableEntry> vfield_table;
    std::map<std::pair<int64_t, int64_t>, int64_t> vmethod_map;
    std::map<std::pair<int64_t, int64_t>, int64_t> vfield_map;
    std::unique_ptr<Profiler> profiler;
    std::vector<Owner<Entity>> heap = {};
    std::vector<StackFrame> stack_of_functions;
    std::vector<Reference<Entity>> operand_stack;
    GarbageCollector<Tag> garbage_collector;
};

#undef CHECK_STACK_EMPTY
#undef CHECK_REF
}
