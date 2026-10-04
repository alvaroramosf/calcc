/**
 * @file 11_call_depth_overflow.c
 * @brief Negative test: Exceeding MK-56 hardware call stack depth limit of 5 levels.
 */

int f6(int x) { return x + 1; }
int f5(int x) { return f6(x) + 1; }
int f4(int x) { return f5(x) + 1; }
int f3(int x) { return f4(x) + 1; }
int f2(int x) { return f3(x) + 1; }
int f1(int x) { return f2(x) + 1; }

int main(void) {
    int res = f1(1);
    display(res);
    return res;
}
