/**
 * @file 07_edge_cases.c
 * @brief Tests edge cases: scientific float literals, comments, empty statements.
 */

/* Multi-line comment block
   testing parser resilience */

int main(void) {
    // Single line comment
    ;;; // Empty statements

    float pi = 3.14159;
    float large = 1.25e2;
    float small = .005;

    float total = (pi + large) * small;

    {
        /* Nested empty block */
    }

    return 0;
}
