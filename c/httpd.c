// chapter 16

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define MAX_REQUEST_BODY_LENGTH 4096
#define LINE_BUF_SIZE 255

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

static void upcase(char *str) {
    char *p;
            // null文字ではない間
    for (p = str; *p; p++) {
        *p = (char)toupper((int)*p);
    }
}

static void read_request_line(struct HTTPRequest *req, FILE *in) {
    char buf[LINE_BUF_SIZE];
    char *path, *p;

    if (!fgets(buf, LINE_BUF_SIZE, in)) {
        // error もしくは EOF(何も読めなかった場合)
        log_exit("no request line");
    }

    // GET /path/to/file HTTP/1.1 の部分を解析

    /* メソッド部分を読み込み */
    // 最初の空白を差す文字列のポインタを返す
    p = strchr(buf, ' ');
    if (!p) log_exit("parse error on request line (1): %s", buf);
    // GET<空白>/hogeの空白部分をnull文字で置き換えてからポインタを一個進めている (以後/を指す)
    // equivalent: *p = '\0'; p++;
    *p++ = '\0';
    req->method = xmalloc(p - buf);
    // null文字までコピーするのでGET\0がコピーされる
    strcpy(req->method, buf);
    upcase(req->method);

    /* パス部分を読み込み */
    path = p;
    p = strchr(path, ' ');
    if (!p) log_exit("parse error on request line (2): %s", buf);
    *p++ = '\0';
    req->path = xmalloc(p - path);
    strcpy(req->path, path);

    /* HTTPバージョン部分 */
    // 大文字小文字を無視しメジャーバージョンまで一致することを確認
    if (strncasecmp(p, "HTTP/1.", strlen("HTTP/1.")) != 0)
        log_exit("parse error on request line (3): %s", buf);
    // マイナーバージョンを取り出す
    p += strlen("HTTP/1.");
    req->protocol_minor_version = atoi(p);
}

static struct HTTPHeaderField *read_header_field(FILE *in) {

}

static long content_length(struct HTTPRequest *req) {
    return 0;
}

static struct HTTPRequest *read_request(FILE *in) {
    struct HTTPRequest *req;
    struct HTTPHeaderField *h;

    req = xmalloc(sizeof(struct HTTPRequest));
    // GET /path/to/file HTTP/1.1 の部分を解析
    read_request_line(req, in);
    req->header = NULL;
    while ((h = read_header_field(in))) {
        h->next = req->header;
        req->header = h;
    }
    // リクエストのエンティティボディを読む、GETの場合は存在しないので読まない
    req->length = content_length(req);
    if (req->length != 0) {
        if (req->length > MAX_REQUEST_BODY_LENGTH)
            log_exit("request body too long");
        req->body = xmalloc(req->length);
        if (fread(req->body, req->length, 1, in) < 1)
            log_exit("failed to read request body");
    } else {
        req->body = NULL;
    }

    return req;
}

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

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot) {

}

static void service(FILE *in, FILE *out, char *docroot) {
    struct HTTPRequest *req;

    req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

int main(int argc, char *argv[]) {
    struct HTTPRequest req;
    // read_request_line(&req, stdin);

    FILE *file;
    file = fopen("test.txt", "r");
    read_request_line(&req, file);

    printf("method: %s, path: %s, minor_version: %d", req.method, req.path, req.protocol_minor_version);
    exit(0);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <docroot>\n", argv[0]);
        exit(1);
    }

    install_signal_handlers();
    service(stdin, stdout, argv[1]);
    exit(0);
}

