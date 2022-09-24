// chapter 17

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void stop(const char *message) {
    printf("# %s\n", message);
    getchar();
}

static void log_exit(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

int main(int argc, char *argv[]) {
    const char *port = argv[1];
    const char *file_name = argv[2];

    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // Allows IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    int addrinfo_err;
    if ((addrinfo_err = getaddrinfo("localhost", port, &hints, &result)) != 0) {
        log_exit("getaddrinfo(3): %s", gai_strerror(addrinfo_err));
    }
    
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // listen(2)のbacklog+1に指定した以上connectを呼び出すとブロックする
        // 逆にbacklog以内であればノンブロッキングで成功する
        // accept(2)する前にread(2)を呼び出すとその段階でブロックする
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) != 0) {
            log_exit("failed to connect(2): %s", strerror(errno));
        }
    }

    // printf("connect(2) success!\n");

    // stop("before request");
    // FILE *http_reader = fdopen(sock, "r");
    FILE *http_writer = fdopen(sock, "w");

    fprintf(http_writer, "GET /%s HTTP/1.0\r\n", file_name);
    fprintf(http_writer, "\r\n");
    fflush(http_writer);

    // stop("after request");

    char buf[256];
    int n;
    for (;;) {
        // stop("before read");
        n = read(sock, buf, sizeof buf);
        if (n < 0) {
            log_exit("read(2) failed: %s", strerror(errno));
        }
        if (n == 0) {
            break;
        }
        // stop("before write");
        if (write(1, buf, n) < 0) {
            log_exit("write(2) failed: %s", strerror(errno));
        }
    }

    close(sock);
    exit(0);
}
