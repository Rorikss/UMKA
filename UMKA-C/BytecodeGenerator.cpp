#include "BytecodeGenerator.h"
#include <fstream>
#include <iostream>

static void appendInt64(std::vector<uint8_t> &buf, int64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i * 8)) & 0xFF);
}

static void appendByte(std::vector<uint8_t> &buf, uint8_t b) {
    buf.push_back(b);
}

static void appendDouble(std::vector<uint8_t> &buf, double dv) {
    static_assert(sizeof(double) == 8);
    union {
        double d;
        uint8_t b[8];
    } u;
    u.d = dv;
    for (int i = 0; i < 8; i++) buf.push_back(u.b[i]);
}

void BytecodeGenerator::collectFunctions(const std::vector<Stmt *> &program) {
    userFuncIndex.clear();

    FunctionDefStmt *mainFunc = nullptr;
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt *>(s)) {
            if (fd->name == "main") {
                mainFunc = fd;
                break;
            }
        }
    }

    int64_t idx = 0;

    if (mainFunc) {
        userFuncIndex["main"] = idx++;
    }

    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt *>(s)) {
            if (fd->name == "main") continue; // <-- важно
            if (userFuncIndex.find(fd->name) == userFuncIndex.end()) {
                userFuncIndex[fd->name] = idx++;
            } else {
                std::cerr << "Warning: duplicate function name '" << fd->name << "'\n";
            }
        }
    }

    funcBuilders.clear();
    funcBuilders.resize(idx, FuncBuilder(&constPool));
    funcTable.clear();
    funcTable.resize(idx);
}


void BytecodeGenerator::buildFunctions(const std::vector<Stmt *> &program) {
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt *>(s)) {
            auto it = userFuncIndex.find(fd->name);
            if (it == userFuncIndex.end()) continue;
            int64_t fidx = it->second;
            FuncBuilder &fb = funcBuilders[fidx];
            fb.constPoolRef = &constPool;

            for (size_t i = 0; i < fd->params.size(); ++i) {
                fb.varIndex[fd->params[i]] = i;
            }
            fb.nextVarIndex = fd->params.size();

            genStmtInFunc(fd->body, fb);

            if (fb.code.empty() || fb.code.back() != OP_RETURN) {
                fb.emitReturn();
            }

            FunctionEntry fe;
            fe.argCount = fd->params.size();
            fe.localCount = fb.nextVarIndex;
            funcTable[fidx] = fe;
        }
    }
}

std::vector<uint8_t> BytecodeGenerator::concatenateFunctionCodes() {
    std::vector<uint8_t> finalCode;
    int64_t offset = 0;
    for (size_t i = 0; i < funcBuilders.size(); ++i) {
        FuncBuilder &fb = funcBuilders[i];
        fb.resolvePending();
        funcTable[i].codeOffset = offset;
        finalCode.insert(finalCode.end(), fb.code.begin(), fb.code.end());
        offset += fb.code.size();
    }
    return finalCode;
}

void BytecodeGenerator::generateAll(const std::vector<Stmt *> &program) {
    collectFunctions(program);
    buildFunctions(program);
    codeSection = concatenateFunctionCodes();
}

void BytecodeGenerator::writeToFile(const std::string &path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Cannot open " << path << " for writing\n";
        return;
    }

    uint8_t version = 1;
    f.write((char *) &version, 1);

    uint16_t constCount = (uint16_t) constPool.size();
    f.write((char *) &constCount, 2);

    uint16_t funcCount = (uint16_t) funcTable.size();
    f.write((char *) &funcCount, 2);

    uint32_t codeSize = (uint32_t) codeSection.size();
    f.write((char *) &codeSize, 4);

    for (auto &c: constPool) {
        uint8_t t = (uint8_t) c.type;
        f.write((char *) &t, 1);
        if (c.type == ConstEntry::INT) f.write((char *) &c.i, 8);
        else if (c.type == ConstEntry::DOUBLE) f.write((char *) &c.d, 8);
        else if (c.type == ConstEntry::STRING) {
            int64_t len = c.s.size();
            f.write((char *) &len, 8);
            if (len) f.write(c.s.data(), len);
        }
    }

    for (auto &fe: funcTable) {
        f.write((char *) &fe.codeOffset, 8);
        f.write((char *) &fe.argCount, 8);
        f.write((char *) &fe.localCount, 8);
    }

    if (!codeSection.empty()) f.write((char *) codeSection.data(), codeSection.size());
    f.close();

    std::cout << "Wrote bytecode: " << path
              << " (consts=" << constPool.size()
              << ", funcs=" << funcTable.size()
              << ", code=" << codeSection.size() << " bytes)\n";
}

