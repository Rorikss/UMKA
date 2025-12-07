%{
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace std;
extern FILE* yyin;
using namespace std;

/* --- forward declarations AST --- */
struct Expr;
struct Stmt;

using ExprPtr = Expr*;
using StmtPtr = Stmt*;

/* --- базовый класс выражений --- */
struct Expr {
    virtual ~Expr() {}
};

/* --- базовый класс операторов --- */
struct Stmt {
    virtual ~Stmt() {}
};

/* --- выражения: литералы, идентификаторы, бинарные, unary, array literal, function call --- */

struct IntExpr : Expr {
    long long v;
    IntExpr(long long x) : v(x) {}
};

struct DoubleExpr : Expr {
    double v;
    DoubleExpr(double x) : v(x) {}
};

struct StringExpr : Expr {
    string s;
    StringExpr(const string& ss) : s(ss) {}
};

struct BoolExpr : Expr {
    bool b;
    BoolExpr(bool bb) : b(bb) {}
};

struct UnitExpr : Expr {
    UnitExpr() {}
};

struct IdentExpr : Expr {
    string name;
    IdentExpr(const string& n) : name(n) {}
};

struct ArrayExpr : Expr {
    vector<ExprPtr> elems;
    ArrayExpr(const vector<ExprPtr>& e) : elems(e) {}
};

struct CallExpr : Expr {
    string name;
    vector<ExprPtr> args;
    CallExpr(const string& n, const vector<ExprPtr>& a) : name(n), args(a) {}
};

struct UnaryExpr : Expr {
    char op;
    ExprPtr rhs;
    UnaryExpr(char o, ExprPtr r) : op(o), rhs(r) {}
};

struct BinaryExpr : Expr {
    string op;
    ExprPtr left;
    ExprPtr right;
    BinaryExpr(const string& o, ExprPtr l, ExprPtr r) : op(o), left(l), right(r) {}
};

/* --- операторы (statements) --- */
struct LetStmt : Stmt {
    string name;
    ExprPtr expr;
    LetStmt(const string& n, ExprPtr e) : name(n), expr(e) {}
};

struct AssignStmt : Stmt {
    string name;
    ExprPtr expr;
    AssignStmt(const string& n, ExprPtr e) : name(n), expr(e) {}
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(ExprPtr e) : expr(e) {}
};

struct BlockStmt : Stmt {
    vector<StmtPtr> stmts;
    BlockStmt(const vector<StmtPtr>& s) : stmts(s) {}
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
    ExprPtr  cond;
    StmtPtr post;
    StmtPtr body;
    ForStmt(StmtPtr i, ExprPtr c, StmtPtr p, StmtPtr b) : init(i), cond(c), post(p), body(b) {}
};

struct ReturnStmt : Stmt {
    ExprPtr expr;
    ReturnStmt(ExprPtr e) : expr(e) {}
};

struct FunctionDefStmt : Stmt {
    string name;
    vector<string> params;
    string ret_type;
    StmtPtr body;
    FunctionDefStmt(const string& n, const vector<string>& p, const string& rt, StmtPtr b)
        : name(n), params(p), ret_type(rt), body(b) {}
};

struct ClassDefStmt : Stmt {
    string name;
    vector<StmtPtr> fields;
    ClassDefStmt(const string& n, const vector<StmtPtr>& f):name(n), fields(f){}
};

struct MethodDefStmt : Stmt {
    string class_name;
    string method_name;
    vector<string> params;
    string ret_type;
    StmtPtr body;
    MethodDefStmt(const string& cn, const string& mn, const vector<string>& p, const string& rt, StmtPtr b)
        : class_name(cn), method_name(mn), params(p), ret_type(rt), body(b) {}
};

struct MemberAccessExpr : Expr {
    string object_name;
    string field;
    MemberAccessExpr(const string& obj, const string& f): object_name(obj), field(f) {}
};

struct MethodCallExpr : Expr {
    string object_name;
    string method_name;
    vector<ExprPtr> args;
    MethodCallExpr(const string& obj, const string& m, const vector<ExprPtr>& a)
      : object_name(obj), method_name(m), args(a) {}
};

struct MemberAssignStmt : Stmt {
    string object_name;
    string field;
    ExprPtr expr;
    MemberAssignStmt(const string& obj, const string& f, ExprPtr e)
      : object_name(obj), field(f), expr(e) {}
};

/* --- вспом. контейнеры для списков --- */
vector<StmtPtr> program_stmts;

/* --- помощники для освобождения строк, выражений, stmts --- */
static void free_expr(ExprPtr e) {
    // простая рекурсия освобождения узлов: (не реализовано полностью — можно добавить при необходимости)
    delete e;
}

static void free_stmt(StmtPtr s) {
    delete s;
}

/* --- готовые функции для добавления stmts в список --- */
static void add_program_stmt(StmtPtr s) {
    program_stmts.push_back(s);
}

/* --- debug / печать AST --- */
static void print_indent(int n) {
    for (int i = 0; i < n; ++i) std::cerr << "  ";
}

static void print_expr(Expr* e, int indent);
static void print_stmt(Stmt* s, int indent);

static void print_expr_list(const std::vector<Expr*>& v, int indent) {
    for (size_t i = 0; i < v.size(); ++i) {
        print_indent(indent);
        std::cerr << "[" << i << "]\n";
        print_expr(v[i], indent + 1);
    }
}

static void print_stmt_list(const std::vector<Stmt*>& v, int indent) {
    for (size_t i = 0; i < v.size(); ++i) {
        print_indent(indent);
        std::cerr << "[" << i << "]\n";
        print_stmt(v[i], indent + 1);
    }
}

static void print_expr(Expr* e, int indent) {
    if (!e) {
        print_indent(indent);
        std::cerr << "<null expr>\n";
        return;
    }

    if (auto ie = dynamic_cast<IntExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Int: " << ie->v << "\n";
    } else if (auto de = dynamic_cast<DoubleExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Double: " << de->v << "\n";
    } else if (auto se = dynamic_cast<StringExpr*>(e)) {
        print_indent(indent);
        std::cerr << "String: \"" << se->s << "\"\n";
    } else if (auto be = dynamic_cast<BoolExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Bool: " << (be->b ? "true" : "false") << "\n";
    } else if (auto ue = dynamic_cast<UnitExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Unit\n";
    } else if (auto id = dynamic_cast<IdentExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Ident: " << id->name << "\n";
    } else if (auto ar = dynamic_cast<ArrayExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Array:\n";
        print_expr_list(ar->elems, indent + 1);
    } else if (auto ce = dynamic_cast<CallExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Call: " << ce->name << " (args: " << ce->args.size() << ")\n";
        print_expr_list(ce->args, indent + 1);
    } else if (auto ma = dynamic_cast<MemberAccessExpr*>(e)) {
        print_indent(indent);
        std::cerr << "MemberAccess: object=" << ma->object_name << " field=" << ma->field << "\n";
    } else if (auto mc = dynamic_cast<MethodCallExpr*>(e)) {
        print_indent(indent);
        std::cerr << "MethodCall: object=" << mc->object_name << " method=" << mc->method_name
                  << " (args: " << mc->args.size() << ")\n";
        print_expr_list(mc->args, indent + 1);
    } else if (auto ue = dynamic_cast<UnaryExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Unary: '" << ue->op << "'\n";
        print_expr(ue->rhs, indent + 1);
    } else if (auto bex = dynamic_cast<BinaryExpr*>(e)) {
        print_indent(indent);
        std::cerr << "Binary: op=\"" << bex->op << "\"\n";
        print_indent(indent);
        std::cerr << " Left:\n";
        print_expr(bex->left, indent + 1);
        print_indent(indent);
        std::cerr << " Right:\n";
        print_expr(bex->right, indent + 1);
    } else {
        print_indent(indent);
        std::cerr << "Unknown Expr node\n";
    }
}

static void print_stmt(Stmt* s, int indent) {
    if (!s) {
        print_indent(indent);
        std::cerr << "<null stmt>\n";
        return;
    }

    if (auto ls = dynamic_cast<LetStmt*>(s)) {
        print_indent(indent);
        std::cerr << "Let: " << ls->name << " =\n";
        print_expr(ls->expr, indent + 1);
    } else if (auto as = dynamic_cast<AssignStmt*>(s)) {
        print_indent(indent);
        std::cerr << "Assign: " << as->name << " =\n";
        print_expr(as->expr, indent + 1);
    } else if (auto mas = dynamic_cast<MemberAssignStmt*>(s)) {
        print_indent(indent);
        std::cerr << "MemberAssign: object=" << mas->object_name << " field=" << mas->field << " =\n";
        print_expr(mas->expr, indent + 1);
    } else if (auto es = dynamic_cast<ExprStmt*>(s)) {
        print_indent(indent);
        std::cerr << "ExprStmt:\n";
        print_expr(es->expr, indent + 1);
    } else if (auto bs = dynamic_cast<BlockStmt*>(s)) {
        print_indent(indent);
        std::cerr << "BlockStmt:\n";
        print_stmt_list(bs->stmts, indent + 1);
    } else if (auto is = dynamic_cast<IfStmt*>(s)) {
        print_indent(indent);
        std::cerr << "IfStmt: condition:\n";
        print_expr(is->cond, indent + 1);
        print_indent(indent);
        std::cerr << "Then:\n";
        print_stmt(is->thenb, indent + 1);
        if (is->elseb) {
            print_indent(indent);
            std::cerr << "Else:\n";
            print_stmt(is->elseb, indent + 1);
        }
    } else if (auto ws = dynamic_cast<WhileStmt*>(s)) {
        print_indent(indent);
        std::cerr << "WhileStmt: condition:\n";
        print_expr(ws->cond, indent + 1);
        print_indent(indent);
        std::cerr << "Body:\n";
        print_stmt(ws->body, indent + 1);
    } else if (auto fs = dynamic_cast<ForStmt*>(s)) {
        print_indent(indent);
        std::cerr << "ForStmt:\n";
        print_indent(indent);
        std::cerr << " Init:\n";
        print_stmt(fs->init, indent + 1);
        print_indent(indent);
        std::cerr << " Cond:\n";
        print_expr(fs->cond, indent + 1);
        print_indent(indent);
        std::cerr << " Post:\n";
        print_stmt(fs->post, indent + 1);
        print_indent(indent);
        std::cerr << " Body:\n";
        print_stmt(fs->body, indent + 1);
    } else if (auto rs = dynamic_cast<ReturnStmt*>(s)) {
        print_indent(indent);
        std::cerr << "Return:\n";
        if (rs->expr) print_expr(rs->expr, indent + 1);
        else {
            print_indent(indent + 1);
            std::cerr << "<void>\n";
        }
    } else if (auto fd = dynamic_cast<FunctionDefStmt*>(s)) {
        print_indent(indent);
        std::cerr << "FunctionDef: " << fd->name << " (params:";
        for (size_t i = 0; i < fd->params.size(); ++i) {
            std::cerr << (i ? ", " : " ") << fd->params[i];
        }
         std::cerr << " ) -> " << fd->ret_type << "\n";
        print_indent(indent);
        std::cerr << " Body:\n";
        print_stmt(fd->body, indent + 1);
    } else if (auto cd = dynamic_cast<ClassDefStmt*>(s)) {
        print_indent(indent);
        std::cerr << "ClassDef: " << cd->name << "\n";
        print_indent(indent);
        std::cerr << " Fields:\n";
        print_stmt_list(cd->fields, indent + 1);
    } else if (auto md = dynamic_cast<MethodDefStmt*>(s)) {
        print_indent(indent);
        std::cerr << "MethodDef: class=" << md->class_name << " name=" << md->method_name << " (params:";
        for (size_t i = 0; i < md->params.size(); ++i) {
            std::cerr << (i ? ", " : " ") << md->params[i];
        }
        std::cerr << " ) -> " << md->ret_type << "\n";
        print_indent(indent);
        std::cerr << " Body:\n";
        print_stmt(md->body, indent + 1);
    } else {
        print_indent(indent);
        std::cerr << "Unknown Stmt node\n";
    }
}

void print_program_ast() {
    std::cerr << "=== AST: program_stmts.size() = " << program_stmts.size() << " ===\n";
    for (size_t i = 0; i < program_stmts.size(); ++i) {
        print_indent(0);
        std::cerr << "[" << i << "]\n";
        print_stmt(program_stmts[i], 1);
    }
    std::cerr << "=== END AST ===\n";
}



/* --- парсер: прототипы лексера и ошибки --- */
extern int yylex();
extern int yyparse();
extern int yylineno;   /* из лексера */
extern char* yytext;

void yyerror(const char* s) {
    cerr << "Parse error at token: " << yytext << " -> " << s << endl;
}
%}


