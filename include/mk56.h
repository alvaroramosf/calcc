/**
 * @file mk56.h
 * @brief Hardware Definitions and Runtime Intrinsics for Elektronika MK-56 / MK-52.
 *
 * Provides physical constants, register mappings, and runtime I/O intrinsics
 * for programs targeting the Soviet B3-34 microprocessor architecture.
 */

#ifndef ELEKTRONIKA_MK56_H
#define ELEKTRONIKA_MK56_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Hardware Architectural Constants (Elektronika MK-56 / MK-52)
 * ========================================================================== */

#define MK56_MAX_PROGRAM_STEPS   98   /**< Maximum memory capacity (addresses 00..97) */
#define MK56_DATA_REGISTERS      14   /**< Addressable data registers: 0..9, a..e */
#define MK56_STACK_DEPTH         4    /**< Physical RPN operational stack registers (X, Y, Z, T) */
#define MK56_CALL_STACK_DEPTH    5    /**< Subroutine call return address levels */

/* Register ID definitions (0 to 13) */
#define MK56_REG_0   0
#define MK56_REG_1   1
#define MK56_REG_2   2
#define MK56_REG_3   3
#define MK56_REG_4   4
#define MK56_REG_5   5
#define MK56_REG_6   6
#define MK56_REG_7   7
#define MK56_REG_8   8
#define MK56_REG_9   9
#define MK56_REG_A   10
#define MK56_REG_B   11
#define MK56_REG_C   12
#define MK56_REG_D   13
#define MK56_REG_E   14

/* ==========================================================================
 * Runtime Intrinsics
 * ========================================================================== */

/**
 * @brief Pauses execution (emits C/P [0x0E]), waiting for the user to type
 * a numerical value into the display register (X) and press C/P to resume.
 *
 * @return The value entered by the user into the X register.
 */
double input(void);

/**
 * @brief Loads the value into display register X (IP reg) and pauses execution (C/P [0x0E]).
 * Program resumes when the user presses C/P.
 *
 * @param val Value to display.
 */
void display(double val);

/**
 * @brief Triggers a momentary display flash (~0.5s) without halting execution
 * using the hardware instruction [F] Ind (0x54).
 */
void pause(void);

/**
 * @brief Clears the display register X (emits Cx [0x01]).
 */
void clear_x(void);

/**
 * @brief Swaps contents of registers X and Y (emits X<->Y [0x0D]).
 */
void swap_xy(void);

#ifdef __cplusplus
}
#endif

#endif /* ELEKTRONIKA_MK56_H */
