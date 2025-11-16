#pragma once

#include "../model/model.h"
#include "stack_machine.h"
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <istream>
#include <memory>

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

    void parse(std::istream& bytecode_stream) {
        BytecodeHeader header;
        bytecode_stream.read(reinterpret_cast<char*>(&header), sizeof(BytecodeHeader));
        if (bytecode_stream.gcount() != sizeof(BytecodeHeader)) {
            throw std::runtime_error("Invalid bytecode - too small for header");
        }
        
        commands.clear();
        const_pool.clear();
        func_table.clear();

        for (uint16_t i = 0; i < header.const_count; ++i) {
            Constant constant;
            bytecode_stream.read(reinterpret_cast<char*>(&constant.type), 1);
            if (bytecode_stream.gcount() != 1) {
                throw std::runtime_error("Unexpected end of bytecode in constant pool");
            }
            
            size_t data_size = 0;
            switch (constant.type) {
                case 0x01: // int64
                case 0x02: // double
                    data_size = 8;
                    break;
                case 0x03: { // string
                    int64_t str_len;
                    bytecode_stream.read(reinterpret_cast<char*>(&str_len), sizeof(int64_t));
                    if (bytecode_stream.gcount() != sizeof(int64_t)) {
                        throw std::runtime_error("Invalid string length in constant pool");
                    }
                    data_size = str_len;
                    break;
                }
                default:
                    throw std::runtime_error("Unknown constant type");
            }

            constant.data.resize(data_size);
            bytecode_stream.read(reinterpret_cast<char*>(constant.data.data()), data_size);
            if (bytecode_stream.gcount() != static_cast<std::streamsize>(data_size)) {
                throw std::runtime_error("Unexpected end of constant data");
            }
            
            const_pool.push_back(constant);
        }

        for (uint16_t i = 0; i < header.func_count; i++) {
            FunctionTableEntry entry;
            bytecode_stream.read(reinterpret_cast<char*>(&entry), sizeof(FunctionTableEntry));
            if (bytecode_stream.gcount() != sizeof(FunctionTableEntry)) {
                throw std::runtime_error("Unexpected end of bytecode in function table");
            }
            func_table.push_back(entry);
        }

        while (bytecode_stream.peek() != EOF) {
            uint8_t opcode;
            bytecode_stream.read(reinterpret_cast<char*>(&opcode), 1);
            if (bytecode_stream.gcount() != 1) break;
            
            int64_t arg = 0;
            if (has_operand(opcode)) {
                bytecode_stream.read(reinterpret_cast<char*>(&arg), sizeof(int64_t));
                if (bytecode_stream.gcount() != sizeof(int64_t)) {
                    throw std::runtime_error("Missing operand for opcode");
                }
            }
            
            commands.push_back(Command{opcode, arg});
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