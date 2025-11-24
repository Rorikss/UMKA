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
    StmtPtr body;

    FunctionDefStmt(const std::string& n, const std::vector<std::string>& p, StmtPtr b) : name(n), params(p), body(b) {}
};

extern std::vector<StmtPtr> program_stmts;