// fish_test_helper is a little program with no fish dependencies that acts like certain other
// programs, allowing fish to test its behavior.

#include <fcntl.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void become_foreground_then_print_stderr() {
    if (tcsetpgrp(STDOUT_FILENO, getpgrp()) < 0) {
        perror("tcsetgrp");
        exit(EXIT_FAILURE);
    }
    usleep(1000000 / 4);  //.25 secs
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
        usleep(1000000 / 2);
    }
}

static void sigint_parent() {
    // SIGINT the parent after 1 second, then exit
    int parent = getppid();
    usleep(1000000 / 4);  //.25 secs
    kill(parent, SIGINT);
    fprintf(stderr, "Sent SIGINT to %d\n", parent);
}

static void print_stdout_stderr() {
    fprintf(stdout, "stdout\n");
    fprintf(stderr, "stderr\n");
    fflush(nullptr);
}

static void print_pid_then_sleep() {
    fprintf(stdout, "%d\n", getpid());
    fflush(nullptr);
    usleep(1000000 / 2);  //.5 secs
}

static void print_pgrp() { fprintf(stdout, "%d\n", getpgrp()); }

static void print_fds() {
    bool needs_space = false;
    for (int fd = 0; fd <= 100; fd++) {
        if (fcntl(fd, F_GETFD) >= 0) {
            fprintf(stdout, "%s%d", needs_space ? " " : "", fd);
            needs_space = true;
        }
    }
    fputc('\n', stdout);
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
        } else if (!strcmp(argv[i], "print_pid_then_sleep")) {
            print_pid_then_sleep();
        } else if (!strcmp(argv[i], "print_pgrp")) {
            print_pgrp();
        } else if (!strcmp(argv[i], "print_fds")) {
            print_fds();
        } else {
            fprintf(stderr, "%s: Unknown command: %s\n", argv[0], argv[i]);
            return EXIT_FAILURE;
        }
    }
    return 0;
}
