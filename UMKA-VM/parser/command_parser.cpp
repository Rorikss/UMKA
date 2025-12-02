#include "command_parser.h"
#include "../runtime/stack_machine.h"
#include <stdexcept>
#include <string>

void CommandParser::parse(std::istream& bytecode_stream) {
    BytecodeHeader header;
    bytecode_stream.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    if (bytecode_stream.gcount() != sizeof(header.version)) {
        throw std::runtime_error("Invalid bytecode - too small for header");
    }
    bytecode_stream.read(reinterpret_cast<char*>(&header.const_count), sizeof(header.const_count));
    if (bytecode_stream.gcount() != sizeof(header.const_count)) {
        throw std::runtime_error("Invalid bytecode - too small for header");
    }
    bytecode_stream.read(reinterpret_cast<char*>(&header.func_count), sizeof(header.func_count));
    if (bytecode_stream.gcount() != sizeof(header.func_count)) {
        throw std::runtime_error("Invalid bytecode - too small for header");
    }
    bytecode_stream.read(reinterpret_cast<char*>(&header.code_size), sizeof(header.code_size));
    if (bytecode_stream.gcount() != sizeof(header.code_size)) {
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
                throw std::runtime_error("Unknown constant type" + std::to_string(constant.type));
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
        entry.id = i;
        int64_t code_offset_beg;
        bytecode_stream.read(reinterpret_cast<char*>(&code_offset_beg), sizeof(int64_t));
        if (bytecode_stream.gcount() != sizeof(int64_t)) {
            throw std::runtime_error("Unexpected end of bytecode in function table");
        }
        bytecode_stream.read(reinterpret_cast<char*>(&entry.code_offset_end), sizeof(int64_t));
        if (bytecode_stream.gcount() != sizeof(int64_t)) {
            throw std::runtime_error("Unexpected end of bytecode in function table");
        }
        bytecode_stream.read(reinterpret_cast<char*>(&entry.arg_count), sizeof(int64_t));
        if (bytecode_stream.gcount() != sizeof(int64_t)) {
            throw std::runtime_error("Unexpected end of bytecode in function table");
        }
        bytecode_stream.read(reinterpret_cast<char*>(&entry.local_count), sizeof(int64_t));
        if (bytecode_stream.gcount() != sizeof(int64_t)) {
            throw std::runtime_error("Unexpected end of bytecode in function table");
        }
        
        entry.code_offset = code_offset_beg;
        
        func_table[entry.id] = entry;
    }

    size_t bytes_read = 0;
    while (bytes_read < header.code_size) {
        if (bytecode_stream.peek() == EOF) {
            throw std::runtime_error("Unexpected end of bytecode in code section");
        }
        
        uint8_t opcode;
        bytecode_stream.read(reinterpret_cast<char*>(&opcode), 1);
        if (bytecode_stream.gcount() != 1) {
            throw std::runtime_error("Unexpected end of bytecode in code section");
        }
        bytes_read += 1;
        
        int64_t arg = 0;
        if (has_operand(opcode)) {
            if (bytes_read + sizeof(int64_t) > header.code_size) {
                throw std::runtime_error("Missing operand for opcode");
            }
            bytecode_stream.read(reinterpret_cast<char*>(&arg), sizeof(int64_t));
            if (bytecode_stream.gcount() != sizeof(int64_t)) {
                throw std::runtime_error("Unexpected end of bytecode in code section");
            }
            bytes_read += sizeof(int64_t);
        }
        
        commands.push_back(Command{opcode, arg});
    }
}

const std::vector<Command>& CommandParser::get_commands() const { return commands; }
const std::vector<Constant>& CommandParser::get_const_pool() const { return const_pool; }
const std::unordered_map<size_t, FunctionTableEntry>& CommandParser::get_func_table() const { return func_table; }

bool CommandParser::has_operand(uint8_t opcode) const {
    switch(static_cast<OpCode>(opcode)) {
        case OpCode::PUSH_CONST:
        case OpCode::STORE:
        case OpCode::LOAD:
        case OpCode::JMP:
        case OpCode::JMP_IF_FALSE:
        case OpCode::JMP_IF_TRUE:
        case OpCode::CALL:
        case OpCode::BUILD_ARR:
        case OpCode::OPCOT:
        case OpCode::REM:
            return true;
        default:
            return false;
    }
}