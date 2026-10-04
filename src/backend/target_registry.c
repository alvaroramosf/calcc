/**
 * @file target_registry.c
 * @brief Registry and factory for calculator hardware target profiles.
 */

#include <stdio.h>
#include <string.h>
#include "target_api.h"

#define MAX_TARGETS 16

static const TargetInterface *g_targets[MAX_TARGETS];
static int g_target_count = 0;

/* Built-in targets */
extern const TargetInterface mk56_target;

static void register_builtins(void) {
    static bool initialized = false;
    if (initialized) return;
    target_register(&mk56_target);
    initialized = true;
}

bool target_register(const TargetInterface *target) {
    if (!target || !target->caps.name) return false;
    if (g_target_count >= MAX_TARGETS) return false;

    for (int i = 0; i < g_target_count; i++) {
        if (strcmp(g_targets[i]->caps.name, target->caps.name) == 0) {
            return false; /* Duplicate target name */
        }
    }

    g_targets[g_target_count++] = target;
    return true;
}

const TargetInterface *target_find(const char *name) {
    register_builtins();
    if (!name) return NULL;

    for (int i = 0; i < g_target_count; i++) {
        if (strcmp(g_targets[i]->caps.name, name) == 0) {
            return g_targets[i];
        }
    }
    return NULL;
}

void target_list_all(void) {
    register_builtins();
    printf("Available Targets (%d registered):\n", g_target_count);
    for (int i = 0; i < g_target_count; i++) {
        const TargetCaps *caps = &g_targets[i]->caps;
        printf("  - %-8s : %s\n", caps->name, caps->display_name);
        printf("               Max Steps: %u, Registers: %u, Stack: %u (%s)\n",
               caps->max_program_steps,
               caps->data_registers_count,
               caps->stack_depth,
               caps->architecture);
    }
}
