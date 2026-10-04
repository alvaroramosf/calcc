/**
 * @file 03_dice_simulator.c
 * @brief Pseudo-random dice simulator (1..6) using an LCG generator on MK-56.
 *
 * Demonstrates:
 * - Linear Congruential PRNG implementation for retro calculators
 * - Modulo reduction via repeated subtraction
 * - Interactive seed input
 * - Reusable subroutine
 */

#include <mk56.h>

int roll(int seed) {
    // Linear congruential generator: next = (seed * 7 + 5) mod 6
    int val = seed * 7 + 5;
    while (val >= 6) {
        val = val - 6;
    }
    return val + 1; // Produces roll in range [1..6]
}

int main(void) {
    // Prompt user to enter initial integer seed (e.g. 17 or any integer)
    int seed = input();

    // Roll dice and display result
    int dice = roll(seed);
    display(dice);

    return dice;
}
