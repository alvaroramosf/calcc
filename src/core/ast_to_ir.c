/**
 * @file ast_to_ir.c
 * @brief Traversal of the AST and emission of 3-Address Code instructions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast_to_ir.h"

#define MAX_FUNCTIONS 32
#define MAX_ARRAYS 16

typedef struct {
    char name[64];
    IROperand label;
    int param_count;
    char param_names[8][64];
    bool is_main;
    const ASTNode *decl_node;
} FnInfo;

static FnInfo g_functions[MAX_FUNCTIONS];
static int g_function_count = 0;
static bool g_current_is_main = false;

typedef struct {
    char name[64];
    int size;
} ArraySymbol;

static ArraySymbol g_arrays[MAX_ARRAYS];
static int g_array_count = 0;

static FnInfo *find_function(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_function_count; i++) {
        if (strcmp(g_functions[i].name, name) == 0) {
            return &g_functions[i];
        }
    }
    return NULL;
}

static IROperand lower_expression(IRProgram *prog, const ASTNode *node);
static void lower_statement(IRProgram *prog, const ASTNode *node);

static void lower_condition_and_branch(IRProgram *prog, const ASTNode *cond, IROperand l_true, IROperand l_false, int line) {
    if (!cond) return;

    if (cond->type == AST_BINARY_EXPR) {
        IROpcode jmp_op = IR_NOP;
        switch (cond->as.binary_expr.op) {
            case OP_EQ:  jmp_op = IR_JEQ; break;
            case OP_NEQ: jmp_op = IR_JNE; break;
            case OP_LT:  jmp_op = IR_JLT; break;
            case OP_LTE: jmp_op = IR_JLE; break;
            case OP_GT:  jmp_op = IR_JGT; break;
            case OP_GTE: jmp_op = IR_JGE; break;
            default: break;
        }

        if (jmp_op != IR_NOP) {
            IROperand left = lower_expression(prog, cond->as.binary_expr.left);
            IROperand right = lower_expression(prog, cond->as.binary_expr.right);
            ir_emit(prog, jmp_op, l_true, left, right, line);
            ir_emit(prog, IR_JUMP, l_false, ir_op_none(), ir_op_none(), line);
            return;
        }
    }

    IROperand val = lower_expression(prog, cond);
    ir_emit(prog, IR_JNE, l_true, val, ir_op_const_int(0), line);
    ir_emit(prog, IR_JUMP, l_false, ir_op_none(), ir_op_none(), line);
}

static IROperand lower_expression(IRProgram *prog, const ASTNode *node) {
    if (!node) return ir_op_none();

    switch (node->type) {
        case AST_LITERAL_INT:
            return ir_op_const_int(node->as.literal_int.value);

        case AST_LITERAL_FLOAT:
            return ir_op_const_float(node->as.literal_float.value);

        case AST_IDENTIFIER:
            return ir_op_var(node->as.identifier.name);

        case AST_ASSIGN_EXPR: {
            IROperand val = lower_expression(prog, node->as.assign_expr.value);
            IROperand target_var = ir_op_var(node->as.assign_expr.var_name);
            ir_emit(prog, IR_ASSIGN, target_var, val, ir_op_none(), node->loc.line);
            return target_var;
        }

        case AST_BINARY_EXPR: {
            OperatorType op_type = node->as.binary_expr.op;
            if (op_type == OP_EQ || op_type == OP_NEQ ||
                op_type == OP_LT || op_type == OP_LTE ||
                op_type == OP_GT || op_type == OP_GTE) {
                /* Comparison evaluated as boolean value (0 or 1) */
                IROperand dest = ir_alloc_vreg(prog);
                IROperand l_true = ir_alloc_label(prog);
                IROperand l_end = ir_alloc_label(prog);

                ir_emit(prog, IR_ASSIGN, dest, ir_op_const_int(0), ir_op_none(), node->loc.line);
                lower_condition_and_branch(prog, node, l_true, l_end, node->loc.line);

                ir_emit(prog, IR_LABEL, l_true, ir_op_none(), ir_op_none(), node->loc.line);
                ir_emit(prog, IR_ASSIGN, dest, ir_op_const_int(1), ir_op_none(), node->loc.line);
                ir_emit(prog, IR_LABEL, l_end, ir_op_none(), ir_op_none(), node->loc.line);
                return dest;
            }

            IROperand left = lower_expression(prog, node->as.binary_expr.left);
            IROperand right = lower_expression(prog, node->as.binary_expr.right);
            IROperand dest = ir_alloc_vreg(prog);

            IROpcode op = IR_ADD;
            switch (op_type) {
                case OP_ADD: op = IR_ADD; break;
                case OP_SUB: op = IR_SUB; break;
                case OP_MUL: op = IR_MUL; break;
                case OP_DIV: op = IR_DIV; break;
                default: break;
            }

            ir_emit(prog, op, dest, left, right, node->loc.line);
            return dest;
        }

        case AST_UNARY_EXPR: {
            IROperand operand = lower_expression(prog, node->as.unary_expr.operand);
            IROperand dest = ir_alloc_vreg(prog);
            ir_emit(prog, IR_NEG, dest, operand, ir_op_none(), node->loc.line);
            return dest;
        }

        case AST_CALL_EXPR: {
            const char *callee = node->as.call_expr.callee;
            if (callee && strcmp(callee, "display") == 0) {
                if (node->as.call_expr.arguments && node->as.call_expr.arguments->node) {
                    IROperand arg = lower_expression(prog, node->as.call_expr.arguments->node);
                    ir_emit(prog, IR_DISPLAY, ir_op_none(), arg, ir_op_none(), node->loc.line);
                }
                return ir_op_none();
            } else if (callee && strcmp(callee, "input") == 0) {
                IROperand dest = ir_alloc_vreg(prog);
                ir_emit(prog, IR_INPUT, dest, ir_op_none(), ir_op_none(), node->loc.line);
                return dest;
            } else if (callee && strcmp(callee, "pause") == 0) {
                ir_emit(prog, IR_PAUSE, ir_op_none(), ir_op_none(), ir_op_none(), node->loc.line);
                return ir_op_none();
            } else {
                /* User-defined subroutine call */
                FnInfo *fn = find_function(callee);
                if (fn) {
                    ASTNodeList *arg = node->as.call_expr.arguments;
                    for (int p = 0; p < fn->param_count && arg; p++, arg = arg->next) {
                        IROperand arg_val = lower_expression(prog, arg->node);
                        IROperand param_var = ir_op_var(fn->param_names[p]);
                        ir_emit(prog, IR_ASSIGN, param_var, arg_val, ir_op_none(), node->loc.line);
                    }
                    IROperand ret_dest = ir_alloc_vreg(prog);
                    ir_emit(prog, IR_CALL, ret_dest, fn->label, ir_op_none(), node->loc.line);
                    return ret_dest;
                }
            }
            return ir_op_none();
        }

        case AST_INDEX_EXPR: {
            const char *arr_name = node->as.index_expr.array_name;
            const ASTNode *idx_node = node->as.index_expr.index;
            if (idx_node->type == AST_LITERAL_INT) {
                char elem_name[64];
                snprintf(elem_name, sizeof(elem_name), "%s_%ld", arr_name, (long)idx_node->as.literal_int.value);
                return ir_op_var(elem_name);
            } else {
                IROperand idx_op = lower_expression(prog, idx_node);
                IROperand dest = ir_alloc_vreg(prog);
                ir_emit(prog, IR_LOAD_INDIRECT, dest, idx_op, ir_op_none(), node->loc.line);
                return dest;
            }
        }

        case AST_INDEX_ASSIGN_EXPR: {
            const char *arr_name = node->as.index_assign_expr.array_name;
            const ASTNode *idx_node = node->as.index_assign_expr.index;
            IROperand val = lower_expression(prog, node->as.index_assign_expr.value);
            if (idx_node->type == AST_LITERAL_INT) {
                char elem_name[64];
                snprintf(elem_name, sizeof(elem_name), "%s_%ld", arr_name, (long)idx_node->as.literal_int.value);
                IROperand target_elem = ir_op_var(elem_name);
                ir_emit(prog, IR_ASSIGN, target_elem, val, ir_op_none(), node->loc.line);
                return target_elem;
            } else {
                IROperand idx_op = lower_expression(prog, idx_node);
                ir_emit(prog, IR_STORE_INDIRECT, idx_op, val, ir_op_none(), node->loc.line);
                return val;
            }
        }

        default:
            return ir_op_none();
    }
}

