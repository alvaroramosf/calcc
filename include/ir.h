/**
 * @file ir.h
 * @brief Three-Address Code (TAC) Intermediate Representation.
 *
 * Defines the target-agnostic intermediate representation used between
 * front-end parsing and retro-calculator code emission.
 * Provides virtual register abstraction, linear instruction sequences,
 * and data structures for liveness analysis and register allocation.
 */

#ifndef IR_H
#define IR_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Categorization of IR instruction opcodes.
 */
typedef enum {
    IR_NOP = 0,

    /* Labels and Control Flow */
    IR_LABEL,
    IR_JUMP,
    IR_JEQ,         /**< Jump if op1 == op2 */
    IR_JNE,         /**< Jump if op1 != op2 */
    IR_JLT,         /**< Jump if op1 <  op2 */
    IR_JLE,         /**< Jump if op1 <= op2 */
    IR_JGT,         /**< Jump if op1 >  op2 */
    IR_JGE,         /**< Jump if op1 >= op2 */
    IR_LOOP,        /**< Dedicated hardware loop: decrement reg, jump if != 0 */

    /* Data Movement and Memory */
    IR_ASSIGN,          /**< dest = src1 */
    IR_LOAD_INDIRECT,   /**< dest = memory[src1] (indirect recall via pointer register) */
    IR_STORE_INDIRECT,  /**< memory[dest] = src1 (indirect store via pointer register) */

    /* Arithmetic Operations */
    IR_ADD,         /**< dest = src1 + src2 */
    IR_SUB,         /**< dest = src1 - src2 */
    IR_MUL,         /**< dest = src1 * src2 */
    IR_DIV,         /**< dest = src1 / src2 */
    IR_NEG,         /**< dest = -src1 */

    /* Subroutines and Program Termination */
    IR_CALL,
    IR_RETURN,
    IR_HALT,

    /* Calculator Intrinsics */
    IR_INPUT,       /**< dest = input() */
    IR_DISPLAY,     /**< display(src1) */
    IR_PAUSE        /**< pause() */
} IROpcode;

/**
 * @brief Operand types in Three-Address Code.
 */
typedef enum {
    IR_OPERAND_NONE = 0,
    IR_OPERAND_CONST_INT,
    IR_OPERAND_CONST_FLOAT,
    IR_OPERAND_VAR,       /**< Named source-level variable */
    IR_OPERAND_VREG,      /**< Virtual temporary register */
    IR_OPERAND_LABEL,     /**< Jump target label */
    IR_OPERAND_STACK      /**< Transient value residing in RPN operational stack accumulator (X register) */
} IROperandType;

/**
 * @brief Single operand structure.
 */
typedef struct {
    IROperandType type;
    int physical_reg;     /**< Hardware register assigned by register allocator (-1 if unassigned) */
    union {
        int64_t  ival;
        double   fval;
        char    *name;    /**< Variable identifier */
        int      vreg_id; /**< Virtual register ID (0, 1, 2, ...) */
        int      label_id;/**< Label numerical index */
    } as;
} IROperand;

/**
 * @brief Three-Address Code (TAC) Quadruple / Instruction.
 */
typedef struct IRInstruction {
    IROpcode opcode;
    IROperand dest;
    IROperand src1;
    IROperand src2;

    int line;                     /**< Source line for error reporting */
    int index;                    /**< Linear instruction index in program sequence */

    struct IRInstruction *prev;
    struct IRInstruction *next;
} IRInstruction;

/**
 * @brief Container representing an entire linear IR sequence.
 */
typedef struct {
    IRInstruction *head;
    IRInstruction *tail;
    int instruction_count;
    int next_vreg_id;
    int next_label_id;
} IRProgram;

/* Operand Constructor Helpers */
IROperand ir_op_none(void);
IROperand ir_op_const_int(int64_t val);
IROperand ir_op_const_float(double val);
IROperand ir_op_var(const char *name);
IROperand ir_op_vreg(int vreg_id);
IROperand ir_op_label(int label_id);
IROperand ir_op_stack(void);

/* IR Program Lifecycle */
IRProgram *ir_program_new(void);
void ir_program_free(IRProgram *prog);

/* IR Instruction Construction and Insertion */
IRInstruction *ir_emit(IRProgram *prog, IROpcode op, IROperand dest, IROperand src1, IROperand src2, int line);
IROperand ir_alloc_vreg(IRProgram *prog);
IROperand ir_alloc_label(IRProgram *prog);

/* Diagnostic and Inspection */
void ir_dump(const IRProgram *prog, FILE *stream);
const char *ir_opcode_to_string(IROpcode op);

#endif /* IR_H */