%code requires {
  #include <vector>
  #include <cstdio>   /* для FILE* yyin */
}

/* --- YYSTYPE --- */
%union {
    long long ival;                  /* целые, из лексера */
    double dval;                     /* double, из лексера */
    char* sval;                      /* идентификаторы / строки */
    bool boolval;                    /* логические из лексера */
    struct Expr* expr;
    struct Stmt* stmt;
    std::vector<Expr*>* expr_list;
    std::vector<Stmt*>* stmt_list;
    std::vector<char*>* param_list;  /* список параметров как char* */
}

/* --- типы нетерминалов --- */
%type <stmt> block_statement statement let_statement assignment_statement expression_statement if_statement while_statement for_statement return_statement function_definition method_definition class_definition
%type <expr> expression cat_expression logical_expression logical_or logical_and logical_comparison comparison arithmetic_expression term factor unary_arithmetic arithmetic_primary function_call method_call member_access array_literal
%type <expr_list> expression_list argument_list
%type <param_list> parameter_list
%type <stmt_list> statement_list class_body
%type <sval> return_type


/* токены */
%token <ival> INTEGER
%token <dval> DOUBLE
%token <sval> STRING
%token <sval> IDENT
%token <boolval> BOOLEAN

%token TYPE_UNIT
%token LET FUN IF ELSE WHILE FOR RETURN PRINT READ WRITE LEN ADD REMOVE GET SET STR INT_TO_DOUBLE DOUBLE_TO_INT
%token CAT
%token ARROW
%token CLASS
%token METHOD
%token TYPE_INT
%token TYPE_DOUBLE
%token TYPE_STRING
%token TYPE_BOOL

