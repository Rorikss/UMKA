#pragma once
#include "../model/model.h"
#include "../model/command_parser.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <stack>
#include <stdexcept>

class StackMachine {
public:
    StackMachine(const std::vector<Command>& cmds) : commands(cmds) {} 


    void run() {
        size_t program_counter = 0;
        while (program_counter < commands.size()) {
            execute_command(commands[program_counter], program_counter);
            program_counter++;
        }
    }

private:
    void execute_command(const Command& cmd, size_t& op_code) {
        switch (cmd.code) {
            case 0x01: // PUSH_CONST
                // TODO
                break;
            case 0x02: // POP
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                operand_stack.pop();
                break;
            case 0x03: // STORE
                // TODO
                break;
            case 0x04: // LOAD
                // TODO
                break;
            case 0x10: // ADD
                if (operand_stack.size() < 2) throw std::runtime_error("Not enough operands for ADD");
                {
                    auto [lhs, rhs] = get_operands_from_stack();
                    // TODO
                }
                break;
            case 0x11: // SUB
                if (operand_stack.size() < 2) throw std::runtime_error("Not enough operands for SUB");
                {
                    auto [lhs, rhs] = get_operands_from_stack();
                    // TODO
                }
                break;
            case 0x12: // MUL
                if (operand_stack.size() < 2) throw std::runtime_error("Not enough operands for MUL");
                {
                    auto [lhs, rhs] = get_operands_from_stack();
                    // TODO
                }
                break;
            case 0x13: // DIV
                if (operand_stack.size() < 2) throw std::runtime_error("Not enough operands for DIV");
                {
                    auto [lhs, rhs] = get_operands_from_stack();
                    // TODO
                }
                break;
            case 0x14: // MOD
                if (operand_stack.size() < 2) throw std::runtime_error("Not enough operands for MOD");
                {
                    auto [lhs, rhs] = get_operands_from_stack();
                    // TODO
                }
                break;
            case 0x20: // JMP
                op_code = cmd.arg - 1; // -1 because pc will increment after this
                break;
            case 0x21: // JMP_IF_FALSE
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                // TODO
                operand_stack.pop();
                break;
            case 0x22: // JMP_IF_TRUE
                if (operand_stack.empty()) throw std::runtime_error("Stack underflow");
                // TODO
                operand_stack.pop();
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
        Entity lhs = operand_stack.top();
        operand_stack.pop();
        Entity rhs = operand_stack.top();
        operand_stack.pop();
        return {lhs, rhs};
    }

    std::vector<Command> commands;
    std::vector<std::shared_ptr<Entity>> heap = {};
    std::vector<StackFrame> stack_of_functions = {};
    std::stack<Entity> operand_stack;
};