static bool has_assignment_to_var(const ASTNode *node, const char *var_name) {
    if (!node || !var_name) return false;
    switch (node->type) {
        case AST_ASSIGN_EXPR:
            if (node->as.assign_expr.var_name && strcmp(node->as.assign_expr.var_name, var_name) == 0) {
                return true;
            }
            return has_assignment_to_var(node->as.assign_expr.value, var_name);
        case AST_INDEX_ASSIGN_EXPR:
            if (node->as.index_assign_expr.array_name && strcmp(node->as.index_assign_expr.array_name, var_name) == 0) {
                return true;
            }
            return has_assignment_to_var(node->as.index_assign_expr.index, var_name) ||
                   has_assignment_to_var(node->as.index_assign_expr.value, var_name);
        case AST_INDEX_EXPR:
            return has_assignment_to_var(node->as.index_expr.index, var_name);
        case AST_VAR_DECL:
            if (node->as.var_decl.name && strcmp(node->as.var_decl.name, var_name) == 0) {
                return true;
            }
            return has_assignment_to_var(node->as.var_decl.init_expr, var_name);
        case AST_BLOCK_STMT:
            for (ASTNodeList *l = node->as.block_stmt.statements; l; l = l->next) {
                if (has_assignment_to_var(l->node, var_name)) return true;
            }
            return false;
        case AST_EXPR_STMT:
            return has_assignment_to_var(node->as.expr_stmt.expr, var_name);
        case AST_IF_STMT:
            return has_assignment_to_var(node->as.if_stmt.condition, var_name) ||
                   has_assignment_to_var(node->as.if_stmt.then_branch, var_name) ||
                   has_assignment_to_var(node->as.if_stmt.else_branch, var_name);
        case AST_WHILE_STMT:
            return has_assignment_to_var(node->as.while_stmt.condition, var_name) ||
                   has_assignment_to_var(node->as.while_stmt.body, var_name);
        case AST_BINARY_EXPR:
            return has_assignment_to_var(node->as.binary_expr.left, var_name) ||
                   has_assignment_to_var(node->as.binary_expr.right, var_name);
        case AST_UNARY_EXPR:
            return has_assignment_to_var(node->as.unary_expr.operand, var_name);
        default:
            return false;
    }
}

