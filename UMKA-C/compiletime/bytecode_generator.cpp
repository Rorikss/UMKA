#include "bytecode_generator.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>

namespace umka::compiler {
static void append_byte(std::vector<uint8_t>& buf, uint8_t b) {
    buf.push_back(b);
}

static void append_uint16(std::vector<uint8_t>& buf, uint16_t v) {
    for (int i = 0; i < 2; ++i) buf.push_back((v >> (i * 8)) & 0xFF);
}

static void append_uint32(std::vector<uint8_t>& buf, uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back((v >> (i * 8)) & 0xFF);
}

static void append_int64(std::vector<uint8_t>& buf, int64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i * 8)) & 0xFF);
}

static void append_double(std::vector<uint8_t>& buf, double dv) {
    static_assert(sizeof(double) == 8);

    uint64_t bits;
    std::memcpy(&bits, &dv, sizeof(double));

    for (int i = 0; i < 8; ++i) {
        buf.push_back((bits >> (i * 8)) & 0xFF);
    }
}


void BytecodeGenerator::build_function_common(FuncBuilder& fb, const std::vector<std::string>& params, Stmt* body) {
    for (size_t i = 0; i < params.size(); ++i) {
        fb.var_index[params.at(i)] = i;
    }
    fb.nextVarIndex = params.size();
    gen_stmt_in_func(body, fb);

    if (fb.code.empty() || fb.code.back() != OP_RETURN) {
        int64_t idx = fb.add_const(ConstEntry());
        fb.emit_push_const_index(idx);
        fb.emit_return();
    }
}

void BytecodeGenerator::build_functions(const std::vector<Stmt*>& program) {
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
            auto it = userFuncIndex.find(fd->name);
            if (it == userFuncIndex.end()) continue;
            int64_t fidx = it->second;
            FuncBuilder& fb = funcBuilders.at(fidx);

            build_function_common(fb, fd->params, fd->body);

            FunctionEntry fe;
            fe.arg_count = fd->params.size();
            fe.local_count = fb.nextVarIndex;
            funcTable.at(fidx) = fe;
        } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
            std::string methodFullName = md->class_name + "$" + md->method_name;
            auto it = userFuncIndex.find(methodFullName);
            if (it == userFuncIndex.end()) continue;
            int64_t fidx = it->second;
            FuncBuilder& fb = funcBuilders.at(fidx);

            build_function_common(fb, md->params, md->body);

            FunctionEntry fe;
            fe.arg_count = md->params.size();
            fe.local_count = fb.nextVarIndex;
            funcTable.at(fidx) = fe;
        }
    }
}

