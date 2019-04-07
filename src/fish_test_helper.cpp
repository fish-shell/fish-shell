// fish_test_helper is a little program with no fish dependencies that acts like certain other
// programs, allowing fish to test its behavior.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void become_foreground_then_print_stderr() {
    if (tcsetpgrp(STDOUT_FILENO, getpgrp()) < 0) {
        perror("tcsetgrp");
        exit(EXIT_FAILURE);
    }
    usleep(1000000 / 4);  //.25 secs
    fprintf(stderr, "become_foreground_then_print_stderr done\n");
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        fprintf(stderr, "No commands given.");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "become_foreground_then_print_stderr")) {
            become_foreground_then_print_stderr();
        } else {
            fprintf(stderr, "%s: Unknown command: %s\n", argv[0], argv[i]);
            return EXIT_FAILURE;
        }
    }
    return 0;
}
