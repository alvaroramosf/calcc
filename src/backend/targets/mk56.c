/**
 * @file mk56.c
 * @brief Backend Hardware Profile for Elektronika MK-56 / MK-52.
 *
 * Implements the TargetInterface for Soviet RPN programmable calculators
 * based on the B3-34 architecture (K145IK1302/K145IK1303 microprocessors).
 *
 * Architecture specifications:
 * - 98 program steps (addresses 00 through 97)
 * - 14 addressable data registers: 0-9, a, b, c, d, e (stored in static memory)
 * - 4-level operational RPN stack: X, Y, Z, T, plus X1 shadow register
 * - Inverted conditional branch semantics (jump taken when condition IS met)
 * - Dedicated hardware loop counter instructions (F L0 - F Ld)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "target_api.h"
#include "exporter.h"

/* Official MK-56 Instruction Opcodes (Technical Manual Table 4) */
#define MK56_OP_CLEAR     0x0D /* Сх (Clear X) */
#define MK56_OP_ENTER     0x0E /* В↑ (Enter / Push stack) */
#define MK56_OP_NEG       0x0B /* /-/ (Change sign) */
#define MK56_OP_ADD       0x10 /* + */
#define MK56_OP_SUB       0x11 /* - */
#define MK56_OP_MUL       0x12 /* * (Multiplication) */
#define MK56_OP_DIV       0x13 /* ÷ (Division) */
#define MK56_OP_SWAP      0x14 /* ↔ (Swap X and Y) */
#define MK56_OP_PI        0x20 /* F π (Load constant Pi) */

#define MK56_OP_HALT      0x50 /* С/П (Start / Stop) */
#define MK56_OP_JUMP      0x51 /* БП nn (Direct unconditional jump) */
#define MK56_OP_RETURN    0x52 /* В/О (Return / Reset) */
#define MK56_OP_CALL      0x53 /* ПП nn (Call subroutine) */
#define MK56_OP_NOP       0x54 /* K НОП (No Operation) */

#define MK56_OP_JNZ       0x57 /* F x!=0 nn (Inverted: jumps if X == 0) */
#define MK56_OP_JGE       0x59 /* F x>=0 nn (Inverted: jumps if X < 0) */
#define MK56_OP_JLT       0x5C /* F x<0 nn  (Inverted: jumps if X >= 0) */
#define MK56_OP_JZ        0x5E /* F x=0 nn  (Inverted: jumps if X != 0) */

#define MK56_OP_STORE_BASE 0x40 /* x→П 0..d (0x40 - 0x4D) */
#define MK56_OP_LOAD_BASE  0x60 /* П→x 0..d (0x60 - 0x6D) */

/* Forward declarations */
static bool mk56_init(TargetContext *ctx);
static void mk56_emit_prologue(TargetContext *ctx);
static void mk56_emit_epilogue(TargetContext *ctx);
static void mk56_emit_push_constant(TargetContext *ctx, double value);
static void mk56_emit_load_reg(TargetContext *ctx, int reg_id);
static void mk56_emit_store_reg(TargetContext *ctx, int reg_id);
static void mk56_emit_binop(TargetContext *ctx, OperatorType op);
static void mk56_emit_unop(TargetContext *ctx, OperatorType op);
static void mk56_emit_jump(TargetContext *ctx, uint16_t target_step);
static void mk56_emit_cjump(TargetContext *ctx, OperatorType cond, uint16_t target_step);
static void mk56_emit_loop(TargetContext *ctx, int reg_id, uint16_t target_step);
static void mk56_emit_call(TargetContext *ctx, uint16_t target_step);
static void mk56_emit_return(TargetContext *ctx);
static void mk56_emit_halt(TargetContext *ctx);
static void mk56_emit_input(TargetContext *ctx, int dest_reg);
static void mk56_emit_display(TargetContext *ctx, int src_reg);
static void mk56_emit_pause(TargetContext *ctx);
static void mk56_emit_load_indirect(TargetContext *ctx, int pointer_reg);
static void mk56_emit_store_indirect(TargetContext *ctx, int pointer_reg);
static void mk56_emit_raw_step(TargetContext *ctx, const char *mnemonic, uint8_t opcode, const char *comment);
static void mk56_cleanup(TargetContext *ctx);