/* операторы/разделители — можно объявить как обычные символы или токены */
%token EQ NEQ GE LE AND OR


%%

program:
      /* empty */
    | program statement {
        add_program_stmt($2);
    }
    ;

/* ------------- statements ------------- */
statement:
      let_statement ';'                    { $$ = $1; }
    | assignment_statement ';'             { $$ = $1; }
    | expression_statement ';'             { $$ = $1; }
    | if_statement                         { $$ = $1; }
    | while_statement                      { $$ = $1; }
    | for_statement                        { $$ = $1; }
    | return_statement ';'                 { $$ = $1; }
    | block_statement                      { $$ = $1; }
    | function_definition                  { $$ = $1; }
    | class_definition                     { $$ = $1; }
    | method_definition                    { $$ = $1; }
    ;

/* let and assignment */
let_statement:
    LET IDENT '=' expression {
        $$ = new LetStmt(std::string($2), $4);
        free($2);
    }
    ;

assignment_statement:
    IDENT '=' expression {
        $$ = new AssignStmt(std::string($1), $3);
        free($1);
    }
  | IDENT ':' IDENT '=' expression {
          $$ = new MemberAssignStmt(std::string($1), std::string($3), $5);
          free($1);
          free($3);
      }
    ;

/* expression statement */
expression_statement:
    expression { $$ = new ExprStmt($1); }
    ;

