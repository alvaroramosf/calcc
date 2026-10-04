/**
 * @file 05_nested_control_flow.c
 * @brief Tests nested while loops and if-else branches.
 */

int main(void) {
    int n = 10;
    int acc = 0;

    while (n > 0) {
        if (n > 5) {
            acc = acc + 2;
        } else {
            acc = acc + 1;
        }
        n = n - 1;
    }

    return acc;
}
