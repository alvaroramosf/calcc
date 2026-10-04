%{
/**
 * @file parser.y
 * @brief Bison parser for Micro C generating Abstract Syntax Tree (AST).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylex(void);
extern int current_line;
extern int current_column;

void yyerror(const char *s);

ASTNode *g_ast_root = NULL;

static SourceLocation make_loc(int line, int col) {
    SourceLocation loc;
    loc.line = line;
    loc.column = col;
    loc.filename = "input.c";
    return loc;
}
%}

%locations

%code requires {
    #include "ast.h"
}

%union {
    ASTNode     *node;
    ASTNodeList *list;
    int64_t      ival;
    double       fval;
    char        *str;
    OperatorType op;
}

/* Tokens */
%token TOK_INT TOK_FLOAT TOK_VOID
%token TOK_IF TOK_ELSE TOK_WHILE TOK_FOR TOK_RETURN TOK_HALT
%token TOK_EQ TOK_NEQ TOK_LTE TOK_GTE TOK_AND TOK_OR

%token <ival> TOK_INT_LIT
%token <fval> TOK_FLOAT_LIT
%token <str>  TOK_IDENT

/* Non-terminal types */
%type <node> program function_decl var_decl statement compound_stmt
%type <node> expr_stmt if_stmt while_stmt for_stmt return_stmt halt_stmt
%type <node> expression assignment_expr logical_or_expr logical_and_expr
%type <node> equality_expr relational_expr additive_expr multiplicative_expr
%type <node> unary_expr primary_expr call_expr param_decl
%type <list> declaration_list statement_list argument_list argument_list_opt param_list param_decl_list

/* Operator Precedence and Associativity */
%right '='
%left TOK_OR
%left TOK_AND
%left TOK_EQ TOK_NEQ
%left '<' '>' TOK_LTE TOK_GTE
%left '+' '-'
%left '*' '/'
%right UMINUS '!'

%nonassoc LOWER_THAN_ELSE
%nonassoc TOK_ELSE

%%

program
    : declaration_list {
        ASTNode *p = ast_node_new(AST_PROGRAM, make_loc(1, 1));
        p->as.program.declarations = $1;
        g_ast_root = p;
        $$ = p;
    }
    ;

declaration_list
    : declaration_list function_decl {
        ast_node_list_append(&$1, $2);
        $$ = $1;
    }
    | declaration_list var_decl {
        ast_node_list_append(&$1, $2);
        $$ = $1;
    }
    | declaration_list type_specifier TOK_IDENT '(' param_list ')' ';' {
        /* Function prototype: ignore body */
        $$ = $1;
    }
    | /* empty */ {
        $$ = NULL;
    }
    ;

type_specifier
    : TOK_INT
    | TOK_FLOAT
    | TOK_VOID
    ;

function_decl
    : type_specifier TOK_IDENT '(' param_list ')' compound_stmt {
        ASTNode *fn = ast_node_new(AST_FUNCTION_DECL, make_loc(@2.first_line, @2.first_column));
        fn->as.function_decl.name = $2;
        fn->as.function_decl.parameters = $4;
        fn->as.function_decl.body = $6;
        $$ = fn;
    }
    ;

param_list
    : /* empty */       { $$ = NULL; }
    | TOK_VOID          { $$ = NULL; }
    | param_decl_list   { $$ = $1; }
    ;

param_decl_list
    : param_decl {
        ASTNodeList *l = NULL;
        ast_node_list_append(&l, $1);
        $$ = l;
    }
    | param_decl_list ',' param_decl {
        ast_node_list_append(&$1, $3);
        $$ = $1;
    }
    ;

param_decl
    : type_specifier TOK_IDENT {
        ASTNode *v = ast_node_new(AST_VAR_DECL, make_loc(@2.first_line, @2.first_column));
        v->as.var_decl.name = $2;
        v->as.var_decl.init_expr = NULL;
        v->as.var_decl.assigned_register = -1;
        v->as.var_decl.array_size = 0;
        $$ = v;
    }
    ;

compound_stmt
    : '{' statement_list '}' {
        ASTNode *b = ast_node_new(AST_BLOCK_STMT, make_loc(@1.first_line, @1.first_column));
        b->as.block_stmt.statements = $2;
        $$ = b;
    }
    ;

statement_list
    : statement_list statement {
        ast_node_list_append(&$1, $2);
        $$ = $1;
    }
    | /* empty */ {
        $$ = NULL;
    }
    ;

statement
    : var_decl
    | expr_stmt
    | compound_stmt
    | if_stmt
    | while_stmt
    | for_stmt
    | return_stmt
    | halt_stmt
    ;

var_decl
    : type_specifier TOK_IDENT ';' {
        ASTNode *v = ast_node_new(AST_VAR_DECL, make_loc(@2.first_line, @2.first_column));
        v->as.var_decl.name = $2;
        v->as.var_decl.init_expr = NULL;
        v->as.var_decl.assigned_register = -1;
        v->as.var_decl.array_size = 0;
        $$ = v;
    }
    | type_specifier TOK_IDENT '=' expression ';' {
        ASTNode *v = ast_node_new(AST_VAR_DECL, make_loc(@2.first_line, @2.first_column));
        v->as.var_decl.name = $2;
        v->as.var_decl.init_expr = $4;
        v->as.var_decl.assigned_register = -1;
        v->as.var_decl.array_size = 0;
        $$ = v;
    }
    | type_specifier TOK_IDENT '[' TOK_INT_LIT ']' ';' {
        ASTNode *v = ast_node_new(AST_VAR_DECL, make_loc(@2.first_line, @2.first_column));
        v->as.var_decl.name = $2;
        v->as.var_decl.init_expr = NULL;
        v->as.var_decl.assigned_register = -1;
        v->as.var_decl.array_size = (int)$4;
        $$ = v;
    }
    ;

expr_stmt
    : expression ';' {
        ASTNode *e = ast_node_new(AST_EXPR_STMT, make_loc(@1.first_line, @1.first_column));
        e->as.expr_stmt.expr = $1;
        $$ = e;
    }
    | ';' {
        ASTNode *e = ast_node_new(AST_EXPR_STMT, make_loc(@1.first_line, @1.first_column));
        e->as.expr_stmt.expr = NULL;
        $$ = e;
    }
    ;

if_stmt
    : TOK_IF '(' expression ')' statement %prec LOWER_THAN_ELSE {
        ASTNode *s = ast_node_new(AST_IF_STMT, make_loc(@1.first_line, @1.first_column));
        s->as.if_stmt.condition = $3;
        s->as.if_stmt.then_branch = $5;
        s->as.if_stmt.else_branch = NULL;
        $$ = s;
    }
    | TOK_IF '(' expression ')' statement TOK_ELSE statement {
        ASTNode *s = ast_node_new(AST_IF_STMT, make_loc(@1.first_line, @1.first_column));
        s->as.if_stmt.condition = $3;
        s->as.if_stmt.then_branch = $5;
        s->as.if_stmt.else_branch = $7;
        $$ = s;
    }
    ;

while_stmt
    : TOK_WHILE '(' expression ')' statement {
        ASTNode *s = ast_node_new(AST_WHILE_STMT, make_loc(@1.first_line, @1.first_column));
        s->as.while_stmt.condition = $3;
        s->as.while_stmt.body = $5;
        $$ = s;
    }
    ;

for_stmt
    : TOK_FOR '(' expression ';' expression ';' expression ')' statement {
        ASTNode *s = ast_node_new(AST_FOR_STMT, make_loc(@1.first_line, @1.first_column));
        s->as.for_stmt.init = $3;
        s->as.for_stmt.condition = $5;
        s->as.for_stmt.step = $7;
        s->as.for_stmt.body = $9;
        $$ = s;
    }
    ;

return_stmt
    : TOK_RETURN expression ';' {
        ASTNode *s = ast_node_new(AST_RETURN_STMT, make_loc(@1.first_line, @1.first_column));
        s->as.return_stmt.expr = $2;
        $$ = s;
    }
    | TOK_RETURN ';' {
        ASTNode *s = ast_node_new(AST_RETURN_STMT, make_loc(@1.first_line, @1.first_column));
        s->as.return_stmt.expr = NULL;
        $$ = s;
    }
    ;

halt_stmt
    : TOK_HALT ';' {
        ASTNode *s = ast_node_new(AST_HALT_STMT, make_loc(@1.first_line, @1.first_column));
        $$ = s;
    }
    ;

expression
    : assignment_expr
    ;

assignment_expr
    : TOK_IDENT '=' assignment_expr {
        ASTNode *a = ast_node_new(AST_ASSIGN_EXPR, make_loc(@1.first_line, @1.first_column));
        a->as.assign_expr.var_name = $1;
        a->as.assign_expr.value = $3;
        $$ = a;
    }
    | TOK_IDENT '[' expression ']' '=' assignment_expr {
        ASTNode *a = ast_node_new(AST_INDEX_ASSIGN_EXPR, make_loc(@1.first_line, @1.first_column));
        a->as.index_assign_expr.array_name = $1;
        a->as.index_assign_expr.index = $3;
        a->as.index_assign_expr.value = $6;
        $$ = a;
    }
    | logical_or_expr
    ;

logical_or_expr
    : logical_and_expr
    ;

logical_and_expr
    : equality_expr
    ;

equality_expr
    : relational_expr
    | equality_expr TOK_EQ relational_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_EQ;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    | equality_expr TOK_NEQ relational_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_NEQ;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    ;

relational_expr
    : additive_expr
    | relational_expr '<' additive_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_LT;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    | relational_expr '>' additive_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_GT;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    | relational_expr TOK_LTE additive_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_LTE;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    | relational_expr TOK_GTE additive_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_GTE;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    ;

additive_expr
    : multiplicative_expr
    | additive_expr '+' multiplicative_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_ADD;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    | additive_expr '-' multiplicative_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_SUB;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    ;

multiplicative_expr
    : unary_expr
    | multiplicative_expr '*' unary_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_MUL;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    | multiplicative_expr '/' unary_expr {
        ASTNode *b = ast_node_new(AST_BINARY_EXPR, make_loc(@2.first_line, @2.first_column));
        b->as.binary_expr.op = OP_DIV;
        b->as.binary_expr.left = $1;
        b->as.binary_expr.right = $3;
        $$ = b;
    }
    ;

unary_expr
    : primary_expr
    | '-' unary_expr %prec UMINUS {
        ASTNode *u = ast_node_new(AST_UNARY_EXPR, make_loc(@1.first_line, @1.first_column));
        u->as.unary_expr.op = OP_NEG;
        u->as.unary_expr.operand = $2;
        $$ = u;
    }
    | '!' unary_expr {
        ASTNode *u = ast_node_new(AST_UNARY_EXPR, make_loc(@1.first_line, @1.first_column));
        u->as.unary_expr.op = OP_NOT;
        u->as.unary_expr.operand = $2;
        $$ = u;
    }
    ;

primary_expr
    : TOK_IDENT {
        ASTNode *id = ast_node_new(AST_IDENTIFIER, make_loc(@1.first_line, @1.first_column));
        id->as.identifier.name = $1;
        id->as.identifier.register_hint = -1;
        $$ = id;
    }
    | TOK_INT_LIT {
        ASTNode *lit = ast_node_new(AST_LITERAL_INT, make_loc(@1.first_line, @1.first_column));
        lit->as.literal_int.value = $1;
        $$ = lit;
    }
    | TOK_FLOAT_LIT {
        ASTNode *lit = ast_node_new(AST_LITERAL_FLOAT, make_loc(@1.first_line, @1.first_column));
        lit->as.literal_float.value = $1;
        $$ = lit;
    }
    | TOK_IDENT '[' expression ']' {
        ASTNode *idx = ast_node_new(AST_INDEX_EXPR, make_loc(@1.first_line, @1.first_column));
        idx->as.index_expr.array_name = $1;
        idx->as.index_expr.index = $3;
        $$ = idx;
    }
    | call_expr
    | '(' expression ')' {
        $$ = $2;
    }
    ;

call_expr
    : TOK_IDENT '(' argument_list_opt ')' {
        ASTNode *c = ast_node_new(AST_CALL_EXPR, make_loc(@1.first_line, @1.first_column));
        c->as.call_expr.callee = $1;
        c->as.call_expr.arguments = $3;
        $$ = c;
    }
    ;

argument_list_opt
    : /* empty */ { $$ = NULL; }
    | argument_list
    ;

argument_list
    : expression {
        ASTNodeList *l = NULL;
        ast_node_list_append(&l, $1);
        $$ = l;
    }
    | argument_list ',' expression {
        ast_node_list_append(&$1, $3);
        $$ = $1;
    }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Syntax Error: %s at line %d, col %d\n", s, current_line, current_column);
}
