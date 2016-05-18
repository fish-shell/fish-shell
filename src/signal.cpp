// The library for various signal related issues.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <pthread.h>
#include <stdbool.h>

#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "proc.h"
#include "reader.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

/// Struct describing an entry for the lookup table used to convert between signal names and signal
/// ids, etc.
struct lookup_entry {
    /// Signal id.
    int signal;
    /// Signal name.
    const wchar_t *name;
    /// Signal description.
    const wchar_t *desc;
};

/// The number of signal blocks in place. Increased by signal_block, decreased by signal_unblock.
static int block_count = 0;

/// Lookup table used to convert between signal names and signal ids, etc.
static const struct lookup_entry lookup[] = {
#ifdef SIGHUP
    {SIGHUP, L"SIGHUP", N_(L"Terminal hung up")},
#endif
#ifdef SIGINT
    {SIGINT, L"SIGINT", N_(L"Quit request from job control (^C)")},
#endif
#ifdef SIGQUIT
    {SIGQUIT, L"SIGQUIT", N_(L"Quit request from job control with core dump (^\\)")},
#endif
#ifdef SIGILL
    {SIGILL, L"SIGILL", N_(L"Illegal instruction")},
#endif
#ifdef SIGTRAP
    {SIGTRAP, L"SIGTRAP", N_(L"Trace or breakpoint trap")},
#endif
#ifdef SIGABRT
    {SIGABRT, L"SIGABRT", N_(L"Abort")},
#endif
#ifdef SIGBUS
    {SIGBUS, L"SIGBUS", N_(L"Misaligned address error")},
#endif
#ifdef SIGFPE
    {SIGFPE, L"SIGFPE", N_(L"Floating point exception")},
#endif
#ifdef SIGKILL
    {SIGKILL, L"SIGKILL", N_(L"Forced quit")},
#endif
#ifdef SIGUSR1
    {SIGUSR1, L"SIGUSR1", N_(L"User defined signal 1")},
#endif
#ifdef SIGUSR2
    {SIGUSR2, L"SIGUSR2", N_(L"User defined signal 2")},
#endif
#ifdef SIGSEGV
    {SIGSEGV, L"SIGSEGV", N_(L"Address boundary error")},
#endif
#ifdef SIGPIPE
    {SIGPIPE, L"SIGPIPE", N_(L"Broken pipe")},
#endif
#ifdef SIGALRM
    {SIGALRM, L"SIGALRM", N_(L"Timer expired")},
#endif
#ifdef SIGTERM
    {SIGTERM, L"SIGTERM", N_(L"Polite quit request")},
#endif
#ifdef SIGCHLD
    {SIGCHLD, L"SIGCHLD", N_(L"Child process status changed")},
#endif
#ifdef SIGCONT
    {SIGCONT, L"SIGCONT", N_(L"Continue previously stopped process")},
#endif
#ifdef SIGSTOP
    {SIGSTOP, L"SIGSTOP", N_(L"Forced stop")},
#endif
#ifdef SIGTSTP
    {SIGTSTP, L"SIGTSTP", N_(L"Stop request from job control (^Z)")},
#endif
#ifdef SIGTTIN
    {SIGTTIN, L"SIGTTIN", N_(L"Stop from terminal input")},
#endif
#ifdef SIGTTOU
    {SIGTTOU, L"SIGTTOU", N_(L"Stop from terminal output")},
#endif
#ifdef SIGURG
    {SIGURG, L"SIGURG", N_(L"Urgent socket condition")},
#endif
#ifdef SIGXCPU
    {SIGXCPU, L"SIGXCPU", N_(L"CPU time limit exceeded")},
#endif
#ifdef SIGXFSZ
    {SIGXFSZ, L"SIGXFSZ", N_(L"File size limit exceeded")},
#endif
#ifdef SIGVTALRM
    {SIGVTALRM, L"SIGVTALRM", N_(L"Virtual timer expired")},
#endif
#ifdef SIGPROF
    {SIGPROF, L"SIGPROF", N_(L"Profiling timer expired")},
#endif
#ifdef SIGWINCH
    {SIGWINCH, L"SIGWINCH", N_(L"Window size change")},
#endif
#ifdef SIGWIND
    {SIGWIND, L"SIGWIND", N_(L"Window size change")},
#endif
#ifdef SIGIO
    {SIGIO, L"SIGIO", N_(L"I/O on asynchronous file descriptor is possible")},
#endif
#ifdef SIGPWR
    {SIGPWR, L"SIGPWR", N_(L"Power failure")},
#endif
#ifdef SIGSYS
    {SIGSYS, L"SIGSYS", N_(L"Bad system call")},
#endif
#ifdef SIGINFO
    {SIGINFO, L"SIGINFO", N_(L"Information request")},
#endif
#ifdef SIGSTKFLT
    {SIGSTKFLT, L"SISTKFLT", N_(L"Stack fault")},
#endif
#ifdef SIGEMT
    {SIGEMT, L"SIGEMT", N_(L"Emulator trap")},
#endif
#ifdef SIGIOT
    {SIGIOT, L"SIGIOT", N_(L"Abort (Alias for SIGABRT)")},
#endif
#ifdef SIGUNUSED
    {SIGUNUSED, L"SIGUNUSED", N_(L"Unused signal")},
#endif
    {0, 0, 0}};

