// The library for various signal related issues.
#ifndef FISH_SIGNALH
#define FISH_SIGNALH

#include <signal.h>

/// Get the integer signal value representing the specified signal, or -1 of no signal was found.
int wcs2sig(const wchar_t *str);

/// Get string representation of a signal.
const wchar_t *sig2wcs(int sig);

/// Returns a description of the specified signal.
const wchar_t *signal_get_desc(int sig);

/// Set all signal handlers to SIG_DFL.
void signal_reset_handlers();

/// Set signal handlers to fish default handlers.
void signal_set_handlers();

/// Tell fish what to do on the specified signal.
///
/// \param sig The signal to specify the action of
/// \param do_handle If true fish will catch the specified signal and fire an event, otherwise the
/// default action (SIG_DFL) will be set
void signal_handle(int sig, int do_handle);

/// Ensure we did not inherit any blocked signals. See issue #3964.
void signal_unblock_all();

/// Returns signals with non-default handlers.
void get_signals_with_handlers(sigset_t *set);

/// A sigint_detector_t can be used to check if a SIGINT (or SIGHUP) has been delivered.
class sigint_checker_t {
    uint64_t gen_{0};

   public:
    sigint_checker_t();

    /// Check if a sigint has been delivered since the last call to check(), or since the detector
    /// was created.
    bool check();

    /// Wait until a sigint is delivered.
    void wait();
};

#endif