void BytecodeGenerator::collect_functions(const std::vector<Stmt*>& program) {
    userFuncIndex.clear();
    classFieldIndices.clear();
    classFieldCount.clear();
    classFieldDefaults.clear();
    classIDs.clear();
    methodIDs.clear();
    fieldIDs.clear();
    vmethodTable.clear();
    vfieldTable.clear();

    int64_t nextMethodID = 0;
    int64_t nextFieldID = 0;
    
    for (auto s: program) {
        if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
            for (auto fieldStmt: cd->fields) {
                if (auto ls = dynamic_cast<LetStmt*>(fieldStmt)) {
                    if (fieldIDs.find(ls->name) == fieldIDs.end()) {
                        fieldIDs[ls->name] = nextFieldID++;
                    }
                }
            }
        } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
            // Collect method names
            if (methodIDs.find(md->method_name) == methodIDs.end()) {
                methodIDs[md->method_name] = nextMethodID++;
            }
        }
    }

    int64_t nextClassID = 0;
    for (auto s: program) {
        if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
            std::unordered_map<std::string, int64_t> fieldIndices;
            std::unordered_map<std::string, Expr*> fieldDefaults;
            int64_t fieldCount = 0;
            
            int64_t classID = nextClassID++;
            classIDs[cd->name] = classID;

            for (auto fieldStmt: cd->fields) {
                if (auto ls = dynamic_cast<LetStmt*>(fieldStmt)) {
                    int64_t fieldIndex = fieldCount + 1;
                    fieldIndices[ls->name] = fieldIndex;
                    fieldDefaults[ls->name] = ls->expr;
                    fieldCount++;
                    
                    int64_t fieldID = fieldIDs[ls->name];
                    vfieldTable.push_back(std::make_tuple(classID, fieldID, fieldIndex));
                } else {
                    std::cerr << "Warning: class field must be a let statement\n";
                }
            }
            
            classFieldIndices[cd->name] = fieldIndices;
            classFieldCount[cd->name] = fieldCount;
            classFieldDefaults[cd->name] = fieldDefaults;
        }
    }


    std::vector<Stmt*> topLevelStatements;
    FunctionDefStmt* mainFunc = nullptr;
    
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
            if (fd->name == "main") {
                mainFunc = fd;
            }
        } else if (dynamic_cast<ClassDefStmt*>(s) || dynamic_cast<MethodDefStmt*>(s)) {
            continue;
        } else {
            topLevelStatements.push_back(s);
        }
    }
    
    if (!topLevelStatements.empty()) {
        if (!mainFunc) {
            auto block = new BlockStmt(topLevelStatements);
            mainFunc = new FunctionDefStmt("main", {}, "unit", block);
            program_stmts.push_back(mainFunc);
        } else {
            if (auto block = dynamic_cast<BlockStmt*>(mainFunc->body)) {
                block->stmts.insert(block->stmts.begin(),
                                   topLevelStatements.begin(),
                                   topLevelStatements.end());
            }
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
                userFuncIndex[fd->name] = idx++;
            } else {
                std::cerr << "Warning: duplicate function name '" << fd->name << "'\n";
            }
        } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
            std::string methodFullName = md->class_name + "$" + md->method_name;
            if (userFuncIndex.find(methodFullName) == userFuncIndex.end()) {
                int64_t functionID = idx++;
                userFuncIndex[methodFullName] = functionID;
                
                auto classIDIt = classIDs.find(md->class_name);
                auto methodIDIt = methodIDs.find(md->method_name);
                if (classIDIt != classIDs.end() && methodIDIt != methodIDs.end()) {
                    vmethodTable.push_back(std::make_tuple(
                        classIDIt->second,
                        methodIDIt->second,
                        functionID
                    ));
                }
            } else {
                std::cerr << "Warning: duplicate method name '" << methodFullName << "'\n";
            }
        } else if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
            continue;
        }
    }

    funcBuilders.clear();
    funcBuilders.resize(idx, FuncBuilder(&constPool));
    funcTable.clear();
    funcTable.resize(idx);
}


std::vector<uint8_t> BytecodeGenerator::concatenate_function_codes() {
    std::vector<uint8_t> finalCode;
    int64_t instruction_offset = 0;
    for (size_t i = 0; i < funcBuilders.size(); ++i) {
        FuncBuilder& fb = funcBuilders.at(i);
        fb.resolve_pending();
        
        int64_t function_instruction_count = fb.instruction_positions.size();
        
        funcTable.at(i).instruction_offset_beg = instruction_offset;
        finalCode.insert(finalCode.end(), fb.code.begin(), fb.code.end());
        funcTable.at(i).instruction_offset_end = instruction_offset + function_instruction_count;
        instruction_offset += function_instruction_count;
    }
    return finalCode;
}

void BytecodeGenerator::generate_all(const std::vector<Stmt*>& program) {
    collect_functions(program);
    build_functions(program);
    codeSection = concatenate_function_codes();
}