static bool is_hardware_loop_candidate(const ASTNode *while_node, const char **out_var_name, ASTNodeList **out_last_stmt_node) {
    if (!while_node || while_node->type != AST_WHILE_STMT) return false;

    /* Condition must be: var > 0 or var != 0 */
    const ASTNode *cond = while_node->as.while_stmt.condition;
    if (!cond || cond->type != AST_BINARY_EXPR) return false;
    if (cond->as.binary_expr.op != OP_GT && cond->as.binary_expr.op != OP_NEQ) return false;

    const ASTNode *left = cond->as.binary_expr.left;
    const ASTNode *right = cond->as.binary_expr.right;
    if (!left || left->type != AST_IDENTIFIER) return false;
    if (!right || right->type != AST_LITERAL_INT || right->as.literal_int.value != 0) return false;

    const char *var = left->as.identifier.name;
    if (!var) return false;

    /* Body must be a block statement */
    const ASTNode *body = while_node->as.while_stmt.body;
    if (!body || body->type != AST_BLOCK_STMT) return false;
    if (!body->as.block_stmt.statements) return false;

    /* Find the last statement in the block */
    ASTNodeList *last = NULL;
    for (ASTNodeList *l = body->as.block_stmt.statements; l; l = l->next) {
        last = l;
    }
    if (!last || !last->node) return false;

    /* The last statement must be: var = var - 1; */
    if (last->node->type != AST_EXPR_STMT) return false;
    const ASTNode *expr = last->node->as.expr_stmt.expr;
    if (!expr || expr->type != AST_ASSIGN_EXPR) return false;
    if (!expr->as.assign_expr.var_name || strcmp(expr->as.assign_expr.var_name, var) != 0) return false;

    const ASTNode *val = expr->as.assign_expr.value;
    if (!val || val->type != AST_BINARY_EXPR) return false;
    if (val->as.binary_expr.op != OP_SUB) return false;
    if (!val->as.binary_expr.left || val->as.binary_expr.left->type != AST_IDENTIFIER) return false;
    if (strcmp(val->as.binary_expr.left->as.identifier.name, var) != 0) return false;
    if (!val->as.binary_expr.right || val->as.binary_expr.right->type != AST_LITERAL_INT) return false;
    if (val->as.binary_expr.right->as.literal_int.value != 1) return false;

    /* Verify that 'var' is NOT modified anywhere else in the body prior to the last statement */
    for (ASTNodeList *l = body->as.block_stmt.statements; l && l != last; l = l->next) {
        if (has_assignment_to_var(l->node, var)) {
            return false;
        }
    }

    if (out_var_name) *out_var_name = var;
    if (out_last_stmt_node) *out_last_stmt_node = last;
    return true;
}