/* return */
return_statement:
    RETURN { $$ = new ReturnStmt(nullptr); }
  | RETURN expression { $$ = new ReturnStmt($2); }
  ;

/* block */
block_statement:
    '{' statement_list '}' {
        vector<Stmt*> stmtsv;
        if ($2) {
            for (auto s : *$2) stmtsv.push_back(s);
            delete $2;
        }
        $$ = new BlockStmt(stmtsv);
    }
  ;

statement_list:
    /* empty */ { $$ = new vector<Stmt*>(); }
  | statement_list statement {
        if (!$1) $1 = new vector<Stmt*>();
        $1->push_back($2);
        $$ = $1;
    }
  ;

/* if / while / for */
if_statement:
    IF '(' expression ')' block_statement {
        $$ = new IfStmt($3, $5, nullptr);
    }
  | IF '(' expression ')' block_statement ELSE block_statement {
        $$ = new IfStmt($3, $5, $7);
    }
  ;

while_statement:
    WHILE '(' expression ')' block_statement {
        $$ = new WhileStmt($3, $5);
    }
  ;

for_statement:
    FOR '(' let_statement ';' expression ';' assignment_statement ')' block_statement {
        Stmt* init = $3;
        Expr* cond = $5;
        Stmt* post = $7;
        $$ = new ForStmt(init, cond, post, $9);
    }
  ;

