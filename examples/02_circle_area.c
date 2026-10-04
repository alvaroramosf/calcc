/**
 * @file 02_circle_area.c
 * @brief Computes the area of a circle (Area = pi * r^2) on Elektronika MK-56.
 *
 * Demonstrates:
 * - Subroutine modularization with typed parameters and return value
 * - Floating-point constant loading (pi = 3.14159265)
 * - Subroutine call (PP / 0x53) and return (B/O / 0x52)
 * - RPN operational stack multiplication
 */

#include <mk56.h>

float circle_area(float r) {
    float pi = 3.14159265;
    return pi * r * r;
}

int main(void) {
    // Prompt user to enter radius 'r'
    float r = input();

    // Compute circle area via subroutine call
    float area = circle_area(r);

    // Display calculated area on VFD display
    display(area);

    return 0;
}
