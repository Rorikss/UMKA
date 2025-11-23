#include "bytecode_generator.h"
#include <fstream>
#include <iostream>
#include <cstring>

static void append_int_64(std::vector<uint8_t>& buf, int64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i * 8)) & 0xFF);
}

static void append_byte(std::vector<uint8_t>& buf, uint8_t b) {
    buf.push_back(b);
}

static void append_double(std::vector<uint8_t>& buf, double dv) {
    static_assert(sizeof(double) == 8);

    uint64_t bits;
    std::memcpy(&bits, &dv, sizeof(double));

    for (int i = 0; i < 8; ++i) {
        buf.push_back((bits >> (i * 8)) & 0xFF);
    }
}

void BytecodeGenerator::collect_functions(const std::vector<Stmt*>& program) {
    userFuncIndex.clear();

    FunctionDefStmt* mainFunc = nullptr;
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s); fd->name == "main") {
            mainFunc = fd;
            break;
        }
    }

    int64_t idx = 0;

    if (mainFunc) {
        userFuncIndex["main"] = idx++;
    }

    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
            if (fd->name == "main") continue;
            if (userFuncIndex.find(fd->name) == userFuncIndex.end()) {
                userFuncIndex[fd->name] = ++idx;
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


void BytecodeGenerator::build_functions(const std::vector<Stmt*>& program) {
    for (auto s: program) {
        if (!dynamic_cast<FunctionDefStmt*>(s)) continue;
        auto fd = static_cast<FunctionDefStmt*>(s);

        auto it = userFuncIndex.find(fd->name);
        if (it == userFuncIndex.end()) continue;
        int64_t fidx = it->second;
        FuncBuilder& fb = funcBuilders[fidx];
        //fb.constPoolRef = &constPool;

        for (size_t i = 0; i < fd->params.size(); ++i) {
            fb.var_index[fd->params[i]] = i;
        }
        fb.nextVarIndex = fd->params.size();

        gen_stmt_in_func(fd->body, fb);

        if (fb.code.empty() || fb.code.back() != OP_RETURN) {
            fb.emit_return();
        }

        FunctionEntry fe;
        fe.arg_count = fd->params.size();
        fe.local_count = fb.nextVarIndex;
        funcTable[fidx] = fe;
    }
}

std::vector<uint8_t> BytecodeGenerator::concatenate_function_codes() {
    std::vector<uint8_t> finalCode;
    int64_t offset = 0;
    for (size_t i = 0; i < funcBuilders.size(); ++i) {
        FuncBuilder& fb = funcBuilders[i];
        fb.resolve_pending();
        funcTable[i].code_offset_beg = offset;
        finalCode.insert(finalCode.end(), fb.code.begin(), fb.code.end());
        funcTable[i].code_offset_end = offset + fb.code.size();
        offset += fb.code.size();
    }
    return finalCode;
}

void BytecodeGenerator::generate_all(const std::vector<Stmt*>& program) {
    collect_functions(program);
    build_functions(program);
    codeSection = concatenate_function_codes();
}

void BytecodeGenerator::write_to_file(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << path << " for writing\n";
        return;
    }

    uint8_t version = 1;
    file.write((char*)& version, sizeof(version));

    auto constCount = (uint16_t) constPool.size();
    file.write((char*)& constCount, sizeof(constCount));

    auto funcCount = (uint16_t) funcTable.size();
    file.write((char*)& funcCount, sizeof(funcCount));

    auto codeSize = (uint32_t) codeSection.size();
    file.write((char*)& codeSize, sizeof(codeSize));

    for (auto& c: constPool) {
        auto t = (uint8_t) c.type;
        file.write((char*)& t, sizeof(t));
        if (c.type == ConstEntry::INT){
            file.write((char*)& c._int, sizeof(c._int));
        }
        else if (c.type == ConstEntry::DOUBLE){
            file.write((char*)& c._double, sizeof(c._double));
        }
        else if (c.type == ConstEntry::STRING) {
            int64_t len = c._str.size();
            file.write((char*)& len, sizeof(len));
            if (len) file.write(c._str.data(), len);
        }
    }

    for (auto& fe: funcTable) {
        file.write((char*)& fe.code_offset_beg, sizeof(fe.code_offset_beg));
        file.write((char*)& fe.code_offset_end, sizeof(fe.code_offset_end));
        file.write((char*)& fe.arg_count, sizeof(fe.arg_count));
        file.write((char*)& fe.local_count, sizeof(fe.local_count));
    }

    if (!codeSection.empty()) file.write((char*) codeSection.data(), codeSection.size());

    std::cout << "Wrote bytecode: " << path
              << " (consts=" << constPool.size()
              << ", funcs=" << funcTable.size()
              << ", code=" << codeSection.size() << " bytes)\n";
}

