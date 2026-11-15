#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t n = 4000, len = 0, cap = 64;
    char *s = malloc(cap);
    s[0] = 0;
    for (size_t i = 0; i < n; i++) {
        if (len + 2 > cap) { cap *= 2; s = realloc(s, cap); }
        s[len++] = 'x';
        s[len] = 0;
    }
    printf("%zu\n", len);
    free(s);
    return 0;
}
