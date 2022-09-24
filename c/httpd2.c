// chapter 17

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <getopt.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_REQUEST_BODY_LENGTH 4096
#define LINE_BUF_SIZE 255
#define BLOCK_BUF_SIZE 4096
#define TIME_BUF_SIZE 4096
#define HTTP_MINOR_VERSION 0
#define SERVER_NAME "httpd2"
#define SERVER_VERSION "1.0"

#define USAGE "Usage: %s [--port=n] [--chroot --user=u --group=g] <docroot>\n"
#define MAX_BACKLOG 2

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

static void noop_handler(int sig) {
    ;
}

// 子プロセスがゾンビになるのを防止する、wait()を呼ばないと宣言する
static void detach_children(void) {
    struct sigaction act;

    act.sa_handler = noop_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART | SA_NOCLDWAIT;
    if (sigaction(SIGCHLD, &act, NULL) < 0) {
        log_exit("sigaction(2) failed: %s", strerror(errno));
    }
}


static void install_signal_handlers(void) {
    trap_signal(SIGPIPE, signal_exit);
    detach_children();
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
    struct HTTPHeaderField *h;
    char buf[LINE_BUF_SIZE];
    char *p;

    // ヘッダを1行読み込む
    if (!fgets(buf, LINE_BUF_SIZE, in)) {
        // TODO: 追加した、終端まで読み込んでいたら成功扱いとしたい
        // if (feof(in)) return NULL;
        log_exit("failed to read request header field: %s", 
            strerror(errno));
    }
    // 改行文字だった場合は最後まで読んだ or ヘッダが何もなかった
    if ((buf[0] == '\n') || (strcmp(buf, "\r\n") == 0)) {
        return NULL;
    }

    /* ヘッダ名を読み込む Connection部分 */
    // Connection: close
    p = strchr(buf, ':');
    if (!p) log_exit("parse error on request header field: %s", buf);
    *p++ = '\0';
    h = xmalloc(sizeof(struct HTTPHeaderField));
    h->name = xmalloc(p - buf);
    strcpy(h->name, buf);

    /* ヘッダ値を読み込む ' close'部分の'close' */
    // Connection: close

    // 先頭の空白orタブ文字が連続している文字列長をstrspnは返すのでその分ポインタを進める 
    size_t space_length = strspn(p, " \t");
    p += space_length;

    
    // 改行文字をコピーしないように少し修正している
    char *value = p;
    p = strchr(value, '\n');
    if (!p) {
        p = strstr(value, "\r\n");
    }
    *p = '\0';
    h->value = xmalloc(strlen(value) + 1);
    // resolved: 改行文字もコピーしてしまっているが？
    // strcpy(h->value, p);
    strcpy(h->value, value);

    return h;
}

static char *lookup_header_field_value(struct HTTPRequest *req, char *field_name) {
    struct HTTPHeaderField *h;
    for (h = req->header; h; h = h->next) {
        // ヘッダーは大文字小文字を無視して比較する
        if (strcasecmp(h->name, field_name) == 0) {
            return h->value;
        }
    }

    return NULL;
}

static long content_length(struct HTTPRequest *req) {
    char *val;
    long len;

    val = lookup_header_field_value(req, "Content-Length");
    if (!val) return 0;

    len = atol(val);
    if (len < 0) log_exit("negative Content-Length value");

    return len;
}

static struct HTTPRequest *read_request(FILE *in) {
    struct HTTPRequest *req;
    struct HTTPHeaderField *h;

    req = xmalloc(sizeof(struct HTTPRequest));
    // GET /path/to/file HTTP/1.1 の部分を解析

    // リクエストラインを読む
    read_request_line(req, in);
    
