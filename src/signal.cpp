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
#include "signal.h"
#include "termsize.h"
#include "topic_monitor.h"
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

/// Lookup table used to convert between signal names and signal ids, etc.
static const struct lookup_entry signal_table[] = {
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
};

/// Test if \c name is a string describing the signal named \c canonical.
static int match_signal_name(const wchar_t *canonical, const wchar_t *name) {
    if (wcsncasecmp(name, L"sig", 3) == 0) name += 3;

    return wcscasecmp(canonical + 3, name) == 0;
}

int wcs2sig(const wchar_t *str) {
    for (const auto &data : signal_table) {
        if (match_signal_name(data.name, str)) {
            return data.signal;
        }
    }

    int res = fish_wcstoi(str);
    if (errno || res < 0) return -1;
    return res;
}

const wchar_t *sig2wcs(int sig) {
    for (const auto &data : signal_table) {
        if (data.signal == sig) {
            return data.name;
        }
    }

    return _(L"Unknown");
}

const wchar_t *signal_get_desc(int sig) {
    for (const auto &data : signal_table) {
        if (data.signal == sig) {
            return _(data.desc);
        }
    }

    return _(L"Unknown");
}

/// Store the "main" pid. This allows us to reliably determine if we are in a forked child.
static const pid_t s_main_pid = getpid();

/// It's possible that we receive a signal after we have forked, but before we have reset the signal
/// handlers (or even run the pthread_atfork calls). In that event we will do something dumb like
/// swallow SIGINT. Ensure that doesn't happen. Check if we are the main fish process; if not reset
/// and re-raise the signal. \return whether we re-raised the signal.
static bool reraise_if_forked_child(int sig) {
    // Don't use is_forked_child, that relies on atfork handlers which maybe have not run yet.
    if (getpid() == s_main_pid) {
        return false;
    }
    signal(sig, SIG_DFL);
    raise(sig);
    return true;
}

/// The cancellation signal we have received.
/// Of course this is modified from a signal handler.
static volatile relaxed_atomic_t<sig_atomic_t> s_cancellation_signal{0};

void signal_clear_cancel() { s_cancellation_signal = 0; }

int signal_check_cancel() { return s_cancellation_signal; }

/// Number of SIGIO events.
static volatile relaxed_atomic_t<uint32_t> s_sigio_count{0};

uint32_t signal_get_sigio_count() { return s_sigio_count; }

/// The single signal handler. By centralizing signal handling we ensure that we can never install
/// the "wrong" signal handler (see #5969).
static void fish_signal_handler(int sig, siginfo_t *info, void *context) {
    UNUSED(info);
    UNUSED(context);

    // Ensure we preserve errno.
    const int saved_errno = errno;

    // Check if we are a forked child.
    if (reraise_if_forked_child(sig)) {
        errno = saved_errno;
        return;
    }

    // Check if fish script cares about this.
    const bool observed = event_is_signal_observed(sig);
    if (observed) {
        event_enqueue_signal(sig);
    }

    // Do some signal-specific stuff.
    switch (sig) {
#ifdef SIGWINCH
        case SIGWINCH:
            /// Respond to a winch signal by telling the termsize container.
            termsize_container_t::handle_winch();
            break;
#endif

        case SIGHUP:
            /// Respond to a hup signal by exiting, unless it is caught by a shellscript function,
            /// in which case we do nothing.
            if (!observed) {
                reader_sighup();
            }
            topic_monitor_t::principal().post(topic_t::sighupint);
            break;

        case SIGTERM:
            /// Handle sigterm. The only thing we do is restore the front process ID, then die.
            restore_term_foreground_process_group_for_exit();
            signal(SIGTERM, SIG_DFL);
            raise(SIGTERM);
            break;

        case SIGINT:
            /// Interactive mode ^C handler. Respond to int signal by setting interrupted-flag and
            /// stopping all loops and conditionals.
            s_cancellation_signal = SIGINT;
            reader_handle_sigint();
            topic_monitor_t::principal().post(topic_t::sighupint);
            break;

        case SIGCHLD:
            // A child process stopped or exited.
            topic_monitor_t::principal().post(topic_t::sigchld);
            break;

        case SIGALRM:
            // We have a sigalarm handler that does nothing. This is used in the signal torture
            // test, to verify that we behave correctly when receiving lots of irrelevant signals.
            break;

#if defined(SIGIO)
        case SIGIO:
            // An async FD became readable/writable/etc.
            // Don't try to look at si_code, it is not set under BSD.
            // Don't use ++ to avoid a CAS.
            s_sigio_count = s_sigio_count + 1;
            break;
#endif
    }
    errno = saved_errno;
}

