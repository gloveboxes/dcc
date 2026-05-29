/* generates digits of PI */

#include <stdio.h>

/* pi spigot benchmark -- tests %.4d format and long array indexing */

#define SIZE 500
long r[SIZE + 1];

int main() {
    long i, k, b, d, c;

    c = 0;
    for (i = 0; i <= SIZE; i++)
        r[i] = 2000;
    for (k = SIZE; k > 0; k -= 14) {
        d = 0;
        i = k;
        for (;;) {
            d += r[i] * 10000;
            b = 2 * i - 1;
            r[i] = d % b;
            d /= b;
            i--;
            if (i == 0) break;
            d *= i;
        }
        printf("%.4d", c + d / 10000);
        fflush(stdout);
        c = d % 10000;
    }
    printf("\n");
    return 0;
}
