#pragma once

#include <string>
#include <vector>

struct Expr;
struct Stmt;

using ExprPtr = Expr *;
using StmtPtr = Stmt *;

// --- Expr ---
struct Expr {
    virtual ~Expr() {}
};

struct IntExpr : Expr {
    long long v;

    IntExpr(long long x) : v(x) {}
};

struct DoubleExpr : Expr {
    double v;

    DoubleExpr(double x) : v(x) {}
};

struct StringExpr : Expr {
    std::string s;

    StringExpr(const std::string& ss) : s(ss) {}
};

struct BoolExpr : Expr {
    bool b;

    BoolExpr(bool bb) : b(bb) {}
};

struct UnitExpr : Expr {
    UnitExpr() {}
};

struct IdentExpr : Expr {
    std::string name;

    IdentExpr(const std::string& n) : name(n) {}
};

struct ArrayExpr : Expr {
    std::vector<ExprPtr> elems;

    ArrayExpr(const std::vector<ExprPtr>& e) : elems(e) {}
};

struct CallExpr : Expr {
    std::string name;
    std::vector<ExprPtr> args;

    CallExpr(const std::string& n, const std::vector<ExprPtr>& a) : name(n), args(a) {}
};

struct UnaryExpr : Expr {
    char op;
    ExprPtr rhs;

    UnaryExpr(char o, ExprPtr r) : op(o), rhs(r) {}
};

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(const std::string& o, ExprPtr l, ExprPtr r) : op(o), left(l), right(r) {}
};

// --- Stmt ---
struct Stmt {
    virtual ~Stmt() {}
};

struct LetStmt : Stmt {
    std::string name;
    ExprPtr expr;

    LetStmt(const std::string& n, ExprPtr e) : name(n), expr(e) {}
};

struct AssignStmt : Stmt {
    std::string name;
    ExprPtr expr;

    AssignStmt(const std::string& n, ExprPtr e) : name(n), expr(e) {}
};

struct ExprStmt : Stmt {
    ExprPtr expr;

    ExprStmt(ExprPtr e) : expr(e) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;

    BlockStmt(const std::vector<StmtPtr>& s) : stmts(s) {}
};

struct IfStmt : Stmt {
    ExprPtr cond;
    StmtPtr thenb;
    StmtPtr elseb;

    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e) : cond(c), thenb(t), elseb(e) {}
};

struct WhileStmt : Stmt {
    ExprPtr cond;
    StmtPtr body;

    WhileStmt(ExprPtr c, StmtPtr b) : cond(c), body(b) {}
};

struct ForStmt : Stmt {
    StmtPtr init;
    ExprPtr cond;
    StmtPtr post;
    StmtPtr body;

    ForStmt(StmtPtr i, ExprPtr c, StmtPtr p, StmtPtr b) : init(i), cond(c), post(p), body(b) {}
};

struct ReturnStmt : Stmt {
    ExprPtr expr;

    ReturnStmt(ExprPtr e) : expr(e) {}
};

struct FunctionDefStmt : Stmt {
    std::string name;
    std::vector<std::string> params;
    std::string ret_type;
    StmtPtr body;

    FunctionDefStmt(const std::string& n, const std::vector<std::string>& p, const std::string& rt, StmtPtr b) : name(n), params(p), ret_type(rt), body(b) {}
};

struct ClassDefStmt : Stmt {
    std::string name;
    std::vector<StmtPtr> fields;
    ClassDefStmt(const std::string& n, const std::vector<StmtPtr>& f):name(n), fields(f){}
};

struct MethodDefStmt : Stmt {
    std::string class_name;
    std::string method_name;
    std::vector<std::string> params;
    std::string ret_type;
    StmtPtr body;
    MethodDefStmt(const std::string& cn, const std::string& mn, const std::vector<std::string>& p, const std::string& rt, StmtPtr b)
        : class_name(cn), method_name(mn), params(p), ret_type(rt), body(b) {}
};

struct FieldAccessExpr : Expr {
    ExprPtr target;
    std::string field;
    FieldAccessExpr(ExprPtr t, const std::string& f): target(t), field(f) {}
};

struct MethodCallExpr : Expr {
    ExprPtr target;
    std::string method_name;
    std::vector<ExprPtr> args;
    MethodCallExpr(ExprPtr t, const std::string& m, const std::vector<ExprPtr>& a)
      : target(t), method_name(m), args(a) {}
};

struct MemberAccessExpr : Expr {
    std::string object_name;
    std::string field;
    MemberAccessExpr(const std::string& obj, const std::string& f): object_name(obj), field(f) {}
};

struct MemberAssignStmt : Stmt {
    std::string object_name;
    std::string field;
    ExprPtr expr;
    MemberAssignStmt(const std::string& obj, const std::string& f, ExprPtr e)
      : object_name(obj), field(f), expr(e) {}
};

extern std::vector<StmtPtr> program_stmts;
