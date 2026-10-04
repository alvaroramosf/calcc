/**
 * @file 10_indirect_array.c
 * @brief Array declaration and indirect memory addressing (K x->P, K P->x).
 */

int main(void) {
    int a[4];
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;

    int idx = 2;
    int val = a[idx];

    display(val);
    return val;
}
