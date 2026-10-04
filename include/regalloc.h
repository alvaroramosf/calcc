/**
 * @file regalloc.h
 * @brief Liveness Analysis and Target-Aware Register Allocator.
 *
 * Implements variable lifetime analysis, dead variable elimination,
 * and physical register allocation (linear scan / interval coloring)
 * constrained by the target calculator's hardware register capacity.
 */

#ifndef REGALLOC_H
#define REGALLOC_H

#include <stdbool.h>
#include "ir.h"
#include "target_api.h"

#define MAX_TRACKED_VARS 128

/**
 * @brief Lifetime tracking interval for a virtual register or named variable.
 */
typedef struct {
    char name[64];        /**< Name of variable or virtual register (e.g. "counter", "v0") */
    int var_id;           /**< Identifier index */
    bool is_vreg;         /**< True if temporary vreg, false if user variable */

    int first_def;        /**< Index of first assignment instruction */
    int last_use;         /**< Index of last read instruction */
    int def_count;        /**< Total times defined */
    int use_count;        /**< Total times read */

    bool is_dead;         /**< True if defined but never used (candidate for elimination) */
    bool is_loop_counter; /**< True if used in dedicated hardware loop (demands registers 0..3) */
    int physical_reg;     /**< Assigned hardware register (0..N-1), or -1 */
} LiveInterval;

/**
 * @brief Register allocation result report and diagnostics.
 */
typedef struct {
    bool success;
    int total_vars_tracked;
    int dead_vars_count;
    int physical_regs_used;
    int max_concurrent_live;
    char error_message[512];
} RegAllocResult;

/**
 * @brief Performs liveness analysis on an IR program and allocates physical registers.
 *
 * Checks if the required concurrent registers fit within target->caps.data_registers_count.
 * Variables with disjoint lifetimes share registers. Dead variables are identified.
 *
 * @param prog The IR program to analyze and patch with physical registers.
 * @param target The target calculator profile specifying hardware limits.
 * @return RegAllocResult with success status and diagnostic details.
 */
RegAllocResult regalloc_run(IRProgram *prog, const TargetInterface *target);

/**
 * @brief Prints the computed variable live intervals to the specified stream.
 */
void regalloc_dump_intervals(FILE *stream);

#endif /* REGALLOC_H */
