#include <stdio.h>
#include <stdlib.h>


void head(int line_num);

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("please specify number of lines");
        exit(1);
    }

    const char *line_num_str = argv[1];
    int line_num = atoi(line_num_str);
    if (line_num < 1) {
        printf("%s: not integer", line_num_str);
        exit(1);
    }

    head(line_num);
}


void head(int line_num) {
    int i;

    char buf[4096];

    for (i = 0; i < line_num; i++) {
        if (fgets(buf, sizeof buf, stdin) != NULL) {
            fputs(buf, stdout);
        } else {
            break;
        }
    }
}
