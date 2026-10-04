/**
 * @file regalloc.c
 * @brief Implementation of liveness analysis, dead variable detection, and register allocation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "regalloc.h"

static LiveInterval g_intervals[MAX_TRACKED_VARS];
static int g_interval_count = 0;

static int find_or_add_var(const char *name, bool is_vreg, int vreg_id) {
    char key[64];
    if (is_vreg) {
        snprintf(key, sizeof(key), "v%d", vreg_id);
    } else {
        snprintf(key, sizeof(key), "%s", name ? name : "");
    }

    for (int i = 0; i < g_interval_count; i++) {
        if (strcmp(g_intervals[i].name, key) == 0) {
            return i;
        }
    }

    if (g_interval_count >= MAX_TRACKED_VARS) {
        return -1;
    }

    int idx = g_interval_count++;
    LiveInterval *inv = &g_intervals[idx];
    memset(inv, 0, sizeof(LiveInterval));
    snprintf(inv->name, sizeof(inv->name), "%s", key);
    inv->var_id = is_vreg ? vreg_id : idx;
    inv->is_vreg = is_vreg;
    inv->first_def = -1;
    inv->last_use = -1;
    inv->def_count = 0;
    inv->use_count = 0;
    inv->is_dead = false;
    inv->physical_reg = -1;

    return idx;
}

static void register_operand_use(const IROperand *op, int inst_idx) {
    if (!op) return;
    int idx = -1;
    if (op->type == IR_OPERAND_VAR) {
        idx = find_or_add_var(op->as.name, false, 0);
    } else if (op->type == IR_OPERAND_VREG) {
        idx = find_or_add_var(NULL, true, op->as.vreg_id);
    }

    if (idx >= 0) {
        LiveInterval *inv = &g_intervals[idx];
        inv->use_count++;
        if (inst_idx > inv->last_use) {
            inv->last_use = inst_idx;
        }
        if (inv->first_def == -1) {
            /* Read before write (e.g. global or function parameter) */
            inv->first_def = 0;
        }
    }
}

static void register_operand_def(const IROperand *op, int inst_idx) {
    if (!op) return;
    int idx = -1;
    if (op->type == IR_OPERAND_VAR) {
        idx = find_or_add_var(op->as.name, false, 0);
    } else if (op->type == IR_OPERAND_VREG) {
        idx = find_or_add_var(NULL, true, op->as.vreg_id);
    }

    if (idx >= 0) {
        LiveInterval *inv = &g_intervals[idx];
        inv->def_count++;
        if (inv->first_def == -1) {
            inv->first_def = inst_idx;
        }
        if (inv->last_use < inst_idx) {
            inv->last_use = inst_idx;
        }
    }
}

static bool intervals_overlap(const LiveInterval *a, const LiveInterval *b) {
    if (a->is_dead || b->is_dead) return false;
    return (a->first_def <= b->last_use) && (b->first_def <= a->last_use);
}