/* return type */
return_type:
      TYPE_UNIT         { $$ = strdup("unit"); }
    | TYPE_INT          { $$ = strdup("int"); }
    | TYPE_DOUBLE       { $$ = strdup("double"); }
    | TYPE_STRING       { $$ = strdup("string"); }
    | TYPE_BOOL         { $$ = strdup("bool"); }
    | '[' ']'           { $$ = strdup("[]"); } /* array type */
    | IDENT             { $$ = $1; }
    ;

/* function definition */
function_definition:
    FUN IDENT '(' parameter_list ')' ARROW return_type block_statement {
        vector<string> params;
        if ($4) {
            for (auto p : *$4) {
                params.push_back(std::string(p));
                free(p);
            }
            delete $4;
        }
        std::string rt = $7 ? std::string($7) : std::string("");
        if($7) free($7);
        $$ = new FunctionDefStmt(std::string($2), params, rt, $8);
        free($2);
    }
  ;

/* class body: только let statements внутри */
class_body:
      /* empty */ { $$ = new std::vector<Stmt*>(); }
    | class_body let_statement ';' {
          if (!$1) $1 = new std::vector<Stmt*>();
          $1->push_back($2);
          $$ = $1;
      }
    ;

/* class definition */
class_definition:
    CLASS IDENT '{' class_body '}' {
        // $2 = IDENT (char*); $4 = class_body (vector<Stmt*>*)
        vector<Stmt*> fields;
        if ($4) {
            for (auto s : *$4) fields.push_back(s);
            delete $4;
        }
        $$ = new ClassDefStmt(std::string($2), fields);
        free($2);
    }
  ;

/* method definition */
method_definition:
    METHOD IDENT IDENT '(' parameter_list ')' ARROW return_type block_statement {
        vector<string> params;
        if ($5) {
            for (auto p : *$5) { params.push_back(std::string(p)); free(p); }
            delete $5;
        }
        std::string rt = $8 ? std::string($8) : std::string("");
        if($8) free($8);
        $$ = new MethodDefStmt(std::string($2), std::string($3), params, rt, $9);
        free($2);
        free($3);
    }
  ;

/* parameter list (identifiers) */
parameter_list:
    /* empty */ { $$ = new std::vector<char*>(); }
  | parameter_list ',' IDENT {
        if (!$1) $1 = new std::vector<char*>();
        $1->push_back($3);
        $$ = $1;
    }
  | IDENT {
        $$ = new std::vector<char*>();
        $$->push_back($1);
    }
  ;

/* ------------- expressions ------------- */
expression:
    cat_expression { $$ = $1; }
  ;

/* CAT token '^-^' */
cat_expression:
    logical_expression { $$ = $1; }
  | cat_expression CAT logical_expression { $$ = new BinaryExpr("^-^", $1, $3); }
  ;

/* logical chain */
logical_expression:
    logical_or { $$ = $1; }
  ;

logical_or:
    logical_and { $$ = $1; }
  | logical_or OR logical_and { $$ = new BinaryExpr("||", $1, $3); }
  ;

logical_and:
    logical_comparison { $$ = $1; }
  | logical_and AND logical_comparison { $$ = new BinaryExpr("&&", $1, $3); }
  ;

logical_comparison:
    comparison { $$ = $1; }
  | '!' logical_comparison { $$ = new UnaryExpr('!', $2); }
  ;

