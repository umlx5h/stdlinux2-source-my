#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static void do_cat(const char *path, int is_stdin);
static void die(const char *s);

int main(int argc, char *argv[]) {
    int i;

    if (argc < 2) {
        do_cat("", 1);
    } else {
        for (i = 1; i < argc; i++) {
            do_cat(argv[i], 0);
        } 
    }


    exit(0);
}

#define BUFFER_SIZE 200

static void do_cat(const char *path, int is_stdin) {
    int fd;
    unsigned char buf[BUFFER_SIZE];
    ssize_t n;

    fd = STDIN_FILENO;

    if (is_stdin == 0) {
        fd = open(path, O_RDONLY);
        if (fd < 0) die(path);
    }

    for(;;) {
        // lseek(fd, 150, SEEK_CUR);
        n = read(fd, buf, sizeof buf);
        if (n < 0) die(path);
        if (n == 0) break;
        if (write(STDOUT_FILENO, buf, n) < 0) die(path);
    }

    if (is_stdin == 0) {
        if (close(fd) < 0) die(path);
    }
}

static void die(const char *s) {
    perror(s);
    exit(1);
}