void BytecodeGenerator::gen_expr_in_func(Expr* expr, FuncBuilder& fb) {
    if (!expr) return;

    if (auto ie = dynamic_cast<IntExpr*>(expr)) {
        int64_t idx = fb.add_const(ConstEntry((int64_t) ie->v));
        fb.emit_push_const_index(idx);
    } else if (auto de = dynamic_cast<DoubleExpr*>(expr)) {
        int64_t idx = fb.add_const(ConstEntry((double) de->v));
        fb.emit_push_const_index(idx);
    } else if (auto se = dynamic_cast<StringExpr*>(expr)) {
        int64_t idx = fb.add_const(ConstEntry(se->s));
        fb.emit_push_const_index(idx);
    } else if (auto be = dynamic_cast<BoolExpr*>(expr)) {
        int64_t idx = fb.add_const(ConstEntry(be->b ? 1LL : 0LL));
        fb.emit_push_const_index(idx);
    } else if (auto id = dynamic_cast<IdentExpr*>(expr)) {
        auto it = fb.var_index.find(id->name);
        if (it == fb.var_index.end()) {
            std::cerr << "genExpr: unknown local var '" << id->name << "'\n";
            int64_t idx = fb.add_const(ConstEntry(0LL));
            fb.emit_push_const_index(idx);
            return;
        }
        fb.emit_load(it->second);
    } else if (auto arr = dynamic_cast<ArrayExpr*>(expr)) {
        for (auto el: arr->elems) gen_expr_in_func(el, fb);
        int64_t countIdx = fb.add_const(ConstEntry((int64_t) arr->elems.size()));
        fb.emit_build_arr(countIdx);
    } else if (auto call = dynamic_cast<CallExpr*>(expr)) {
        if (call->name == "to_int" ||
            call->name == "to_double" ||
            call->name == "to_string") {
            if (call->args.size() != 1) {
                std::cerr << "Cast '" << call->name << "' requires exactly 1 argument\n";
            }

            if (!call->args.empty()){
                gen_expr_in_func(call->args[0], fb);
            }

            if (call->name == "to_int") {
                fb.emit_byte(OP_TO_INT);
            } else if (call->name == "to_double") {
                fb.emit_byte(OP_TO_DOUBLE);
            } else {
                fb.emit_byte(OP_TO_STRING);
            }
            return;
        }

        for (auto arg: call->args) gen_expr_in_func(arg, fb);

        auto itb = builtinIDs.find(call->name);
        if (itb != builtinIDs.end()) {
            fb.emit_call(itb->second);
        } else {
            auto itf = userFuncIndex.find(call->name);
            if (itf == userFuncIndex.end()) {
                std::cerr << "genExpr: call to unknown function '" << call->name << "'\n";
                fb.emit_call(-1);
            } else {
                fb.emit_call(itf->second);
            }
        }
    } else if (auto bex = dynamic_cast<BinaryExpr*>(expr)) {
        gen_expr_in_func(bex->left, fb);
        gen_expr_in_func(bex->right, fb);
        auto it = BINOP_MAP.find(bex->op);
        if (it == BINOP_MAP.end()) {
            std::cerr << "genExpr: unknown binary op '" << bex->op << "'\n";
        } else {
            fb.emit_byte(it->second);
        }
    } else if (auto ue = dynamic_cast<UnaryExpr*>(expr)) {
        gen_expr_in_func(ue->rhs, fb);
        if (ue->op == '!') fb.emit_byte(OP_NOT);
        else if (ue->op == '+') { /* noop */ }
        else if (ue->op == '-') {
            int64_t idx0 = fb.add_const(ConstEntry((int64_t) 0));
            fb.emit_push_const_index(idx0);
            fb.emit_byte(OP_SUB);
        } else {
            std::cerr << "genExpr: unknown unary op '" << ue->op << "'\n";
        }
    } else {
        std::cerr << "genExpr: unknown expr node\n";
    }
}