/**
 * @brief Singleton TargetInterface descriptor for Elektronika MK-56.
 */
const TargetInterface mk56_target = {
    .caps = {
        .name = "mk56",
        .display_name = "Elektronika MK-56 / MK-52 (B3-34 Series)",
        .architecture = "Soviet RPN (4-level X/Y/Z/T + X1)",
        .max_program_steps = 98,
        .data_registers_count = 14,
        .stack_depth = 4,
        .call_stack_depth = 5,
        .has_shadow_register = true,
        .has_hardware_loops = true,
        .supports_indirect_addr = true
    },
    .ops = {
        .init = mk56_init,
        .emit_prologue = mk56_emit_prologue,
        .emit_epilogue = mk56_emit_epilogue,
        .emit_push_constant = mk56_emit_push_constant,
        .emit_load_reg = mk56_emit_load_reg,
        .emit_store_reg = mk56_emit_store_reg,
        .emit_binop = mk56_emit_binop,
        .emit_unop = mk56_emit_unop,
        .emit_jump = mk56_emit_jump,
        .emit_cjump = mk56_emit_cjump,
        .emit_loop = mk56_emit_loop,
        .emit_call = mk56_emit_call,
        .emit_return = mk56_emit_return,
        .emit_halt = mk56_emit_halt,
        .emit_input = mk56_emit_input,
        .emit_display = mk56_emit_display,
        .emit_pause = mk56_emit_pause,
        .emit_load_indirect = mk56_emit_load_indirect,
        .emit_store_indirect = mk56_emit_store_indirect,
        .emit_raw_step = mk56_emit_raw_step,
        .cleanup = mk56_cleanup
    }
};

/* Implementation of Target Operations */

static bool mk56_init(TargetContext *ctx) {
    if (!ctx) return false;
    ctx->current_step = 0;
    ctx->error_count = 0;
    return true;
}

static void mk56_emit_prologue(TargetContext *ctx) {
    if (!ctx || !ctx->out) return;
    fprintf(ctx->out, "; ==========================================================\n");
    fprintf(ctx->out, "; Target: %s\n", ctx->target->caps.display_name);
    fprintf(ctx->out, "; Architecture: %s\n", ctx->target->caps.architecture);
    fprintf(ctx->out, "; Max Capacity: %u steps, %u memory registers\n",
            ctx->target->caps.max_program_steps,
            ctx->target->caps.data_registers_count);
    fprintf(ctx->out, "; Generated by Micro C Transpiler\n");
    fprintf(ctx->out, "; ==========================================================\n");
    fprintf(ctx->out, "; Step | VFD  | Hex   | Cyrillic | Description\n");
    fprintf(ctx->out, "; ----------------------------------------------------------\n");
}

static void mk56_emit_epilogue(TargetContext *ctx) {
    if (!ctx) return;
    StepRecorder *rec = (StepRecorder *)ctx->target_data;
    bool already_terminated = false;
    if (rec && rec->count > 0) {
        uint8_t last_op = rec->steps[rec->count - 1].opcode;
        if (last_op == MK56_OP_HALT || last_op == MK56_OP_RETURN) {
            already_terminated = true;
        }
    }
    if (!already_terminated) {
        mk56_emit_halt(ctx);
    }
    if (!ctx->out) return;
    fprintf(ctx->out, "; ----------------------------------------------------------\n");
    fprintf(ctx->out, "; Program compilation summary:\n");
    fprintf(ctx->out, "; Total steps consumed: %u / %u (%.1f%% memory used)\n",
            ctx->current_step,
            ctx->target->caps.max_program_steps,
            (100.0f * ctx->current_step) / ctx->target->caps.max_program_steps);
    if (ctx->current_step > ctx->target->caps.max_program_steps) {
        fprintf(ctx->out, "; ERROR: Program exceeds MK-56 98-step hardware capacity!\n");
        ctx->error_count++;
    }
}

