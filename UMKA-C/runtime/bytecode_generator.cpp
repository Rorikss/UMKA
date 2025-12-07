#include "bytecode_generator.h"
#include <fstream>
#include <iostream>
#include <cstring>


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


void BytecodeGenerator::build_functions(const std::vector<Stmt*>& program) {
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
            std::cout << "Building function " << fd->name << std::endl;
            auto it = userFuncIndex.find(fd->name);
            if (it == userFuncIndex.end()) continue;
            int64_t fidx = it->second;
            FuncBuilder& fb = funcBuilders.at(fidx);

            for (size_t i = 0; i < fd->params.size(); ++i) {
                fb.var_index[fd->params.at(i)] = i;
            }
            fb.nextVarIndex = fd->params.size();
            std::cout << "Building function " << fd->name << " gen()" << std::endl;
            gen_stmt_in_func(fd->body, fb);
            std::cout << "Building function " << fd->name << " gen() done" << std::endl; std::cout.flush();

            if (fb.code.empty() || fb.code.back() != OP_RETURN) {
                int64_t idx = fb.add_const(ConstEntry());
                fb.emit_push_const_index(idx);
                fb.emit_return();
            }

            FunctionEntry fe;
            fe.arg_count = fd->params.size();
            fe.local_count = fb.nextVarIndex;
            funcTable.at(fidx) = fe;
        } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
            // Method names are prefixed with class name
            std::string methodFullName = md->class_name + "$" + md->method_name;
            std::cout << "Building method " << methodFullName << std::endl;
            auto it = userFuncIndex.find(methodFullName);
            if (it == userFuncIndex.end()) continue;
            int64_t fidx = it->second;
            FuncBuilder& fb = funcBuilders.at(fidx);

            // First parameter is always 'self' for methods
            fb.var_index["self"] = 0;
            for (size_t i = 0; i < md->params.size(); ++i) {
                fb.var_index[md->params.at(i)] = i + 1; // +1 because 'self' is parameter 0
            }
            fb.nextVarIndex = md->params.size() + 1; // +1 for 'self'

            gen_stmt_in_func(md->body, fb);

            if (fb.code.empty() || fb.code.back() != OP_RETURN) {
                int64_t idx = fb.add_const(ConstEntry());
                fb.emit_push_const_index(idx);
                fb.emit_return();
            }

            FunctionEntry fe;
            fe.arg_count = md->params.size() + 1; // +1 for 'self'
            fe.local_count = fb.nextVarIndex;
            funcTable.at(fidx) = fe;
        }
    }
}

// Collect class information before building functions
void BytecodeGenerator::collect_functions(const std::vector<Stmt*>& program) {
    userFuncIndex.clear();
    classFieldIndices.clear();
    classFieldCount.clear();
    classFieldDefaults.clear();

    // First pass: collect class definitions and their fields
    std::cout << "Collecting class definitions" << std::endl;
    for (auto s: program) {
        if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
            std::cout << "Collecting class definition for " << cd->name << std::endl;
            std::unordered_map<std::string, int64_t> fieldIndices;
            std::unordered_map<std::string, Expr*> fieldDefaults;
            int64_t fieldCount = 0;
            
            // Assign indices to fields and store default values
            for (auto fieldStmt: cd->fields) {
                if (auto ls = dynamic_cast<LetStmt*>(fieldStmt)) {
                    fieldIndices[ls->name] = fieldCount++;
                    fieldDefaults[ls->name] = ls->expr;
                } else {
                    std::cerr << "Warning: class field must be a let statement\n";
                }
            }
            
            classFieldIndices[cd->name] = fieldIndices;
            classFieldCount[cd->name] = fieldCount;
            classFieldDefaults[cd->name] = fieldDefaults;
        }
    }


    // Second pass: collect functions and methods
    std::cout << "Collecting func definitions: finding main()" << std::endl;
    FunctionDefStmt* mainFunc = nullptr;
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s); fd && fd->name == "main") {
            mainFunc = fd;
            break;
        }
    }

    int64_t idx = 0;

    if (mainFunc) {
        userFuncIndex["main"] = idx++;
    }

    std::cout << "Collecting func definitions: building" << std::endl; std::cout.flush();
    for (auto s: program) {
        if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
            std::cout << "Collecting func definition for function " << fd->name << std::endl;
            if (fd->name == "main") continue;
            if (userFuncIndex.find(fd->name) == userFuncIndex.end()) {
                userFuncIndex[fd->name] = ++idx;
            } else {
                std::cerr << "Warning: duplicate function name '" << fd->name << "'\n";
            }
        } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
            // Method names are prefixed with class name
            std::cout << "Collecting func definition for method " << md->class_name << "$" << md->method_name << std::endl;
            std::string methodFullName = md->class_name + "$" + md->method_name;
            if (userFuncIndex.find(methodFullName) == userFuncIndex.end()) {
                userFuncIndex[methodFullName] = ++idx;
            } else {
                std::cerr << "Warning: duplicate method name '" << methodFullName << "'\n";
            }
        } else if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
            continue;
        } else {
            std::cerr << "Warning: unknown statement type\n";
        }
    }
    std::cout << "Collecting fun definitions: done" << std::endl;

    funcBuilders.clear();
    funcBuilders.resize(idx + 1, FuncBuilder(&constPool));
    funcTable.clear();
    funcTable.resize(idx + 1);
}


