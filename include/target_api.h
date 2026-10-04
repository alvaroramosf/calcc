/**
 * @file target_api.h
 * @brief Target Hardware Abstraction Interface for Programmable Calculators.
 *
 * Defines the polymorphic interface and hardware capability descriptors
 * that each backend target (e.g., Elektronika MK-56/52, HP RPN, TI series)
 * must implement to translate intermediate representations or AST into
 * device-specific key codes, mnemonics, and binary bytecode representations.
 */

#ifndef TARGET_API_H
#define TARGET_API_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ast.h"

/**
 * @brief Hardware limits and architectural capabilities of a calculator target.
 */
typedef struct {
    const char *name;             /**< Short identifier (e.g., "mk56", "mk52") */
    const char *display_name;     /**< Human-readable name */
    const char *architecture;     /**< e.g., "RPN-4", "Algebraic-AOS" */

    uint16_t max_program_steps;   /**< Total available program memory steps (e.g., 98 for MK-56, 105 for MK-52) */
    uint8_t  data_registers_count;/**< Number of addressable user memory registers (e.g., 14) */
    uint8_t  stack_depth;         /**< Operational stack depth (e.g., 4 for X, Y, Z, T) */
    uint8_t  call_stack_depth;    /**< Subroutine return stack depth */

    bool has_shadow_register;     /**< MK-56 style X1 shadow register */
    bool has_hardware_loops;      /**< Dedicated loop instructions like F Lk */
    bool supports_indirect_addr;  /**< Indirect addressing support */
} TargetCaps;

/**
 * @brief Target compilation context and output state.
 */
typedef struct TargetContext {
    const struct TargetInterface *target;
    FILE *out;                    /**< Output stream for emitted code/mnemonics */
    uint16_t current_step;        /**< Current instruction address counter */
    int error_count;              /**< Compilation error counter */
    bool verbose;                 /**< Verbose disassembly / comments enabled */
    void *target_data;            /**< Target-private backend data */
} TargetContext;

/**
 * @brief Virtual method table of target code emission hooks.
 */
typedef struct TargetOps {
    /**
     * @brief Target-specific context initialization.
     */
    bool (*init)(TargetContext *ctx);

    /**
     * @brief Emits file header, architecture banner, and initial calculator reset.
     */
    void (*emit_prologue)(TargetContext *ctx);

    /**
     * @brief Emits program finalization, padding, and summary metadata.
     */
    void (*emit_epilogue)(TargetContext *ctx);

    /**
     * @brief Emits numeric constant load into the bottom of the stack (X register).
     */
    void (*emit_push_constant)(TargetContext *ctx, double value);

    /**
     * @brief Emits load from memory register to X register.
     */
    void (*emit_load_reg)(TargetContext *ctx, int reg_id);

    /**
     * @brief Emits store from X register to memory register.
     */
    void (*emit_store_reg)(TargetContext *ctx, int reg_id);

    /**
     * @brief Emits stack binary operator (+, -, *, /).
     */
    void (*emit_binop)(TargetContext *ctx, OperatorType op);

    /**
     * @brief Emits stack unary operator (negation, absolute value, etc.).
     */
    void (*emit_unop)(TargetContext *ctx, OperatorType op);

    /**
     * @brief Emits an unconditional jump to an absolute address.
     */
    void (*emit_jump)(TargetContext *ctx, uint16_t target_step);

    /**
     * @brief Emits a conditional branch based on X register evaluation.
     */
    void (*emit_cjump)(TargetContext *ctx, OperatorType cond, uint16_t target_step);

    /**
     * @brief Emits hardware loop decrement and jump (e.g., F Lk).
     */
    void (*emit_loop)(TargetContext *ctx, int reg_id, uint16_t target_step);

    /**
     * @brief Emits subroutine call (e.g., PP nn).
     */
    void (*emit_call)(TargetContext *ctx, uint16_t target_step);

    /**
     * @brief Emits subroutine return instruction (e.g., V/O).
     */
    void (*emit_return)(TargetContext *ctx);

    /**
     * @brief Emits execution halt / stop instruction (e.g., S/P).
     */
    void (*emit_halt)(TargetContext *ctx);

    /**
     * @brief Emits calculator interactive input (e.g., S/P on MK-56, PROMPT on HP).
     * The input entered into X is stored into dest_reg if dest_reg >= 0.
     */
    void (*emit_input)(TargetContext *ctx, int dest_reg);

    /**
     * @brief Emits value display and pause/halt (e.g., load X and S/P).
     */
    void (*emit_display)(TargetContext *ctx, int src_reg);

    /**
     * @brief Emits brief visual indication/pause without halting (e.g., [F] Ind).
     */
    void (*emit_pause)(TargetContext *ctx);

    /**
     * @brief Emits indirect memory read: loads from register pointed by pointer_reg into X.
     */
    void (*emit_load_indirect)(TargetContext *ctx, int pointer_reg);

    /**
     * @brief Emits indirect memory write: stores X into register pointed by pointer_reg.
     */
    void (*emit_store_indirect)(TargetContext *ctx, int pointer_reg);

    /**
     * @brief Emits a raw step with mnemonic and hex opcode.
     */
    void (*emit_raw_step)(TargetContext *ctx, const char *mnemonic, uint8_t opcode, const char *comment);

    /**
     * @brief Target-specific context teardown.
     */
    void (*cleanup)(TargetContext *ctx);
} TargetOps;

/**
 * @brief Public Target Interface binding capabilities with emission hooks.
 */
typedef struct TargetInterface {
    TargetCaps caps;
    TargetOps  ops;
} TargetInterface;

/* Target Registry API */

/**
 * @brief Registers a hardware target profile into the global target registry.
 * @param target Pointer to static TargetInterface descriptor.
 * @return true on success, false if registry full or duplicate.
 */
bool target_register(const TargetInterface *target);

/**
 * @brief Looks up a target by its short identifier name.
 * @param name Target short name (e.g., "mk56", "mk52").
 * @return TargetInterface pointer, or NULL if not found.
 */
const TargetInterface *target_find(const char *name);

/**
 * @brief Prints available targets and hardware limits to stdout.
 */
void target_list_all(void);

#endif /* TARGET_API_H */
