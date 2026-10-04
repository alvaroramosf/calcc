/**
 * @file 02_loop.c
 * @brief Micro C test case: counter loop suitable for MK-56 hardware loop (F Lk).
 */

int main(void) {
    int counter = 10;
    int acc = 0;

    while (counter > 0) {
        acc = acc + 5;
        counter = counter - 1;
    }

    return acc;
}
