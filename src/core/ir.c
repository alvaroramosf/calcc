/**
 * @file ir.c
 * @brief Implementation of Three-Address Code (TAC) data structures and printing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"

IROperand ir_op_none(void) {
    IROperand op;
    op.type = IR_OPERAND_NONE;
    op.physical_reg = -1;
    op.as.ival = 0;
    return op;
}

IROperand ir_op_const_int(int64_t val) {
    IROperand op;
    op.type = IR_OPERAND_CONST_INT;
    op.physical_reg = -1;
    op.as.ival = val;
    return op;
}

IROperand ir_op_const_float(double val) {
    IROperand op;
    op.type = IR_OPERAND_CONST_FLOAT;
    op.physical_reg = -1;
    op.as.fval = val;
    return op;
}

IROperand ir_op_var(const char *name) {
    IROperand op;
    op.type = IR_OPERAND_VAR;
    op.physical_reg = -1;
    op.as.name = name ? strdup(name) : NULL;
    return op;
}

IROperand ir_op_vreg(int vreg_id) {
    IROperand op;
    op.type = IR_OPERAND_VREG;
    op.physical_reg = -1;
    op.as.vreg_id = vreg_id;
    return op;
}

IROperand ir_op_label(int label_id) {
    IROperand op;
    op.type = IR_OPERAND_LABEL;
    op.physical_reg = -1;
    op.as.label_id = label_id;
    return op;
}

IROperand ir_op_stack(void) {
    IROperand op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_STACK;
    op.physical_reg = -1;
    return op;
}

IRProgram *ir_program_new(void) {
    IRProgram *prog = (IRProgram *)calloc(1, sizeof(IRProgram));
    if (!prog) return NULL;
    prog->next_vreg_id = 0;
    prog->next_label_id = 0;
    return prog;
}

static void ir_operand_cleanup(IROperand *op) {
    if (op && op->type == IR_OPERAND_VAR && op->as.name) {
        free(op->as.name);
        op->as.name = NULL;
    }
}

void ir_program_free(IRProgram *prog) {
    if (!prog) return;

    IRInstruction *curr = prog->head;
    while (curr) {
        IRInstruction *next = curr->next;
        ir_operand_cleanup(&curr->dest);
        ir_operand_cleanup(&curr->src1);
        ir_operand_cleanup(&curr->src2);
        free(curr);
        curr = next;
    }
    free(prog);
}

IRInstruction *ir_emit(IRProgram *prog, IROpcode op, IROperand dest, IROperand src1, IROperand src2, int line) {
    if (!prog) return NULL;

    IRInstruction *inst = (IRInstruction *)calloc(1, sizeof(IRInstruction));
    if (!inst) return NULL;

    inst->opcode = op;
    inst->dest = dest;
    inst->src1 = src1;
    inst->src2 = src2;
    inst->line = line;
    inst->index = prog->instruction_count++;

    if (!prog->head) {
        prog->head = inst;
        prog->tail = inst;
    } else {
        prog->tail->next = inst;
        inst->prev = prog->tail;
        prog->tail = inst;
    }

    return inst;
}

IROperand ir_alloc_vreg(IRProgram *prog) {
    if (!prog) return ir_op_none();
    return ir_op_vreg(prog->next_vreg_id++);
}

IROperand ir_alloc_label(IRProgram *prog) {
    if (!prog) return ir_op_none();
    return ir_op_label(prog->next_label_id++);
}

const char *ir_opcode_to_string(IROpcode op) {
    switch (op) {
        case IR_NOP:      return "NOP";
        case IR_LABEL:    return "LABEL";
        case IR_JUMP:     return "JUMP";
        case IR_JEQ:      return "JEQ";
        case IR_JNE:      return "JNE";
        case IR_JLT:      return "JLT";
        case IR_JLE:      return "JLE";
        case IR_JGT:      return "JGT";
        case IR_JGE:      return "JGE";
        case IR_LOOP:     return "LOOP";
        case IR_ASSIGN:         return "ASSIGN";
        case IR_LOAD_INDIRECT:  return "LOAD_INDIRECT";
        case IR_STORE_INDIRECT: return "STORE_INDIRECT";
        case IR_ADD:            return "ADD";
        case IR_SUB:            return "SUB";
        case IR_MUL:            return "MUL";
        case IR_DIV:            return "DIV";
        case IR_NEG:            return "NEG";
        case IR_CALL:     return "CALL";
        case IR_RETURN:   return "RETURN";
        case IR_HALT:     return "HALT";
        case IR_INPUT:    return "INPUT";
        case IR_DISPLAY:  return "DISPLAY";
        case IR_PAUSE:    return "PAUSE";
        default:          return "UNKNOWN";
    }
}

static void print_operand(const IROperand *op, FILE *stream) {
    switch (op->type) {
        case IR_OPERAND_NONE:
            break;
        case IR_OPERAND_CONST_INT:
            fprintf(stream, "%ld", (long)op->as.ival);
            break;
        case IR_OPERAND_CONST_FLOAT:
            fprintf(stream, "%.4g", op->as.fval);
            break;
        case IR_OPERAND_VAR:
            if (op->physical_reg >= 0) {
                fprintf(stream, "%s(R%d)", op->as.name, op->physical_reg);
            } else {
                fprintf(stream, "%s", op->as.name ? op->as.name : "?");
            }
            break;
        case IR_OPERAND_VREG:
            if (op->physical_reg >= 0) {
                fprintf(stream, "v%d(R%d)", op->as.vreg_id, op->physical_reg);
            } else {
                fprintf(stream, "v%d", op->as.vreg_id);
            }
            break;
        case IR_OPERAND_LABEL:
            fprintf(stream, "L%d", op->as.label_id);
            break;
        case IR_OPERAND_STACK:
            fprintf(stream, "$stack");
            break;
    }
}

void ir_dump(const IRProgram *prog, FILE *stream) {
    if (!prog || !stream) return;

    fprintf(stream, "; ==========================================================\n");
    fprintf(stream, "; Intermediate Representation (3-Address Code)\n");
    fprintf(stream, "; Total Instructions: %d, Virtual Registers: %d\n",
            prog->instruction_count, prog->next_vreg_id);
    fprintf(stream, "; ==========================================================\n");

    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        if (curr->opcode == IR_LABEL) {
            fprintf(stream, "L%d:\n", curr->dest.as.label_id);
            continue;
        }

        fprintf(stream, "  [%03d] ", curr->index);

        switch (curr->opcode) {
            case IR_ASSIGN:
                print_operand(&curr->dest, stream);
                fprintf(stream, " = ");
                print_operand(&curr->src1, stream);
                break;
            case IR_LOAD_INDIRECT:
                print_operand(&curr->dest, stream);
                fprintf(stream, " = memory[");
                print_operand(&curr->src1, stream);
                fprintf(stream, "]");
                break;
            case IR_STORE_INDIRECT:
                fprintf(stream, "memory[");
                print_operand(&curr->dest, stream);
                fprintf(stream, "] = ");
                print_operand(&curr->src1, stream);
                break;
            case IR_CALL:
                if (curr->dest.type != IR_OPERAND_NONE) {
                    print_operand(&curr->dest, stream);
                    fprintf(stream, " = ");
                }
                fprintf(stream, "call ");
                print_operand(&curr->src1, stream);
                break;
            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV: {
                const char *symbol = "+";
                if (curr->opcode == IR_SUB) symbol = "-";
                else if (curr->opcode == IR_MUL) symbol = "*";
                else if (curr->opcode == IR_DIV) symbol = "/";

                print_operand(&curr->dest, stream);
                fprintf(stream, " = ");
                print_operand(&curr->src1, stream);
                fprintf(stream, " %s ", symbol);
                print_operand(&curr->src2, stream);
                break;
            }
            case IR_NEG:
                print_operand(&curr->dest, stream);
                fprintf(stream, " = -");
                print_operand(&curr->src1, stream);
                break;
            case IR_JUMP:
                fprintf(stream, "goto ");
                print_operand(&curr->dest, stream);
                break;
            case IR_JEQ:
            case IR_JNE:
            case IR_JLT:
            case IR_JLE:
            case IR_JGT:
            case IR_JGE: {
                const char *rel = "==";
                if (curr->opcode == IR_JNE) rel = "!=";
                else if (curr->opcode == IR_JLT) rel = "<";
                else if (curr->opcode == IR_JLE) rel = "<=";
                else if (curr->opcode == IR_JGT) rel = ">";
                else if (curr->opcode == IR_JGE) rel = ">=";

                fprintf(stream, "if ");
                print_operand(&curr->src1, stream);
                fprintf(stream, " %s ", rel);
                print_operand(&curr->src2, stream);
                fprintf(stream, " goto ");
                print_operand(&curr->dest, stream);
                break;
            }
            case IR_LOOP:
                fprintf(stream, "loop ");
                print_operand(&curr->src1, stream);
                fprintf(stream, " goto ");
                print_operand(&curr->dest, stream);
                break;
            case IR_RETURN:
                fprintf(stream, "return ");
                print_operand(&curr->src1, stream);
                break;
            case IR_HALT:
                fprintf(stream, "halt");
                break;
            case IR_INPUT:
                print_operand(&curr->dest, stream);
                fprintf(stream, " = input()");
                break;
            case IR_DISPLAY:
                fprintf(stream, "display(");
                print_operand(&curr->src1, stream);
                fprintf(stream, ")");
                break;
            case IR_PAUSE:
                fprintf(stream, "pause()");
                break;
            default:
                fprintf(stream, "%s", ir_opcode_to_string(curr->opcode));
                break;
        }
        fprintf(stream, "\n");
    }
}