    // 連結リストは後ろのヘッダから格納される
    // A1\nA2\nA3\n -> A3 -> A2 -> A1 -> NULL
    req->header = NULL;
    while ((h = read_header_field(in))) {
        h->next = req->header;
        req->header = h;
    }
    // リクエストのエンティティボディを読む、GETの場合は存在しないので読まない
    req->length = content_length(req);
    if (req->length > 0) {
        if (req->length > MAX_REQUEST_BODY_LENGTH)
            log_exit("request body too long");
        req->body = xmalloc(req->length);
        if (fread(req->body, req->length, 1, in) < 1)
            log_exit("failed to read request body: %s", strerror(errno));
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

static char *build_fspath(char *docroot, char *urlpath) {
    char *path;
    //                            スラッシュ              NULL文字
    path = xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
    sprintf(path, "%s/%s", docroot, urlpath);

    return path;
}


struct FileInfo {
    char *path;
    long size;
    int ok;
};

static void free_fileinfo(struct FileInfo *info) {
    free(info->path);
    free(info);
}

static struct FileInfo *get_fileinfo(char *docroot, char *urlpath) {
    struct FileInfo *info;
    struct stat st;

    info = xmalloc(sizeof(struct FileInfo));
    info->path = build_fspath(docroot, urlpath);
    info->ok = 0;
    if (lstat(info->path, &st) < 0) return info;
    // regular fileか確認
    if (!S_ISREG(st.st_mode)) return info;
    info->ok = 1;
    info->size = st.st_size;
    return info;
}

static char *guess_content_type(struct FileInfo *info) {
    return "text/plain";
}

static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status) {
    time_t t;
    struct tm *tm;
    char buf[TIME_BUF_SIZE];

    t = time(NULL);
    tm = gmtime(&t);
    if (!tm) log_exit("gmtime() failed: %s", strerror(errno));
    strftime(buf, TIME_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);
    fprintf(out, "HTTP/1.%d %s\r\n", HTTP_MINOR_VERSION, status);
    fprintf(out, "Date: %s\r\n", buf);
    fprintf(out, "Server: %s/%s\r\n", SERVER_NAME, SERVER_VERSION);
    fprintf(out, "Connection: close\r\n");
}

static void method_not_allowed(struct HTTPRequest *req, FILE *out) {
    output_common_header_fields(req, out, "405 Method Not Allowed");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    fprintf(out, "<html>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<title>405 Method Not Allowed</title>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<body>\r\n");
    fprintf(out, "<p>The request method %s is not allowed</p>\r\n", req->method);
    fprintf(out, "</body>\r\n");
    fprintf(out, "</html>\r\n");
    fflush(out);
}

static void not_implemented(struct HTTPRequest *req, FILE *out) {
    output_common_header_fields(req, out, "501 Not Implemented");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    fprintf(out, "<html>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<title>501 Not Implemented</title>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<body>\r\n");
    fprintf(out, "<p>The request method %s is not implemented</p>\r\n", req->method);
    fprintf(out, "</body>\r\n");
    fprintf(out, "</html>\r\n");
    fflush(out);
}

static void not_found(struct HTTPRequest *req, FILE *out) {
    output_common_header_fields(req, out, "404 Not Found");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    if (strcmp(req->method, "HEAD") != 0) {
        fprintf(out, "<html>\r\n");
        fprintf(out, "<header><title>Not Found</title><header>\r\n");
        fprintf(out, "<body><p>File not found</p></body>\r\n");
        fprintf(out, "</html>\r\n");
    }
    fflush(out);
}

static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot) {
    struct FileInfo *info;

    info = get_fileinfo(docroot, req->path);
    if (!info->ok) {
        free_fileinfo(info);
        not_found(req, out);
        return;
    }
    output_common_header_fields(req, out, "200 OK");
    fprintf(out, "Content-Length: %ld\r\n", info->size);
    fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
    fprintf(out, "\r\n");
    // if GET or POST or etc...
    if (strcmp(req->method, "HEAD") != 0) {
        int fd;
        char buf[BLOCK_BUF_SIZE];
        ssize_t n;

        fd = open(info->path, O_RDONLY);

        if (fd < 0)
            log_exit("failed to open %s: %s", info->path, strerror(errno));
        for (;;) {
            n = read(fd, buf, BLOCK_BUF_SIZE);
            if (n < 0)
                log_exit("failed to read %s: %s", info->path, strerror(errno));
            
            // 最後まで読んだ場合
            if (n == 0)
                break;
            // 呼んだ分だけソケットに対して書き込み
            if (fwrite(buf, 1, n, out) < n)
                log_exit("failed to write to socket: %s", strerror(errno));
        }
        close(fd);
    }

    fflush(out);
    free_fileinfo(info);
}

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot) {
    if (strcmp(req->method, "GET") == 0)
        do_file_response(req, out, docroot);
    else if (strcmp(req->method, "HEAD") == 0)
        do_file_response(req, out, docroot);
    else if (strcmp(req->method, "POST") == 0)
        method_not_allowed(req, out);
    else
        not_implemented(req, out);
}

