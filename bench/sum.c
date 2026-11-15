#include <stdio.h>

int main(void) {
    long long s = 0;
    for (int i = 0; i < 20000000; i++) s += i;
    printf("%lld\n", s);
    return 0;
}
