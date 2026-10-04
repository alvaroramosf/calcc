/**
 * @file exporter.h
 * @brief Multi-Format Exporter for Retro Calculators.
 *
 * Provides dedicated formatting backends for:
 * 1. Assembly Listing (.mk56): Disassembly with comments and architecture stats.
 * 2. Physical Keystroke Guide (.keys): Human typing instructions for the physical keyboard.
 * 3. Raw Bytecode Image (.hex): Hexadecimal memory image for software emulators.
 */

#ifndef EXPORTER_H
#define EXPORTER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "target_api.h"

#define MAX_RECORDED_STEPS 256

/**
 * @brief Represents a single physical calculator program step.
 */
typedef struct {
    uint8_t address;             /**< Memory address (00..97) */
    uint8_t opcode;              /**< Hexadecimal machine bytecode */
    char vfd_code[8];            /**< 7-segment display glyph (e.g. "5Г", "0-") */
    char mnemonic[32];           /**< Standard Cyrillic mnemonic */
    char keystrokes[64];         /**< Button sequence (e.g., "[ F ] [ x=0 ]") */
    char description[128];       /**< High-level operation comment */
} ProgramStep;

/**
 * @brief Supported export formats.
 */
typedef enum {
    FORMAT_ASM,
    FORMAT_KEYS,
    FORMAT_HEX,
    FORMAT_ALL
} ExportFormatType;

/**
 * @brief Container of recorded program steps.
 */
typedef struct {
    ProgramStep steps[MAX_RECORDED_STEPS];
    int count;
} StepRecorder;

/* Step Recording API */
void recorder_reset(StepRecorder *rec);
void recorder_add_step(StepRecorder *rec, uint8_t addr, uint8_t opcode,
                       const char *vfd_code, const char *mnemonic,
                       const char *keystrokes, const char *desc);

/* Exporter API */
void export_to_asm(FILE *out, const TargetCaps *caps, const StepRecorder *rec);
void export_to_keys(FILE *out, const TargetCaps *caps, const StepRecorder *rec);
void export_to_hex(FILE *out, const TargetCaps *caps, const StepRecorder *rec);

/* UTF-8 Visual Alignment Helpers */
int utf8_display_width(const char *s);
void fprint_utf8_padded(FILE *out, const char *s, int target_width);

/**
 * @brief Exports all formats to the given base filename (.mk56, .keys, .hex).
 * @param base_filename Base path (e.g., "build/loop").
 * @param caps Target capabilities descriptor.
 * @param rec Recorded steps buffer.
 */
bool export_all_files(const char *base_filename, const TargetCaps *caps, const StepRecorder *rec);

#endif /* EXPORTER_H */
