/**
 * @file 03_many_vars.c
 * @brief Micro C test case: declaring more than 14 variables to verify
 * that dead variable elimination and live-range sharing fit within
 * the MK-56 physical limit of 14 registers (0-9, a-e).
 */

#include "mk56.h"

int main(void) {
    /* Batch 1: Early variables (active lines 12-20) */
    int a1 = 10;
    int a2 = 20;
    int a3 = 30;
    int a4 = a1 + a2 + a3;

    /* Dead variables: declared and assigned, but never used */
    int unused_x = 999;
    int unused_y = 888;
    int unused_z = 777;

    display(a4); /* a1, a2, a3 die here */

    /* Batch 2: Disjoint lifetime variables (reusing registers freed by a1, a2, a3) */
    int b1 = 5;
    int b2 = 15;
    int b3 = 25;
    int b4 = b1 * b2 * b3;

    display(b4); /* b1, b2, b3 die here */

    /* Batch 3: More disjoint variables */
    int c1 = 100;
    int c2 = 200;
    int c3 = c1 + c2;

    /* Total declared variables in this function: 13 active + 3 dead = 16 variables! */
    return c3;
}