/// Test if \c name is a string describing the signal named \c canonical.
static int match_signal_name(const wchar_t *canonical, const wchar_t *name) {
    if (wcsncasecmp(name, L"sig", 3) == 0) name += 3;

    return wcscasecmp(canonical + 3, name) == 0;
}

int wcs2sig(const wchar_t *str) {
    int i;
    wchar_t *end = 0;

    for (i = 0; lookup[i].desc; i++) {
        if (match_signal_name(lookup[i].name, str)) {
            return lookup[i].signal;
        }
    }
    errno = 0;
    int res = fish_wcstoi(str, &end, 10);
    if (!errno && res >= 0 && !*end) return res;

    return -1;
}

const wchar_t *sig2wcs(int sig) {
    int i;

    for (i = 0; lookup[i].desc; i++) {
        if (lookup[i].signal == sig) {
            return lookup[i].name;
        }
    }

    return _(L"Unknown");
}

const wchar_t *signal_get_desc(int sig) {
    int i;

    for (i = 0; lookup[i].desc; i++) {
        if (lookup[i].signal == sig) {
            return _(lookup[i].desc);
        }
    }

    return _(L"Unknown");
}

/// Standard signal handler.
static void default_handler(int signal, siginfo_t *info, void *context) {
    if (event_is_signal_observed(signal)) {
        event_fire_signal(signal);
    }
}

/// Respond to a winch signal by checking the terminal size.
static void handle_winch(int sig, siginfo_t *info, void *context) {
    common_handle_winch(sig);
    default_handler(sig, 0, 0);
}

/// Respond to a hup signal by exiting, unless it is caught by a shellscript function, in which case
/// we do nothing.
static void handle_hup(int sig, siginfo_t *info, void *context) {
    if (event_is_signal_observed(SIGHUP)) {
        default_handler(sig, 0, 0);
    } else {
        reader_exit(1, 1);
    }
}

/// Handle sigterm. The only thing we do is restore the front process ID, then die.
static void handle_term(int sig, siginfo_t *info, void *context) {
    restore_term_foreground_process_group();
    signal(SIGTERM, SIG_DFL);
    raise(SIGTERM);
}

/// Interactive mode ^C handler. Respond to int signal by setting interrupted-flag and stopping all
/// loops and conditionals.
static void handle_int(int sig, siginfo_t *info, void *context) {
    reader_handle_int(sig);
    default_handler(sig, info, context);
}

/// sigchld handler. Does notification and calls the handler in proc.c.
static void handle_chld(int sig, siginfo_t *info, void *context) {
    job_handle_signal(sig, info, context);
    default_handler(sig, info, context);
}

void signal_reset_handlers() {
    int i;

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = SIG_DFL;

    for (i = 0; lookup[i].desc; i++) {
        sigaction(lookup[i].signal, &act, 0);
    }
}

