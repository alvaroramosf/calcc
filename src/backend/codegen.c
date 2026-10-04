/**
 * @file codegen.c
 * @brief Two-Pass Target Code Generator and Label Backpatcher implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"

#define MAX_LABELS 256

static int measure_instruction_steps(const IRInstruction *inst) {
    if (!inst) return 0;

    switch (inst->opcode) {
        case IR_LABEL:
            return 0;

        case IR_ASSIGN:
            if (inst->dest.physical_reg >= 0) {
                if (inst->src1.type == IR_OPERAND_CONST_INT || inst->src1.type == IR_OPERAND_CONST_FLOAT) {
                    return 3; /* constant (1) + B^ (1) + P reg (1) */
                } else if (inst->src1.physical_reg >= 0) {
                    return 2; /* IP src (1) + P dest (1) */
                }
            }
            return 0;

        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV: {
            int cost = 0;
            if (inst->src1.type != IR_OPERAND_STACK) {
                if (inst->src1.type == IR_OPERAND_CONST_INT || inst->src1.type == IR_OPERAND_CONST_FLOAT) cost += 2;
                else if (inst->src1.physical_reg >= 0) cost++;
            }
            if (inst->src2.type != IR_OPERAND_STACK) {
                if (inst->src2.type == IR_OPERAND_CONST_INT || inst->src2.type == IR_OPERAND_CONST_FLOAT) cost += 2;
                else if (inst->src2.physical_reg >= 0) cost++;
            }
            cost++; /* Binop (+, -, *, /) */
            if (inst->dest.type != IR_OPERAND_STACK && inst->dest.physical_reg >= 0) cost++;
            return cost;
        }

        case IR_NEG: {
            int cost = 0;
            if (inst->src1.type != IR_OPERAND_STACK && inst->src1.physical_reg >= 0) cost++;
            cost++; /* /-/ */
            if (inst->dest.type != IR_OPERAND_STACK && inst->dest.physical_reg >= 0) cost++;
            return cost;
        }

        case IR_JUMP:
            return 2; /* BP (1) + target address (1) */

        case IR_JEQ:
        case IR_JNE:
        case IR_JLT:
        case IR_JLE:
        case IR_JGT:
        case IR_JGE:
            if (inst->src2.type == IR_OPERAND_CONST_INT && inst->src2.as.ival == 0) {
                return 3; /* IP src1 (1) + branch (2) */
            } else {
                return 5; /* IP src1 (1) + IP src2 (1) + '-' (1) + branch (2) */
            }

        case IR_LOOP:
            return 2; /* F Lk (1) + target address (1) */

        case IR_INPUT:
            return 2; /* C/P (1) + P dest (1) */

        case IR_DISPLAY:
            return ((inst->src1.type != IR_OPERAND_STACK && inst->src1.physical_reg >= 0) ? 1 : 0) + 1; /* IP src (1) + C/P (1) */

        case IR_PAUSE:
            return 1; /* F Ind (1) */

        case IR_CALL:
            return 2 + ((inst->dest.type != IR_OPERAND_STACK && inst->dest.physical_reg >= 0) ? 1 : 0); /* PP (1) + target (1) + store result (1) */

        case IR_RETURN:
            return ((inst->src1.type != IR_OPERAND_STACK && inst->src1.physical_reg >= 0) ? 1 : 0) + 1; /* IP src (1) + B/O (1) */

        case IR_LOAD_INDIRECT: {
            int cost = 0;
            if (inst->src1.type != IR_OPERAND_STACK) {
                if (inst->src1.type == IR_OPERAND_CONST_INT || inst->src1.type == IR_OPERAND_CONST_FLOAT) cost += 2;
                else if (inst->src1.physical_reg >= 0) cost++;
            }
            cost += 2; /* P 7 (1) + K P->x (1) */
            if (inst->dest.type != IR_OPERAND_STACK && inst->dest.physical_reg >= 0) cost++;
            return cost;
        }

        case IR_STORE_INDIRECT: {
            int cost = 0;
            if (inst->dest.type == IR_OPERAND_CONST_INT || inst->dest.type == IR_OPERAND_CONST_FLOAT) cost += 2;
            else if (inst->dest.physical_reg >= 0) cost++;
            cost++; /* P 7 */
            if (inst->src1.type == IR_OPERAND_CONST_INT || inst->src1.type == IR_OPERAND_CONST_FLOAT) cost += 2;
            else if (inst->src1.physical_reg >= 0) cost++;
            cost++; /* K x->P */
            return cost;
        }

        case IR_HALT:
            return ((inst->src1.type != IR_OPERAND_STACK && inst->src1.physical_reg >= 0) ? 1 : 0) + 1; /* IP src (1) + C/P (1) */

        default:
            return 0;
    }
}

