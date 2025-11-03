%{
#include <bits/stdc++.h>
extern FILE* yyin;
using namespace std;

/* --- Value: динамический тип (int64, double, string, bool, array, void) --- */
struct Value {
    enum Kind { VINT, VDOUBLE, VSTRING, VBOOL, VARRAY, VVOID } kind;
    long long ival;
    double dval;
    string sval;
    vector<Value> arr;

    Value(): kind(VVOID), ival(0), dval(0.0), sval(), arr() {}
    static Value make_int(long long v){ Value a; a.kind=VINT; a.ival=v; return a;}
    static Value make_double(double v){ Value a; a.kind=VDOUBLE; a.dval=v; return a;}
    static Value make_string(const string &s){ Value a; a.kind=VSTRING; a.sval=s; return a;}
    static Value make_bool(bool b){ Value a; a.kind=VBOOL; a.ival=b?1:0; return a;}
    static Value make_array(const vector<Value>& v){ Value a; a.kind=VARRAY; a.arr=v; return a;}
    static Value make_void(){ Value a; a.kind=VVOID; return a;}

    string to_string_val() const {
        switch(kind){
            case VINT: return std::to_string(ival);
            case VDOUBLE: {
                std::ostringstream os; os << dval; return os.str();
            }
            case VSTRING: return sval;
            case VBOOL: return (ival ? "true":"false");
            case VARRAY: {
                string out = "[";
                for(size_t i=0;i<arr.size();++i){
                    if(i) out += ", ";
                    out += arr[i].to_string_val();
                }
                out += "]";
                return out;
            }
            case VVOID: return "uint";
        }
        return "";
    }
};

/* --- окружение / стек переменных --- */
static vector<unordered_map<string, Value>> env;

static void push_env(){ env.emplace_back(); }
static void pop_env(){ if(!env.empty()) env.pop_back(); }
static void set_var(const string& name, const Value& v){
    if(env.empty()) push_env();
    // ищем в стеке сверху вниз — если нашли, перезапишем; иначе в текущей
    for(int i=(int)env.size()-1;i>=0;--i){
        if(env[i].count(name)){ env[i][name]=v; return;}
    }
    env.back()[name]=v;
}
static bool has_var_local(const string& name){
    if(env.empty()) return false;
    return env.back().count(name)>0;
}
static Value get_var(const string& name){
    for(int i=(int)env.size()-1;i>=0;--i){
        auto it = env[i].find(name);
        if(it!=env[i].end()) return it->second;
    }
    // если нет — вернуть void / 0
    return Value::make_void();
}

/* --- обработка ошибок / возврат из функции --- */
struct ReturnExc {
    Value v;
    ReturnExc(const Value& vv): v(vv) {}
};

/* --- forward declarations AST --- */
struct Expr;
struct Stmt;

using ExprPtr = Expr*;
using StmtPtr = Stmt*;

/* --- базовый класс выражений --- */
struct Expr {
    virtual ~Expr(){}
    virtual Value eval() = 0;
};

/* --- базовый класс операторов --- */
struct Stmt {
    virtual ~Stmt(){}
    virtual void run() = 0;
};

/* --- выражения: литералы, идентификаторы, бинарные, unary, array literal, function call --- */
struct IntExpr : Expr { long long v; IntExpr(long long x):v(x){} Value eval() override { return Value::make_int(v);} };
struct DoubleExpr : Expr { double v; DoubleExpr(double x):v(x){} Value eval() override { return Value::make_double(v);} };
struct StringExpr : Expr { string s; StringExpr(const string& ss):s(ss){} Value eval() override { return Value::make_string(s);} };
struct BoolExpr : Expr { bool b; BoolExpr(bool bb):b(bb){} Value eval() override { return Value::make_bool(b);} };

struct IdentExpr : Expr {
    string name;
    IdentExpr(const string& n):name(n){}
    Value eval() override { return get_var(name); }
};

struct ArrayExpr : Expr {
    vector<ExprPtr> elems;
    ArrayExpr(const vector<ExprPtr>& e):elems(e){}
    Value eval() override {
        vector<Value> vals;
        for(auto p: elems) vals.push_back(p->eval());
        return Value::make_array(vals);
    }
};

struct CallExpr : Expr {
    string name;
    vector<ExprPtr> args;
    CallExpr(const string& n, const vector<ExprPtr>& a):name(n), args(a){}
    Value eval() override;
};

struct UnaryExpr : Expr {
    char op; ExprPtr rhs;
    UnaryExpr(char o, ExprPtr r):op(o), rhs(r){}
    Value eval() override {
        Value v = rhs->eval();
        if(op=='!'){
            bool val = false;
            if(v.kind==Value::VBOOL) val = (v.ival != 0);
            else if(v.kind==Value::VINT) val = (v.ival != 0);
            else if(v.kind==Value::VDOUBLE) val = (v.dval != 0.0);
            return Value::make_bool(!val);
        } else if(op=='-' || op=='+'){
            if(v.kind==Value::VINT){
                if(op=='-') return Value::make_int(-v.ival);
                else return v;
            } else if(v.kind==Value::VDOUBLE){
                if(op=='-') return Value::make_double(-v.dval);
                else return v;
            }
        }
        return Value::make_void();
    }
};

struct BinaryExpr : Expr {
    string op; ExprPtr left; ExprPtr right;
    BinaryExpr(const string& o, ExprPtr l, ExprPtr r):op(o), left(l), right(r){}
    Value eval() override {
        Value L = left->eval();
        Value R = right->eval();

        // arithmetic for ints/doubles, comparisons, logicals, concat (CAT)
        if(op == "+"){
            if(L.kind==Value::VINT && R.kind==Value::VINT) return Value::make_int(L.ival + R.ival);
            if(L.kind==Value::VDOUBLE || R.kind==Value::VDOUBLE){
                double a = (L.kind==Value::VDOUBLE?L.dval: (L.kind==Value::VINT? (double)L.ival:0.0));
                double b = (R.kind==Value::VDOUBLE?R.dval: (R.kind==Value::VINT? (double)R.ival:0.0));
                return Value::make_double(a+b);
            }
            if(L.kind==Value::VSTRING || R.kind==Value::VSTRING){
                return Value::make_string(L.to_string_val() + R.to_string_val());
            }
        }
        if(op == "-"){
            if(L.kind==Value::VINT && R.kind==Value::VINT) return Value::make_int(L.ival - R.ival);
            if(L.kind==Value::VDOUBLE || R.kind==Value::VDOUBLE){
                double a = (L.kind==Value::VDOUBLE?L.dval: (L.kind==Value::VINT? (double)L.ival:0.0));
                double b = (R.kind==Value::VDOUBLE?R.dval: (R.kind==Value::VINT? (double)R.ival:0.0));
                return Value::make_double(a-b);
            }
        }
        if(op == "*"){
            if(L.kind==Value::VINT && R.kind==Value::VINT) return Value::make_int(L.ival * R.ival);
            if(L.kind==Value::VDOUBLE || R.kind==Value::VDOUBLE){
                double a = (L.kind==Value::VDOUBLE?L.dval: (L.kind==Value::VINT? (double)L.ival:0.0));
                double b = (R.kind==Value::VDOUBLE?R.dval: (R.kind==Value::VINT? (double)R.ival:0.0));
                return Value::make_double(a*b);
            }
        }
        if(op == "/"){
            double a = (L.kind==Value::VDOUBLE?L.dval: (L.kind==Value::VINT? (double)L.ival:0.0));
            double b = (R.kind==Value::VDOUBLE?R.dval: (R.kind==Value::VINT? (double)R.ival:0.0));
            return Value::make_double(a/b);
        }
        if(op == "%"){
            if(L.kind==Value::VINT && R.kind==Value::VINT) return Value::make_int(L.ival % R.ival);
        }

        // comparisons (==' , '!=' , '>' , '<' , '>=' , '<=')
        if(op == "==" || op == "!="){
            bool eq=false;
            if(L.kind==Value::VINT && R.kind==Value::VINT) eq = (L.ival==R.ival);
            else if(L.kind==Value::VDOUBLE && R.kind==Value::VDOUBLE) eq = (L.dval==R.dval);
            else eq = (L.to_string_val() == R.to_string_val());
            if(op=="==") return Value::make_bool(eq);
            else return Value::make_bool(!eq);
        }
        if(op == ">" || op == "<" || op== ">=" || op=="<="){
            double a = (L.kind==Value::VDOUBLE?L.dval: (L.kind==Value::VINT? (double)L.ival:0.0));
            double b = (R.kind==Value::VDOUBLE?R.dval: (R.kind==Value::VINT? (double)R.ival:0.0));
            if(op==">") return Value::make_bool(a>b);
            if(op=="<") return Value::make_bool(a<b);
            if(op==">=") return Value::make_bool(a>=b);
            if(op=="<=") return Value::make_bool(a<=b);
        }

        // logical && ||
        if(op == "&&"){
            bool la = (L.kind==Value::VBOOL? L.ival : (L.kind==Value::VINT? L.ival != 0 : true));
            bool ra = (R.kind==Value::VBOOL? R.ival : (R.kind==Value::VINT? R.ival != 0 : true));
            return Value::make_bool(la && ra);
        }
        if(op == "||"){
            bool la = (L.kind==Value::VBOOL? L.ival : (L.kind==Value::VINT? L.ival != 0 : true));
            bool ra = (R.kind==Value::VBOOL? R.ival : (R.kind==Value::VINT? R.ival != 0 : true));
            return Value::make_bool(la || ra);
        }

        // concatenation token (CAT)
        if(op == "^-^"){
            return Value::make_string(L.to_string_val() + R.to_string_val());
        }

        return Value::make_void();
    }
};

/* --- вызовы встроенных функций и пользовательских --- */
struct Function {
    vector<string> params;
    StmtPtr body;
};

static unordered_map<string, Function*> functions;

Value CallExpr::eval() {
    // встроенные функции
    if(name == "print"){
        for(auto a: args){
            Value v = a->eval();
            cout << v.to_string_val();
        }
        cout << endl;
        return Value::make_void();
    }
    if(name == "len"){
        if(args.size()>=1){
            Value v = args[0]->eval();
            if(v.kind==Value::VARRAY) return Value::make_int((long long)v.arr.size());
            if(v.kind==Value::VSTRING) return Value::make_int((long long)v.sval.size());
        }
        return Value::make_int(0);
    }
    if(name == "get"){
        if(args.size()>=2){
            Value arrv = args[0]->eval();
            Value idxv = args[1]->eval();
            if(arrv.kind==Value::VARRAY && idxv.kind==Value::VINT){
                int idx = (int)idxv.ival;
                if(idx>=0 && idx < (int)arrv.arr.size()) return arrv.arr[idx];
            }
        }
        return Value::make_void();
    }
    if(name == "set"){
        // set(arr, index, elem)
        if(args.size()>=3){
            // first arg must be identifier pointing to array variable
            // but in our CALL we only get evaluated values. For set operation it's common to have set(arr, idx, val)
            // We implement by evaluating first arg (should be array var) then mutating it in env if it is a variable:
            // As simpler fallback — mutate variable if first arg is an IdentExpr
            ExprPtr e0 = args[0];
            // try dynamic cast to IdentExpr pointer
            IdentExpr* ide = dynamic_cast<IdentExpr*>(e0);
            if(ide){
                Value arrv = get_var(ide->name);
                Value idxv = args[1]->eval();
                Value valv = args[2]->eval();
                if(arrv.kind==Value::VARRAY && idxv.kind==Value::VINT){
                    int idx = (int)idxv.ival;
                    if(idx>=0 && idx < (int)arrv.arr.size()){
                        arrv.arr[idx] = valv;
                        set_var(ide->name, arrv);
                        return Value::make_void();
                    }
                }
            }
        }
        return Value::make_void();
    }
    if(name == "add"){
        if(args.size()>=2){
            IdentExpr* ide = dynamic_cast<IdentExpr*>(args[0]);
            if(ide){
                Value arrv = get_var(ide->name);
                Value valv = args[1]->eval();
                if(arrv.kind==Value::VARRAY){
                    arrv.arr.push_back(valv);
                    set_var(ide->name, arrv);
                }
            }
        }
        return Value::make_void();
    }
    if(name == "remove"){
        if(args.size()>=2){
            IdentExpr* ide = dynamic_cast<IdentExpr*>(args[0]);
            if(ide){
                Value arrv = get_var(ide->name);
                Value idxv = args[1]->eval();
                if(arrv.kind==Value::VARRAY && idxv.kind==Value::VINT){
                    int idx = (int)idxv.ival;
                    if(idx>=0 && idx < (int)arrv.arr.size()){
                        arrv.arr.erase(arrv.arr.begin()+idx);
                        set_var(ide->name, arrv);
                    }
                }
            }
        }
        return Value::make_void();
    }
    if(name == "str"){
        if(args.size()>=1){
            Value v = args[0]->eval();
            return Value::make_string(v.to_string_val());
        }
        return Value::make_string("");
    }
    if(name == "int_to_double"){
        if(args.size()>=1){
            Value v = args[0]->eval();
            if(v.kind==Value::VINT) return Value::make_double((double)v.ival);
            if(v.kind==Value::VDOUBLE) return v;
        }
        return Value::make_double(0.0);
    }
    if(name == "double_to_int"){
        if(args.size()>=1){
            Value v = args[0]->eval();
            if(v.kind==Value::VDOUBLE) return Value::make_int((long long)v.dval);
            if(v.kind==Value::VINT) return v;
        }
        return Value::make_int(0);
    }
    if(name == "write"){
        if(args.size()>=2){
            Value f = args[0]->eval();
            Value cont = args[1]->eval();
            if(f.kind==Value::VSTRING){
                ofstream ofs(f.sval);
                if(ofs){
                    ofs << cont.to_string_val();
                    ofs.close();
                }
            }
        }
        return Value::make_void();
    }
    if(name == "read"){
        if(args.size()>=1){
            Value f = args[0]->eval();
            if(f.kind==Value::VSTRING){
                ifstream ifs(f.sval);
                vector<Value> lines;
                if(ifs){
                    string line;
                    while(std::getline(ifs, line)){
                        lines.push_back(Value::make_string(line));
                    }
                }
                return Value::make_array(lines);
            }
        }
        return Value::make_array({});
    }

    // пользовательские функции
    auto it = functions.find(name);
    if(it != functions.end()){
        Function* fn = it->second;
        // подготовка локального окружения
        push_env();
        for(size_t i=0;i<fn->params.size() && i<args.size(); ++i){
            Value av = args[i]->eval();
            env.back()[fn->params[i]] = av;
        }
        try {
            fn->body->run();
        } catch(ReturnExc &ret){
            Value rv = ret.v;
            pop_env();
            return rv;
        }
        pop_env();
        return Value::make_void();
    }

    // неизвестная функция -> void
    return Value::make_void();
}

/* --- операторы (statements) --- */
struct LetStmt : Stmt {
    string name; ExprPtr expr;
    LetStmt(const string& n, ExprPtr e):name(n),expr(e){}
    void run() override {
        Value v = expr->eval();
        set_var(name, v);
    }
};

struct AssignStmt : Stmt {
    string name; ExprPtr expr;
    AssignStmt(const string& n, ExprPtr e):name(n),expr(e){}
    void run() override {
        Value v = expr->eval();
        set_var(name, v);
    }
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(ExprPtr e):expr(e){}
    void run() override { expr->eval(); }
};

struct BlockStmt : Stmt {
    vector<StmtPtr> stmts;
    BlockStmt(const vector<StmtPtr>& s):stmts(s){}
    void run() override {
        push_env();
        for(auto s: stmts) s->run();
        pop_env();
    }
};

struct IfStmt : Stmt {
    ExprPtr cond; StmtPtr thenb; StmtPtr elseb;
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e):cond(c),thenb(t),elseb(e){}
    void run() override {
        Value v = cond->eval();
        bool b = (v.kind==Value::VBOOL ? v.ival : (v.kind==Value::VINT ? v.ival != 0 : true));
        if(b) thenb->run();
        else if(elseb) elseb->run();
    }
};