static void lower_statement(IRProgram *prog, const ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_BLOCK_STMT:
            for (ASTNodeList *l = node->as.block_stmt.statements; l; l = l->next) {
                lower_statement(prog, l->node);
            }
            break;

        case AST_VAR_DECL:
            if (node->as.var_decl.array_size > 0) {
                if (g_array_count < MAX_ARRAYS) {
                    ArraySymbol *arr = &g_arrays[g_array_count++];
                    snprintf(arr->name, sizeof(arr->name), "%s", node->as.var_decl.name);
                    arr->size = node->as.var_decl.array_size;
                }
            } else if (node->as.var_decl.init_expr) {
                IROperand init_val = lower_expression(prog, node->as.var_decl.init_expr);
                IROperand var = ir_op_var(node->as.var_decl.name);
                ir_emit(prog, IR_ASSIGN, var, init_val, ir_op_none(), node->loc.line);
            }
            break;

        case AST_EXPR_STMT:
            if (node->as.expr_stmt.expr) {
                lower_expression(prog, node->as.expr_stmt.expr);
            }
            break;

        case AST_IF_STMT: {
            IROperand l_then = ir_alloc_label(prog);
            IROperand l_else = ir_alloc_label(prog);
            IROperand l_end = ir_alloc_label(prog);

            lower_condition_and_branch(prog, node->as.if_stmt.condition, l_then, l_else, node->loc.line);

            ir_emit(prog, IR_LABEL, l_then, ir_op_none(), ir_op_none(), node->loc.line);
            lower_statement(prog, node->as.if_stmt.then_branch);
            ir_emit(prog, IR_JUMP, l_end, ir_op_none(), ir_op_none(), node->loc.line);

            ir_emit(prog, IR_LABEL, l_else, ir_op_none(), ir_op_none(), node->loc.line);
            if (node->as.if_stmt.else_branch) {
                lower_statement(prog, node->as.if_stmt.else_branch);
            }
            ir_emit(prog, IR_LABEL, l_end, ir_op_none(), ir_op_none(), node->loc.line);
            break;
        }

        case AST_WHILE_STMT: {
            const char *loop_var_name = NULL;
            ASTNodeList *last_stmt_node = NULL;
            if (is_hardware_loop_candidate(node, &loop_var_name, &last_stmt_node)) {
                IROperand l_body = ir_alloc_label(prog);
                IROperand loop_var = ir_op_var(loop_var_name);

                ir_emit(prog, IR_LABEL, l_body, ir_op_none(), ir_op_none(), node->loc.line);
                for (ASTNodeList *l = node->as.while_stmt.body->as.block_stmt.statements; l; l = l->next) {
                    if (l == last_stmt_node) break; /* Skip the explicit decrement */
                    lower_statement(prog, l->node);
                }
                ir_emit(prog, IR_LOOP, l_body, loop_var, ir_op_none(), node->loc.line);
                break;
            }

            IROperand l_start = ir_alloc_label(prog);
            IROperand l_body = ir_alloc_label(prog);
            IROperand l_end = ir_alloc_label(prog);

            ir_emit(prog, IR_LABEL, l_start, ir_op_none(), ir_op_none(), node->loc.line);
            lower_condition_and_branch(prog, node->as.while_stmt.condition, l_body, l_end, node->loc.line);

            ir_emit(prog, IR_LABEL, l_body, ir_op_none(), ir_op_none(), node->loc.line);
            lower_statement(prog, node->as.while_stmt.body);
            ir_emit(prog, IR_JUMP, l_start, ir_op_none(), ir_op_none(), node->loc.line);

            ir_emit(prog, IR_LABEL, l_end, ir_op_none(), ir_op_none(), node->loc.line);
            break;
        }

        case AST_RETURN_STMT: {
            IROperand ret_val = ir_op_none();
            if (node->as.return_stmt.expr) {
                ret_val = lower_expression(prog, node->as.return_stmt.expr);
            }
            if (g_current_is_main) {
                ir_emit(prog, IR_HALT, ir_op_none(), ret_val, ir_op_none(), node->loc.line);
            } else {
                ir_emit(prog, IR_RETURN, ir_op_none(), ret_val, ir_op_none(), node->loc.line);
            }
            break;
        }

        case AST_HALT_STMT:
            ir_emit(prog, IR_HALT, ir_op_none(), ir_op_none(), ir_op_none(), node->loc.line);
            break;

        default:
            break;
    }
}

