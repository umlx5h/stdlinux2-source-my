#include <stdio.h>
#include <stdlib.h>

int plus1(int n) {
    return n + 1;
}

int main(int argc, char *argv[]) {
    int (*f)(int);
    int result;

    f = plus1;
    result = f(5);
    printf("%d\n", result);

    exit(0);
}