void BytecodeGenerator::write_to_file(const std::string& path) {
    std::vector<uint8_t> buffer;

    append_byte(buffer, 1);
    append_uint16(buffer, (uint16_t)constPool.size());
    append_uint16(buffer, (uint16_t)funcTable.size());
    append_uint32(buffer, (uint32_t)codeSection.size());
    append_uint16(buffer, (uint16_t)vmethodTable.size());
    append_uint16(buffer, (uint16_t)vfieldTable.size());

    for (auto& c : constPool) {
        append_byte(buffer, (uint8_t)c.type);

        switch (c.type) {
            case ConstEntry::INT:
                append_int64(buffer, c._int);
                break;
            case ConstEntry::DOUBLE:
                append_double(buffer, c._double);
                break;
            case ConstEntry::STRING:
                append_int64(buffer, c._str.size());
                buffer.insert(buffer.end(), c._str.begin(), c._str.end());
                break;
            case ConstEntry::UNIT:
                break;
        }
    }

    for (auto& fe : funcTable) {
        append_int64(buffer, fe.instruction_offset_beg);
        append_int64(buffer, fe.instruction_offset_end);
        append_int64(buffer, fe.arg_count);
        append_int64(buffer, fe.local_count);
    }

    for (const auto& [class_id, method_id, function_id] : vmethodTable) {
        append_int64(buffer, class_id);
        append_int64(buffer, method_id);
        append_int64(buffer, function_id);
    }

    for (const auto& [class_id, field_id, field_index] : vfieldTable) {
        append_int64(buffer, class_id);
        append_int64(buffer, field_id);
        append_int64(buffer, field_index);
    }

    buffer.insert(buffer.end(), codeSection.begin(), codeSection.end());

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << path << " for writing\n";
        return;
    }

    if (!buffer.empty()) {
        file.write((char*)buffer.data(), buffer.size());
    }

    std::cout << "Wrote bytecode: " << path
              << " (consts=" << constPool.size()
              << ", funcs=" << funcTable.size()
              << ", vmethods=" << vmethodTable.size()
              << ", vfields=" << vfieldTable.size()
              << ", code=" << codeSection.size() << " bytes)\n";
}

void BytecodeGenerator::emit_push_zero_const(FuncBuilder& fb) {
    int64_t idx = fb.add_const(ConstEntry((int64_t) 0));
    fb.emit_push_const_index(idx);
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
        int64_t idx = fb.add_const(ConstEntry((int64_t) be->b));
        fb.emit_push_const_index(idx);
    } else if (auto ue = dynamic_cast<UnitExpr*>(expr)) {
        int64_t idx = fb.add_const(ConstEntry());
        fb.emit_push_const_index(idx);
    } else if (auto id = dynamic_cast<IdentExpr*>(expr)) {
        auto classIt = classFieldCount.find(id->name);
        if (classIt != classFieldCount.end()) {
            gen_class_instantiation(id->name, fb);
        } else {
            auto it = fb.var_index.find(id->name);
            if (it == fb.var_index.end()) {
                std::cerr << "genExpr: unknown local var '" << id->name << "'\n";
                emit_push_zero_const(fb);
                return;
            }
            fb.emit_load(it->second);
        }
    } else if (auto arr = dynamic_cast<ArrayExpr*>(expr)) {
        for (auto el: arr->elems) gen_expr_in_func(el, fb);
        fb.emit_build_arr(arr->elems.size());
    } else if (auto call = dynamic_cast<CallExpr*>(expr)) {
        static const std::unordered_map<std::string, uint8_t> castOps = {
            {"to_int", OP_TO_INT},
            {"to_double", OP_TO_DOUBLE},
            {"to_string", OP_TO_STRING}
        };
        
        auto castIt = castOps.find(call->name);
        if (castIt != castOps.end()) {
            if (call->args.size() != 1) {
                std::cerr << "Cast '" << call->name << "' requires exactly 1 argument\n";
            }
            if (!call->args.empty()) {
                gen_expr_in_func(call->args.at(0), fb);
            }
            fb.emit_byte(castIt->second);
            return;
        }

        for (auto arg: call->args | std::views::reverse) gen_expr_in_func(arg, fb);

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
        gen_expr_in_func(bex->right, fb);
        gen_expr_in_func(bex->left, fb);
        auto it = BINOP_MAP.find(bex->op);
        if (it == BINOP_MAP.end()) {
            std::cerr << "genExpr: unknown binary op '" << bex->op << "'\n";
        } else {
            fb.emit_byte(it->second);
        }
    } else if (auto ue = dynamic_cast<UnaryExpr*>(expr)) {
        switch (ue->op) {
            gen_expr_in_func(ue->rhs, fb);
            case '!':
                fb.emit_byte(OP_NOT);
                break;
            case '+':
                break;
            case '-':
                emit_push_zero_const(fb);
                fb.emit_byte(OP_SUB);
                break;
            default:
                std::cerr << "genExpr: unknown unary op '" << ue->op << "'\n";
                break;
        }
    } else if (auto fa = dynamic_cast<FieldAccessExpr*>(expr)) {
        gen_field_access_expr(fa, fb);
    } else if (auto ma = dynamic_cast<MemberAccessExpr*>(expr)) {
        gen_member_access_expr(ma, fb);
    } else if (auto mc = dynamic_cast<MethodCallExpr*>(expr)) {
        gen_method_call_expr(mc, fb);
    } else {
        std::cerr << "genExpr: unknown expr node\n";
    }
}