static void mk56_emit_push_constant(TargetContext *ctx, double value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4g", value);

    /* Special case: Pi constant (Elektronika MK-56 native F π instruction) */
    if (fabs(value - 3.1415926535) < 1e-4) {
        mk56_emit_raw_step(ctx, "F π", MK56_OP_PI, "Load constant Pi into X");
        mk56_emit_raw_step(ctx, "B^", MK56_OP_ENTER, "Push X to Y (Enter)");
        return;
    }

    /* Single decimal digit: 0..9 (MK-56 native opcodes 0x00..0x09) */
    if (value >= 0.0 && value <= 9.0 && (double)((int)value) == value) {
        int d = (int)value;
        char digit_str[4];
        snprintf(digit_str, sizeof(digit_str), "%d", d);
        mk56_emit_raw_step(ctx, digit_str, (uint8_t)d, "Load numeric constant into X");
        mk56_emit_raw_step(ctx, "B^", MK56_OP_ENTER, "Push X to Y (Enter)");
        return;
    }

    /* Multi-digit integer or general constant */
    uint8_t opcode = 0x00;
    if (value >= 0.0 && value <= 255.0 && (double)((int)value) == value) {
        opcode = (uint8_t)value;
    }
    mk56_emit_raw_step(ctx, buf, opcode, "Load numeric constant into X");
    mk56_emit_raw_step(ctx, "B^", MK56_OP_ENTER, "Push X to Y (Enter)");
}

static void mk56_emit_load_reg(TargetContext *ctx, int reg_id) {
    if (reg_id < 0 || reg_id >= 14) {
        if (ctx) ctx->error_count++;
        return;
    }
    char mnem[16];
    char reg_char = (reg_id < 10) ? ('0' + reg_id) : ('a' + (reg_id - 10));
    snprintf(mnem, sizeof(mnem), "ИП %c", reg_char);
    uint8_t opcode = MK56_OP_LOAD_BASE + reg_id;
    mk56_emit_raw_step(ctx, mnem, opcode, "Load register into X");
}

static void mk56_emit_store_reg(TargetContext *ctx, int reg_id) {
    if (reg_id < 0 || reg_id >= 14) {
        if (ctx) ctx->error_count++;
        return;
    }
    char mnem[16];
    char reg_char = (reg_id < 10) ? ('0' + reg_id) : ('a' + (reg_id - 10));
    snprintf(mnem, sizeof(mnem), "П %c", reg_char);
    uint8_t opcode = MK56_OP_STORE_BASE + reg_id;
    mk56_emit_raw_step(ctx, mnem, opcode, "Store X into register");
}

static void mk56_emit_binop(TargetContext *ctx, OperatorType op) {
    switch (op) {
        case OP_ADD:
            mk56_emit_raw_step(ctx, "+", MK56_OP_ADD, "X = Y + X");
            break;
        case OP_SUB:
            mk56_emit_raw_step(ctx, "-", MK56_OP_SUB, "X = Y - X");
            break;
        case OP_MUL:
            mk56_emit_raw_step(ctx, "*", MK56_OP_MUL, "X = Y * X");
            break;
        case OP_DIV:
            mk56_emit_raw_step(ctx, "/", MK56_OP_DIV, "X = Y / X");
            break;
        default:
            if (ctx) ctx->error_count++;
            break;
    }
}

static void mk56_emit_unop(TargetContext *ctx, OperatorType op) {
    switch (op) {
        case OP_NEG:
            mk56_emit_raw_step(ctx, "/-/", MK56_OP_NEG, "X = -X (Negate)");
            break;
        default:
            if (ctx) ctx->error_count++;
            break;
    }
}

static void mk56_emit_jump(TargetContext *ctx, uint16_t target_step) {
    char target_str[8];
    snprintf(target_str, sizeof(target_str), "%02u", target_step);
    mk56_emit_raw_step(ctx, "БП", MK56_OP_JUMP, "Unconditional Jump");
    mk56_emit_raw_step(ctx, target_str, (uint8_t)target_step, "Jump destination address");
}

