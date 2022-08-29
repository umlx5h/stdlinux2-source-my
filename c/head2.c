#include <stdio.h>
#include <stdlib.h>

static void do_head(FILE *f, long nlines);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s n [file file...]\n", argv[0]);
        exit(1);
    }

    long nlines = atol(argv[1]);
    if (argc == 2) {
        // ファイルの引数がなかったら標準入力から読み込む
        do_head(stdin, nlines);
    } else {
        int i;
        for (i = 2; i < argc; i++) {
            FILE *f;
            f = fopen(argv[i], "r");
            if (f == NULL) {
                perror(argv[i]);
                exit(1);
            }
            do_head(f, nlines);
            fclose(f);
        }
    }
}


static void do_head(FILE *f, long nlines) {
    int c;

    if (nlines <= 0) return;

    while ((c = getc(f)) != EOF) {
        if (putchar(c) < 0) exit(1);
        if (c == '\n') {
            nlines--;
            if (nlines == 0) return;
        }
    }
}