void BytecodeGenerator::gen_stmt_in_func(Stmt* s, FuncBuilder& fb) {
    if (!s) return;

    if (auto ls = dynamic_cast<LetStmt*>(s)) {
        int64_t idx = ++fb.nextVarIndex;
        fb.var_index[ls->name] = idx;
        
        if (auto idExpr = dynamic_cast<IdentExpr*>(ls->expr)) {
            auto classIt = classFieldCount.find(idExpr->name);
            if (classIt != classFieldCount.end()) {
                fb.var_types[ls->name] = idExpr->name;
                gen_class_instantiation(idExpr->name, fb);
            } else {
                gen_expr_in_func(ls->expr, fb);
            }
        } else {
            gen_expr_in_func(ls->expr, fb);
        }
        
        fb.emit_store(idx);
    } else if (auto as = dynamic_cast<AssignStmt*>(s)) {
        auto it = fb.var_index.find(as->name);
        if (it == fb.var_index.end()) {
            std::cerr << "Assign to unknown var '" << as->name << "'\n";
            gen_expr_in_func(as->expr, fb);
            emit_push_zero_const(fb);
            fb.emit_byte(OP_POP);
            return;
        }
        gen_expr_in_func(as->expr, fb);
        fb.emit_store(it->second);
    } else if (auto es = dynamic_cast<ExprStmt*>(s)) {
        
        gen_expr_in_func(es->expr, fb);
        fb.emit_byte(OP_POP);
    } else if (auto bs = dynamic_cast<BlockStmt*>(s)) {

        for (auto st: bs->stmts) { 
            gen_stmt_in_func(st, fb); 
        }
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
        
        if (fs->cond) {
            gen_expr_in_func(fs->cond, fb);
        } else {
            int64_t idx1 = fb.add_const(ConstEntry((int64_t) 1));
            fb.emit_push_const_index(idx1);
        }
        
        fb.emit_jmp_place_holder(OP_JMP_IF_FALSE, endL);
        gen_stmt_in_func(fs->body, fb);
        if (fs->post) gen_stmt_in_func(fs->post, fb);
        fb.emit_jmp_place_holder(OP_JMP, startL);
        fb.place_label(endL);
    } else if (auto rs = dynamic_cast<ReturnStmt*>(s)) {
        if (rs->expr) {
            gen_expr_in_func(rs->expr, fb);
        } else {
            int64_t idx = fb.add_const(ConstEntry());
            fb.emit_push_const_index(idx);
        }
        fb.emit_return();
    } else if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
    } else if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
    } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
    } else if (auto mas = dynamic_cast<MemberAssignStmt*>(s)) {
        gen_member_assign_stmt(mas, fb);
    } else {
        std::cerr << "gen_stmt_in_func: unknown stmt node\n";
    }
}

int64_t BytecodeGenerator::get_field_id_or_error(const std::string& fieldName, FuncBuilder& fb) {
    auto fieldIDIt = fieldIDs.find(fieldName);
    if (fieldIDIt == fieldIDs.end()) {
        std::cerr << "Field access to unknown field '" << fieldName << "'\n";
        emit_push_zero_const(fb);
        return -1;
    }
    return fieldIDIt->second;
}

