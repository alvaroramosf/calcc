/**
 * @file 08_register_overflow.c
 * @brief Negative test case: 16 simultaneously live variables competing
 * for 14 physical registers on the MK-56 target.
 * Expected result: Compilation failure with register capacity diagnostic.
 */

int main(void) {
    int v0 = 1;
    int v1 = 2;
    int v2 = 3;
    int v3 = 4;
    int v4 = 5;
    int v5 = 6;
    int v6 = 7;
    int v7 = 8;
    int v8 = 9;
    int v9 = 10;
    int v10 = 11;
    int v11 = 12;
    int v12 = 13;
    int v13 = 14;
    int v14 = 15;
    int v15 = 16;

    /* All 16 variables are used in the final sum, so all 16 are live simultaneously! */
    int total = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 +
                v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15;

    return total;
}
