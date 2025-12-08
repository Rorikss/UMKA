#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "parser.hpp"
#include "../models/ast.h"
#include "../models/opcodes.h"
#include "../models/funk_builder.h"

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

    // Map class name to field names and their indices
    std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> classFieldIndices;
    // Map class name to field count
    std::unordered_map<std::string, int64_t> classFieldCount;
    // Map class name to field default values (as expression pointers)
    std::unordered_map<std::string, std::unordered_map<std::string, Expr*>> classFieldDefaults;

    std::vector<FuncBuilder> funcBuilders;
    std::vector<uint8_t> codeSection;

    BytecodeGenerator() = default;

    void generate_all(const std::vector<Stmt*>& program);
    void write_to_file(const std::string& path);

private:
    void collect_functions(const std::vector<Stmt*>& program);
    void build_functions(const std::vector<Stmt*>& program);
    std::vector<uint8_t> concatenate_function_codes();
    void gen_stmt_in_func(Stmt* s, FuncBuilder& fb);
    void gen_expr_in_func(Expr* e, FuncBuilder& fb);
    
    // Helper functions for class operations
    void gen_member_access_expr(MemberAccessExpr* expr, FuncBuilder& fb);
    void gen_method_call_expr(MethodCallExpr* expr, FuncBuilder& fb);
    void gen_member_assign_stmt(MemberAssignStmt* stmt, FuncBuilder& fb);
    void gen_class_instantiation(const std::string& className, FuncBuilder& fb);
};