struct WhileStmt : Stmt {
    ExprPtr cond; StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b):cond(c),body(b){}
    void run() override {
        while(true){
            Value v = cond->eval();
            bool b = (v.kind==Value::VBOOL ? v.ival : (v.kind==Value::VINT ? v.ival != 0 : true));
            if(!b) break;
            body->run();
        }
    }
};

struct ForStmt : Stmt {
    StmtPtr init; ExprPtr cond; StmtPtr post; StmtPtr body;
    ForStmt(StmtPtr i, ExprPtr c, StmtPtr p, StmtPtr b):init(i),cond(c),post(p),body(b){}
    void run() override {
        push_env();
        if(init) init->run();
        while(true){
            if(cond){
                Value v = cond->eval();
                bool b = (v.kind==Value::VBOOL ? v.ival : (v.kind==Value::VINT ? v.ival != 0 : true));
                if(!b) break;
            }
            body->run();
            if(post) post->run();
        }
        pop_env();
    }
};

struct ReturnStmt : Stmt {
    ExprPtr expr;
    ReturnStmt(ExprPtr e):expr(e){}
    void run() override {
        Value v = (expr? expr->eval(): Value::make_void());
        throw ReturnExc(v);
    }
};

struct FunctionDefStmt : Stmt {
    string name;
    vector<string> params;
    StmtPtr body;
    FunctionDefStmt(const string& n, const vector<string>& p, StmtPtr b):name(n),params(p),body(b){}
    void run() override {
        Function* f = new Function();
        f->params = params;
        f->body = body;
        functions[name] = f;
    }
};