std::vector<uint8_t> BytecodeGenerator::concatenate_function_codes() {
    std::vector<uint8_t> finalCode;
    int64_t instruction_offset = 0;
    for (size_t i = 0; i < funcBuilders.size(); ++i) {
        FuncBuilder& fb = funcBuilders.at(i);
        fb.resolve_pending();
        
        // Calculate instruction count for this function
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
    std::cout << "Collecting func definitions: done" << std::endl;
    build_functions(program);
    codeSection = concatenate_function_codes();
}

void BytecodeGenerator::write_to_file(const std::string& path) {
    std::vector<uint8_t> buffer;

    append_byte(buffer, 1);  // version
    append_uint16(buffer, (uint16_t)constPool.size());
    append_uint16(buffer, (uint16_t)funcTable.size());
    append_uint32(buffer, (uint32_t)codeSection.size());

    for (auto& c : constPool) {
        append_byte(buffer, (uint8_t)c.type);

        if (c.type == ConstEntry::INT) {
            append_int64(buffer, c._int);
        }
        else if (c.type == ConstEntry::DOUBLE) {
            append_double(buffer, c._double);
        }
        else if (c.type == ConstEntry::STRING) {
            append_int64(buffer, c._str.size());
            buffer.insert(buffer.end(), c._str.begin(), c._str.end());
        }
        else if (c.type == ConstEntry::UNIT) { /* No action needed */ }
    }

    for (auto& fe : funcTable) {
        append_int64(buffer, fe.instruction_offset_beg);
        append_int64(buffer, fe.instruction_offset_end);
        append_int64(buffer, fe.arg_count);
        append_int64(buffer, fe.local_count);
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
    } else if (auto ue = dynamic_cast<UnitExpr*>(expr)) {
        int64_t idx = fb.add_const(ConstEntry());
        fb.emit_push_const_index(idx);
    } else if (auto id = dynamic_cast<IdentExpr*>(expr)) {
        // Check if this identifier is a class name
        auto classIt = classFieldCount.find(id->name);
        if (classIt != classFieldCount.end()) {
            // This is a class name, so we need to instantiate the class
            // We don't have a variable name here, so we pass empty string
            gen_class_instantiation(id->name, "", fb);
        } else {
            // This is a regular variable
            auto it = fb.var_index.find(id->name);
            if (it == fb.var_index.end()) {
                std::cerr << "genExpr: unknown local var '" << id->name << "'\n";
                int64_t idx = fb.add_const(ConstEntry(0LL));
                fb.emit_push_const_index(idx);
                return;
            }
            fb.emit_load(it->second);
        }
    } else if (auto arr = dynamic_cast<ArrayExpr*>(expr)) {
        for (auto el: arr->elems) gen_expr_in_func(el, fb);
        // int64_t countIdx = fb.add_const(ConstEntry((int64_t) arr->elems.size()));
        // fb.emit_build_arr(countIdx);
        fb.emit_build_arr(arr->elems.size());
    } else if (auto call = dynamic_cast<CallExpr*>(expr)) {
        if (call->name == "to_int" ||
            call->name == "to_double" ||
            call->name == "to_string") {
            if (call->args.size() != 1) {
                std::cerr << "Cast '" << call->name << "' requires exactly 1 argument\n";
            }

            if (!call->args.empty()){
                gen_expr_in_func(call->args.at(0), fb);
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
        gen_expr_in_func(bex->right, fb);
        gen_expr_in_func(bex->left, fb);
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
        std::cout << "gen_stmt_in_func::LetStmt" << std::endl; std::cout.flush();
        
        std::cout << "gen_stmt_in_func::LetStmt" << ls->name << std::endl; std::cout.flush();
        int64_t idx = ++fb.nextVarIndex;
        fb.var_index[ls->name] = idx;
        
        // Check if the expression is an identifier that refers to a class
        if (auto idExpr = dynamic_cast<IdentExpr*>(ls->expr)) {
            std::cout << "gen_stmt_in_func::IdentExpr" << idExpr->name << std::endl; std::cout.flush();
            auto classIt = classFieldCount.find(idExpr->name);
            if (classIt != classFieldCount.end()) {
                // This is a class instantiation, track the type
                fb.var_types[ls->name] = idExpr->name;
                gen_class_instantiation(idExpr->name, ls->name, fb);
            } else {
                // Regular variable assignment
                gen_expr_in_func(ls->expr, fb);
            }
        } else {
            std::cout << "gen_stmt_in_func::???" << std::endl; std::cout.flush();
            // Regular expression assignment
            gen_expr_in_func(ls->expr, fb);
        }
        
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
        std::cout << "gen_stmt_in_func::BlockStmt" << std::endl; std::cout.flush();
        
        std::cout << "gen_stmt_in_func::BlockStmt sz=" << bs->stmts.size() << std::endl; std::cout.flush();

        for (auto st: bs->stmts) { 
            std::cout << "gen_stmt_in_func::BlockStmt st=" << (int64_t)st << std::endl; std::cout.flush();
            
            gen_stmt_in_func(st, fb); 
        }
    } else if (auto is = dynamic_cast<IfStmt*>(s)) {
        std::cout << "gen_stmt_in_func::IfStmt" << std::endl; std::cout.flush();
        
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
        std::cout << "gen_stmt_in_func::WhileStmt" << std::endl; std::cout.flush();
        
        std::string startL = fb.new_label();
        std::string endL = fb.new_label();
        fb.place_label(startL);
        gen_expr_in_func(ws->cond, fb);
        fb.emit_jmp_place_holder(OP_JMP_IF_FALSE, endL);
        gen_stmt_in_func(ws->body, fb);
        fb.emit_jmp_place_holder(OP_JMP, startL);
        fb.place_label(endL);
    } else if (auto fs = dynamic_cast<ForStmt*>(s)) {
        std::cout << "gen_stmt_in_func::ForStmt" << std::endl; std::cout.flush();
        
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
        std::cout << "gen_stmt_in_func::ReturnStmt" << std::endl; std::cout.flush();
        
        if (rs->expr) gen_expr_in_func(rs->expr, fb);
        else {
            int64_t idx = fb.add_const(ConstEntry());
            fb.emit_push_const_index(idx);
        }
        fb.emit_return();
    } else if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
        std::cout << "gen_stmt_in_func::FunctionDefStmt" << std::endl; std::cout.flush();
        
        // processed in build_functions, ignore here
    } else if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
        std::cout << "gen_stmt_in_func::ClassDefStmt" << std::endl; std::cout.flush();
        
        // Class definitions are handled at the top level, not inside functions
    } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
        std::cout << "gen_stmt_in_func::MethodDefStmt" << std::endl; std::cout.flush();
        
        // Method definitions are handled at the top level, not inside functions
    } else if (auto mas = dynamic_cast<MemberAssignStmt*>(s)) {
        std::cout << "gen_stmt_in_func::MemberAssignStmt" << std::endl; std::cout.flush();
        
        gen_member_assign_stmt(mas, fb);
    } else {
        std::cerr << "gen_stmt_in_func: unknown stmt node" << std::endl; std::cerr.flush(); std::cout.flush();
        
    }
    
}

