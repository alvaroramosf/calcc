/**
 * @file ast.h
 * @brief Abstract Syntax Tree (AST) definitions for the Micro C transpiler.
 *
 * Defines the node types, operator enumerations, and structures that represent
 * parsed Micro C code prior to intermediate representation (IR) lowering and
 * calculator bytecode/key-sequence code generation.
 */

#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Categorization of AST node types.
 */
typedef enum {
    /* Declarations and Top-Level */
    AST_PROGRAM,
    AST_FUNCTION_DECL,
    AST_VAR_DECL,

    /* Statements */
    AST_BLOCK_STMT,
    AST_EXPR_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_RETURN_STMT,
    AST_HALT_STMT,

    /* Expressions */
    AST_ASSIGN_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,        /**< arr[index] */
    AST_INDEX_ASSIGN_EXPR, /**< arr[index] = value */
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_IDENTIFIER
} ASTNodeType;

/**
 * @brief Binary and unary operators supported in Micro C.
 */
typedef enum {
    OP_NONE = 0,

    /* Arithmetic Operators */
    OP_ADD,         /* + */
    OP_SUB,         /* - */
    OP_MUL,         /* * */
    OP_DIV,         /* / */

    /* Relational Operators */
    OP_EQ,          /* == */
    OP_NEQ,         /* != */
    OP_LT,          /* <  */
    OP_LTE,         /* <= */
    OP_GT,          /* >  */
    OP_GTE,         /* >= */

    /* Unary Operators */
    OP_NEG,         /* - (unary negation) */
    OP_NOT,         /* ! */

    /* Assignment Operators */
    OP_ASSIGN       /* = */
} OperatorType;

/**
 * @brief Source coordinate for error diagnostics.
 */
typedef struct {
    int line;
    int column;
    const char *filename;
} SourceLocation;

/* Forward declaration */
typedef struct ASTNode ASTNode;

/**
 * @brief Node list for statement sequences and argument lists.
 */
typedef struct ASTNodeList {
    ASTNode *node;
    struct ASTNodeList *next;
} ASTNodeList;

/**
 * @brief Generic Abstract Syntax Tree Node structure.
 */
struct ASTNode {
    ASTNodeType type;
    SourceLocation loc;

    union {
        /* AST_PROGRAM: List of functions / global declarations */
        struct {
            ASTNodeList *declarations;
        } program;

        /* AST_FUNCTION_DECL */
        struct {
            char *name;
            ASTNodeList *parameters;
            ASTNode *body;
        } function_decl;

        /* AST_VAR_DECL */
        struct {
            char *name;
            ASTNode *init_expr;
            int assigned_register; /* Assigned hardware register ID (-1 if unassigned) */
            int array_size;        /* 0 for scalar variable, >0 for array capacity */
        } var_decl;

        /* AST_BLOCK_STMT */
        struct {
            ASTNodeList *statements;
        } block_stmt;

        /* AST_EXPR_STMT */
        struct {
            ASTNode *expr;
        } expr_stmt;

        /* AST_IF_STMT */
        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_stmt;

        /* AST_WHILE_STMT */
        struct {
            ASTNode *condition;
            ASTNode *body;
        } while_stmt;

        /* AST_FOR_STMT */
        struct {
            ASTNode *init;
            ASTNode *condition;
            ASTNode *step;
            ASTNode *body;
        } for_stmt;

        /* AST_RETURN_STMT */
        struct {
            ASTNode *expr;
        } return_stmt;

        /* AST_ASSIGN_EXPR */
        struct {
            char *var_name;
            ASTNode *value;
        } assign_expr;

        /* AST_BINARY_EXPR */
        struct {
            OperatorType op;
            ASTNode *left;
            ASTNode *right;
        } binary_expr;

        /* AST_UNARY_EXPR */
        struct {
            OperatorType op;
            ASTNode *operand;
        } unary_expr;

        /* AST_CALL_EXPR */
        struct {
            char *callee;
            ASTNodeList *arguments;
        } call_expr;

        /* AST_INDEX_EXPR */
        struct {
            char *array_name;
            ASTNode *index;
        } index_expr;

        /* AST_INDEX_ASSIGN_EXPR */
        struct {
            char *array_name;
            ASTNode *index;
            ASTNode *value;
        } index_assign_expr;

        /* AST_LITERAL_INT */
        struct {
            int64_t value;
        } literal_int;

        /* AST_LITERAL_FLOAT */
        struct {
            double value;
        } literal_float;

        /* AST_IDENTIFIER */
        struct {
            char *name;
            int register_hint; /* Memory register hint if specified */
        } identifier;
    } as;
};

/* AST Construction & Lifetime Management Prototypes */

/**
 * @brief Allocates and initializes a new AST node.
 * @param type Type of AST node to create.
 * @param loc Source location for diagnostics.
 * @return Pointer to newly allocated ASTNode or NULL on failure.
 */
ASTNode *ast_node_new(ASTNodeType type, SourceLocation loc);

/**
 * @brief Recursively frees an AST node and all children.
 * @param node Root of the tree/subtree to free.
 */
void ast_node_free(ASTNode *node);

/**
 * @brief Appends a node to an ASTNodeList.
 * @param list Pointer to the head pointer of the list.
 * @param node Node to append.
 */
void ast_node_list_append(ASTNodeList **list, ASTNode *node);

/**
 * @brief Frees an ASTNodeList (and optionally the contained nodes).
 * @param list Head of list to free.
 * @param free_nodes If true, recursively frees contained nodes as well.
 */
void ast_node_list_free(ASTNodeList *list, bool free_nodes);

/**
 * @brief Prints an ASCII representation of the AST for debugging.
 * @param node Root node to display.
 * @param indent Current indentation depth.
 */
void ast_dump(const ASTNode *node, int indent);

#endif /* AST_H */
