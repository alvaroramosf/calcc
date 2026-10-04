/**
 * @file ast_to_ir.h
 * @brief AST to Three-Address Code (TAC) Lowering pass.
 */

#ifndef AST_TO_IR_H
#define AST_TO_IR_H

#include "ast.h"
#include "ir.h"

/**
 * @brief Lowers a parsed Abstract Syntax Tree into a linear 3-Address Code program.
 * @param ast Root AST node (typically AST_PROGRAM).
 * @return Newly allocated IRProgram, or NULL on error.
 */
IRProgram *ast_to_ir(const ASTNode *ast);

#endif /* AST_TO_IR_H */
