/**
 * @file 06_complex_precedence.c
 * @brief Tests operator precedence, unary negation, and nested parentheses.
 */

int main(void) {
    int a = 10;
    int b = 2;
    int c = 5;
    int d = 1;
    int e = 4;
    int f = 8;
    int g = 2;

    int result = (a + b) * (c - d) + -e * (f / g);

    return result;
}
