#include "command_parser.h"
#include "../runtime/stack_machine.h"
#include <stdexcept>
#include <string>

void CommandParser::parse(std::istream& bytecode_stream) {
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
            case TYPE_INT64:
            case TYPE_DOUBLE:
                data_size = 8;
                break;
            case TYPE_STRING: {
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

    for (uint16_t i = 0; i < header.func_count; ++i) {
        FunctionTableEntry entry;
        bytecode_stream.read(reinterpret_cast<char*>(&entry.id), sizeof(uint64_t));
        bytecode_stream.read(reinterpret_cast<char*>(&entry.code_offset), sizeof(int64_t));
        bytecode_stream.read(reinterpret_cast<char*>(&entry.code_offset_end), sizeof(int64_t));
        bytecode_stream.read(reinterpret_cast<char*>(&entry.arg_count), sizeof(int64_t));
        bytecode_stream.read(reinterpret_cast<char*>(&entry.local_count), sizeof(int64_t));
        
        if (bytecode_stream.gcount() != sizeof(uint64_t) + 4*sizeof(int64_t)) {
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

const std::vector<Command>& CommandParser::get_commands() const { return commands; }
const std::vector<Constant>& CommandParser::get_const_pool() const { return const_pool; }
const std::vector<FunctionTableEntry>& CommandParser::get_func_table() const { return func_table; }

bool CommandParser::has_operand(uint8_t opcode) const {
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