// chapter 16

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

static void log_exit(char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static void* xmalloc(size_t sz) {
    void *p;

    p = malloc(sz);
    if (!p) log_exit("failed to allocate memory");
    return p;
}

static void trap_signal(int sig, sighandler_t handler) {
    struct sigaction act;

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    if (sigaction(sig, &act, NULL) < 0)
        log_exit("sigaction(2) failed: %s", strerror(errno));
}

static void signal_exit(int sig) {
    log_exit("exit by signal %d", sig);
}

static void install_signal_handlers(void) {
    trap_signal(SIGPIPE, signal_exit);
}

struct HTTPHeaderField {
    char *name;
    char *value;
    struct HTTPHeaderField *next;
};

struct HTTPRequest {
    int protocol_minor_version;
    char *method;
    char *path;
    struct HTTPHeaderField *header;
    char *body;
    long length;
};

static void free_request(struct HTTPRequest *req) {
    struct HTTPHeaderField *h, *head;

    // ヘッダは連結リストなので
    head = req->header;
    while (head) { // 非NULLの間
        h = head;
        head = head->next;
        // 構造体のメンバ
        free(h->name);
        free(h->value);
        // 構造体自身
        free(h);
    }
    free(req->method);
    free(req->path);
    free(req->body);
    free(req);
}

static void service(FILE *in, FILE *out, char *docroot) {
    struct HTTPRequest *req;

    req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <docroot>\n", argv[0]);
        exit(1);
    }

    install_signal_handlers();
    service(stdin, stdout, argv[1]);
    exit(0);
}

