#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        FILE *f;
        int c;

        f = fopen(argv[i], "r");
        if (!f) {
            // nullだったら
            perror(argv[i]);
            exit(1);
        }

        while ((c = fgetc(f)) != EOF) {
            // printf("%c", (char) c);
            // printf("#print character!");
            // if (putchar(c) < 0) exit(1);
            if (fputc(c, stdout) < 0) exit(1);
        }

        fclose(f);
    }

    exit(0);
}
