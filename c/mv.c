// chapter 10

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int i;
    
    if (argc != 3) {
        fprintf(stderr, "%s: wrong arguments\n", argv[0]);
        exit(1);
    }

    if (rename(argv[1], argv[2]) < 0) {
        perror(argv[1]);
        exit(1);
    }

    exit(0);
}