static void find_callees_in_ast(const ASTNode *node, const char *callees[], int *count, int max_callees) {
    if (!node || *count >= max_callees) return;
    if (node->type == AST_CALL_EXPR) {
        const char *name = node->as.call_expr.callee;
        if (name && strcmp(name, "display") != 0 &&
            strcmp(name, "input") != 0 &&
            strcmp(name, "pause") != 0) {
            bool exists = false;
            for (int i = 0; i < *count; i++) {
                if (strcmp(callees[i], name) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists && *count < max_callees) {
                callees[(*count)++] = name;
            }
        }
        for (ASTNodeList *arg = node->as.call_expr.arguments; arg; arg = arg->next) {
            find_callees_in_ast(arg->node, callees, count, max_callees);
        }
        return;
    }
    switch (node->type) {
        case AST_BLOCK_STMT:
            for (ASTNodeList *l = node->as.block_stmt.statements; l; l = l->next) {
                find_callees_in_ast(l->node, callees, count, max_callees);
            }
            break;
        case AST_EXPR_STMT:
            find_callees_in_ast(node->as.expr_stmt.expr, callees, count, max_callees);
            break;
        case AST_IF_STMT:
            find_callees_in_ast(node->as.if_stmt.condition, callees, count, max_callees);
            find_callees_in_ast(node->as.if_stmt.then_branch, callees, count, max_callees);
            find_callees_in_ast(node->as.if_stmt.else_branch, callees, count, max_callees);
            break;
        case AST_WHILE_STMT:
            find_callees_in_ast(node->as.while_stmt.condition, callees, count, max_callees);
            find_callees_in_ast(node->as.while_stmt.body, callees, count, max_callees);
            break;
        case AST_RETURN_STMT:
            find_callees_in_ast(node->as.return_stmt.expr, callees, count, max_callees);
            break;
        case AST_VAR_DECL:
            find_callees_in_ast(node->as.var_decl.init_expr, callees, count, max_callees);
            break;
        case AST_BINARY_EXPR:
            find_callees_in_ast(node->as.binary_expr.left, callees, count, max_callees);
            find_callees_in_ast(node->as.binary_expr.right, callees, count, max_callees);
            break;
        case AST_UNARY_EXPR:
            find_callees_in_ast(node->as.unary_expr.operand, callees, count, max_callees);
            break;
        case AST_ASSIGN_EXPR:
            find_callees_in_ast(node->as.assign_expr.value, callees, count, max_callees);
            break;
        case AST_INDEX_ASSIGN_EXPR:
            find_callees_in_ast(node->as.index_assign_expr.index, callees, count, max_callees);
            find_callees_in_ast(node->as.index_assign_expr.value, callees, count, max_callees);
            break;
        case AST_INDEX_EXPR:
            find_callees_in_ast(node->as.index_expr.index, callees, count, max_callees);
            break;
        default:
            break;
    }
}

static int check_call_graph(const char *fn_name, int depth, bool in_path[], char error_buf[], size_t error_buf_sz) {
    FnInfo *fn = find_function(fn_name);
    if (!fn) return depth;

    int fn_idx = (int)(fn - &g_functions[0]);
    if (in_path[fn_idx]) {
        snprintf(error_buf, error_buf_sz,
                 "Recursive function call detected in '%s'. MK-56 hardware lacks dynamic stack frames and does not support recursion.",
                 fn_name);
        return -1;
    }

    if (depth > 5) {
        snprintf(error_buf, error_buf_sz,
                 "Call stack depth (%d) exceeds MK-56 hardware capacity of 5 levels.",
                 depth);
        return -1;
    }

    in_path[fn_idx] = true;

    const char *callees[32];
    int callee_count = 0;
    find_callees_in_ast(fn->decl_node->as.function_decl.body, callees, &callee_count, 32);

    int max_child_depth = depth;
    for (int i = 0; i < callee_count; i++) {
        int d = check_call_graph(callees[i], depth + 1, in_path, error_buf, error_buf_sz);
        if (d < 0) {
            in_path[fn_idx] = false;
            return -1;
        }
        if (d > max_child_depth) max_child_depth = d;
    }

    in_path[fn_idx] = false;
    return max_child_depth;
}

IRProgram *ast_to_ir(const ASTNode *ast) {
    if (!ast) return NULL;

    IRProgram *prog = ir_program_new();
    g_function_count = 0;
    g_array_count = 0;

    if (ast->type == AST_PROGRAM) {
        /* Pass 1: Collect all function signatures */
        for (ASTNodeList *l = ast->as.program.declarations; l; l = l->next) {
            ASTNode *decl = l->node;
            if (decl->type == AST_FUNCTION_DECL) {
                if (g_function_count < MAX_FUNCTIONS) {
                    FnInfo *fn = &g_functions[g_function_count++];
                    snprintf(fn->name, sizeof(fn->name), "%s", decl->as.function_decl.name);
                    fn->label = ir_alloc_label(prog);
                    fn->is_main = (strcmp(fn->name, "main") == 0);
                    fn->decl_node = decl;
                    fn->param_count = 0;
                    for (ASTNodeList *p = decl->as.function_decl.parameters; p && fn->param_count < 8; p = p->next) {
                        snprintf(fn->param_names[fn->param_count++], 64, "%s", p->node->as.var_decl.name);
                    }
                }
            }
        }

        /* Static Call Graph Verification: enforce MK-56 5-level call stack limit & recursion check */
        FnInfo *main_fn = find_function("main");
        if (main_fn) {
            bool in_path[MAX_FUNCTIONS] = {false};
            char call_err[256] = {0};
            if (check_call_graph("main", 0, in_path, call_err, sizeof(call_err)) < 0) {
                fprintf(stderr, "Error: %s\n", call_err);
                ir_program_free(prog);
                return NULL;
            }
        }

        /* Check entry point: if main exists and is not the first function, emit jump to main */
        if (main_fn && g_function_count > 1 && &g_functions[0] != main_fn) {
            ir_emit(prog, IR_JUMP, main_fn->label, ir_op_none(), ir_op_none(), 1);
        }

        /* Pass 2: Lower declarations */
        for (ASTNodeList *l = ast->as.program.declarations; l; l = l->next) {
            ASTNode *decl = l->node;
            if (decl->type == AST_FUNCTION_DECL) {
                FnInfo *fn = find_function(decl->as.function_decl.name);
                if (fn) {
                    g_current_is_main = fn->is_main;
                    ir_emit(prog, IR_LABEL, fn->label, ir_op_none(), ir_op_none(), decl->loc.line);
                    lower_statement(prog, decl->as.function_decl.body);

                    /* Ensure function has appropriate epilogue */
                    if (prog->tail && prog->tail->opcode != IR_HALT && prog->tail->opcode != IR_RETURN) {
                        if (fn->is_main) {
                            ir_emit(prog, IR_HALT, ir_op_none(), ir_op_none(), ir_op_none(), decl->loc.line);
                        } else {
                            ir_emit(prog, IR_RETURN, ir_op_none(), ir_op_none(), ir_op_none(), decl->loc.line);
                        }
                    }
                }
            } else if (decl->type == AST_VAR_DECL) {
                lower_statement(prog, decl);
            }
        }
    } else {
        g_current_is_main = true;
        lower_statement(prog, ast);
        if (prog->tail && prog->tail->opcode != IR_HALT && prog->tail->opcode != IR_RETURN) {
            ir_emit(prog, IR_HALT, ir_op_none(), ir_op_none(), ir_op_none(), ast->loc.line);
        }
    }

    return prog;
}