static void service(FILE *in, FILE *out, char *docroot) {
    struct HTTPRequest *req;

    req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

void debug() {
    struct HTTPRequest *req;
    FILE *file;
    file = fopen("testdata/get_withbody.txt", "r");
    req = read_request(file);

    printf("read request line. method: %s, path: %s, minor_version: %d\n", req->method, req->path, req->protocol_minor_version);

    struct HTTPHeaderField *h;
    printf("read reader.\n");
    for (h = req->header; h; h = h->next) {
        printf("%s=%s\n", h->name, h->value);
    }
    printf("read reader end.\n");

    printf("content-length: %ld\n", req->length);

    if (req->length > 0) {
        printf("request body: %s\n", req->body);
    }
}

static void setup_environment(char *docroot, char *user, char *group) {

}

static void become_daemon() {

}

// socket, bind, listenを実行してソケットを返す
static int listen_socket(char *port) {
    struct addrinfo hints, *res, *ai;
    int err;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4接続
    hints.ai_socktype = SOCK_STREAM; // TCP (コネクション型)
    hints.ai_flags = AI_PASSIVE; // サーバー側はPASSIVEに設定
    if ((err = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        log_exit(gai_strerror(err));
    }

    char *err_msg = NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        int sock; 

        // 1. socket(2) で ソケットを作成
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) continue;
        // TIME_WAITのソケットが残っていると、bind(2)で「Address already in use」で失敗してしまうので、失敗しないようにSO_REUSEADDRの設定を入れる
        int optval = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
            log_exit("faild to set sockopt");

        // 2. bind(2) で 特定ポートにソケットをバインドする
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            err_msg = strerror(errno);
            continue;
        }

        // 3. listen(2) で ソケットをパッシブソケットとして設定する (接続要求を受けつけれるようにする
        // クライアント側を逆にactive socketという

        // backlogはここで指定した数だけaccept(2)を呼ぶ前にconnect(2)をしたときにESTABLISHになるソケットの数(カーネルが管理するキューサイズ)を指定する
        // このサイズ以上にconnect(2)を実行するとブロックする (SYN-SENT状態になる)
        if (listen(sock, MAX_BACKLOG) < 0) {
            close(sock);
            continue;
        }

        freeaddrinfo(res);
        return sock;
    }

    // not reached here
    log_exit("failed to listen socket: %s", err_msg);

    return -1; 
}

// accept(2)をループする関数
static void server_main(int server_fd, char *docroot) {
    for (;;) {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof addr;
        int sock;
        int pid;

        // TODO: 事前にforkしておいてそれぞれのプロセスでacceptを呼び出せばプリフォークモデルとなる
        // これは並行モデル (concurrency model)
        // accpetしたらすぐにforkして子プロセスがクライアントと通信する
        // stop("before accpet(2)");
        sock = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
        if (sock < 0) log_exit("accept(2) failed: %s", strerror(errno));
        // リクエスト解析&レスポンスを返す処理は子プロセスに任せる
        pid = fork();
        if (pid < 0) exit(3);
        if (pid == 0) { // 子プロセス
            // 子プロセスではlistening socketは使ってないのでクローズ
            close(server_fd);

            // 読み込み書き込み両方とも同じソケットを使う (accept(2)でもらったソケット)
            // forkすることで子プロセスにファイルディスクリプタがコピーされる
            // カーネルが管理している情報を指すポインタをコピーしているとイメージすればOK
            // 実体をコピーしているわけではない、あくまでも同じ情報を指している
            FILE *inf = fdopen(sock, "r");
            FILE *outf = fdopen(sock, "w");

            service(inf, outf, docroot);
            exit(0);
        }

        // 親プロセスでは使っていないためcloseしないといけない
        // closeすることで参照カウントを1つ減らすことになる(fdが差すポインタを子プロセスにコピーしているような挙動になっている)
        close(sock);
    }
}

static int debug_mode = 0;
static struct option longopts[] = {
    {"debug", no_argument, &debug_mode, 1},
    {"chroot", no_argument, NULL, 'c'},
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"port", required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
};

int main(int argc, char *argv[]) {
    int server_fd;
    char *port = NULL;
    char *docroot;
    int do_chroot = 0;
    char *user = NULL;
    char *group = NULL;
    int opt;
    
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case 0:
            break;
        case 'c':
            do_chroot = 1;
            break;
        case 'u':
            user = optarg;
            break;
        case 'g':
            group = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 'h':
            fprintf(stdout, USAGE, argv[0]);
            exit(0);
        case '?':
            fprintf(stderr, USAGE, argv[0]);
            exit(1);
        }
    }
    if (optind != argc - 1) {
        fprintf(stderr, USAGE, argv[0]);
        exit(1);
    } 
    docroot = argv[optind];

    if (do_chroot) {
        setup_environment(docroot, user, group);
        docroot = "";
    }

    install_signal_handlers();
    server_fd = listen_socket(port);

    if (!debug_mode) {
        openlog(SERVER_NAME, LOG_PID|LOG_NDELAY, LOG_DAEMON);
        become_daemon();
    }

    server_main(server_fd, docroot);
    exit(0);
}

