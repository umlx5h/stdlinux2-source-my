// chapter 10

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    int i;
    
    if (argc < 2) {
        fprintf(stderr, "no mode given\n");
        exit(1);
    }

    int mode = strtol(argv[1], NULL, 8);

    for (i = 2; i < argc; i++) {
        if (chmod(argv[1], mode) < 0) {
            perror(argv[1]);
        }
    }

    exit(0);
}