bool codegen_generate(const IRProgram *prog, TargetContext *ctx) {
    if (!prog || !ctx || !ctx->target) return false;

    uint16_t label_steps[MAX_LABELS] = {0};
    bool label_defined[MAX_LABELS] = {false};

    /* -------------------------------------------------------------------------
     * Pass 1: Address calculation and Label Resolution
     * ------------------------------------------------------------------------- */
    uint16_t sim_step = 0;
    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        if (curr->opcode == IR_LABEL) {
            int lid = curr->dest.as.label_id;
            if (lid >= 0 && lid < MAX_LABELS) {
                label_steps[lid] = sim_step;
                label_defined[lid] = true;
            }
        } else {
            sim_step += measure_instruction_steps(curr);
        }
    }

    /* -------------------------------------------------------------------------
     * Pass 2: Concrete Target Emission with Resolved Jump Targets
     * ------------------------------------------------------------------------- */
    if (ctx->target->ops.emit_prologue) {
        ctx->target->ops.emit_prologue(ctx);
    }

    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        switch (curr->opcode) {
            case IR_LABEL:
                if (ctx->out && ctx->verbose) {
                    fprintf(ctx->out, "; --- Label L%d (Step %02u) ---\n",
                            curr->dest.as.label_id, ctx->current_step);
                }
                break;

            case IR_ASSIGN:
                if (curr->dest.physical_reg >= 0) {
                    if (curr->src1.type == IR_OPERAND_CONST_INT) {
                        ctx->target->ops.emit_push_constant(ctx, (double)curr->src1.as.ival);
                    } else if (curr->src1.type == IR_OPERAND_CONST_FLOAT) {
                        ctx->target->ops.emit_push_constant(ctx, curr->src1.as.fval);
                    } else if (curr->src1.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                    }
                    ctx->target->ops.emit_store_reg(ctx, curr->dest.physical_reg);
                }
                break;

            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV:
                if (curr->src1.type != IR_OPERAND_STACK) {
                    if (curr->src1.type == IR_OPERAND_CONST_INT) {
                        ctx->target->ops.emit_push_constant(ctx, (double)curr->src1.as.ival);
                    } else if (curr->src1.type == IR_OPERAND_CONST_FLOAT) {
                        ctx->target->ops.emit_push_constant(ctx, curr->src1.as.fval);
                    } else if (curr->src1.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                    }
                }
                if (curr->src2.type != IR_OPERAND_STACK) {
                    if (curr->src2.type == IR_OPERAND_CONST_INT) {
                        ctx->target->ops.emit_push_constant(ctx, (double)curr->src2.as.ival);
                    } else if (curr->src2.type == IR_OPERAND_CONST_FLOAT) {
                        ctx->target->ops.emit_push_constant(ctx, curr->src2.as.fval);
                    } else if (curr->src2.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src2.physical_reg);
                    }
                }
                {
                    OperatorType op = (curr->opcode == IR_ADD) ? OP_ADD :
                                      (curr->opcode == IR_SUB) ? OP_SUB :
                                      (curr->opcode == IR_MUL) ? OP_MUL : OP_DIV;
                    ctx->target->ops.emit_binop(ctx, op);
                }
                if (curr->dest.type != IR_OPERAND_STACK && curr->dest.physical_reg >= 0) {
                    ctx->target->ops.emit_store_reg(ctx, curr->dest.physical_reg);
                }
                break;

            case IR_NEG:
                if (curr->src1.type != IR_OPERAND_STACK && curr->src1.physical_reg >= 0) {
                    ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                }
                ctx->target->ops.emit_unop(ctx, OP_NEG);
                if (curr->dest.type != IR_OPERAND_STACK && curr->dest.physical_reg >= 0) {
                    ctx->target->ops.emit_store_reg(ctx, curr->dest.physical_reg);
                }
                break;

            case IR_JUMP: {
                int lid = curr->dest.as.label_id;
                uint16_t target_addr = (lid >= 0 && lid < MAX_LABELS && label_defined[lid])
                                       ? label_steps[lid] : 0;
                ctx->target->ops.emit_jump(ctx, target_addr);
                break;
            }

            case IR_JEQ:
            case IR_JNE:
            case IR_JLT:
            case IR_JLE:
            case IR_JGT:
            case IR_JGE: {
                int lid = curr->dest.as.label_id;
                uint16_t target_addr = (lid >= 0 && lid < MAX_LABELS && label_defined[lid])
                                       ? label_steps[lid] : 0;

                if (curr->src2.type == IR_OPERAND_CONST_INT && curr->src2.as.ival == 0) {
                    /* Comparison against zero */
                    if (curr->src1.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                    }
                } else {
                    /* Comparison between two operands: evaluate src1 - src2 in X */
                    if (curr->src1.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                    }
                    if (curr->src2.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src2.physical_reg);
                    }
                    ctx->target->ops.emit_binop(ctx, OP_SUB);
                }

                OperatorType cond_op = OP_EQ;
                switch (curr->opcode) {
                    case IR_JEQ: cond_op = OP_EQ; break;
                    case IR_JNE: cond_op = OP_NEQ; break;
                    case IR_JLT: cond_op = OP_LT; break;
                    case IR_JGE: cond_op = OP_GTE; break;
                    default: break;
                }

                ctx->target->ops.emit_cjump(ctx, cond_op, target_addr);
                break;
            }

            case IR_LOOP: {
                int lid = curr->dest.as.label_id;
                uint16_t target_addr = (lid >= 0 && lid < MAX_LABELS && label_defined[lid])
                                       ? label_steps[lid] : 0;
                int reg_id = curr->src1.physical_reg;
                if (ctx->target->ops.emit_loop) {
                    ctx->target->ops.emit_loop(ctx, reg_id, target_addr);
                }
                break;
            }

            case IR_INPUT:
                if (ctx->target->ops.emit_input) {
                    ctx->target->ops.emit_input(ctx, curr->dest.physical_reg);
                }
                break;

            case IR_DISPLAY:
                if (curr->src1.type != IR_OPERAND_STACK && curr->src1.physical_reg >= 0) {
                    ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                }
                if (ctx->target->ops.emit_display) {
                    ctx->target->ops.emit_display(ctx, -1);
                }
                break;

            case IR_PAUSE:
                if (ctx->target->ops.emit_pause) {
                    ctx->target->ops.emit_pause(ctx);
                }
                break;

            case IR_CALL: {
                int lid = curr->src1.as.label_id;
                uint16_t target_addr = (lid >= 0 && lid < MAX_LABELS && label_defined[lid])
                                       ? label_steps[lid] : 0;
                if (ctx->target->ops.emit_call) {
                    ctx->target->ops.emit_call(ctx, target_addr);
                }
                if (curr->dest.type != IR_OPERAND_STACK && curr->dest.physical_reg >= 0) {
                    ctx->target->ops.emit_store_reg(ctx, curr->dest.physical_reg);
                }
                break;
            }

            case IR_RETURN:
                if (curr->src1.type != IR_OPERAND_STACK && curr->src1.physical_reg >= 0) {
                    ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                }
                if (ctx->target->ops.emit_return) {
                    ctx->target->ops.emit_return(ctx);
                }
                break;

            case IR_LOAD_INDIRECT: {
                int ptr_reg = 7; /* Hardware unmodified pointer register RG7 */
                if (curr->src1.type != IR_OPERAND_STACK) {
                    if (curr->src1.type == IR_OPERAND_CONST_INT) {
                        ctx->target->ops.emit_push_constant(ctx, (double)curr->src1.as.ival);
                    } else if (curr->src1.type == IR_OPERAND_CONST_FLOAT) {
                        ctx->target->ops.emit_push_constant(ctx, curr->src1.as.fval);
                    } else if (curr->src1.physical_reg >= 0) {
                        ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                    }
                }
                ctx->target->ops.emit_store_reg(ctx, ptr_reg);
                if (ctx->target->ops.emit_load_indirect) {
                    ctx->target->ops.emit_load_indirect(ctx, ptr_reg);
                }
                if (curr->dest.type != IR_OPERAND_STACK && curr->dest.physical_reg >= 0) {
                    ctx->target->ops.emit_store_reg(ctx, curr->dest.physical_reg);
                }
                break;
            }

            case IR_STORE_INDIRECT: {
                int ptr_reg = 7; /* Hardware unmodified pointer register RG7 */
                if (curr->dest.type == IR_OPERAND_CONST_INT) {
                    ctx->target->ops.emit_push_constant(ctx, (double)curr->dest.as.ival);
                } else if (curr->dest.type == IR_OPERAND_CONST_FLOAT) {
                    ctx->target->ops.emit_push_constant(ctx, curr->dest.as.fval);
                } else if (curr->dest.physical_reg >= 0) {
                    ctx->target->ops.emit_load_reg(ctx, curr->dest.physical_reg);
                }
                ctx->target->ops.emit_store_reg(ctx, ptr_reg);
                if (curr->src1.type == IR_OPERAND_CONST_INT) {
                    ctx->target->ops.emit_push_constant(ctx, (double)curr->src1.as.ival);
                } else if (curr->src1.type == IR_OPERAND_CONST_FLOAT) {
                    ctx->target->ops.emit_push_constant(ctx, curr->src1.as.fval);
                } else if (curr->src1.physical_reg >= 0) {
                    ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                }
                if (ctx->target->ops.emit_store_indirect) {
                    ctx->target->ops.emit_store_indirect(ctx, ptr_reg);
                }
                break;
            }

            case IR_HALT:
                if (curr->src1.type != IR_OPERAND_STACK && curr->src1.physical_reg >= 0) {
                    ctx->target->ops.emit_load_reg(ctx, curr->src1.physical_reg);
                }
                ctx->target->ops.emit_halt(ctx);
                break;

            default:
                break;
        }
    }

    if (ctx->target->ops.emit_epilogue) {
        ctx->target->ops.emit_epilogue(ctx);
    }

    return (ctx->error_count == 0);
}
