#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "../build/parser.hpp"
#include "ast.h"

enum Opcode : uint8_t {
    OP_PUSH_CONST = 0x01,
    OP_POP        = 0x02,
    OP_STORE      = 0x03,
    OP_LOAD       = 0x04,
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
    OP_TO_STRING = 0x60,
    OP_TO_DOUBLE = 0x61,
    OP_TO_INT    = 0x62
};

struct ConstEntry {
    enum Type : uint8_t { INT = 1, DOUBLE = 2, STRING = 3 } type;
    int64_t i{};
    double d{};
    std::string s;

    ConstEntry(int64_t v)  : type(INT), i(v) {}
    ConstEntry(double v)   : type(DOUBLE), d(v) {}
    ConstEntry(const std::string& ss) : type(STRING), s(ss) {}
    ConstEntry() = default;
};

struct FunctionEntry {
    int64_t codeOffset{0};
    int64_t argCount{0};
    int64_t localCount{0};
};

class BytecodeGenerator {
public:
    std::vector<ConstEntry> constPool;
    std::vector<FunctionEntry> funcTable;
    std::unordered_map<std::string, int64_t> userFuncIndex;

    static inline const std::unordered_map<std::string, int64_t> builtinIDs = {
            {"print",      9223372036854775807LL},
            {"len",        9223372036854775806LL},
            {"get",        9223372036854775805LL},
            {"set",        9223372036854775804LL},
            {"add",        9223372036854775803LL},
            {"remove",     9223372036854775802LL},
            {"write",      9223372036854775800LL},
            {"read",       9223372036854775799LL}
    };

    static inline const std::unordered_map<std::string, uint8_t> BINOP_MAP = {
            {"+", OP_ADD}, {"-", OP_SUB}, {"*", OP_MUL}, {"/", OP_DIV}, {"%", OP_REM},
            {"&&", OP_AND}, {"||", OP_OR},
            {"==", OP_EQ}, {"!=", OP_NEQ}, {">", OP_GT}, {"<", OP_LT}, {">=", OP_GTE}, {"<=", OP_LTE},
            {"^-^", OP_OPCOT}
    };

    struct FuncBuilder {
        std::vector<uint8_t> code;
        std::unordered_map<std::string, size_t> labelPos;
        struct PendingJump { size_t pos; std::string label; uint8_t opcode; };
        std::vector<PendingJump> pending;
        std::unordered_map<std::string,int64_t> varIndex;
        int64_t nextVarIndex = 0;
        int labelCounter = 0;
        std::vector<ConstEntry>* constPoolRef = nullptr;

        FuncBuilder() = default;
        FuncBuilder(std::vector<ConstEntry>* pool) : constPoolRef(pool) {}

        std::string newLabel() { return "L" + std::to_string(labelCounter++); }
        void placeLabel(const std::string& name) { labelPos[name] = code.size(); }

        void emitByte(uint8_t b) { code.push_back(b); }
        void emitInt64(int64_t v) {
            for (int i = 0; i < 8; ++i) code.push_back((v >> (i*8)) & 0xFF);
        }

        int64_t addConst(const ConstEntry& c) {
            auto &pool = *constPoolRef;
            for (size_t i = 0; i < pool.size(); ++i) {
                const auto &e = pool[i];
                if (e.type != c.type) continue;
                if (e.type == ConstEntry::INT && e.i == c.i) return i;
                if (e.type == ConstEntry::DOUBLE && e.d == c.d) return i;
                if (e.type == ConstEntry::STRING && e.s == c.s) return i;
            }
            pool.push_back(c);
            return pool.size() - 1;
        }

        void emitPushConstIndex(int64_t idx) { emitByte(OP_PUSH_CONST); emitInt64(idx); }
        void emitLoad(int64_t idx) { emitByte(OP_LOAD); emitInt64(idx); }
        void emitStore(int64_t idx) { emitByte(OP_STORE); emitInt64(idx); }
        void emitCall(int64_t id) { emitByte(OP_CALL); emitInt64(id); }
        void emitReturn() { emitByte(OP_RETURN); }
        void emitBuildArr(int64_t idx) { emitByte(OP_BUILD_ARR); emitInt64(idx); }
        void emitJmpPlaceholder(uint8_t opcode, const std::string& label) {
            emitByte(opcode);
            size_t pos = code.size();
            for(int i=0;i<8;i++) code.push_back(0);
            pending.push_back({pos,label,opcode});
        }
        void resolvePending() {
            for(auto &pj : pending) {
                auto it = labelPos.find(pj.label);
                if(it==labelPos.end()) continue;
                int64_t target = it->second;
                int64_t offset = target - (int64_t)(pj.pos + 8);
                for(int i=0;i<8;i++) code[pj.pos+i] = (offset >> (i*8)) & 0xFF;
            }
            pending.clear();
        }
    };

    std::vector<FuncBuilder> funcBuilders;
    std::vector<uint8_t> codeSection;

    BytecodeGenerator() = default;

    void generateAll(const std::vector<Stmt*>& program);
    void writeToFile(const std::string& path);

private:
    void collectFunctions(const std::vector<Stmt*>& program);
    void buildFunctions(const std::vector<Stmt*>& program);
    std::vector<uint8_t> concatenateFunctionCodes();
    void genStmtInFunc(Stmt* s, FuncBuilder &fb);
    void genExprInFunc(Expr* e, FuncBuilder &fb);
};