int64_t BytecodeGenerator::get_method_id_or_error(const std::string& methodName) {
    auto methodIDIt = methodIDs.find(methodName);
    if (methodIDIt == methodIDs.end()) {
        std::cerr << "Method call to unknown method '" << methodName << "'\n";
        return -1;
    }
    return methodIDIt->second;
}

void BytecodeGenerator::gen_field_access_expr(FieldAccessExpr* expr, FuncBuilder& fb) {
    gen_expr_in_func(expr->target, fb);
    
    int64_t field_id = get_field_id_or_error(expr->field, fb);
    if (field_id == -1) return;
    
    fb.emit_byte(OP_GET_FIELD);
    fb.emit_int64(field_id);
}

void BytecodeGenerator::gen_member_access_expr(MemberAccessExpr* expr, FuncBuilder& fb) {
    auto it = fb.var_index.find(expr->object_name);
    if (it == fb.var_index.end()) {
        std::cerr << "Member access to unknown object '" << expr->object_name << "'\n";
        emit_push_zero_const(fb);
        return;
    }
    fb.emit_load(it->second);
    
    int64_t field_id = get_field_id_or_error(expr->field, fb);
    if (field_id == -1) return;
    
    fb.emit_byte(OP_GET_FIELD);
    fb.emit_int64(field_id);
}

void BytecodeGenerator::gen_method_call_expr(MethodCallExpr* expr, FuncBuilder& fb) { 
    for (auto arg: expr->args | std::views::reverse) gen_expr_in_func(arg, fb);
    gen_expr_in_func(expr->target, fb);

    int64_t method_id = get_method_id_or_error(expr->method_name);
    
    fb.emit_byte(OP_CALL_METHOD);
    fb.emit_int64(method_id);
}

void BytecodeGenerator::gen_member_assign_stmt(MemberAssignStmt* stmt, FuncBuilder& fb) {
    auto it = fb.var_index.find(stmt->object_name);
    if (it == fb.var_index.end()) {
        std::cerr << "Member assignment to unknown object '" << stmt->object_name << "'\n";
        return;
    }
    fb.emit_load(it->second);
    
    int64_t field_id = get_field_id_or_error(stmt->field, fb);
    if (field_id == -1) return;
    
    fb.emit_byte(OP_SET_FIELD);
    fb.emit_int64(field_id);
    
    gen_expr_in_func(stmt->expr, fb);
}

void BytecodeGenerator::gen_class_instantiation(const std::string& className, FuncBuilder& fb) {
    auto countIt = classFieldCount.find(className);
    int64_t fieldCount = (countIt != classFieldCount.end()) ? countIt->second : 0;

    auto defaultsIt = classFieldDefaults.find(className);
    if (defaultsIt == classFieldDefaults.end()) {
        std::cerr << "No default values for class " << className << std::endl;
        return;
    }
    
    auto classIDIt = classIDs.find(className);
    if (classIDIt == classIDs.end()) {
        std::cerr << "No class ID for class " << className << std::endl;
        return;
    }
    int64_t classID = classIDIt->second;
    
    const auto& fieldDefaults = defaultsIt->second;
    std::map<size_t, Expr*> to_push;
    for (const auto& pair : fieldDefaults) {
        const std::string& fieldName = pair.first;
        Expr* defaultExpr = pair.second;
        
        auto indicesIt = classFieldIndices.find(className);
        if (indicesIt == classFieldIndices.end()) continue;

        auto fieldIt = indicesIt->second.find(fieldName);
        if (fieldIt == indicesIt->second.end()) continue;
        
        int64_t fieldIndex = fieldIt->second;

        to_push[fieldIndex] = defaultExpr;
    }
    int64_t classIDConstIdx = fb.add_const(ConstEntry(classID));
    fb.emit_push_const_index(classIDConstIdx);

    for (auto [_, expr] : to_push) {
        gen_expr_in_func(expr, fb);
    }

    fb.emit_build_arr(fieldCount + 1);
}
}