static void mk56_emit_cjump(TargetContext *ctx, OperatorType cond, uint16_t target_step) {
    const char *mnem = "x=0";
    uint8_t opcode = MK56_OP_JZ;

    switch (cond) {
        case OP_EQ:
            mnem = "x=0";
            opcode = MK56_OP_JZ;
            break;
        case OP_NEQ:
            mnem = "x!=0";
            opcode = MK56_OP_JNZ;
            break;
        case OP_GTE:
            mnem = "x>=0";
            opcode = MK56_OP_JGE;
            break;
        case OP_LT:
            mnem = "x<0";
            opcode = MK56_OP_JLT;
            break;
        default:
            break;
    }

    char target_str[8];
    snprintf(target_str, sizeof(target_str), "%02u", target_step);
    mk56_emit_raw_step(ctx, mnem, opcode, "Conditional branch");
    mk56_emit_raw_step(ctx, target_str, (uint8_t)target_step, "Branch target address");
}

static void mk56_emit_loop(TargetContext *ctx, int reg_id, uint16_t target_step) {
    if (reg_id < 0 || reg_id > 3) {
        /* Standard loop for registers beyond 0..3 */
        if (ctx) ctx->error_count++;
        return;
    }
    static const uint8_t loop_opcodes[4] = {0x5D, 0x5B, 0x58, 0x5A};
    char mnem[16];
    char target_str[8];
    snprintf(mnem, sizeof(mnem), "F L%d", reg_id);
    snprintf(target_str, sizeof(target_str), "%02u", target_step);

    mk56_emit_raw_step(ctx, mnem, loop_opcodes[reg_id], "Hardware loop decrement & branch if != 0");
    mk56_emit_raw_step(ctx, target_str, (uint8_t)target_step, "Loop restart address");
}

static void mk56_emit_call(TargetContext *ctx, uint16_t target_step) {
    char target_str[8];
    snprintf(target_str, sizeof(target_str), "%02u", target_step);
    mk56_emit_raw_step(ctx, "ПП", MK56_OP_CALL, "Call subroutine");
    mk56_emit_raw_step(ctx, target_str, (uint8_t)target_step, "Subroutine address");
}

static void mk56_emit_return(TargetContext *ctx) {
    mk56_emit_raw_step(ctx, "В/О", MK56_OP_RETURN, "Return from subroutine");
}

static void mk56_emit_halt(TargetContext *ctx) {
    mk56_emit_raw_step(ctx, "С/П", MK56_OP_HALT, "Stop program execution");
}

static void mk56_emit_input(TargetContext *ctx, int dest_reg) {
    mk56_emit_raw_step(ctx, "С/П", MK56_OP_HALT, "Prompt user: Enter value into X and press C/P");
    if (dest_reg >= 0) {
        mk56_emit_store_reg(ctx, dest_reg);
    }
}

static void mk56_emit_display(TargetContext *ctx, int src_reg) {
    if (src_reg >= 0) {
        mk56_emit_load_reg(ctx, src_reg);
    }
    mk56_emit_raw_step(ctx, "С/П", MK56_OP_HALT, "Display value in X and pause for user");
}

static void mk56_emit_pause(TargetContext *ctx) {
    /* [F] Ind: Brief display flash without halting execution (~0.5s) */
    mk56_emit_raw_step(ctx, "F Инд", 0x54, "Momentary display indication (Pause)");
}

static void mk56_emit_load_indirect(TargetContext *ctx, int pointer_reg) {
    if (pointer_reg < 0 || pointer_reg >= 14) {
        if (ctx) ctx->error_count++;
        return;
    }
    char mnem[16];
    char reg_char = (pointer_reg < 10) ? ('0' + pointer_reg) : ('a' + (pointer_reg - 10));
    snprintf(mnem, sizeof(mnem), "K П→x %c", reg_char);
    uint8_t opcode = 0xD0 + pointer_reg;
    mk56_emit_raw_step(ctx, mnem, opcode, "Indirect load: X = memory[R]");
}

