/**
 * @file 09_subroutine.c
 * @brief Subroutine call (PP / 0x53) and return (B/O / 0x52) verification.
 */

int square(int x) {
    return x * x;
}

int add3(int a, int b, int c) {
    return a + b + c;
}

int main(void) {
    int s = square(4);
    int total = add3(s, 5, 6);
    display(total);
    return total;
}
