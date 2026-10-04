/**
 * @file 01_fibonacci.c
 * @brief Computes Fibonacci numbers up to term N on Elektronika MK-56.
 *
 * Prompts user to input the number of terms N.
 * Computes and displays each subsequent term on the VFD.
 * Demonstrates:
 * - Interactive user input via input()
 * - Intermediate display pauses via display()
 * - Hardware decrement loop counter (F L0 / 0x5D)
 */

#include <mk56.h>

int main(void) {
    // Prompt user for number of terms N
    int n = input();
    int a = 0;
    int b = 1;

    // Display initial term F(0) = 0
    display(a);

    // Iteratively compute and display Fibonacci numbers
    while (n > 0) {
        display(b);
        int next = a + b;
        a = b;
        b = next;
        n = n - 1;
    }

    return b;
}