void BytecodeGenerator::gen_stmt_in_func(Stmt* s, FuncBuilder& fb) {
    if (!s) return;

    if (auto ls = dynamic_cast<LetStmt*>(s)) {
        int64_t idx = ++fb.nextVarIndex;
        fb.var_index[ls->name] = idx;
        gen_expr_in_func(ls->expr, fb);
        fb.emit_store(idx);
    } else if (auto as = dynamic_cast<AssignStmt*>(s)) {
        auto it = fb.var_index.find(as->name);
        if (it == fb.var_index.end()) {
            std::cerr << "Assign to unknown var '" << as->name << "'\n";
            gen_expr_in_func(as->expr, fb);
            int64_t idx0 = fb.add_const(ConstEntry((int64_t) 0));
            fb.emit_push_const_index(idx0); // push 0 to maintain stack
            fb.emit_byte(OP_POP);
            return;
        }
        gen_expr_in_func(as->expr, fb);
        fb.emit_store(it->second);
    } else if (auto es = dynamic_cast<ExprStmt*>(s)) {
        gen_expr_in_func(es->expr, fb);
        fb.emit_byte(OP_POP);
    } else if (auto bs = dynamic_cast<BlockStmt*>(s)) {
        for (auto st: bs->stmts) gen_stmt_in_func(st, fb);
    } else if (auto is = dynamic_cast<IfStmt*>(s)) {
        std::string elseL = fb.new_label();
        std::string endL = fb.new_label();
        gen_expr_in_func(is->cond, fb);
        fb.emit_jmp_place_holder(OP_JMP_IF_FALSE, elseL);
        gen_stmt_in_func(is->thenb, fb);
        fb.emit_jmp_place_holder(OP_JMP, endL);
        fb.place_label(elseL);
        if (is->elseb) gen_stmt_in_func(is->elseb, fb);
        fb.place_label(endL);
    } else if (auto ws = dynamic_cast<WhileStmt*>(s)) {
        std::string startL = fb.new_label();
        std::string endL = fb.new_label();
        fb.place_label(startL);
        gen_expr_in_func(ws->cond, fb);
        fb.emit_jmp_place_holder(OP_JMP_IF_FALSE, endL);
        gen_stmt_in_func(ws->body, fb);
        fb.emit_jmp_place_holder(OP_JMP, startL);
        fb.place_label(endL);
    } else if (auto fs = dynamic_cast<ForStmt*>(s)) {
        if (fs->init) gen_stmt_in_func(fs->init, fb);
        std::string startL = fb.new_label();
        std::string endL = fb.new_label();
        fb.place_label(startL);
        if (fs->cond) gen_expr_in_func(fs->cond, fb);
        else {
            int64_t idx1 = fb.add_const(ConstEntry(1LL));
            fb.emit_push_const_index(idx1);
        }
        fb.emit_jmp_place_holder(OP_JMP_IF_FALSE, endL);
        gen_stmt_in_func(fs->body, fb);
        if (fs->post) gen_stmt_in_func(fs->post, fb);
        fb.emit_jmp_place_holder(OP_JMP, startL);
        fb.place_label(endL);
    } else if (auto rs = dynamic_cast<ReturnStmt*>(s)) {
        if (rs->expr) gen_expr_in_func(rs->expr, fb);
        fb.emit_return();
    } else if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
        // processed in build_functions, ignore here
    } else {
        std::cerr << "gen_stmt_in_func: unknown stmt node\n";
    }
}
