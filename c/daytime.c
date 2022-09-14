// chapter 16

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>


#include <arpa/inet.h>


static int open_connection(char *host, char *service);

int main(int argc, char *argv[]) {
    int sock; 
    FILE *f;
    char buf[1024];

    // sock = open_connection(argc > 1 ? argv[1] : "localhost", "daytime");
    sock = open_connection(argc > 1 ? argv[1] : "time.com", NULL);

    f = fdopen(sock, "r");
    if (!f) {
        perror("fdopen(3)");
        exit(1);
    }

    exit(0);

    fgets(buf, sizeof buf, f);
    fclose(f);
    fputs(buf, stdout);

    exit(0);
}

static int open_connection(char *host, char *service) {
    int sock;

    struct addrinfo hints, *res, *ai;
    int err;

    memset(&hints, 0, sizeof(struct addrinfo));
    // hints.ai_family = AF_UNSPEC;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((err = getaddrinfo(host, service, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo(3): %s\n", gai_strerror(err));
        exit(1);
    }


    struct in_addr addr;
    addr.s_addr= ((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
    printf("ip addres: %s\n", inet_ntoa(addr));

    printf("success getaddrinfo\n");

    return 0;
}
