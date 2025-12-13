#pragma once

#include "../model/model.h"
#include <cstdint>
#include <vector>
#include <istream>
#include <unordered_map>

namespace umka::vm {
enum OpCode : uint8_t {
    PUSH_CONST = 0x01,
    POP = 0x02,
    STORE = 0x03,
    LOAD = 0x04,
    ADD = 0x10,
    SUB = 0x11,
    MUL = 0x12,
    DIV = 0x13,
    REM = 0x14,
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
    CALL_METHOD = 0x50,
    GET_FIELD = 0x51,
    TO_STRING = 0x60,
    TO_INT = 0x61,
    TO_DOUBLE = 0x62,
};

class CommandParser {
public:
    struct BytecodeHeader {
        uint8_t version;
        uint16_t const_count;
        uint16_t func_count;
        uint32_t code_size;
        uint16_t vmethod_count;
        uint16_t vfield_count;
    } __attribute__((packed));

    void parse(std::istream& bytecode_stream);

    std::vector<Command>&& extract_commands();
    std::vector<Constant>&& extract_const_pool();
    std::unordered_map<size_t, FunctionTableEntry>&& extract_func_table();
    std::vector<VMethodTableEntry>&& extract_vmethod_table();
    std::vector<VFieldTableEntry>&& extract_vfield_table();

    const std::vector<Command>& get_commands() const;
    const std::vector<Constant>& get_const_pool() const;
    const std::unordered_map<size_t, FunctionTableEntry>& get_func_table() const;
    const std::vector<VMethodTableEntry>& get_vmethod_table() const;
    const std::vector<VFieldTableEntry>& get_vfield_table() const;

private:
    bool has_operand(uint8_t opcode) const;
    
    std::vector<Command> commands;
    std::vector<Constant> const_pool;
    std::unordered_map<size_t, FunctionTableEntry> func_table;
    std::vector<VMethodTableEntry> vmethod_table;
    std::vector<VFieldTableEntry> vfield_table;
};
}
