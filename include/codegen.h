/**
 * @file codegen.h
 * @brief Two-Pass Target Code Generator and Label Backpatcher.
 *
 * Resolves symbolic jump labels into absolute physical calculator step addresses,
 * converts intermediate 3-address instructions into target machine key sequences,
 * and emits the disassembled hardware listing.
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>
#include "ir.h"
#include "target_api.h"

/**
 * @brief Generates complete target machine code from a lowered IR program.
 *
 * Performs Pass 1 (Step address measurement & label resolution) followed by
 * Pass 2 (Emission with backpatched jump target addresses).
 *
 * @param prog Intermediate Representation program.
 * @param ctx Target compilation context.
 * @return true on success, false if hardware capacity is exceeded.
 */
bool codegen_generate(const IRProgram *prog, TargetContext *ctx);

#endif /* CODEGEN_H */