RegAllocResult regalloc_run(IRProgram *prog, const TargetInterface *target) {
    RegAllocResult res;
    memset(&res, 0, sizeof(RegAllocResult));
    res.success = true;

    if (!prog || !target) {
        res.success = false;
        snprintf(res.error_message, sizeof(res.error_message), "Null IRProgram or Target provided");
        return res;
    }

    g_interval_count = 0;
    int max_capacity = target->caps.data_registers_count;

    bool has_indirect = false;
    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        if (curr->opcode == IR_LOAD_INDIRECT || curr->opcode == IR_STORE_INDIRECT) {
            has_indirect = true;
            break;
        }
    }

    /* Pass 1: Build definitions and use intervals */
    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        /* Reads occur on src1 and src2 */
        register_operand_use(&curr->src1, curr->index);
        register_operand_use(&curr->src2, curr->index);

        /* Writes occur on dest */
        if (curr->opcode != IR_LABEL &&
            curr->opcode != IR_JUMP &&
            curr->opcode != IR_JEQ &&
            curr->opcode != IR_JNE &&
            curr->opcode != IR_JLT &&
            curr->opcode != IR_JLE &&
            curr->opcode != IR_JGT &&
            curr->opcode != IR_JGE &&
            curr->opcode != IR_LOOP &&
            curr->opcode != IR_DISPLAY &&
            curr->opcode != IR_RETURN &&
            curr->opcode != IR_HALT &&
            curr->opcode != IR_PAUSE &&
            curr->opcode != IR_STORE_INDIRECT) {
            register_operand_def(&curr->dest, curr->index);
        } else if (curr->opcode == IR_DISPLAY || curr->opcode == IR_RETURN ||
                   curr->opcode == IR_HALT || curr->opcode == IR_STORE_INDIRECT) {
            /* Instructions where dest is read */
            register_operand_use(&curr->dest, curr->index);
        }

        if (curr->opcode == IR_LOOP) {
            register_operand_use(&curr->src1, curr->index);
            register_operand_def(&curr->src1, curr->index);
            if (curr->src1.type == IR_OPERAND_VAR) {
                int idx = find_or_add_var(curr->src1.as.name, false, 0);
                if (idx >= 0) g_intervals[idx].is_loop_counter = true;
            } else if (curr->src1.type == IR_OPERAND_VREG) {
                int idx = find_or_add_var(NULL, true, curr->src1.as.vreg_id);
                if (idx >= 0) g_intervals[idx].is_loop_counter = true;
            }
        }
    }

    res.total_vars_tracked = g_interval_count;

    /* Pass 2: Dead variable detection and array persistence */
    for (int i = 0; i < g_interval_count; i++) {
        LiveInterval *inv = &g_intervals[i];
        char *us = strrchr(inv->name, '_');
        if (has_indirect && us && isdigit((unsigned char)us[1])) {
            inv->is_dead = false;
            inv->last_use = prog->instruction_count - 1;
            continue;
        }

        if (inv->use_count == 0 && inv->def_count > 0) {
            inv->is_dead = true;
            inv->physical_reg = -1;
            res.dead_vars_count++;
        }
    }

    /* Pass 3: Greedy Interval Coloring / Register Assignment */
    int highest_reg_used = -1;

    /* Pass 3-pre: Pre-allocate array elements (e.g. a_0, a_1, ...) to fixed registers */
    for (int i = 0; i < g_interval_count; i++) {
        LiveInterval *curr = &g_intervals[i];
        if (curr->is_dead) continue;
        char *us = strrchr(curr->name, '_');
        if (us && isdigit((unsigned char)us[1])) {
            int elem_idx = atoi(us + 1);
            if (elem_idx >= 0 && elem_idx < max_capacity && (!has_indirect || elem_idx != 7)) {
                curr->physical_reg = elem_idx;
                if (elem_idx > highest_reg_used) {
                    highest_reg_used = elem_idx;
                }
            }
        }
    }

    /* Pass 3a: Prioritize dedicated hardware loop counters (0..3) */
    for (int i = 0; i < g_interval_count; i++) {
        LiveInterval *curr = &g_intervals[i];
        if (curr->is_dead || !curr->is_loop_counter) continue;

        bool reg_busy[64] = {false};
        if (has_indirect) reg_busy[7] = true;

        for (int j = 0; j < g_interval_count; j++) {
            LiveInterval *prev = &g_intervals[j];
            if (prev != curr && !prev->is_dead && prev->physical_reg >= 0 && intervals_overlap(curr, prev)) {
                if (prev->physical_reg < 64) {
                    reg_busy[prev->physical_reg] = true;
                }
            }
        }

        int assigned = -1;
        int max_loop_regs = (max_capacity < 4) ? max_capacity : 4;
        for (int r = 0; r < max_loop_regs; r++) {
            if (!reg_busy[r]) {
                assigned = r;
                break;
            }
        }

        if (assigned == -1) {
            res.success = false;
            snprintf(res.error_message, sizeof(res.error_message),
                     "Register allocation failed for target '%.32s': Hardware loop counter '%.48s' requires registers 0..3, but none were available.",
                     target->caps.name, curr->name);
            return res;
        }

        curr->physical_reg = assigned;
        if (assigned > highest_reg_used) {
            highest_reg_used = assigned;
        }
    }

    /* Pass 3b: Allocate remaining active variables (0..max_capacity-1) */
    for (int i = 0; i < g_interval_count; i++) {
        LiveInterval *curr = &g_intervals[i];
        if (curr->is_dead || curr->is_loop_counter || curr->physical_reg >= 0) continue;

        bool reg_busy[64] = {false};
        if (has_indirect) reg_busy[7] = true;

        for (int j = 0; j < g_interval_count; j++) {
            LiveInterval *prev = &g_intervals[j];
            if (prev != curr && !prev->is_dead && prev->physical_reg >= 0 && intervals_overlap(curr, prev)) {
                if (prev->physical_reg < 64) {
                    reg_busy[prev->physical_reg] = true;
                }
            }
        }

        int assigned = -1;
        for (int r = 0; r < max_capacity; r++) {
            if (!reg_busy[r]) {
                assigned = r;
                break;
            }
        }

        if (assigned == -1) {
            /* Out of physical registers! Target capacity exceeded */
            res.success = false;
            snprintf(res.error_message, sizeof(res.error_message),
                     "Register allocation failed for target '%.32s': Exceeded hardware limit of %d physical registers. Variable '%.48s' could not be allocated.",
                     target->caps.name, max_capacity, curr->name);
            return res;
        }

        curr->physical_reg = assigned;
        if (assigned > highest_reg_used) {
            highest_reg_used = assigned;
        }
    }

    res.physical_regs_used = highest_reg_used + 1;

    /* Pass 4: Backpatch physical registers into all IR instructions */
    for (IRInstruction *curr = prog->head; curr; curr = curr->next) {
        IROperand *ops[3] = {&curr->dest, &curr->src1, &curr->src2};
        for (int k = 0; k < 3; k++) {
            IROperand *op = ops[k];
            if (op->type == IR_OPERAND_VAR) {
                int idx = find_or_add_var(op->as.name, false, 0);
                if (idx >= 0) op->physical_reg = g_intervals[idx].physical_reg;
            } else if (op->type == IR_OPERAND_VREG) {
                int idx = find_or_add_var(NULL, true, op->as.vreg_id);
                if (idx >= 0) op->physical_reg = g_intervals[idx].physical_reg;
            }
        }
    }

    return res;
}

void regalloc_dump_intervals(FILE *stream) {
    if (!stream) return;
    fprintf(stream, "\n; --- Variable Liveness & Register Allocation ---\n");
    fprintf(stream, "; %-12s | %-5s | %-5s | %-5s | %-5s | %-8s | %s\n",
            "Variable", "Start", "End", "Defs", "Uses", "Status", "Physical Reg");
    fprintf(stream, "; ----------------------------------------------------------------\n");

    for (int i = 0; i < g_interval_count; i++) {
        LiveInterval *inv = &g_intervals[i];
        fprintf(stream, "; %-12s | %03d   | %03d   | %-5d | %-5d | %-8s | ",
                inv->name, inv->first_def, inv->last_use,
                inv->def_count, inv->use_count,
                inv->is_dead ? "DEAD" : "ACTIVE");
        if (inv->physical_reg >= 0) {
            fprintf(stream, "R%d\n", inv->physical_reg);
        } else {
            fprintf(stream, "NONE\n");
        }
    }
}
