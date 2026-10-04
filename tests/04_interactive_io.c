/**
 * @file 04_interactive_io.c
 * @brief Tests interactive calculator intrinsics (input, display, pause).
 */

#include "mk56.h"

int main(void) {
    /* Prompt user for width */
    double width = input();

    /* Flash temporary indication */
    pause();

    /* Prompt user for height */
    double height = input();

    /* Calculate triangle area */
    double area = (width * height) / 2.0;

    /* Display final result to user */
    display(area);

    return 0;
}