static void mk56_emit_store_indirect(TargetContext *ctx, int pointer_reg) {
    if (pointer_reg < 0 || pointer_reg >= 14) {
        if (ctx) ctx->error_count++;
        return;
    }
    char mnem[16];
    char reg_char = (pointer_reg < 10) ? ('0' + pointer_reg) : ('a' + (pointer_reg - 10));
    snprintf(mnem, sizeof(mnem), "K x→П %c", reg_char);
    uint8_t opcode = 0xB0 + pointer_reg;
    mk56_emit_raw_step(ctx, mnem, opcode, "Indirect store: memory[R] = X");
}

static void get_vfd_and_keys(uint8_t opcode, const char *mnem, char *vfd, size_t vfd_sz, char *keys, size_t keys_sz) {
    if (mnem && strlen(mnem) == 2 && mnem[0] >= '0' && mnem[0] <= '9' && mnem[1] >= '0' && mnem[1] <= '9') {
        snprintf(vfd, vfd_sz, "%s", mnem);
        snprintf(keys, keys_sz, "[ %c ] [ %c ]", mnem[0], mnem[1]);
    } else if (opcode <= 0x09) {
        snprintf(vfd, vfd_sz, "%02X", opcode);
        snprintf(keys, keys_sz, "[ %d ]", opcode);
    } else if (opcode == 0x0B) {
        snprintf(vfd, vfd_sz, "0L");
        snprintf(keys, keys_sz, "[ /-/ ]");
    } else if (opcode == 0x0D) {
        snprintf(vfd, vfd_sz, "0Г");
        snprintf(keys, keys_sz, "[ Сх ]");
    } else if (opcode == 0x0E) {
        snprintf(vfd, vfd_sz, "0E");
        snprintf(keys, keys_sz, "[ В↑ ]");
    } else if (opcode >= 0x40 && opcode <= 0x4D) {
        int r = opcode - 0x40;
        char r_ch = (r < 10) ? ('0' + r) : ('a' + (r - 10));
        snprintf(vfd, vfd_sz, "%02X", opcode);
        snprintf(keys, keys_sz, "[ x→П ] [ %c ]", r_ch);
    } else if (opcode >= 0x60 && opcode <= 0x6D) {
        int r = opcode - 0x60;
        char r_ch = (r < 10) ? ('0' + r) : ('a' + (r - 10));
        snprintf(vfd, vfd_sz, "%02X", opcode);
        snprintf(keys, keys_sz, "[ П→x ] [ %c ]", r_ch);
    } else if (opcode >= 0xB0 && opcode <= 0xBD) {
        int r = opcode - 0xB0;
        char r_ch = (r < 10) ? ('0' + r) : ('a' + (r - 10));
        static const char *b_vfd[] = {"L0","L1","L2","L3","L4","L5","L6","L7","L8","L9","L-","LL","LC","LГ"};
        snprintf(vfd, vfd_sz, "%s", b_vfd[r]);
        snprintf(keys, keys_sz, "[ K ] [ x→П ] [ %c ]", r_ch);
    } else if (opcode >= 0xD0 && opcode <= 0xDD) {
        int r = opcode - 0xD0;
        char r_ch = (r < 10) ? ('0' + r) : ('a' + (r - 10));
        static const char *d_vfd[] = {"Г0","Г1","Г2","Г3","Г4","Г5","Г6","Г7","Г8","Г9","Г-","ГL","ГC","ГГ"};
        snprintf(vfd, vfd_sz, "%s", d_vfd[r]);
        snprintf(keys, keys_sz, "[ K ] [ П→x ] [ %c ]", r_ch);
    } else if (opcode == 0x10) {
        snprintf(vfd, vfd_sz, "10");
        snprintf(keys, keys_sz, "[ + ]");
    } else if (opcode == 0x11) {
        snprintf(vfd, vfd_sz, "11");
        snprintf(keys, keys_sz, "[ - ]");
    } else if (opcode == 0x12) {
        snprintf(vfd, vfd_sz, "12");
        snprintf(keys, keys_sz, "[ * ]");
    } else if (opcode == 0x13) {
        snprintf(vfd, vfd_sz, "13");
        snprintf(keys, keys_sz, "[ ÷ ]");
    } else if (opcode == 0x14) {
        snprintf(vfd, vfd_sz, "14");
        snprintf(keys, keys_sz, "[ ↔ ]");
    } else if (opcode == MK56_OP_PI) {
        snprintf(vfd, vfd_sz, "20");
        snprintf(keys, keys_sz, "[ F ] [ π ]");
    } else if (opcode == 0x50) {
        snprintf(vfd, vfd_sz, "50");
        snprintf(keys, keys_sz, "[ С/П ]");
    } else if (opcode == 0x51) {
        snprintf(vfd, vfd_sz, "51");
        snprintf(keys, keys_sz, "[ БП ]");
    } else if (opcode == 0x52) {
        snprintf(vfd, vfd_sz, "52");
        snprintf(keys, keys_sz, "[ В/О ]");
    } else if (opcode == 0x53) {
        snprintf(vfd, vfd_sz, "53");
        snprintf(keys, keys_sz, "[ ПП ]");
    } else if (opcode == 0x54) {
        snprintf(vfd, vfd_sz, "54");
        snprintf(keys, keys_sz, "[ F ] [ Инд ]");
    } else if (opcode == 0x57) {
        snprintf(vfd, vfd_sz, "57");
        snprintf(keys, keys_sz, "[ F ] [ x≠0 ]");
    } else if (opcode == 0x58) {
        snprintf(vfd, vfd_sz, "58");
        snprintf(keys, keys_sz, "[ F ] [ L2 ]");
    } else if (opcode == 0x59) {
        snprintf(vfd, vfd_sz, "59");
        snprintf(keys, keys_sz, "[ F ] [ x≥0 ]");
    } else if (opcode == 0x5A) {
        snprintf(vfd, vfd_sz, "5-");
        snprintf(keys, keys_sz, "[ F ] [ L3 ]");
    } else if (opcode == 0x5B) {
        snprintf(vfd, vfd_sz, "5L");
        snprintf(keys, keys_sz, "[ F ] [ L1 ]");
    } else if (opcode == 0x5C) {
        snprintf(vfd, vfd_sz, "5C");
        snprintf(keys, keys_sz, "[ F ] [ x<0 ]");
    } else if (opcode == 0x5D) {
        snprintf(vfd, vfd_sz, "5Г");
        snprintf(keys, keys_sz, "[ F ] [ L0 ]");
    } else if (opcode == 0x5E) {
        snprintf(vfd, vfd_sz, "5E");
        snprintf(keys, keys_sz, "[ F ] [ x=0 ]");
    } else {
        snprintf(vfd, vfd_sz, "%02X", opcode);
        if (mnem && strlen(mnem) == 2 && mnem[0] >= '0' && mnem[0] <= '9' && mnem[1] >= '0' && mnem[1] <= '9') {
            snprintf(keys, keys_sz, "[ %c ] [ %c ]", mnem[0], mnem[1]);
        } else {
            snprintf(keys, keys_sz, "[ %s ]", mnem ? mnem : "?");
        }
    }
}

static void mk56_emit_raw_step(TargetContext *ctx, const char *mnemonic, uint8_t opcode, const char *comment) {
    if (!ctx) return;

    char vfd[16] = {0};
    char keys[64] = {0};
    get_vfd_and_keys(opcode, mnemonic, vfd, sizeof(vfd), keys, sizeof(keys));

    if (ctx->target_data) {
        recorder_add_step((StepRecorder *)ctx->target_data,
                          (uint8_t)ctx->current_step,
                          opcode,
                          vfd,
                          mnemonic,
                          keys,
                          comment ? comment : "");
    }

    if (ctx->out) {
        fprintf(ctx->out, "  %02u:   ", ctx->current_step);
        fprint_utf8_padded(ctx->out, vfd, 4);
        fprintf(ctx->out, "  0x%02X   ", opcode);
        fprint_utf8_padded(ctx->out, mnemonic, 8);
        fprintf(ctx->out, "  ; %s\n", comment ? comment : "");
    }
    ctx->current_step++;
}

static void mk56_cleanup(TargetContext *ctx) {
    (void)ctx;
}