/* comparisons */
comparison:
    arithmetic_expression { $$ = $1; }
  | arithmetic_expression EQ arithmetic_expression    { $$ = new BinaryExpr("==", $1, $3); }
  | arithmetic_expression NEQ arithmetic_expression   { $$ = new BinaryExpr("!=", $1, $3); }
  | arithmetic_expression '>' arithmetic_expression    { $$ = new BinaryExpr(">", $1, $3); }
  | arithmetic_expression '<' arithmetic_expression    { $$ = new BinaryExpr("<", $1, $3); }
  | arithmetic_expression GE arithmetic_expression     { $$ = new BinaryExpr(">=", $1, $3); }
  | arithmetic_expression LE arithmetic_expression     { $$ = new BinaryExpr("<=", $1, $3); }
  | STRING EQ STRING {
        $$ = new BinaryExpr("==",
                            new StringExpr(string($1)),
                            new StringExpr(string($3)));
        free($1);
        free($3);
    }
  ;

/* arithmetic */
arithmetic_expression:
    term { $$ = $1; }
  ;

term:
    factor { $$ = $1; }
  | term '+' factor { $$ = new BinaryExpr("+", $1, $3); }
  | term '-' factor { $$ = new BinaryExpr("-", $1, $3); }
  ;

factor:
    unary_arithmetic { $$ = $1; }
  | factor '*' unary_arithmetic { $$ = new BinaryExpr("*", $1, $3); }
  | factor '/' unary_arithmetic { $$ = new BinaryExpr("/", $1, $3); }
  | factor '%' unary_arithmetic { $$ = new BinaryExpr("%", $1, $3); }
  ;

unary_arithmetic:
    arithmetic_primary { $$ = $1; }
  | '+' unary_arithmetic { $$ = new UnaryExpr('+', $2); }
  | '-' unary_arithmetic { $$ = new UnaryExpr('-', $2); }
  ;

arithmetic_primary:
    INTEGER         { $$ = new IntExpr($1); }
  | DOUBLE          { $$ = new DoubleExpr($1); }
  | STRING          { $$ = new StringExpr(string($1)); free($1); }
  | BOOLEAN         { $$ = new BoolExpr($1); }
  | TYPE_UNIT       { $$ = new UnitExpr(); }
  | member_access   { $$ = $1; }
  | method_call     { $$ = $1; }
  | IDENT           { $$ = new IdentExpr(string($1)); free($1); }
  | array_literal   { $$ = $1; }
  | function_call   { $$ = $1; }
  | '(' expression ')' { $$ = $2; }
  ;

/* array literal */
array_literal:
    '[' ']' {
        $$ = new ArrayExpr(vector<Expr*>());
    }
  | '[' expression_list ']' {
        vector<Expr*> vals;
        if ($2) {
            for (auto e : *$2) vals.push_back(e);
            delete $2;
        }
        $$ = new ArrayExpr(vals);
    }
  ;

/* expression list */
expression_list:
    expression { $$ = new vector<Expr*>(); $$->push_back($1); }
  | expression_list ',' expression { $1->push_back($3); $$ = $1; }
  ;

/* function call */
function_call:
    IDENT '(' ')' {
        $$ = new CallExpr(string($1), vector<Expr*>());
        free($1);
    }
  | IDENT '(' argument_list ')' {
        vector<Expr*> args;
        if ($3) {
            for (auto e : *$3) args.push_back(e);
            delete $3;
        }
        $$ = new CallExpr(string($1), args);
        free($1);
    }
  ;

/* member access */
member_access:
    IDENT ':' IDENT {
        $$ = new MemberAccessExpr(std::string($1), std::string($3));
        free($1);
        free($3);
    }
  ;

/* method call */
method_call:
    IDENT '$' IDENT '(' ')' {
        $$ = new MethodCallExpr(std::string($1), std::string($3), vector<Expr*>());
        free($1);
        free($3);
    }
  | IDENT '$' IDENT '(' argument_list ')' {
        vector<Expr*> args;
        if ($5) {
            for (auto e : *$5) args.push_back(e);
            delete $5;
        }
        $$ = new MethodCallExpr(std::string($1), std::string($3), args);
        free($1);
        free($3);
    }
  ;

/* argument list */
argument_list:
    expression { $$ = new vector<Expr*>(); $$->push_back($1); }
  | argument_list ',' expression { $1->push_back($3); $$ = $1; }
  ;

%%