// Add new function to handle member access expressions
void BytecodeGenerator::gen_member_access_expr(MemberAccessExpr* expr, FuncBuilder& fb) {
    // For member access: obj:field
    // We need to:
    // 1. Load the object
    // 2. Add field index as constant
    // 3. Emit GET operation using existing array operations
    
    // Load the object
    auto it = fb.var_index.find(expr->object_name);
    if (it == fb.var_index.end()) {
        std::cerr << "Member access to unknown object '" << expr->object_name << "'\n";
        int64_t idx = fb.add_const(ConstEntry(0LL));
        fb.emit_push_const_index(idx);
        return;
    }
    fb.emit_load(it->second);
    
    // Get the class name from the variable type
    auto typeIt = fb.var_types.find(expr->object_name);
    if (typeIt == fb.var_types.end()) {
        std::cerr << "Member access on object with unknown type '" << expr->object_name << "'\n";
        int64_t idx = fb.add_const(ConstEntry(0LL));
        fb.emit_push_const_index(idx);
        return;
    }
    
    // Get the field index from the class field indices
    std::string className = typeIt->second;
    auto classIt = classFieldIndices.find(className);
    if (classIt == classFieldIndices.end()) {
        std::cerr << "Member access on object of unknown class '" << className << "'\n";
        int64_t idx = fb.add_const(ConstEntry(0LL));
        fb.emit_push_const_index(idx);
        return;
    }
    
    auto fieldIt = classIt->second.find(expr->field);
    if (fieldIt == classIt->second.end()) {
        std::cerr << "Member access to unknown field '" << expr->field << "' in class '" << className << "'\n";
        int64_t idx = fb.add_const(ConstEntry(0LL));
        fb.emit_push_const_index(idx);
        return;
    }
    
    int64_t fieldIdx = fb.add_const(ConstEntry(fieldIt->second));
    fb.emit_push_const_index(fieldIdx);
    
    // Use the existing GET operation for arrays
    auto itb = builtinIDs.find("get");
    if (itb != builtinIDs.end()) {
        fb.emit_call(itb->second);
    }
}

