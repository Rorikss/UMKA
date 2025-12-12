#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include "entries.h"

namespace umka::compiler {
struct FuncBuilder {
    std::vector<uint8_t> code;
    std::vector<size_t> instruction_positions;
    std::unordered_map<std::string, size_t> label_pos;
    struct PendingJump {
        size_t pos;
        std::string label;
        uint8_t opcode;
    };
    std::vector<PendingJump> pending;
    std::unordered_map<std::string, int64_t> var_index;
    std::unordered_map<std::string, std::string> var_types;
    int64_t nextVarIndex = 0;
    int labelCounter = 0;
    std::vector<ConstEntry> *constPoolRef = nullptr;

    FuncBuilder() = default;

    FuncBuilder(std::vector<ConstEntry> *pool) : constPoolRef(pool) {}

    std::string new_label() { return "L" + std::to_string(labelCounter++); }

    void place_label(const std::string& name) { label_pos[name] = code.size(); }

    void emit_byte(uint8_t b) {
        instruction_positions.push_back(code.size());
        code.push_back(b);
    }

    void emit_int64(int64_t v) {
        for (int i = 0; i < 8; ++i) code.push_back((v >> (i * 8)) & 0xFF);
    }

    int64_t add_const(const ConstEntry& c) {
        auto& pool = *constPoolRef;
        for (size_t i = 0; i < pool.size(); ++i) {
            const auto& e = pool[i];
            if (e.type != c.type) continue;
            if (e.type == ConstEntry::INT && e._int == c._int) return i;
            if (e.type == ConstEntry::DOUBLE && e._double == c._double) return i;
            if (e.type == ConstEntry::STRING && e._str == c._str) return i;
            if (e.type == ConstEntry::UNIT) return i;
        }
        pool.push_back(c);
        return pool.size() - 1;
    }

    void emit_push_const_index(int64_t idx) {
        emit_byte(OP_PUSH_CONST);
        emit_int64(idx);
    }

    void emit_load(int64_t idx) {
        emit_byte(OP_LOAD);
        emit_int64(idx);
    }

    void emit_store(int64_t idx) {
        emit_byte(OP_STORE);
        emit_int64(idx);
    }

    void emit_call(int64_t id) {
        emit_byte(OP_CALL);
        emit_int64(id);
    }

    void emit_return() { emit_byte(OP_RETURN); }

    void emit_build_arr(int64_t idx) {
        emit_byte(OP_BUILD_ARR);
        emit_int64(idx);
    }

    void emit_jmp_place_holder(uint8_t opcode, const std::string& label) {
        size_t current_pos = instruction_positions.size();
        emit_byte(opcode);
        emit_int64(0);
        pending.push_back({current_pos, label, opcode});
    }

    void resolve_pending() {
        for (auto& pj: pending) {
            auto it = label_pos.find(pj.label);
            if (it == label_pos.end()) continue;
            
            size_t target_idx = std::distance(
                instruction_positions.begin(),
                std::lower_bound(instruction_positions.begin(),
                               instruction_positions.end(),
                               it->second)
            );
            
            int64_t offset = target_idx - (pj.pos + 1);
            size_t arg_pos = instruction_positions[pj.pos] + 1;
            
            for (int i = 0; i < 8; i++) {
                code[arg_pos + i] = (offset >> (i * 8)) & 0xFF;
            }
        }
        pending.clear();
    }
};
}
