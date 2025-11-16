#pragma once

#include "../model/model.h"
#include "stack_machine.h"
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>

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
    RETURN = 0x24,
    BUILD_ARR = 0x30,
    OPCOT = 0x40,
    TO_STRING = 0x60,
    TO_INT = 0x61,
    TO_DOUBLE = 0x62
};

class CommandParser {
public:
    struct BytecodeHeader {
        uint8_t version;
        uint16_t const_count;
        uint16_t func_count;
        uint32_t code_size;
    };

    void parse(const std::vector<uint8_t>& bytecode) {
        if (bytecode.size() < sizeof(BytecodeHeader)) {
            throw std::runtime_error("Invalid bytecode - too small for header");
        }

        // Parse header
        BytecodeHeader header;
        std::memcpy(&header, bytecode.data(), sizeof(BytecodeHeader));
        
        size_t index = sizeof(BytecodeHeader);
        commands.clear();
        const_pool.clear();
        func_table.clear();

        // Parse constant pool
        for (uint16_t i = 0; i < header.const_count; ++i) {
            if (index >= bytecode.size()) {
                throw std::runtime_error("Unexpected end of bytecode in constant pool");
            }

            Constant constant;
            constant.type = bytecode[index++];
            
            size_t data_size = 0;
            switch (constant.type) {
                case 0x01: // int64
                case 0x02: // double
                    data_size = 8;
                    break;
                case 0x03: // string
                    if (index + sizeof(int64_t) > bytecode.size()) {
                        throw std::runtime_error("Invalid string length in constant pool");
                    }
                    int64_t str_len;
                    std::memcpy(&str_len, &bytecode[index], sizeof(int64_t));
                    index += sizeof(int64_t);
                    data_size = str_len;
                    break;
                default:
                    throw std::runtime_error("Unknown constant type");
            }

            if (index + data_size > bytecode.size()) {
                throw std::runtime_error("Unexpected end of constant data");
            }

            constant.data.assign(bytecode.begin() + index, bytecode.begin() + index + data_size);
            index += data_size;
            const_pool.push_back(constant);
        }

        // Parse function table
        for (uint16_t i = 0; i < header.func_count; i++) {
            if (index + sizeof(FunctionTableEntry) > bytecode.size()) {
                throw std::runtime_error("Unexpected end of bytecode in function table");
            }

            FunctionTableEntry entry;
            std::memcpy(&entry, &bytecode[index], sizeof(FunctionTableEntry));
            index += sizeof(FunctionTableEntry);
            func_table.push_back(entry);
        }

        // Parse code section
        while (index < bytecode.size()) {
            uint8_t opcode = bytecode[index++];
            int64_t arg = 0;

            if (has_operand(opcode)) {
                if (index + sizeof(int64_t) > bytecode.size()) {
                    throw std::runtime_error("Missing operand for opcode");
                }
                std::memcpy(&arg, &bytecode[index], sizeof(int64_t));
                index += sizeof(int64_t);
            }
            
            commands.push_back(Command{static_cast<uint8_t>(opcode), arg});
        }
    }
    
    const std::vector<Command>& get_commands() const { return commands; }
    const std::vector<Constant>& get_const_pool() const { return const_pool; }
    const std::vector<FunctionTableEntry>& get_func_table() const { return func_table; }

private:
    bool has_operand(uint8_t opcode) const {
        switch(static_cast<OpCode>(opcode)) {
            case OpCode::PUSH_CONST:
            case OpCode::STORE:
            case OpCode::LOAD:
            case OpCode::JMP:
            case OpCode::JMP_IF_FALSE:
            case OpCode::JMP_IF_TRUE:
            case OpCode::CALL:
            case OpCode::RETURN:
            case OpCode::BUILD_ARR:
            case OpCode::OPCOT:
            case OpCode::TO_STRING:
            case OpCode::TO_INT:
            case OpCode::TO_DOUBLE:
                return true;
            default:
                return false;
        }
    }
    
    std::vector<Command> commands;
    std::vector<Constant> const_pool;
    std::vector<FunctionTableEntry> func_table;
};