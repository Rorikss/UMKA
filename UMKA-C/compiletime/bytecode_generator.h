#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "parser.hpp"
#include "../models/ast.h"
#include "../models/opcodes.h"
#include "../models/funk_builder.h"

namespace umka::compiler {
class BytecodeGenerator {
public:
    std::vector<ConstEntry> constPool;
    std::vector<FunctionEntry> funcTable;
    std::unordered_map<std::string, int64_t> userFuncIndex;
    constexpr static int64_t kMaxI64 = std::numeric_limits<int64_t>::max();

    static inline const std::unordered_map<std::string, int64_t> builtinIDs = {
            {"print",      kMaxI64 - 0},
            {"len",        kMaxI64 - 1},
            {"get",        kMaxI64 - 2},
            {"set",        kMaxI64 - 3},
            {"add",        kMaxI64 - 4},
            {"remove",     kMaxI64 - 5},

            {"write",      kMaxI64 - 7},
            {"read",       kMaxI64 - 8},
            {"assert",     kMaxI64 - 9},
            {"input",      kMaxI64 - 10},
            {"random",     kMaxI64 - 11}
    };

    static inline const std::unordered_map<std::string, uint8_t> BINOP_MAP = {
            {"+", OP_ADD}, {"-", OP_SUB}, {"*", OP_MUL}, {"/", OP_DIV}, {"%", OP_REM},
            {"&&", OP_AND}, {"||", OP_OR},
            {"==", OP_EQ}, {"!=", OP_NEQ}, {">", OP_GT}, {"<", OP_LT}, {">=", OP_GTE}, {"<=", OP_LTE},
            {"^-^", OP_OPCOT}
    };

    std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> classFieldIndices;
    std::unordered_map<std::string, int64_t> classFieldCount;
    std::unordered_map<std::string, std::unordered_map<std::string, Expr*>> classFieldDefaults;
    std::unordered_map<std::string, int64_t> classIDs;
    
    std::unordered_map<std::string, int64_t> methodIDs;
    std::unordered_map<std::string, int64_t> fieldIDs;
    
    std::vector<std::tuple<int64_t, int64_t, int64_t>> vmethodTable;
    std::vector<std::tuple<int64_t, int64_t, int64_t>> vfieldTable;

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
    
    void gen_field_access_expr(FieldAccessExpr* expr, FuncBuilder& fb);
    void gen_member_access_expr(MemberAccessExpr* expr, FuncBuilder& fb);
    void gen_method_call_expr(MethodCallExpr* expr, FuncBuilder& fb);
    void gen_member_assign_stmt(MemberAssignStmt* stmt, FuncBuilder& fb);
    void gen_class_instantiation(const std::string& className, FuncBuilder& fb);
    
    void build_function_common(FuncBuilder& fb, const std::vector<std::string>& params, Stmt* body);
    void emit_push_zero_const(FuncBuilder& fb);
    int64_t get_field_id_or_error(const std::string& fieldName, FuncBuilder& fb);
    int64_t get_method_id_or_error(const std::string& methodName);
};
}