void BytecodeGenerator::genExprInFunc(Expr *e, FuncBuilder &fb) {
    if (!e) return;

    if (auto ie = dynamic_cast<IntExpr *>(e)) {
        int64_t idx = fb.addConst(ConstEntry((int64_t) ie->v));
        fb.emitPushConstIndex(idx);
    } else if (auto de = dynamic_cast<DoubleExpr *>(e)) {
        int64_t idx = fb.addConst(ConstEntry((double) de->v));
        fb.emitPushConstIndex(idx);
    } else if (auto se = dynamic_cast<StringExpr *>(e)) {
        int64_t idx = fb.addConst(ConstEntry(se->s));
        fb.emitPushConstIndex(idx);
    } else if (auto be = dynamic_cast<BoolExpr *>(e)) {
        int64_t idx = fb.addConst(ConstEntry(be->b ? 1LL : 0LL));
        fb.emitPushConstIndex(idx);
    } else if (auto id = dynamic_cast<IdentExpr *>(e)) {
        auto it = fb.varIndex.find(id->name);
        if (it == fb.varIndex.end()) {
            std::cerr << "genExpr: unknown local var '" << id->name << "'\n";
            int64_t idx = fb.addConst(ConstEntry(0LL));
            fb.emitPushConstIndex(idx);
            return;
        }
        fb.emitLoad(it->second);
    } else if (auto arr = dynamic_cast<ArrayExpr *>(e)) {
        for (auto el: arr->elems) genExprInFunc(el, fb);
        int64_t countIdx = fb.addConst(ConstEntry((int64_t) arr->elems.size()));
        fb.emitBuildArr(countIdx);
    } else if (auto call = dynamic_cast<CallExpr *>(e)) {
        if (call->name == "to_int" ||
            call->name == "to_double" ||
            call->name == "to_string") {
            if (call->args.size() != 1) {
                std::cerr << "Cast '" << call->name << "' requires exactly 1 argument\n";
            }

            if (!call->args.empty())
                genExprInFunc(call->args[0], fb);

            if (call->name == "to_int") {
                fb.emitByte(OP_TO_INT);
            } else if (call->name == "to_double") {
                fb.emitByte(OP_TO_DOUBLE);
            } else {
                fb.emitByte(OP_TO_STRING);
            }
            return;
        }

        for (auto arg: call->args) genExprInFunc(arg, fb);

        auto itb = builtinIDs.find(call->name);
        if (itb != builtinIDs.end()) {
            fb.emitCall(itb->second);
        } else {
            auto itf = userFuncIndex.find(call->name);
            if (itf == userFuncIndex.end()) {
                std::cerr << "genExpr: call to unknown function '" << call->name << "'\n";
                fb.emitCall(-1);
            } else {
                fb.emitCall(itf->second);
            }
        }
    } else if (auto bex = dynamic_cast<BinaryExpr *>(e)) {
        genExprInFunc(bex->left, fb);
        genExprInFunc(bex->right, fb);
        auto it = BINOP_MAP.find(bex->op);
        if (it == BINOP_MAP.end()) {
            std::cerr << "genExpr: unknown binary op '" << bex->op << "'\n";
        } else {
            fb.emitByte(it->second);
        }
    } else if (auto ue = dynamic_cast<UnaryExpr *>(e)) {
        genExprInFunc(ue->rhs, fb);
        if (ue->op == '!') fb.emitByte(OP_NOT);
        else if (ue->op == '+') { /* noop */ }
        else if (ue->op == '-') {
            int64_t idx0 = fb.addConst(ConstEntry((int64_t) 0));
            fb.emitPushConstIndex(idx0);
            fb.emitByte(OP_SUB);
        } else {
            std::cerr << "genExpr: unknown unary op '" << ue->op << "'\n";
        }
    } else {
        std::cerr << "genExpr: unknown expr node\n";
    }
}

