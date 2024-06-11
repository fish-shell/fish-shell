// fish_test_helper is a little program with no fish dependencies that acts like certain other
// programs, allowing fish to test its behavior.

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void abandon_tty() {
    // The parent may get SIGSTOPed when it tries to call tcsetpgrp if the child has already done
    // it. Prevent this by ignoring signals.
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    // Both parent and child do the same thing.
    pid_t child = pid ? pid : getpid();
    if (setpgid(child, child)) {
        perror("setpgid");
        exit(EXIT_FAILURE);
    }
    // tcsetpgrp may fail in the parent if the child has already exited.
    // This is the benign race.
    (void)tcsetpgrp(STDIN_FILENO, child);
    // Parent waits for child to exit.
    if (pid > 0) {
        waitpid(child, NULL, 0);
    }
}

static void become_foreground_then_print_stderr() {
    if (tcsetpgrp(STDOUT_FILENO, getpgrp()) < 0) {
        perror("tcsetgrp");
        exit(EXIT_FAILURE);
    }
    usleep(1000000 / 4);  //.25 secs
    fprintf(stderr, "become_foreground_then_print_stderr done\n");
}

static void nohup_wait() {
    pid_t init_parent = getppid();
    if (signal(SIGHUP, SIG_IGN)) {
        perror("tcsetgrp");
        exit(EXIT_FAILURE);
    }
    // Note: these silly close() calls are necessary to prevent our parent process (presumably fish)
    // from getting stuck in the "E" state ("Trying to exit"). This appears to be a (kernel?) bug on
    // macOS: the process is no longer running but is not a zombie either, and so cannot be reaped.
    // It is unclear why closing these fds successfully works around this issue.
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    // To avoid leaving fish_test_helpers around, we exit once our parent changes, meaning the fish
    // instance exited.
    while (getppid() == init_parent) {
        usleep(1000000 / 4);
    }
}

static void report_foreground_loop() {
    int was_fg = -1;
    const pid_t grp = getpgrp();
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
    fflush(NULL);
}

static void print_pid_then_sleep() {
    // On some systems getpid is a long, on others it's an int, let's just cast it.
    fprintf(stdout, "%ld\n", (long)getpid());
    fflush(NULL);
    usleep(1000000 / 2);  //.5 secs
}

static void print_pgrp() { fprintf(stdout, "%ld\n", (long)getpgrp()); }

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
    const char *s = strsignal(sig);
    if (s) {
        fprintf(stderr, "%s", s);
        if (strchr(s, ':') == NULL) {
            fprintf(stderr, ": %d", sig);
        }
        fprintf(stderr, "\n");
    }
}

static void print_blocked_signals() {
    sigset_t sigs;
    sigemptyset(&sigs);
    if (sigprocmask(SIG_SETMASK, NULL, &sigs)) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    // There is no obviously portable way to get the maximum number of signals.
    // POSIX says sigqueue(2) can be used with signo 0 to validate the pid and signo parameters,
    // but it is missing from OpenBSD and returns ENOSYS (not implemented) under WSL.
    // Here we limit it to 32 because strsignal on OpenBSD returns "Unknown signal" for anything
    // above, while NetBSD taps out at 63, and Linux at 64.
    for (int sig = 1; sig < 33; sig++) {
        if (sigismember(&sigs, sig)) {
            print_signal(sig);
        }
    }
}

static void print_ignored_signals() {
    for (int sig = 1; sig < 33; sig++) {
        struct sigaction act = {};
        sigaction(sig, NULL, &act);
        if (act.sa_handler == SIG_IGN) {
            print_signal(sig);
        }
    }
}

static void sigtstp_handler(int x) {
    write(STDOUT_FILENO, "SIGTSTP\n", strlen("SIGTSTP\n"));
    kill(getpid(), SIGSTOP);
}
static void sigcont_handler(int x) {
    write(STDOUT_FILENO, "SIGCONT\n", strlen("SIGCONT\n"));
}
static void print_stop_cont() {
    signal(SIGTSTP, &sigtstp_handler);
    signal(SIGCONT, &sigcont_handler);
    char buff[1];
    for (;;) {
        if (read(STDIN_FILENO, buff, sizeof buff) >= 0) {
            exit(0);
        }
    }
}

static void sigkill_self() {
    kill(getpid(), SIGKILL);
    usleep(20000000);  // 20 secs
    abort();
}

static void sigint_self() {
    kill(getpid(), SIGINT);
    usleep(20000000);  // 20 secs
    abort();
}

static void do_nothing(int x) {
}
static void stdin_make_nonblocking() {
    const int fd = STDIN_FILENO;
    // Catch SIGCONT so pause() wakes us up.
    signal(SIGCONT, &do_nothing);

    for (;;) {
        int flags = fcntl(fd, F_GETFL, 0);
        fprintf(stdout, "stdin was %sblocking\n", (flags & O_NONBLOCK) ? "non" : "");
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }
        pause();
    }
}

static void show_help();

/// A thing that fish_test_helper can do.
typedef struct {
    /// The argument to match against.
    const char *arg;

    /// Function to invoke.
    void (*func)();

    /// Description of what this does.
    const char *desc;
} fth_command_t;

static fth_command_t s_commands[] = {
    {"abandon_tty", abandon_tty, "Create a new pgroup and transfer tty ownership to it"},
    {"become_foreground_then_print_stderr", become_foreground_then_print_stderr,
     "Claim the terminal (tcsetpgrp) and then print to stderr"},
    {"nohup_wait", nohup_wait, "Ignore SIGHUP and just wait"},
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
    {"print_stop_cont", print_stop_cont, "Print when we get SIGTSTP and SIGCONT, exiting on input"},
    {"sigint_self", sigint_self, "Send SIGINT to self"},
    {"sigkill_self", sigkill_self, "Send SIGKILL to self"},
    {"stdin_make_nonblocking", stdin_make_nonblocking,
     "Print if stdin is blocking and then make it nonblocking"},
    {"help", show_help, "Print list of fish_test_helper commands"},
    {NULL, NULL, NULL},
};

static void show_help() {
    printf("fish_test_helper: helper utility for fish\n\n");
    printf("Commands\n");
    printf("--------\n");
    for (int i = 0; s_commands[i].arg; i++) {
        printf("  %s:\n    %s\n\n", s_commands[i].arg, s_commands[i].desc);
    }
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        fprintf(stderr, "No commands given.\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "help") || !strcmp(argv[i], "-h")) {
            show_help();
            return 0;
        }

        const fth_command_t *found = NULL;
        for (int j = 0; s_commands[j].arg; j++) {
            if (!strcmp(argv[i], s_commands[j].arg)) {
                found = &s_commands[j];
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
