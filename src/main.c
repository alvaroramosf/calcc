/**
 * @file main.c
 * @brief Entry point and command line interface for the Micro C Transpiler.
 *
 * Compiles a strict subset of C (Micro C) to retro programmable calculator
 * opcode / key sequences, targeting the Elektronika MK-56 / MK-52 RPN architecture.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

#include "ast.h"
#include "target_api.h"
#include "ir.h"
#include "regalloc.h"
#include "ast_to_ir.h"
#include "codegen.h"
#include "peephole.h"
#include "exporter.h"

extern FILE *yyin;
extern int yyparse(void);
extern ASTNode *g_ast_root;

#define TOOL_NAME "mk56c"
#define TOOL_VERSION "0.1.0-alpha"

static void print_usage(const char *prog_name) {
    printf("Micro C to Retro-Calculator Transpiler (v%s)\n\n", TOOL_VERSION);
    printf("Usage: %s [OPTIONS] <input.c>\n\n", prog_name);
    printf("Options:\n");
    printf("  -t, --target <name>      Target hardware profile (default: mk56)\n");
    printf("  -o, --output <file>      Output file path (default: stdout)\n");
    printf("  -f, --format <fmt>       Output format: asm, keys, hex, all (default: asm)\n");
    printf("      --export-all         Export all formats (.mk56, .keys, .hex)\n");
    printf("  -v, --verbose            Enable verbose compilation diagnostics\n");
    printf("      --ast-dump           Dump Abstract Syntax Tree to stdout\n");
    printf("      --ir-dump            Dump Three-Address Code IR to stdout\n");
    printf("      --list-targets       Display all supported hardware targets\n");
    printf("  -h, --help               Display this help message and exit\n");
    printf("\n");
}

int main(int argc, char **argv) {
    const char *target_name = "mk56";
    const char *output_file = NULL;
    const char *input_file = NULL;
    const char *format_str = "asm";
    bool export_all = false;
    bool verbose = false;
    bool dump_ast = false;
    bool dump_ir = false;

    static struct option long_options[] = {
        {"target",       required_argument, 0, 't'},
        {"output",       required_argument, 0, 'o'},
        {"format",       required_argument, 0, 'f'},
        {"export-all",   no_argument,       0, 1004},
        {"verbose",      no_argument,       0, 'v'},
        {"ast-dump",     no_argument,       0, 1001},
        {"ir-dump",      no_argument,       0, 1003},
        {"list-targets", no_argument,       0, 1002},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "t:o:f:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 't':
                target_name = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'f':
                format_str = optarg;
                break;
            case 1004:
                export_all = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 1001:
                dump_ast = true;
                break;
            case 1003:
                dump_ir = true;
                break;
            case 1002:
                target_list_all();
                return EXIT_SUCCESS;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        input_file = argv[optind];
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified.\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Lookup requested target */
    const TargetInterface *target = target_find(target_name);
    if (!target) {
        fprintf(stderr, "Error: Unknown target profile '%s'. Run with --list-targets to view available targets.\n", target_name);
        return EXIT_FAILURE;
    }

    /* Validate requested output format */
    if (strcmp(format_str, "asm") != 0 && strcmp(format_str, "keys") != 0 &&
        strcmp(format_str, "hex") != 0 && strcmp(format_str, "all") != 0) {
        fprintf(stderr, "Error: Unknown output format '%s'. Supported formats: asm, keys, hex, all.\n", format_str);
        return EXIT_FAILURE;
    }

    bool is_export_all = export_all || (strcmp(format_str, "all") == 0);

    /* Open output destination */
    FILE *out_fp = NULL;
    if (!is_export_all) {
        out_fp = stdout;
        if (output_file) {
            out_fp = fopen(output_file, "w");
            if (!out_fp) {
                fprintf(stderr, "Error: Could not open output file '%s' for writing.\n", output_file);
                return EXIT_FAILURE;
            }
        }
    }

    if (verbose) {
        printf("[INFO] Input source:  %s\n", input_file);
        printf("[INFO] Target chosen: %s (%s)\n", target->caps.name, target->caps.display_name);
        printf("[INFO] Architecture:  %s\n", target->caps.architecture);
        printf("[INFO] Memory Limit:  %u steps, %u data registers\n",
               target->caps.max_program_steps, target->caps.data_registers_count);
        printf("[INFO] Output Format: %s%s\n", is_export_all ? "all (.mk56, .keys, .hex)" : format_str,
               is_export_all ? " (--export-all)" : "");
    }

    /* Open and parse source file using Flex & Bison */
    FILE *in_fp = fopen(input_file, "r");
    if (!in_fp) {
        fprintf(stderr, "Error: Could not open input file '%s'\n", input_file);
        if (out_fp && out_fp != stdout) fclose(out_fp);
        return EXIT_FAILURE;
    }

    yyin = in_fp;
    g_ast_root = NULL;
    if (yyparse() != 0 || !g_ast_root) {
        fprintf(stderr, "Error: Parsing failed for '%s'.\n", input_file);
        fclose(in_fp);
        if (out_fp && out_fp != stdout) fclose(out_fp);
        return EXIT_FAILURE;
    }
    fclose(in_fp);

    if (verbose) {
        printf("[INFO] Syntax tree parsed successfully.\n");
    }

    if (dump_ast) {
        ast_dump(g_ast_root, 0);
    }

    /* Lower Abstract Syntax Tree into Intermediate Representation (TAC) */
    IRProgram *ir_prog = ast_to_ir(g_ast_root);
    if (!ir_prog) {
        fprintf(stderr, "Error: Failed to lower AST to Intermediate Representation.\n");
        ast_node_free(g_ast_root);
        if (out_fp && out_fp != stdout) fclose(out_fp);
        return EXIT_FAILURE;
    }

    /* Run Peephole Optimization pass to eliminate redundant copies & vregs */
    int opt_count = peephole_optimize(ir_prog);
    if (verbose && opt_count > 0) {
        printf("[INFO] Peephole Optimizer: eliminated %d redundant operations.\n", opt_count);
    }

    /* Run variable liveness analysis and register allocation */
    RegAllocResult alloc_res = regalloc_run(ir_prog, target);
    if (!alloc_res.success) {
        fprintf(stderr, "Error: %s\n", alloc_res.error_message);
        ir_program_free(ir_prog);
        ast_node_free(g_ast_root);
        if (out_fp && out_fp != stdout) fclose(out_fp);
        return EXIT_FAILURE;
    }

    if (verbose) {
        printf("[INFO] Register Allocation: %d variables tracked (%d dead, %d physical registers allocated out of %d max)\n",
               alloc_res.total_vars_tracked,
               alloc_res.dead_vars_count,
               alloc_res.physical_regs_used,
               target->caps.data_registers_count);
        regalloc_dump_intervals(stdout);
    }

    if (dump_ir) {
        ir_dump(ir_prog, stdout);
        regalloc_dump_intervals(stdout);
    }

    /* Initialize StepRecorder for multi-format output */
    StepRecorder recorder;
    recorder_reset(&recorder);

    /* Initialize target context */
    TargetContext ctx = {
        .target = target,
        .out = (is_export_all || strcmp(format_str, "keys") == 0 || strcmp(format_str, "hex") == 0) ? NULL : out_fp,
        .current_step = 0,
        .error_count = 0,
        .verbose = verbose,
        .target_data = &recorder
    };

    if (target->ops.init && !target->ops.init(&ctx)) {
        fprintf(stderr, "Error: Target backend initialization failed.\n");
        ir_program_free(ir_prog);
        ast_node_free(g_ast_root);
        if (out_fp && out_fp != stdout) fclose(out_fp);
        return EXIT_FAILURE;
    }

    /* Run 2-Pass Target Code Generation with Label Backpatching */
    codegen_generate(ir_prog, &ctx);

    /* Export to requested format(s) */
    if (is_export_all) {
        const char *base_name = output_file ? output_file : input_file;
        if (!export_all_files(base_name, &target->caps, &recorder)) {
            fprintf(stderr, "Error: Failed to export multi-format files.\n");
            ctx.error_count++;
        } else if (verbose) {
            printf("[INFO] Multi-format export complete (.mk56, .keys, .hex) based on '%s'\n", base_name);
        }
    } else if (strcmp(format_str, "keys") == 0) {
        export_to_keys(out_fp, &target->caps, &recorder);
    } else if (strcmp(format_str, "hex") == 0) {
        export_to_hex(out_fp, &target->caps, &recorder);
    } else {
        /* Default ASM format: instructions streamed during codegen, append regalloc report */
        regalloc_dump_intervals(out_fp);
    }

    if (target->ops.cleanup) {
        target->ops.cleanup(&ctx);
    }

    ast_node_free(g_ast_root);
    ir_program_free(ir_prog);

    if (out_fp && out_fp != stdout) {
        fclose(out_fp);
        if (verbose) {
            printf("[INFO] Output written successfully to '%s'.\n", output_file);
        }
    }

    if (dump_ast) {
        printf("[INFO] AST dump completed.\n");
    }

    return (ctx.error_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
