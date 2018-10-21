// The library for various signal related issues.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <pthread.h>

#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "parser.h"
#include "proc.h"
#include "reader.h"
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
    {0, NULL, NULL}};

/// Test if \c name is a string describing the signal named \c canonical.
static int match_signal_name(const wchar_t *canonical, const wchar_t *name) {
    if (wcsncasecmp(name, L"sig", 3) == 0) name += 3;

    return wcscasecmp(canonical + 3, name) == 0;
}

int wcs2sig(const wchar_t *str) {
    for (int i = 0; lookup[i].desc; i++) {
        if (match_signal_name(lookup[i].name, str)) {
            return lookup[i].signal;
        }
    }

    int res = fish_wcstoi(str);
    if (errno || res < 0) return -1;
    return res;
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
    UNUSED(info);
    UNUSED(context);
    if (event_is_signal_observed(signal)) {
        event_fire_signal(signal);
    }
}

#ifdef SIGWINCH
/// Respond to a winch signal by checking the terminal size.
static void handle_winch(int sig, siginfo_t *info, void *context) {
    UNUSED(info);
    UNUSED(context);
    common_handle_winch(sig);
    default_handler(sig, 0, 0);
}
#endif

/// Respond to a hup signal by exiting, unless it is caught by a shellscript function, in which case
/// we do nothing.
static void handle_hup(int sig, siginfo_t *info, void *context) {
    UNUSED(info);
    UNUSED(context);
    if (event_is_signal_observed(SIGHUP)) {
        default_handler(sig, 0, 0);
    } else {
        reader_exit(1, 1);
    }
}

/// Handle sigterm. The only thing we do is restore the front process ID, then die.
static void handle_sigterm(int sig, siginfo_t *info, void *context) {
    UNUSED(sig);
    UNUSED(info);
    UNUSED(context);
    restore_term_foreground_process_group();
    signal(SIGTERM, SIG_DFL);
    raise(SIGTERM);
}

/// Interactive mode ^C handler. Respond to int signal by setting interrupted-flag and stopping all
/// loops and conditionals.
static void handle_int(int sig, siginfo_t *info, void *context) {
    reader_handle_sigint();
    default_handler(sig, info, context);
}

/// Non-interactive ^C handler.
static void handle_int_notinteractive(int sig, siginfo_t *info, void *context) {
    parser_t::skip_all_blocks();
    default_handler(sig, info, context);
}

/// sigchld handler. Does notification and calls the handler in proc.c.
static void handle_chld(int sig, siginfo_t *info, void *context) {
    job_handle_signal(sig, info, context);
    default_handler(sig, info, context);
}

// We have a sigalarm handler that does nothing. This is used in the signal torture test, to verify
// that we behave correctly when receiving lots of irrelevant signals.
static void handle_sigalarm(int sig, siginfo_t *info, void *context) {
    UNUSED(sig);
    UNUSED(info);
    UNUSED(context);
}

void signal_reset_handlers() {
    int i;

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = SIG_DFL;

    for (i = 0; lookup[i].desc; i++) {
        if (lookup[i].signal == SIGHUP) {
            struct sigaction oact;
            sigaction(SIGHUP, NULL, &oact);
            if (oact.sa_handler == SIG_IGN) continue;
        }
        sigaction(lookup[i].signal, &act, NULL);
    }
}

static void set_interactive_handlers() {
    struct sigaction act, oact;
    act.sa_flags = 0;
    oact.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    // Interactive mode. Ignore interactive signals.  We are a shell, we know what is best for
    // the user.
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);
    sigaction(SIGTTOU, &act, NULL);

    // We don't ignore SIGTTIN because we might send it to ourself.
    act.sa_sigaction = &default_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGTTIN, &act, NULL);

    act.sa_sigaction = &handle_int;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &act, NULL);

    // SIGTERM restores the terminal controlling process before dying.
    act.sa_sigaction = &handle_sigterm;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &act, NULL);

    sigaction(SIGHUP, NULL, &oact);
    if (oact.sa_handler == SIG_DFL) {
        act.sa_sigaction = &handle_hup;
        act.sa_flags = SA_SIGINFO;
        sigaction(SIGHUP, &act, NULL);
    }

    // SIGALARM as part of our signal torture test
    act.sa_sigaction = &handle_sigalarm;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &act, NULL);

#ifdef SIGWINCH
    act.sa_sigaction = &handle_winch;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGWINCH, &act, NULL);
#endif
}

static void set_non_interactive_handlers() {
    struct sigaction act;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    act.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &act, 0);

    act.sa_sigaction = &handle_int_notinteractive;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &act, NULL);
}

/// Sets up appropriate signal handlers.
void signal_set_handlers() {
    struct sigaction act;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    // Ignore SIGPIPE. We'll detect failed writes and deal with them appropriately. We don't want
    // this signal interrupting other syscalls or terminating us.
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, 0);

    // Whether or not we're interactive we want SIGCHLD to not interrupt restartable syscalls.
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &handle_chld;
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGCHLD, &act, 0)) {
        wperror(L"sigaction");
        FATAL_EXIT();
    }

    if (shell_is_interactive()) {
        set_interactive_handlers();
    } else {
        set_non_interactive_handlers();
    }
}

void signal_handle(int sig, int do_handle) {
    struct sigaction act;

    // These should always be handled.
    if ((sig == SIGINT) || (sig == SIGQUIT) || (sig == SIGTSTP) || (sig == SIGTTIN) ||
        (sig == SIGTTOU) || (sig == SIGCHLD))
        return;

    act.sa_flags = 0;
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
        // If SIGHUP is being ignored (e.g., because were were run via `nohup`) don't reset it.
        // We don't special case other signals because if they're being ignored that shouldn't
        // affect processes we spawn. They should get the default behavior for those signals.
        if (lookup[i].signal == SIGHUP && act.sa_handler == SIG_IGN) continue;
        if (act.sa_handler != SIG_DFL) sigaddset(set, lookup[i].signal);
    }
}

void signal_block() {
    ASSERT_IS_MAIN_THREAD();
    sigset_t chldset;

    if (!block_count) {
        sigfillset(&chldset);
        DIE_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &chldset, NULL));
    }

    block_count++;
    // debug( 0, L"signal block level increased to %d", block_count );
}

/// Ensure we did not inherit any blocked signals. See issue #3964.
void signal_unblock_all() {
    sigset_t iset;
    sigemptyset(&iset);
    sigprocmask(SIG_SETMASK, &iset, NULL);
}

void signal_unblock() {
    ASSERT_IS_MAIN_THREAD();

    block_count--;
    if (block_count < 0) {
        debug(0, _(L"Signal block mismatch"));
        bugreport();
        FATAL_EXIT();
    }

    if (!block_count) {
        sigset_t chldset;
        sigfillset(&chldset);
        DIE_ON_FAILURE(pthread_sigmask(SIG_UNBLOCK, &chldset, 0));
    }
}

bool signal_is_blocked() { return static_cast<bool>(block_count); }
