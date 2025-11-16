#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <fstream>

#include "../build/parser.cpp"

// -------------------- OPCODES --------------------
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
    OP_JMP_IF_TRUE  = 0x22,

    OP_CALL = 0x23,
    OP_RETURN = 0x24,

    OP_BUILD_ARR = 0x30,

    OP_OPCOT = 0x40, // concat operator token from your list

    OP_TO_STRING = 0x60,
    OP_TO_DOUBLE = 0x61,
    OP_TO_INT    = 0x62
};

// map operator string to opcode for binary ops
static inline const std::unordered_map<std::string, uint8_t> BINOP_MAP = {
        {"+", (uint8_t)OP_ADD},
        {"-", (uint8_t)OP_SUB},
        {"*", (uint8_t)OP_MUL},
        {"/", (uint8_t)OP_DIV},
        {"%", (uint8_t)OP_REM},

        {"&&", (uint8_t)OP_AND},
        {"||", (uint8_t)OP_OR},

        {"==", (uint8_t)OP_EQ},
        {"!=", (uint8_t)OP_NEQ},
        {">", (uint8_t)OP_GT},
        {"<", (uint8_t)OP_LT},
        {">=", (uint8_t)OP_GTE},
        {"<=", (uint8_t)OP_LTE},

        {"^-^", (uint8_t)OP_OPCOT}
};

// -------------------- CONSTANT POOL ENTRY --------------------
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

// -------------------- FUNCTION TABLE ENTRY --------------------
struct FunctionEntry {
    int64_t codeOffset{0};
    int64_t argCount{0};
    int64_t localCount{0};
};

// -------------------- Bytecode Generator --------------------
class BytecodeGenerator {
public:
    // global constant pool
    std::vector<ConstEntry> constPool;

    // function table (final)
    std::vector<FunctionEntry> funcTable;

    // mapping function name -> user function index (0..N-1)
    std::unordered_map<std::string, int64_t> userFuncIndex;

    // builtin IDs (как ты дал; используем int64 max values)
    static inline const std::unordered_map<std::string, int64_t> builtinIDs = {
            {"print",      9223372036854775807LL},
            {"len",        9223372036854775806LL},
            {"get",        9223372036854775805LL},
            {"set",        9223372036854775804LL},
            {"add",        9223372036854775803LL},
            {"remove",     9223372036854775802LL},
            {"to_string",  9223372036854775801LL},
            {"write",      9223372036854775800LL},
            {"read",       9223372036854775799LL},
            {"to_double",  9223372036854775798LL},
            {"to_int",     9223372036854775797LL}
    };

    // --- helpers for writing little-endian values into buffers ---
    static void appendInt64(std::vector<uint8_t>& buf, int64_t v) {
        for (int i = 0; i < 8; ++i) buf.push_back((uint8_t)((v >> (i*8)) & 0xFF));
    }
    static void appendByte(std::vector<uint8_t>& buf, uint8_t b) {
        buf.push_back(b);
    }
    static void appendDouble(std::vector<uint8_t>& buf, double dv) {
        static_assert(sizeof(double) == 8);
        union { double d; uint8_t b[8]; } u;
        u.d = dv;
        for (int i=0;i<8;i++) buf.push_back(u.b[i]);
    }

    // ---------------- Per-function builder ----------------
    struct FuncBuilder {
        std::vector<uint8_t> code; // instructions for this function
        std::unordered_map<std::string, size_t> labelPos;
        struct PendingJump { size_t pos; std::string label; uint8_t opcode; };
        std::vector<PendingJump> pending;
        std::unordered_map<std::string,int64_t> varIndex; // local var index (including args)
        int64_t nextVarIndex = 0;
        int labelCounter = 0;

        // reference to global constPool to add constants
        std::vector<ConstEntry>* constPoolRef = nullptr;

        FuncBuilder() = default;
        FuncBuilder(std::vector<ConstEntry>* pool) : constPoolRef(pool) {}

