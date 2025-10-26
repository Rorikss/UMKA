#pragma once

#include <vector>
#include <cstdint>
#include "../model/model.h"

class CommandParser {
public:
    void parse(const std::vector<uint8_t>& bytecode) {
        size_t index = 0;
        commands.clear();
        
        while (index < bytecode.size()) {
            uint8_t opcode = bytecode[index++];
            int64_t arg = 0;

            if (has_operand(opcode) && index + sizeof(int64_t) <= bytecode.size()) {
                std::memcpy(&arg, &bytecode[index], sizeof(int64_t));
                index += sizeof(int64_t);
            }
            
            commands.push_back(Command{static_cast<int64_t>(opcode), arg});
        }
    }
    
    const std::vector<Command>& get_commands() const {
        return commands;
    }

private:
    bool has_operand(uint8_t opcode) const {
        switch(opcode) {
            case 0x01: // PUSH_CONST
            case 0x03: // STORE
            case 0x04: // LOAD
            case 0x20: // JMP
            case 0x21: // JMP_IF_FALSE
            case 0x22: // JMP_IF_TRUE
            case 0x23: // CALL
            case 0x30: // BUILD_ARR
            case 0x40: // OPCOT
                return true;
            default:
                return false;
        }
    }
    
    std::vector<Command> commands;
};