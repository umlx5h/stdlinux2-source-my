// chapter 14

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define INIT_BUFSIZE 1024

char* my_getcwd(void);

int main(int argc, char *argv[]) {
    int i;
    
    if (argc != 1) {
        fprintf(stderr, "%s: unexpected arguments\n", argv[0]);
        exit(1);
    }

    // pathがリークする
    char *path = my_getcwd();

    printf("%s\n", path);

    exit(0);
}

char* my_getcwd(void) {
    char *buf, *tmp;
    size_t size = INIT_BUFSIZE;

    buf = malloc(size);
    if (!buf) return NULL;
    for (;;) {
        errno = 0;
        if (getcwd(buf, size))
            return buf;
        // size分以上の長さだった場合はERANGEエラーが返ってくる
        if (errno != ERANGE) break;
        // バッファサイズを2倍
        size *= 2;
        tmp = realloc(buf, size);
        if (!tmp) break;
        buf = tmp;
    }

    // 確保できなかった場合は解放
    free(buf);
    return NULL;
}