/// Sets appropriate signal handlers.
void signal_set_handlers() {
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &default_handler;

    // First reset everything to a use default_handler, a function whose sole action is to fire of
    // an event.
    sigaction(SIGINT, &act, 0);
    sigaction(SIGQUIT, &act, 0);
    sigaction(SIGTSTP, &act, 0);
    sigaction(SIGTTIN, &act, 0);
    sigaction(SIGTTOU, &act, 0);
    sigaction(SIGCHLD, &act, 0);

    // Ignore sigpipe, which we may get from the universal variable notifier.
    sigaction(SIGPIPE, &act, 0);

    if (shell_is_interactive()) {
        // Interactive mode. Ignore interactive signals.  We are a shell, we know what is best for
        // the user.
        act.sa_handler = SIG_IGN;

        sigaction(SIGINT, &act, 0);
        sigaction(SIGQUIT, &act, 0);
        sigaction(SIGTSTP, &act, 0);
        sigaction(SIGTTIN, &act, 0);
        sigaction(SIGTTOU, &act, 0);

        act.sa_sigaction = &handle_int;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGINT, &act, 0)) {
            wperror(L"sigaction");
            FATAL_EXIT();
        }

        act.sa_sigaction = &handle_chld;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGCHLD, &act, 0)) {
            wperror(L"sigaction");
            FATAL_EXIT();
        }

#ifdef SIGWINCH
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = &handle_winch;
        if (sigaction(SIGWINCH, &act, 0)) {
            wperror(L"sigaction");
            FATAL_EXIT();
        }
#endif

        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = &handle_hup;
        if (sigaction(SIGHUP, &act, 0)) {
            wperror(L"sigaction");
            FATAL_EXIT();
        }

        // SIGTERM restores the terminal controlling process before dying.
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = &handle_term;
        if (sigaction(SIGTERM, &act, 0)) {
            wperror(L"sigaction");
            FATAL_EXIT();
        }
    } else {
        // Non-interactive. Ignore interrupt, check exit status of processes to determine result
        // instead.
        act.sa_handler = SIG_IGN;
        sigaction(SIGINT, &act, 0);
        sigaction(SIGQUIT, &act, 0);

        act.sa_handler = SIG_DFL;
        act.sa_sigaction = &handle_chld;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGCHLD, &act, 0)) {
            wperror(L"sigaction");
            exit_without_destructors(1);
        }
    }
}

void signal_handle(int sig, int do_handle) {
    struct sigaction act;

    // These should always be handled.
    if ((sig == SIGINT) || (sig == SIGQUIT) || (sig == SIGTSTP) || (sig == SIGTTIN) ||
        (sig == SIGTTOU) || (sig == SIGCHLD))
        return;

    sigemptyset(&act.sa_mask);
    if (do_handle) {
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = &default_handler;
    } else {
        act.sa_flags = 0;
        act.sa_handler = SIG_DFL;
    }

    sigaction(sig, &act, 0);
}

void get_signals_with_handlers(sigset_t *set) {
    sigemptyset(set);
    for (int i = 0; lookup[i].desc; i++) {
        struct sigaction act = {};
        sigaction(lookup[i].signal, NULL, &act);
        if (act.sa_handler != SIG_DFL) sigaddset(set, lookup[i].signal);
    }
}

void signal_block() {
    ASSERT_IS_MAIN_THREAD();
    sigset_t chldset;

    if (!block_count) {
        sigfillset(&chldset);
        VOMIT_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &chldset, NULL));
    }

    block_count++;
    //	debug( 0, L"signal block level increased to %d", block_count );
}

void signal_unblock() {
    ASSERT_IS_MAIN_THREAD();
    sigset_t chldset;

    block_count--;

    if (block_count < 0) {
        debug(0, _(L"Signal block mismatch"));
        bugreport();
        FATAL_EXIT();
    }

    if (!block_count) {
        sigfillset(&chldset);
        VOMIT_ON_FAILURE(pthread_sigmask(SIG_UNBLOCK, &chldset, 0));
    }
    //	debug( 0, L"signal block level decreased to %d", block_count );
}

bool signal_is_blocked() { return !!block_count; }