        std::string newLabel() {
            return std::string("L") + std::to_string(labelCounter++);
        }

        void placeLabel(const std::string& name) {
            labelPos[name] = code.size();
        }

        void emitByte(uint8_t b) { code.push_back(b); }
        void emitInt64(int64_t v) { appendInt64(code, v); }

        // add constant to shared pool (return its index)
        int64_t addConst(const ConstEntry& c) {
            // linear search duplicate elimination; could be optimized with map
            auto &pool = *constPoolRef;
            for (size_t i=0;i<pool.size();++i) {
                const auto &e = pool[i];
                if (e.type != c.type) continue;
                if (e.type == ConstEntry::INT && e.i == c.i) return (int64_t)i;
                if (e.type == ConstEntry::DOUBLE && e.d == c.d) return (int64_t)i;
                if (e.type == ConstEntry::STRING && e.s == c.s) return (int64_t)i;
            }
            pool.push_back(c);
            return (int64_t)pool.size() - 1;
        }

        void emitPushConstIndex(int64_t constIdx) {
            emitByte((uint8_t)OP_PUSH_CONST);
            emitInt64(constIdx);
        }

        void emitLoad(int64_t varIdx) {
            emitByte((uint8_t)OP_LOAD);
            emitInt64(varIdx);
        }

        void emitStore(int64_t varIdx) {
            emitByte((uint8_t)OP_STORE);
            emitInt64(varIdx);
        }

        void emitCall(int64_t funcId) {
            emitByte((uint8_t)OP_CALL);
            emitInt64(funcId);
        }

        void emitReturn() {
            emitByte((uint8_t)OP_RETURN);
        }

        void emitBuildArr(int64_t constIdx) {
            emitByte((uint8_t)OP_BUILD_ARR);
            emitInt64(constIdx);
        }

        // emit a jump placeholder; writes opcode + 8 zero bytes, records pending
        void emitJmpPlaceholder(uint8_t opcode, const std::string& label) {
            emitByte(opcode);
            size_t pos = code.size();
            for (int i=0;i<8;++i) code.push_back(0);
            pending.push_back({pos, label, opcode});
        }

        void resolvePending() {
            for (auto &pj : pending) {
                auto it = labelPos.find(pj.label);
                if (it == labelPos.end()) {
                    std::cerr << "Unresolved label in func: " << pj.label << "\n";
                    continue;
                }
                int64_t target = (int64_t) it->second;
                // offset is relative to the end of the int64 operand
                int64_t offset = target - (int64_t)(pj.pos + 8);
                for (int i=0;i<8;i++) {
                    code[pj.pos + i] = (uint8_t)((offset >> (i*8)) & 0xFF);
                }
            }
            pending.clear();
        }

        // helpers to push small ints/doubles/strings as constants and emit PUSH_CONST
        void pushInt(int64_t v) {
            int64_t ci = addConst(ConstEntry(v));
            emitPushConstIndex(ci);
        }
        void pushDouble(double d) {
            int64_t ci = addConst(ConstEntry(d));
            emitPushConstIndex(ci);
        }
        void pushString(const std::string &s) {
            int64_t ci = addConst(ConstEntry(s));
            emitPushConstIndex(ci);
        }

        // For debugging: dump code bytes
        void dumpHex(std::ostream &out) {
            for (auto b : code) {
                char buf[4];
                sprintf(buf, "%02X ", b);
                out << buf;
            }
            out << "\n";
        }
    };

    // ---------------- BytecodeGenerator state ----------------
    // builders for each user function in order of indices
    std::vector<FuncBuilder> funcBuilders;

    // ---------------- constructor ----------------
    BytecodeGenerator() = default;

    // ---------------- top-level process ----------------

