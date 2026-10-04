%{
/**
 * @file lexer.l
 * @brief Flex lexical analyzer for Micro C.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "parser.tab.h"

int current_line = 1;
int current_column = 1;

static void update_loc(void) {
    yylloc.first_line = current_line;
    yylloc.first_column = current_column;
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            current_line++;
            current_column = 1;
        } else if (yytext[i] == '\t') {
            current_column += 4 - (current_column % 4);
        } else {
            current_column++;
        }
    }
    yylloc.last_line = current_line;
    yylloc.last_column = current_column;
}

#define YY_USER_ACTION update_loc();
%}

%option noyywrap
%option nounput
%option noinput

DIGIT       [0-9]
LETTER      [a-zA-Z_]
ID          {LETTER}({LETTER}|{DIGIT})*
INT_LIT     {DIGIT}+
FLOAT_LIT   ({DIGIT}+"."{DIGIT}*|{DIGIT}*"."{DIGIT}+)([eE][+-]?{DIGIT}+)?

%%

[ \t\r]+                { /* Ignore whitespace */ }
\n                      { /* Handled by YY_USER_ACTION */ }

"//".*                  { /* Single-line comment */ }
"/*"([^*]|\*+[^*/])*\*+"/" { /* Multi-line comment */ }
"#"[^\n]*               { /* Ignore preprocessor directives (#include, #define) */ }

"int"                   { return TOK_INT; }
"float"                 { return TOK_FLOAT; }
"double"                { return TOK_FLOAT; }
"void"                  { return TOK_VOID; }
"if"                    { return TOK_IF; }
"else"                  { return TOK_ELSE; }
"while"                 { return TOK_WHILE; }
"for"                   { return TOK_FOR; }
"return"                { return TOK_RETURN; }
"halt"                  { return TOK_HALT; }

"=="                    { return TOK_EQ; }
"!="                    { return TOK_NEQ; }
"<="                    { return TOK_LTE; }
">="                    { return TOK_GTE; }
"<"                     { return '<'; }
">"                     { return '>'; }
"="                     { return '='; }
"+"                     { return '+'; }
"-"                     { return '-'; }
"*"                     { return '*'; }
"/"                     { return '/'; }
"!"                     { return '!'; }
"&&"                    { return TOK_AND; }
"||"                    { return TOK_OR; }

"("                     { return '('; }
")"                     { return ')'; }
"{"                     { return '{'; }
"}"                     { return '}'; }
"["                     { return '['; }
"]"                     { return ']'; }
";"                     { return ';'; }
","                     { return ','; }

{FLOAT_LIT}             {
                            yylval.fval = atof(yytext);
                            return TOK_FLOAT_LIT;
                        }

{INT_LIT}               {
                            yylval.ival = strtoll(yytext, NULL, 10);
                            return TOK_INT_LIT;
                        }

{ID}                    {
                            yylval.str = strdup(yytext);
                            return TOK_IDENT;
                        }

.                       {
                            fprintf(stderr, "Lexical Error: Unknown token '%s' at line %d, col %d\n",
                                    yytext, current_line, current_column);
                        }

%%
