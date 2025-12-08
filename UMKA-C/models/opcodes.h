#pragma once

#include <cstdint>

enum Opcode : uint8_t {
    OP_PUSH_CONST = 0x01,
    OP_POP = 0x02,
    OP_STORE = 0x03,
    OP_LOAD = 0x04,
    OP_ADD = 0x10,
    OP_SUB = 0x11,
    OP_MUL = 0x12,
    OP_DIV = 0x13,
    OP_REM = 0x14,
    OP_NOT = 0x17,
    OP_AND = 0x18,
    OP_OR  = 0x19,
    OP_EQ  = 0x1A,
    OP_NEQ = 0x1B,
    OP_GT  = 0x1C,
    OP_LT  = 0x1D,
    OP_GTE = 0x1E,
    OP_LTE = 0x1F,
    OP_JMP = 0x20,
    OP_JMP_IF_FALSE = 0x21,
    OP_JMP_IF_TRUE  = 0x22, // он нам пока не нужен
    OP_CALL = 0x23,
    OP_RETURN = 0x24,
    OP_BUILD_ARR = 0x30,
    OP_OPCOT = 0x40,
    OP_CALL_METHOD = 0x50,
    OP_GET_FIELD = 0x51,
    OP_TO_STRING = 0x60,
    OP_TO_DOUBLE = 0x61,
    OP_TO_INT = 0x62
};