    // 1) scan program_stmts to collect function names and assign indices
    void collectFunctions(const std::vector<Stmt*>& program) {
        int64_t idx = 0;
        for (auto s : program) {
            if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
                userFuncIndex[fd->name] = idx++;
            }
        }
        // prepare funcBuilders
        funcBuilders.clear();
        funcBuilders.resize(userFuncIndex.size(), FuncBuilder(&constPool));
        // also set funcTable placeholders
        funcTable.clear();
        funcTable.resize(userFuncIndex.size());
    }

    // 2) For each function, generate its code into separate FuncBuilder
    void buildFunctions(const std::vector<Stmt*>& program) {
        // traverse program; for each FunctionDefStmt build its code
        for (auto s : program) {
            if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
                auto it = userFuncIndex.find(fd->name);
                if (it == userFuncIndex.end()) continue;
                int64_t fidx = it->second;
                FuncBuilder &fb = funcBuilders[fidx];
                fb.constPoolRef = &constPool;

                // initialize parameter locals: params get indices 0..argCount-1
                for (size_t i = 0; i < fd->params.size(); ++i) {
                    fb.varIndex[fd->params[i]] = (int64_t)i;
                }
                fb.nextVarIndex = (int64_t)fd->params.size();

                // mark function start (we will set codeOffset later when concatenating)
                // generate body into fb.code
                genStmtInFunc(fd->body, fb);

                // ensure function ends with RETURN; if last instruction not RETURN, emit one
                if (fb.code.empty() || fb.code.back() != OP_RETURN) {
                    fb.emitReturn();
                }

                // after building, record argCount and localCount in funcTable (codeOffset filled later)
                FunctionEntry fe;
                fe.argCount = (int64_t)fd->params.size();
                fe.localCount = fb.nextVarIndex; // includes args + local variables allocated
                funcTable[fidx] = fe;
            }
        }
    }

    // 3) Optionally generate non-function top-level code? In your examples main is a function.
    // We'll rely on 'main' function to be present and be the entry point for VM.
    // If needed, can implement a top-level wrapper function.

    // 4) Link function codes into one code section and fill codeOffset in funcTable
    std::vector<uint8_t> concatenateFunctionCodes() {
        std::vector<uint8_t> finalCode;
        int64_t offset = 0;
        for (size_t i = 0; i < funcBuilders.size(); ++i) {
            FuncBuilder &fb = funcBuilders[i];
            // resolve labels inside function
            fb.resolvePending();
            // set codeOffset for this function
            funcTable[i].codeOffset = offset;
            // append bytes
            finalCode.insert(finalCode.end(), fb.code.begin(), fb.code.end());
            offset += (int64_t)fb.code.size();
        }
        return finalCode;
    }

    // ---------------- generation helpers for statements/expressions inside a function ----------------

    // generate expression into function builder fb
    void genExprInFunc(Expr* e, FuncBuilder &fb) {
        if (!e) return;
        if (auto ie = dynamic_cast<IntExpr*>(e)) {
            fb.pushInt((int64_t)ie->v);
        } else if (auto de = dynamic_cast<DoubleExpr*>(e)) {
            fb.pushDouble((double)de->v);
        } else if (auto se = dynamic_cast<StringExpr*>(e)) {
            fb.pushString(std::string(se->s)); // se->s is char* in parser? careful
            // Note: In your parser you used char* sval stored in YYSTYPE then wrapped in StringExpr(string($1))
            // so dynamic type probably stores std::string; adjust if needed.
        } else if (auto be = dynamic_cast<BoolExpr*>(e)) {
            fb.pushInt(be->b ? 1LL : 0LL);
        } else if (auto id = dynamic_cast<IdentExpr*>(e)) {
            auto it = fb.varIndex.find(id->name);
            if (it == fb.varIndex.end()) {
                std::cerr << "genExpr: unknown local var '" << id->name << "'\n";
                // To avoid crash: push 0
                fb.pushInt(0);
                return;
            }
            fb.emitLoad(it->second);
        } else if (auto arr = dynamic_cast<ArrayExpr*>(e)) {
            // push elements left-to-right
            for (auto el : arr->elems) genExprInFunc(el, fb);
            int64_t countIdx = fb.addConst(ConstEntry((int64_t)arr->elems.size()));
            fb.emitBuildArr(countIdx);
        } else if (auto call = dynamic_cast<CallExpr*>(e)) {
            // args: push left-to-right
            for (auto arg : call->args) genExprInFunc(arg, fb);
            // determine function id: builtin or user
            // builtin?
            auto itb = builtinIDs.find(call->name);
            if (itb != builtinIDs.end()) {
                fb.emitCall(itb->second);
            } else {
                // user function
                auto itf = userFuncIndex.find(call->name);
                if (itf == userFuncIndex.end()) {
                    std::cerr << "genExpr: call to unknown function '" << call->name << "'\n";
                    fb.emitCall(-1);
                } else {
                    fb.emitCall(itf->second);
                }
            }
        } else if (auto bex = dynamic_cast<BinaryExpr*>(e)) {
            // left then right
            genExprInFunc(bex->left, fb);
            genExprInFunc(bex->right, fb);
            auto it = BINOP_MAP.find(bex->op);
            if (it == BINOP_MAP.end()) {
                std::cerr << "genExpr: unknown binary op '" << bex->op << "'\n";
            } else {
                fb.emitByte(it->second);
            }
        } else if (auto ue = dynamic_cast<UnaryExpr*>(e)) {
            genExprInFunc(ue->rhs, fb);
            if (ue->op == '!') fb.emitByte(OP_NOT);
            else if (ue->op == '+') { /* noop */ }
            else if (ue->op == '-') {
                // 0 - x
                int64_t c0 = fb.addConst(ConstEntry((int64_t)0));
                fb.emitPushConstIndex(c0);
                fb.emitByte(OP_SUB);
            } else {
                std::cerr << "genExpr: unknown unary op '" << ue->op << "'\n";
            }
        } else {
            std::cerr << "genExpr: unknown expr node\n";
        }
    }

    // generate statement inside a function
    void genStmtInFunc(Stmt* s, FuncBuilder &fb) {
        if (!s) return;
        if (auto ls = dynamic_cast<LetStmt*>(s)) {
            // give new local index
            int64_t idx = fb.nextVarIndex++;
            fb.varIndex[ls->name] = idx;
            // generate rhs
            genExprInFunc(ls->expr, fb);
            fb.emitStore(idx);
        } else if (auto as = dynamic_cast<AssignStmt*>(s)) {
            auto it = fb.varIndex.find(as->name);
            if (it == fb.varIndex.end()) {
                std::cerr << "Assign to unknown var '" << as->name << "'\n";
                // still generate expr to maintain stack
                genExprInFunc(as->expr, fb);
                // discard
                fb.emitByte(OP_POP);
                return;
            }
            genExprInFunc(as->expr, fb);
            fb.emitStore(it->second);
        } else if (auto es = dynamic_cast<ExprStmt*>(s)) {
            genExprInFunc(es->expr, fb);
            fb.emitByte(OP_POP);
        } else if (auto bs = dynamic_cast<BlockStmt*>(s)) {
            for (auto st : bs->stmts) genStmtInFunc(st, fb);
        } else if (auto is = dynamic_cast<IfStmt*>(s)) {
            std::string elseL = fb.newLabel();
            std::string endL = fb.newLabel();
            genExprInFunc(is->cond, fb);
            fb.emitJmpPlaceholder((uint8_t)OP_JMP_IF_FALSE, elseL);
            genStmtInFunc(is->thenb, fb);
            fb.emitJmpPlaceholder((uint8_t)OP_JMP, endL);
            fb.placeLabel(elseL);
            if (is->elseb) genStmtInFunc(is->elseb, fb);
            fb.placeLabel(endL);
        } else if (auto ws = dynamic_cast<WhileStmt*>(s)) {
            std::string startL = fb.newLabel();
            std::string endL = fb.newLabel();
            fb.placeLabel(startL);
            genExprInFunc(ws->cond, fb);
            fb.emitJmpPlaceholder((uint8_t)OP_JMP_IF_FALSE, endL);
            genStmtInFunc(ws->body, fb);
            fb.emitJmpPlaceholder((uint8_t)OP_JMP, startL);
            fb.placeLabel(endL);
        } else if (auto fs = dynamic_cast<ForStmt*>(s)) {
            // for(init; cond; post) body
            // init is Stmt*, cond Expr*, post Stmt*
            if (fs->init) genStmtInFunc(fs->init, fb);
            std::string startL = fb.newLabel();
            std::string endL = fb.newLabel();
            fb.placeLabel(startL);
            if (fs->cond) genExprInFunc(fs->cond, fb);
            else fb.pushInt(1); // if no cond -> true
            fb.emitJmpPlaceholder((uint8_t)OP_JMP_IF_FALSE, endL);
            genStmtInFunc(fs->body, fb);
            if (fs->post) genStmtInFunc(fs->post, fb);
            fb.emitJmpPlaceholder((uint8_t)OP_JMP, startL);
            fb.placeLabel(endL);
        } else if (auto rs = dynamic_cast<ReturnStmt*>(s)) {
            if (rs->expr) genExprInFunc(rs->expr, fb);
            fb.emitReturn();
        } else if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
            // function definitions are processed separately in buildFunctions
            // ignore here
        } else {
            std::cerr << "genStmtInFunc: unknown stmt node\n";
        }
    }

    // public: main entry to generate bytecode from program_stmts
    void generateAll(const std::vector<Stmt*>& program) {
        // 1) collect user functions
        collectFunctions(program);

        // 2) build each function body into funcBuilders
        buildFunctions(program);

        // 3) concatenate codes and fill funcTable offsets
        std::vector<uint8_t> finalCode = concatenateFunctionCodes();

        // Save final code bytes in a member buffer for writing
        codeSection = std::move(finalCode);
    }

    // write binary file according to your format
    void writeToFile(const std::string &path) {
        std::ofstream f(path, std::ios::binary);
        if (!f) {
            std::cerr << "Cannot open " << path << " for writing\n";
            return;
        }

        // version
        uint8_t version = 1;
        f.write((char*)&version, 1);

        // const_count (uint16_t)
        uint16_t constCount = (uint16_t)constPool.size();
        f.write((char*)&constCount, 2);

        // func_count (uint16_t)
        uint16_t funcCount = (uint16_t)funcTable.size();
        f.write((char*)&funcCount, 2);

        // code_size (uint32_t)
        uint32_t codeSize = (uint32_t)codeSection.size();
        f.write((char*)&codeSize, 4);

        // write constant pool entries
        for (auto &c : constPool) {
            // type
            uint8_t t = (uint8_t)c.type;
            f.write((char*)&t, 1);
            if (c.type == ConstEntry::INT) {
                int64_t v = c.i;
                f.write((char*)&v, 8);
            } else if (c.type == ConstEntry::DOUBLE) {
                double dv = c.d;
                f.write((char*)&dv, 8);
            } else if (c.type == ConstEntry::STRING) {
                // store length as int64 then bytes (consistent)
                int64_t len = (int64_t)c.s.size();
                f.write((char*)&len, 8);
                if (len) f.write(c.s.data(), (std::streamsize)len);
            }
        }

        // write function table entries
        for (auto &fe : funcTable) {
            f.write((char*)&fe.codeOffset, 8);
            f.write((char*)&fe.argCount, 8);
            f.write((char*)&fe.localCount, 8);
        }

        // write code section
        if (!codeSection.empty()) f.write((char*)codeSection.data(), codeSection.size());

        f.close();
        std::cout << "Wrote bytecode: " << path << " (consts=" << constPool.size() << ", funcs=" << funcTable.size() << ", code=" << codeSection.size() << " bytes)\n";
    }

private:
    // internal storage of final code bytes
    std::vector<uint8_t> codeSection;

    // convenience: push int constant from builder level when feeding top-level
    // (used in ForStmt if cond missing)
    void pushInt(int64_t v) {
        // not used directly; builders use their own addConst which references shared constPool
    }

};