/* --- вспом. контейнеры для списков --- */
static vector<StmtPtr> program_stmts;

/* --- помощники для освобождения строк, выражений, stmts --- */
static void free_expr(ExprPtr e){
    // простая рекурсия освобождения узлов: (не реализовано полностью — можно добавить при необходимости)
    delete e;
}
static void free_stmt(StmtPtr s){ delete s; }

/* --- готовые функции для добавления stmts в список --- */
static void add_program_stmt(StmtPtr s){ program_stmts.push_back(s); }

/* --- После парсинга вызываем execute_program() --- */
void execute_program(){
    push_env();
    for(auto s: program_stmts) s->run();
    pop_env();
}

/* --- парсер: прототипы лексера и ошибки --- */
extern int yylex();
extern int yyparse();
extern int yylineno;   /* из лексера */
extern char* yytext;
void yyerror(const char* s){
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
%type <stmt> block_statement statement let_statement assignment_statement expression_statement if_statement while_statement for_statement return_statement function_definition
%type <expr> expression logical_expression logical_or logical_and logical_comparison comparison arithmetic_expression term factor unary_arithmetic arithmetic_primary function_call array_literal
%type <expr_list> expression_list argument_list
%type <param_list> parameter_list
%type <stmt_list> statement_list


/* токены */
%token <ival> INTEGER
%token <dval> DOUBLE
%token <sval> STRING
%token <sval> IDENT
%token <boolval> BOOLEAN

%token TYPE_UINT
%token LET FUN IF ELSE WHILE FOR RETURN PRINT READ WRITE LEN ADD REMOVE GET SET STR INT_TO_DOUBLE DOUBLE_TO_INT
%token CAT
%token ARROW
%token TYPE_INT
%token TYPE_DOUBLE
%token TYPE_STRING
%token TYPE_BOOL

/* операторы/разделители — можно объявить как обычные символы или токены */
%token EQ NEQ GE LE AND OR


%%

program:
    /* empty */
  | program statement  {
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
        if($2){
            for(auto s: *$2) stmtsv.push_back(s);
            delete $2;
        }
        $$ = new BlockStmt(stmtsv);
    }
  ;

statement_list:
    /* empty */ { $$ = new vector<Stmt*>(); }
  | statement_list statement {
        if(!$1) $1 = new vector<Stmt*>();
        $1->push_back($2);
        $$ = $1;
    }
  ;

/* if / while / for */
if_statement:
    IF '(' logical_expression ')' block_statement {
        $$ = new IfStmt($3, $5, nullptr);
    }
  | IF '(' logical_expression ')' block_statement ELSE block_statement {
        $$ = new IfStmt($3, $5, $7);
    }
  ;

while_statement:
    WHILE '(' logical_expression ')' block_statement {
        $$ = new WhileStmt($3, $5);
    }
  ;

for_statement:
    FOR '(' let_statement ';' logical_expression ';' assignment_statement ')' block_statement {
            // теперь позиции:
            // $3 = let_statement  (Stmt*)
            // $5 = logical_expression (Expr*)
            // $7 = assignment_statement (Stmt*)
            // $9 = block_statement (Stmt*)
            Stmt* init = $3;                  // уже LetStmt*, лексер/парсер внутри освобождает id
            Expr* cond = $5;
            Stmt* post = $7;
            $$ = new ForStmt(init, cond, post, $9);
        }
;

return_type:
    TYPE_UINT
  | TYPE_INT
  | TYPE_DOUBLE
  | TYPE_STRING
  | TYPE_BOOL
  | '[' ']'   /* array type */
  ;

/* function definition */
function_definition:
    FUN IDENT '(' parameter_list ')' ARROW return_type block_statement {
        // позиции:
        // $2 = IDENT (char*)
        // $4 = parameter_list (vector<char*>*)
        // $7 = TYPE_UINT (токен, если нужен)
        // $8 = block_statement (Stmt*)
        vector<string> params;
        if($4){
            for(auto p : *$4) { params.push_back(std::string(p)); free(p); }
            delete $4;
        }
        $$ = new FunctionDefStmt(std::string($2), params, $8);
        free($2);
    }
;

/* parameter list (identifiers) */
parameter_list:
    /* empty */ { $$ = new std::vector<char*>(); }
  | parameter_list ',' IDENT {
        if(!$1) $1 = new std::vector<char*>();
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
    logical_expression   { $$ = $1; }
  ;

/* logical chain */
logical_expression:
    logical_or           { $$ = $1; }
  ;

logical_or:
    logical_and          { $$ = $1; }
  | logical_or OR logical_and { $$ = new BinaryExpr("||", $1, $3); }
  ;

logical_and:
    logical_comparison   { $$ = $1; }
  | logical_and AND logical_comparison { $$ = new BinaryExpr("&&", $1, $3); }
  ;

logical_comparison:
    comparison           { $$ = $1; }
  | '!' logical_comparison { $$ = new UnaryExpr('!', $2); }
  ;

/* comparisons */
comparison:
    arithmetic_expression { $$ = $1; }
  | arithmetic_expression EQ arithmetic_expression { $$ = new BinaryExpr("==", $1, $3); }
  | arithmetic_expression NEQ arithmetic_expression { $$ = new BinaryExpr("!=", $1, $3); }
  | arithmetic_expression '>' arithmetic_expression { $$ = new BinaryExpr(">", $1, $3); }
  | arithmetic_expression '<' arithmetic_expression { $$ = new BinaryExpr("<", $1, $3); }
  | arithmetic_expression GE arithmetic_expression { $$ = new BinaryExpr(">=", $1, $3); }
  | arithmetic_expression LE arithmetic_expression { $$ = new BinaryExpr("<=", $1, $3); }
  | STRING EQ STRING { $$ = new BinaryExpr("==", new StringExpr(string($1)), new StringExpr(string($3))); free($1); free($3); }
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
    INTEGER { $$ = new IntExpr($1); }
  | DOUBLE { $$ = new DoubleExpr($1); }
  | STRING { $$ = new StringExpr(string($1)); free($1); }
  | BOOLEAN { $$ = new BoolExpr($1); }
  | IDENT { $$ = new IdentExpr(string($1)); free($1); }
  | array_literal { $$ = $1; }
  | function_call { $$ = $1; }
  | '(' arithmetic_expression ')' { $$ = $2; }
  ;

/* array literal */
array_literal:
    '[' ']' { $$ = new ArrayExpr(vector<Expr*>()); }
  | '[' expression_list ']' {
        vector<Expr*> vals;
        if($2){
            for(auto e: *$2) vals.push_back(e);
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
        if($3){
            for(auto e : *$3) args.push_back(e);
            delete $3;
        }
        $$ = new CallExpr(string($1), args);
        free($1);
    }
  ;

/* argument list */
argument_list:
    expression { $$ = new vector<Expr*>(); $$->push_back($1); }
  | argument_list ',' expression { $1->push_back($3); $$ = $1; }
  ;

%%
