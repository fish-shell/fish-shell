// fish_test_helper is a little program with no fish dependencies that acts like certain other
// programs, allowing fish to test its behavior.

#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void become_foreground_then_print_stderr() {
    if (tcsetpgrp(STDOUT_FILENO, getpgrp()) < 0) {
        perror("tcsetgrp");
        exit(EXIT_FAILURE);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    fprintf(stderr, "become_foreground_then_print_stderr done\n");
}

static void report_foreground() {
    int was_fg = -1;
    const auto grp = getpgrp();
    for (;;) {
        int is_fg = (tcgetpgrp(STDIN_FILENO) == grp);
        if (is_fg != was_fg) {
            was_fg = is_fg;
            if (fputs(is_fg ? "foreground\n" : "background\n", stderr) < 0) {
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

static void sigint_parent() {
    // SIGINT the parent after 1 second, then exit
    int parent = getppid();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    kill(parent, SIGINT);
    fprintf(stderr, "Sent SIGINT to %d\n", parent);
}

static void print_stdout_stderr() {
    fprintf(stdout, "stdout\n");
    fprintf(stderr, "stderr\n");
    fflush(nullptr);
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        fprintf(stderr, "No commands given.\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "become_foreground_then_print_stderr")) {
            become_foreground_then_print_stderr();
        } else if (!strcmp(argv[i], "report_foreground")) {
            report_foreground();
        } else if (!strcmp(argv[i], "sigint_parent")) {
            sigint_parent();
        } else if (!strcmp(argv[i], "print_stdout_stderr")) {
            print_stdout_stderr();
        } else {
            fprintf(stderr, "%s: Unknown command: %s\n", argv[0], argv[i]);
            return EXIT_FAILURE;
        }
    }
    return 0;
}
