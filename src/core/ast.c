/**
 * @file ast.c
 * @brief AST node allocation, traversal, and cleanup functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode *ast_node_new(ASTNodeType type, SourceLocation loc) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!node) {
        perror("Failed to allocate ASTNode");
        return NULL;
    }
    node->type = type;
    node->loc = loc;
    return node;
}

void ast_node_list_append(ASTNodeList **list, ASTNode *node) {
    if (!list || !node) return;
    ASTNodeList *new_item = (ASTNodeList *)calloc(1, sizeof(ASTNodeList));
    if (!new_item) return;
    new_item->node = node;
    new_item->next = NULL;

    if (!*list) {
        *list = new_item;
    } else {
        ASTNodeList *curr = *list;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_item;
    }
}

void ast_node_list_free(ASTNodeList *list, bool free_nodes) {
    while (list) {
        ASTNodeList *next = list->next;
        if (free_nodes && list->node) {
            ast_node_free(list->node);
        }
        free(list);
        list = next;
    }
}

void ast_node_free(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            ast_node_list_free(node->as.program.declarations, true);
            break;
        case AST_FUNCTION_DECL:
            free(node->as.function_decl.name);
            ast_node_list_free(node->as.function_decl.parameters, true);
            ast_node_free(node->as.function_decl.body);
            break;
        case AST_VAR_DECL:
            free(node->as.var_decl.name);
            ast_node_free(node->as.var_decl.init_expr);
            break;
        case AST_BLOCK_STMT:
            ast_node_list_free(node->as.block_stmt.statements, true);
            break;
        case AST_EXPR_STMT:
            ast_node_free(node->as.expr_stmt.expr);
            break;
        case AST_IF_STMT:
            ast_node_free(node->as.if_stmt.condition);
            ast_node_free(node->as.if_stmt.then_branch);
            ast_node_free(node->as.if_stmt.else_branch);
            break;
        case AST_WHILE_STMT:
            ast_node_free(node->as.while_stmt.condition);
            ast_node_free(node->as.while_stmt.body);
            break;
        case AST_FOR_STMT:
            ast_node_free(node->as.for_stmt.init);
            ast_node_free(node->as.for_stmt.condition);
            ast_node_free(node->as.for_stmt.step);
            ast_node_free(node->as.for_stmt.body);
            break;
        case AST_RETURN_STMT:
            ast_node_free(node->as.return_stmt.expr);
            break;
        case AST_ASSIGN_EXPR:
            free(node->as.assign_expr.var_name);
            ast_node_free(node->as.assign_expr.value);
            break;
        case AST_BINARY_EXPR:
            ast_node_free(node->as.binary_expr.left);
            ast_node_free(node->as.binary_expr.right);
            break;
        case AST_UNARY_EXPR:
            ast_node_free(node->as.unary_expr.operand);
            break;
        case AST_CALL_EXPR:
            free(node->as.call_expr.callee);
            ast_node_list_free(node->as.call_expr.arguments, true);
            break;
        case AST_INDEX_EXPR:
            free(node->as.index_expr.array_name);
            ast_node_free(node->as.index_expr.index);
            break;
        case AST_INDEX_ASSIGN_EXPR:
            free(node->as.index_assign_expr.array_name);
            ast_node_free(node->as.index_assign_expr.index);
            ast_node_free(node->as.index_assign_expr.value);
            break;
        case AST_IDENTIFIER:
            free(node->as.identifier.name);
            break;
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_HALT_STMT:
        default:
            break;
    }
    free(node);
}

void ast_dump(const ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->type) {
        case AST_PROGRAM:
            printf("Program (line %d)\n", node->loc.line);
            for (ASTNodeList *l = node->as.program.declarations; l; l = l->next) {
                ast_dump(l->node, indent + 1);
            }
            break;
        case AST_FUNCTION_DECL:
            printf("FunctionDecl: %s\n", node->as.function_decl.name);
            ast_dump(node->as.function_decl.body, indent + 1);
            break;
        case AST_VAR_DECL:
            printf("VarDecl: %s (reg: %d)\n", node->as.var_decl.name, node->as.var_decl.assigned_register);
            if (node->as.var_decl.init_expr) {
                ast_dump(node->as.var_decl.init_expr, indent + 1);
            }
            break;
        case AST_BLOCK_STMT:
            printf("Block\n");
            for (ASTNodeList *l = node->as.block_stmt.statements; l; l = l->next) {
                ast_dump(l->node, indent + 1);
            }
            break;
        case AST_EXPR_STMT:
            printf("ExprStmt\n");
            ast_dump(node->as.expr_stmt.expr, indent + 1);
            break;
        case AST_IF_STMT:
            printf("IfStmt\n");
            ast_dump(node->as.if_stmt.condition, indent + 1);
            ast_dump(node->as.if_stmt.then_branch, indent + 1);
            if (node->as.if_stmt.else_branch) ast_dump(node->as.if_stmt.else_branch, indent + 1);
            break;
        case AST_WHILE_STMT:
            printf("WhileStmt\n");
            ast_dump(node->as.while_stmt.condition, indent + 1);
            ast_dump(node->as.while_stmt.body, indent + 1);
            break;
        case AST_FOR_STMT:
            printf("ForStmt\n");
            if (node->as.for_stmt.init) ast_dump(node->as.for_stmt.init, indent + 1);
            if (node->as.for_stmt.condition) ast_dump(node->as.for_stmt.condition, indent + 1);
            if (node->as.for_stmt.step) ast_dump(node->as.for_stmt.step, indent + 1);
            ast_dump(node->as.for_stmt.body, indent + 1);
            break;
        case AST_RETURN_STMT:
            printf("ReturnStmt\n");
            if (node->as.return_stmt.expr) ast_dump(node->as.return_stmt.expr, indent + 1);
            break;
        case AST_ASSIGN_EXPR:
            printf("Assign: %s =\n", node->as.assign_expr.var_name);
            ast_dump(node->as.assign_expr.value, indent + 1);
            break;
        case AST_BINARY_EXPR:
            printf("BinaryOp (%d)\n", node->as.binary_expr.op);
            ast_dump(node->as.binary_expr.left, indent + 1);
            ast_dump(node->as.binary_expr.right, indent + 1);
            break;
        case AST_UNARY_EXPR:
            printf("UnaryOp (%d)\n", node->as.unary_expr.op);
            ast_dump(node->as.unary_expr.operand, indent + 1);
            break;
        case AST_CALL_EXPR:
            printf("Call: %s\n", node->as.call_expr.callee);
            for (ASTNodeList *l = node->as.call_expr.arguments; l; l = l->next) {
                ast_dump(l->node, indent + 1);
            }
            break;
        case AST_INDEX_EXPR:
            printf("IndexExpr: %s[...]\n", node->as.index_expr.array_name);
            ast_dump(node->as.index_expr.index, indent + 1);
            break;
        case AST_INDEX_ASSIGN_EXPR:
            printf("IndexAssign: %s[...] =\n", node->as.index_assign_expr.array_name);
            ast_dump(node->as.index_assign_expr.index, indent + 1);
            ast_dump(node->as.index_assign_expr.value, indent + 1);
            break;
        case AST_LITERAL_INT:
            printf("LiteralInt: %ld\n", (long)node->as.literal_int.value);
            break;
        case AST_LITERAL_FLOAT:
            printf("LiteralFloat: %f\n", node->as.literal_float.value);
            break;
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->as.identifier.name);
            break;
        case AST_HALT_STMT:
            printf("HaltStmt\n");
            break;
    }
}