// Add new function to handle method calls
void BytecodeGenerator::gen_method_call_expr(MethodCallExpr* expr, FuncBuilder& fb) {
    // For method call: obj$method(args...)
    // We need to:
    // 1. Load the object (as first argument to the method)
    // 2. Evaluate all arguments
    // 3. Call the method (which is a function with the object as first parameter)
    
    // Load the object
    auto it = fb.var_index.find(expr->object_name);
    if (it == fb.var_index.end()) {
        std::cerr << "Method call on unknown object '" << expr->object_name << "'\n";
        return;
    }
    fb.emit_load(it->second);
    
    // Evaluate all arguments
    for (auto arg: expr->args) gen_expr_in_func(arg, fb);
    
    // Find the method function using the object's type instead of its name
    auto typeIt = fb.var_types.find(expr->object_name);
    std::string className = (typeIt != fb.var_types.end()) ? typeIt->second : expr->object_name;
    std::string methodFullName = className + "$" + expr->method_name;
    auto itf = userFuncIndex.find(methodFullName);
    if (itf == userFuncIndex.end()) {
        std::cerr << "genExpr: call to unknown method '" << expr->method_name << "'\n";
        fb.emit_call(-1);
    } else {
        fb.emit_call(itf->second);
    }
}

// Add new function to handle member assignment statements
void BytecodeGenerator::gen_member_assign_stmt(MemberAssignStmt* stmt, FuncBuilder& fb) {
    // For member assignment: obj:field = expr
    // We need to:
    // 1. Load the object
    // 2. Evaluate the expression
    // 3. Add field index as constant
    // 4. Emit SET operation using existing array operations
    
    // Load the object
    auto it = fb.var_index.find(stmt->object_name);
    if (it == fb.var_index.end()) {
        std::cerr << "Member assignment to unknown object '" << stmt->object_name << "'\n";
        return;
    }
    fb.emit_load(it->second);
    
    // Get the class name from the variable type
    auto typeIt = fb.var_types.find(stmt->object_name);
    if (typeIt == fb.var_types.end()) {
        std::cerr << "Member assignment on object with unknown type '" << stmt->object_name << "'\n";
        return;
    }
    
    // Get the field index from the class field indices
    std::string className = typeIt->second;
    auto classIt = classFieldIndices.find(className);
    if (classIt == classFieldIndices.end()) {
        std::cerr << "Member assignment on object of unknown class '" << className << "'\n";
        return;
    }
    
    auto fieldIt = classIt->second.find(stmt->field);
    if (fieldIt == classIt->second.end()) {
        std::cerr << "Member assignment to unknown field '" << stmt->field << "' in class '" << className << "'\n";
        return;
    }
    
    // Add field index as constant
    int64_t fieldIdx = fb.add_const(ConstEntry(fieldIt->second));
    fb.emit_push_const_index(fieldIdx);
    
    // Evaluate the expression
    gen_expr_in_func(stmt->expr, fb);
    
    // Use the existing SET operation for arrays
    auto itb = builtinIDs.find("set");
    if (itb != builtinIDs.end()) {
        fb.emit_call(itb->second);
    }
}

