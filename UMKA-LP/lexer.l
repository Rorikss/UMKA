%option noyywrap
%option nounput
%option noinput
%option yylineno


%{
#include "parser.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
%}

DIGIT   [0-9]
DIGITS  {DIGIT}+
ID      [A-Za-z_][A-Za-z0-9_]*
ESC     \\["\\nrt\\]

%%

"//".*                                  { /* skip line comment */ }

"let"                                   { return LET; }
"fun"                                   { return FUN; }
"if"                                    { return IF; }
"else"                                  { return ELSE; }
"while"                                 { return WHILE; }
"for"                                   { return FOR; }
"return"                                { return RETURN; }
"true"                                  { yylval.boolval = true; return BOOLEAN; }
"false"                                 { yylval.boolval = false; return BOOLEAN; }

"int"                                   { return TYPE_INT; }
"double"                                { return TYPE_DOUBLE; }
"string"                                { return TYPE_STRING; }
"bool"                                  { return TYPE_BOOL; }
"unit"                                  { return TYPE_UNIT; }

\"([^"\\]|\\["\\nrt])*\"                {
                                            size_t len = yyleng;
                                            // выделяем и убираем кавычки
                                            char* raw = (char*)malloc(len+1);
                                            strncpy(raw, yytext, len);
                                            raw[len] = 0;
                                            // удаляем начальную и конечную кавычку
                                            raw[len-1] = 0;
                                            char* content = _strdup(raw+1);
                                            free(raw);
                                            yylval.sval = content;
                                            return STRING;
                                        }

{DIGITS}"."{DIGITS}                      { yylval.dval = atof(yytext); return DOUBLE; }

{DIGITS}                                 { yylval.ival = atoll(yytext); return INTEGER; }


{ID}                                     { yylval.sval = _strdup(yytext); return IDENT; }

[ \t\r\n]+                              { /* skip */ }


"=="                                    { return EQ; }
"!="                                    { return NEQ; }
">="                                    { return GE; }
"<="                                    { return LE; }
"&&"                                    { return AND; }
"||"                                    { return OR; }
"^-^"                                   { return CAT; }
"->"                                    { return ARROW; }

">"                                     { return '>'; }
"<"                                     { return '<'; }
"+"                                     { return '+'; }
"-"                                     { return '-'; }
"*"                                     { return '*'; }
"/"                                     { return '/'; }
"%"                                     { return '%'; }
"!"                                     { return '!'; }
"="                                     { return '='; }
";"                                     { return ';'; }
","                                     { return ','; }
"("                                     { return '('; }
")"                                     { return ')'; }
"{"                                     { return '{'; }
"}"                                     { return '}'; }
"["                                     { return '['; }
"]"                                     { return ']'; }

.                                       { return yytext[0]; }

%%
