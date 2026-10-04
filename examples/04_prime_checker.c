/**
 * @file 04_prime_checker.c
 * @brief Primality testing on Elektronika MK-56 via trial division.
 *
 * Prompts user to input a positive integer N.
 * Evaluates primality: displays 1 if N is prime, 0 if composite.
 * Demonstrates:
 * - Subroutine with conditional branches (if / else)
 * - Nested loops (outer loop over potential divisors, inner loop for modulo)
 * - Register reuse across inner and outer loops
 */

#include <mk56.h>

int is_prime(int n) {
    if (n <= 1) {
        return 0; // 0 and 1 are not prime
    }

    int d = 2;
    while (d * d <= n) {
        // Compute rem = n % d using repeated subtraction
        int rem = n;
        while (rem >= d) {
            rem = rem - d;
        }

        // If remainder is zero, n is divisible by d (not prime)
        if (rem == 0) {
            return 0;
        }

        d = d + 1;
    }

    return 1; // Prime!
}

int main(void) {
    // Prompt user to enter candidate number N
    int n = input();

    // Check primality and display result (1 = prime, 0 = composite)
    int result = is_prime(n);
    display(result);

    return result;
}