// Add new function to handle class instantiation
void BytecodeGenerator::gen_class_instantiation(const std::string& className, const std::string& varName, FuncBuilder& fb) {
    // For class instantiation: let obj = ClassName;
    // We need to:
    // 1. Create an array with the appropriate number of fields
    // 2. Initialize fields with default values
    
    // Get the number of fields for this class
    auto countIt = classFieldCount.find(className);
    int64_t fieldCount = (countIt != classFieldCount.end()) ? countIt->second : 0;
    
    // Initialize fields with default values
    if (varName.empty()) {
        std::cout << "No variable name for class " << className << std::endl;
        return;
    }
    auto defaultsIt = classFieldDefaults.find(className);
    if (defaultsIt == classFieldDefaults.end()) {
        std::cout << "No default values for class " << className << std::endl;
        return;
    }
    const auto& fieldDefaults = defaultsIt->second;
    std::vector<Expr*> to_push;
    for (const auto& pair : fieldDefaults) {
        const std::string& fieldName = pair.first;
        Expr* defaultExpr = pair.second;
        
        // Get field index
        auto indicesIt = classFieldIndices.find(className);
        std::cout << "Field index for " << fieldName << ": " << indicesIt->second[fieldName] << std::endl;
        if (indicesIt == classFieldIndices.end()) continue;
        
        auto fieldIt = indicesIt->second.find(fieldName);
        if (fieldIt == indicesIt->second.end()) continue;
        
        int64_t fieldIndex = fieldIt->second;

        // Duplicate the array (object) on the stack
        // auto varIt = fb.var_index.find(varName);
        // if (varIt != fb.var_index.end()) {
        //     fb.emit_load(varIt->second);
        // }
        
        // Add field index as constant
        // int64_t fieldIdx = fb.add_const(ConstEntry(fieldIndex));
        // fb.emit_push_const_index(fieldIdx);
        
        // Evaluate the default expression
        to_push.push_back(defaultExpr);
        
        // Use the existing SET operation for arrays
        // auto itb = builtinIDs.find("set");
        // if (itb != builtinIDs.end()) {
        //     fb.emit_call(itb->second);
        // } else {
        //     // Pop the result if set is not available
        //     fb.emit_byte(OP_POP);
        // }
        
        // Pop the result of the set operation
        // fb.emit_byte(OP_POP);
    }
    std::reverse(to_push.begin(), to_push.end());
    for (auto expr : to_push) {
        gen_expr_in_func(expr, fb);
    }

    // Create an array with the appropriate number of fields
    fb.emit_build_arr(fieldCount);
}

// Update gen_expr_in_func to handle member access and method calls
// We need to modify the existing gen_expr_in_func function to handle new expression types
// But first, let's update gen_stmt_in_func to handle member assignment