void BytecodeGenerator::genStmtInFunc(Stmt *s, FuncBuilder &fb) {
    if (!s) return;

    if (auto ls = dynamic_cast<LetStmt *>(s)) {
        int64_t idx = fb.nextVarIndex++;
        fb.varIndex[ls->name] = idx;
        genExprInFunc(ls->expr, fb);
        fb.emitStore(idx);
    } else if (auto as = dynamic_cast<AssignStmt *>(s)) {
        auto it = fb.varIndex.find(as->name);
        if (it == fb.varIndex.end()) {
            std::cerr << "Assign to unknown var '" << as->name << "'\n";
            genExprInFunc(as->expr, fb);
            int64_t idx0 = fb.addConst(ConstEntry((int64_t) 0));
            fb.emitPushConstIndex(idx0); // push 0 to maintain stack
            fb.emitByte(OP_POP);
            return;
        }
        genExprInFunc(as->expr, fb);
        fb.emitStore(it->second);
    } else if (auto es = dynamic_cast<ExprStmt *>(s)) {
        genExprInFunc(es->expr, fb);
        fb.emitByte(OP_POP);
    } else if (auto bs = dynamic_cast<BlockStmt *>(s)) {
        for (auto st: bs->stmts) genStmtInFunc(st, fb);
    } else if (auto is = dynamic_cast<IfStmt *>(s)) {
        std::string elseL = fb.newLabel();
        std::string endL = fb.newLabel();
        genExprInFunc(is->cond, fb);
        fb.emitJmpPlaceholder(OP_JMP_IF_FALSE, elseL);
        genStmtInFunc(is->thenb, fb);
        fb.emitJmpPlaceholder(OP_JMP, endL);
        fb.placeLabel(elseL);
        if (is->elseb) genStmtInFunc(is->elseb, fb);
        fb.placeLabel(endL);
    } else if (auto ws = dynamic_cast<WhileStmt *>(s)) {
        std::string startL = fb.newLabel();
        std::string endL = fb.newLabel();
        fb.placeLabel(startL);
        genExprInFunc(ws->cond, fb);
        fb.emitJmpPlaceholder(OP_JMP_IF_FALSE, endL);
        genStmtInFunc(ws->body, fb);
        fb.emitJmpPlaceholder(OP_JMP, startL);
        fb.placeLabel(endL);
    } else if (auto fs = dynamic_cast<ForStmt *>(s)) {
        if (fs->init) genStmtInFunc(fs->init, fb);
        std::string startL = fb.newLabel();
        std::string endL = fb.newLabel();
        fb.placeLabel(startL);
        if (fs->cond) genExprInFunc(fs->cond, fb);
        else {
            int64_t idx1 = fb.addConst(ConstEntry(1LL));
            fb.emitPushConstIndex(idx1);
        }
        fb.emitJmpPlaceholder(OP_JMP_IF_FALSE, endL);
        genStmtInFunc(fs->body, fb);
        if (fs->post) genStmtInFunc(fs->post, fb);
        fb.emitJmpPlaceholder(OP_JMP, startL);
        fb.placeLabel(endL);
    } else if (auto rs = dynamic_cast<ReturnStmt *>(s)) {
        if (rs->expr) genExprInFunc(rs->expr, fb);
        fb.emitReturn();
    } else if (auto fd = dynamic_cast<FunctionDefStmt *>(s)) {
        // processed in buildFunctions, ignore here
    } else {
        std::cerr << "genStmtInFunc: unknown stmt node\n";
    }
}
