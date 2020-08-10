// fish_test_helper is a little program with no fish dependencies that acts like certain other
// programs, allowing fish to test its behavior.

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>  // for std::begin/end

static void become_foreground_then_print_stderr() {
    if (tcsetpgrp(STDOUT_FILENO, getpgrp()) < 0) {
        perror("tcsetgrp");
        exit(EXIT_FAILURE);
    }
    usleep(1000000 / 4);  //.25 secs
    fprintf(stderr, "become_foreground_then_print_stderr done\n");
}

static void report_foreground_loop() {
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

static void report_foreground() {
    bool is_fg = (tcgetpgrp(STDIN_FILENO) == getpgrp());
    fputs(is_fg ? "foreground\n" : "background\n", stderr);
}

static void sigint_parent() {
    // SIGINT the parent after a time, then exit
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
    // On some systems getpid is a long, on others it's an int, let's just cast it.
    fprintf(stdout, "%ld\n", static_cast<long>(getpid()));
    fflush(nullptr);
    usleep(1000000 / 2);  //.5 secs
}

static void print_pgrp() { fprintf(stdout, "%ld\n", static_cast<long>(getpgrp())); }

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

static void print_signal(int sig) {
    // Print a signal description to stderr.
    if (const char *s = strsignal(sig)) {
        fprintf(stderr, "%s", s);
        if (strchr(s, ':') == nullptr) {
            fprintf(stderr, ": %d", sig);
        }
        fprintf(stderr, "\n");
    }
}

static void print_blocked_signals() {
    sigset_t sigs;
    sigemptyset(&sigs);
    if (sigprocmask(SIG_SETMASK, nullptr, &sigs)) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    // There is no obviously portable way to get the maximum number of signals.
    // Here we limit it to 32 because strsignal on OpenBSD returns "Unknown signal" for anything
    // above.
    // NetBSD taps out at 63, Linux at 64.
    for (int sig = 1; sig < 33; sig++) {
        if (sigismember(&sigs, sig)) {
            print_signal(sig);
        }
    }
}

static void print_ignored_signals() {
    for (int sig = 1; sig < 33; sig++) {
        struct sigaction act = {};
        sigaction(sig, nullptr, &act);
        if (act.sa_handler == SIG_IGN) {
            print_signal(sig);
        }
    }
}

static void show_help();

/// A thing that fish_test_helper can do.
struct fth_command_t {
    /// The argument to match against.
    const char *arg;

    /// Function to invoke.
    void (*func)();

    /// Description of what this does.
    const char *desc;
};

static fth_command_t s_commands[] = {
    {"become_foreground_then_print_stderr", become_foreground_then_print_stderr,
     "Claim the terminal (tcsetpgrp) and then print to stderr"},
    {"report_foreground", report_foreground, "Report to stderr whether we own the terminal"},
    {"report_foreground_loop", report_foreground_loop,
     "Continually report to stderr whether we own the terminal"},
    {"sigint_parent", sigint_parent, "Wait .25 seconds, then SIGINT the parent process"},
    {"print_stdout_stderr", print_stdout_stderr, "Print 'stdout' to stdout and 'stderr' to stderr"},
    {"print_pid_then_sleep", print_pid_then_sleep, "Print our pid, then sleep for .5 seconds"},
    {"print_pgrp", print_pgrp, "Print our pgroup to stdout"},
    {"print_fds", print_fds, "Print the list of active FDs to stdout"},
    {"print_blocked_signals", print_blocked_signals,
     "Print to stdout the name(s) of blocked signals"},
    {"print_ignored_signals", print_ignored_signals,
     "Print to stdout the name(s) of ignored signals"},
    {"help", show_help, "Print list of fish_test_helper commands"},
};

static void show_help() {
    printf("fish_test_helper: helper utility for fish\n\n");
    printf("Commands\n");
    printf("--------\n");
    for (const auto &cmd : s_commands) {
        printf("  %s:\n    %s\n\n", cmd.arg, cmd.desc);
    }
}

int main(int argc, char *argv[]) {
    std::sort(std::begin(s_commands), std::end(s_commands),
              [](const fth_command_t &lhs, const fth_command_t &rhs) {
                  return strcmp(lhs.arg, rhs.arg) < 0;
              });

    if (argc <= 1) {
        fprintf(stderr, "No commands given.\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "help") || !strcmp(argv[i], "-h")) {
            show_help();
            return 0;
        }

        const fth_command_t *found = nullptr;
        for (const auto &cmd : s_commands) {
            if (!strcmp(argv[i], cmd.arg)) {
                found = &cmd;
                break;
            }
        }
        if (found) {
            found->func();
        } else {
            fprintf(stderr, "%s: Unknown command: %s\n", argv[0], argv[i]);
            return EXIT_FAILURE;
        }
    }
    return 0;
}
