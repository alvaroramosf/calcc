/**
 * @file peephole.c
 * @brief Implementation of copy-propagation and peephole optimizations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "peephole.h"

static bool operands_equal(const IROperand *a, const IROperand *b) {
    if (a->type != b->type) return false;
    switch (a->type) {
        case IR_OPERAND_NONE:
            return true;
        case IR_OPERAND_CONST_INT:
            return a->as.ival == b->as.ival;
        case IR_OPERAND_CONST_FLOAT:
            return a->as.fval == b->as.fval;
        case IR_OPERAND_VAR:
            return (a->as.name && b->as.name && strcmp(a->as.name, b->as.name) == 0);
        case IR_OPERAND_VREG:
            return a->as.vreg_id == b->as.vreg_id;
        case IR_OPERAND_LABEL:
            return a->as.label_id == b->as.label_id;
        case IR_OPERAND_STACK:
            return true;
    }
    return false;
}

static void unlink_instruction(IRProgram *prog, IRInstruction *inst) {
    if (!prog || !inst) return;

    if (inst->prev) {
        inst->prev->next = inst->next;
    } else {
        prog->head = inst->next;
    }

    if (inst->next) {
        inst->next->prev = inst->prev;
    } else {
        prog->tail = inst->prev;
    }

    /* Free strings if any */
    if (inst->dest.type == IR_OPERAND_VAR && inst->dest.as.name) free(inst->dest.as.name);
    if (inst->src1.type == IR_OPERAND_VAR && inst->src1.as.name) free(inst->src1.as.name);
    if (inst->src2.type == IR_OPERAND_VAR && inst->src2.as.name) free(inst->src2.as.name);

    free(inst);
    prog->instruction_count--;
}

static int count_vreg_uses(const IRProgram *prog, int vreg_id) {
    int count = 0;
    for (const IRInstruction *i = prog->head; i; i = i->next) {
        if (i->src1.type == IR_OPERAND_VREG && i->src1.as.vreg_id == vreg_id) count++;
        if (i->src2.type == IR_OPERAND_VREG && i->src2.as.vreg_id == vreg_id) count++;
    }
    return count;
}

int peephole_optimize(IRProgram *prog) {
    if (!prog || !prog->head) return 0;

    int optimizations = 0;
    bool changed = true;

    while (changed) {
        changed = false;
        IRInstruction *curr = prog->head;

        while (curr && curr->next) {
            IRInstruction *next = curr->next;

            /*
             * Pattern 1: Collapse temporary VREG calculation followed by variable assignment.
             *
             *   curr: v0 = expr (ADD, SUB, MUL, DIV, NEG, INPUT)
             *   next: var = v0 (ASSIGN)
             *   ====>
             *   curr: var = expr
             *   (next removed)
             */
            if ((curr->opcode == IR_ADD || curr->opcode == IR_SUB ||
                 curr->opcode == IR_MUL || curr->opcode == IR_DIV ||
                 curr->opcode == IR_NEG || curr->opcode == IR_INPUT) &&
                curr->dest.type == IR_OPERAND_VREG &&
                next->opcode == IR_ASSIGN &&
                next->src1.type == IR_OPERAND_VREG &&
                curr->dest.as.vreg_id == next->src1.as.vreg_id) {

                /* Point curr->dest to next->dest */
                curr->dest = next->dest;
                next->dest = ir_op_none(); /* prevent double-free */

                unlink_instruction(prog, next);
                optimizations++;
                changed = true;
                break;
            }

            /*
             * Pattern 2: Redundant self-assignment (var = var)
             */
            if (curr->opcode == IR_ASSIGN && operands_equal(&curr->dest, &curr->src1)) {
                IRInstruction *to_remove = curr;
                curr = curr->next;
                unlink_instruction(prog, to_remove);
                optimizations++;
                changed = true;
                break;
            }

            /*
             * Pattern 3: RPN Stack Optimization (Transient Accumulator Forwarding)
             *
             * If curr produces a temporary vreg (ADD, SUB, MUL, DIV, NEG, CALL, LOAD_INDIRECT)
             * with dest = vK, and vK is used ONLY ONCE across the entire program (use_count == 1),
             * and that single use is in the IMMEDIATELY following instruction `next`:
             *
             * Case A: `next` is a binary op (ADD, SUB, MUL, DIV) and `next->src1` is vK.
             *   curr: dest = $stack
             *   next: src1 = $stack
             *
             * Case B: `next` is a commutative binary op (ADD, MUL) and `next->src2` is vK.
             *   Swap next->src1 and next->src2, then:
             *   curr: dest = $stack
             *   next: src1 = $stack
             *
             * Case C: `next` is UNARY or SINK (NEG, RETURN, HALT, DISPLAY) and `next->src1` is vK.
             *   curr: dest = $stack
             *   next: src1 = $stack
             */
            if (curr->dest.type == IR_OPERAND_VREG &&
                (curr->opcode == IR_ADD || curr->opcode == IR_SUB ||
                 curr->opcode == IR_MUL || curr->opcode == IR_DIV ||
                 curr->opcode == IR_NEG || curr->opcode == IR_CALL ||
                 curr->opcode == IR_LOAD_INDIRECT)) {

                int vid = curr->dest.as.vreg_id;
                if (count_vreg_uses(prog, vid) == 1) {
                    if (next->opcode == IR_ADD || next->opcode == IR_SUB ||
                        next->opcode == IR_MUL || next->opcode == IR_DIV) {

                        if (next->src1.type == IR_OPERAND_VREG && next->src1.as.vreg_id == vid) {
                            curr->dest = ir_op_stack();
                            next->src1 = ir_op_stack();
                            optimizations++;
                            changed = true;
                            break;
                        } else if (next->src2.type == IR_OPERAND_VREG && next->src2.as.vreg_id == vid &&
                                   (next->opcode == IR_ADD || next->opcode == IR_MUL)) {
                            /* Commutative swap: src1 <-> src2 */
                            IROperand tmp = next->src1;
                            next->src1 = next->src2;
                            next->src2 = tmp;

                            curr->dest = ir_op_stack();
                            next->src1 = ir_op_stack();
                            optimizations++;
                            changed = true;
                            break;
                        }
                    } else if (next->opcode == IR_NEG || next->opcode == IR_RETURN ||
                               next->opcode == IR_HALT || next->opcode == IR_DISPLAY) {
                        if (next->src1.type == IR_OPERAND_VREG && next->src1.as.vreg_id == vid) {
                            curr->dest = ir_op_stack();
                            next->src1 = ir_op_stack();
                            optimizations++;
                            changed = true;
                            break;
                        }
                    }
                }
            }

            curr = curr->next;
        }
    }

    /* Reindex instructions */
    int idx = 0;
    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        curr->index = idx++;
    }
    prog->instruction_count = idx;

    return optimizations;
}
