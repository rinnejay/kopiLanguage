#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *a = malloc(1000 * sizeof(int));
    for (int i = 0; i < 1000; i++) a[i] = i;
    long long s = 0;
    for (int r = 0; r < 20000; r++)
        for (int i = 0; i < 1000; i++) s += a[i];
    printf("%lld\n", s);
    free(a);
    return 0;
}
