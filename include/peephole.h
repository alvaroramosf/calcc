/**
 * @file peephole.h
 * @brief Peephole and Copy-Propagation Optimizer for Micro C IR.
 *
 * Scans the intermediate instruction sequence and performs algebraic
 * simplifications, redundant store-load elimination, and temporary
 * copy propagation to minimize physical step consumption on retro-calculators.
 */

#ifndef PEEPHOLE_H
#define PEEPHOLE_H

#include <stdbool.h>
#include "ir.h"

/**
 * @brief Runs peephole optimization passes on an IR program.
 *
 * Optimizations include:
 * - Copy propagation: collapses `v_temp = expr; var = v_temp;` directly into `var = expr;`
 * - Dead assignment pruning
 * - Identity operation simplifications (e.g. + 0, * 1)
 *
 * @param prog The IR program to optimize in-place.
 * @return Number of optimizations performed.
 */
int peephole_optimize(IRProgram *prog);

#endif /* PEEPHOLE_H */
