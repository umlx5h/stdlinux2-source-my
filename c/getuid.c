// chapter 14

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    printf("real uid: %d\n", getuid());
    printf("effective uid: %d\n", geteuid());
    printf("real gid: %d\n", getgid());
    printf("effective gid: %d\n", getegid());

    exit(0);
}