void signal_reset_handlers() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = SIG_DFL;

    for (const auto &data : signal_table) {
        if (data.signal == SIGHUP) {
            struct sigaction oact;
            sigaction(SIGHUP, nullptr, &oact);
            if (oact.sa_handler == SIG_IGN) continue;
        }
        sigaction(data.signal, &act, nullptr);
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
    sigaction(SIGTSTP, &act, nullptr);
    sigaction(SIGTTOU, &act, nullptr);

    // We don't ignore SIGTTIN because we might send it to ourself.
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGTTIN, &act, nullptr);

    // SIGTERM restores the terminal controlling process before dying.
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &act, nullptr);

    sigaction(SIGHUP, nullptr, &oact);
    if (oact.sa_handler == SIG_DFL) {
        act.sa_sigaction = &fish_signal_handler;
        act.sa_flags = SA_SIGINFO;
        sigaction(SIGHUP, &act, nullptr);
    }

    // SIGALARM as part of our signal torture test
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &act, nullptr);

#ifdef SIGWINCH
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGWINCH, &act, nullptr);
#endif
}

/// Sets up appropriate signal handlers.
void signal_set_handlers(bool interactive) {
    struct sigaction act;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    // Ignore SIGPIPE. We'll detect failed writes and deal with them appropriately. We don't want
    // this signal interrupting other syscalls or terminating us.
    act.sa_sigaction = nullptr;
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);

    // Ignore SIGQUIT.
    act.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &act, nullptr);

    // Apply our SIGINT handler.
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &act, nullptr);

    // Apply our SIGIO handler.
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGIO, &act, nullptr);

    // Whether or not we're interactive we want SIGCHLD to not interrupt restartable syscalls.
    act.sa_sigaction = &fish_signal_handler;
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGCHLD, &act, nullptr)) {
        wperror(L"sigaction");
        FATAL_EXIT();
    }

    if (interactive) {
        set_interactive_handlers();
    }
}

void signal_set_handlers_once(bool interactive) {
    static std::once_flag s_noninter_once;
    std::call_once(s_noninter_once, signal_set_handlers, false);

    static std::once_flag s_inter_once;
    if (interactive) std::call_once(s_inter_once, set_interactive_handlers);
}

void signal_handle(int sig) {
    struct sigaction act;

    // These should always be handled.
    if ((sig == SIGINT) || (sig == SIGQUIT) || (sig == SIGTSTP) || (sig == SIGTTIN) ||
        (sig == SIGTTOU) || (sig == SIGCHLD))
        return;

    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &fish_signal_handler;
    sigaction(sig, &act, nullptr);
}

void get_signals_with_handlers(sigset_t *set) {
    sigemptyset(set);
    for (const auto &data : signal_table) {
        struct sigaction act = {};
        sigaction(data.signal, nullptr, &act);
        // If SIGHUP is being ignored (e.g., because were were run via `nohup`) don't reset it.
        // We don't special case other signals because if they're being ignored that shouldn't
        // affect processes we spawn. They should get the default behavior for those signals.
        if (data.signal == SIGHUP && act.sa_handler == SIG_IGN) continue;
        if (act.sa_handler != SIG_DFL) sigaddset(set, data.signal);
    }
}

/// Ensure we did not inherit any blocked signals. See issue #3964.
void signal_unblock_all() {
    sigset_t iset;
    sigemptyset(&iset);
    sigprocmask(SIG_SETMASK, &iset, nullptr);
}

sigchecker_t::sigchecker_t(topic_t signal) : topic_(signal) {
    // Call check() to update our generation.
    check();
}

bool sigchecker_t::check() {
    auto &tm = topic_monitor_t::principal();
    generation_t gen = tm.generation_for_topic(topic_);
    bool changed = this->gen_ != gen;
    this->gen_ = gen;
    return changed;
}

void sigchecker_t::wait() const {
    auto &tm = topic_monitor_t::principal();
    generation_list_t gens = generation_list_t::invalids();
    gens.at(topic_) = this->gen_;
    tm.check(&gens, true /* wait */);
}
