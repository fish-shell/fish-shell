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
void signal_set_handlers(bool interactive);

/// Latch function. This sets signal handlers, but only the first time it is called.
void signal_set_handlers_once(bool interactive);

/// Tell fish what to do on the specified signal.
///
/// \param sig The signal to specify the action of
void signal_handle(int sig);

/// Ensure we did not inherit any blocked signals. See issue #3964.
void signal_unblock_all();

/// Returns signals with non-default handlers.
void get_signals_with_handlers(sigset_t *set);

/// \return the most recent cancellation signal received by the fish process.
/// Currently only SIGINT is considered a cancellation signal.
/// This is thread safe.
int signal_check_cancel();

/// Set the cancellation signal to zero.
/// In generaly this should only be done in interactive sessions.
void signal_clear_cancel();

/// \return a count of SIGIO signals.
/// This is used by universal variables, and is a simple unsigned counter which wraps to 0.
uint32_t signal_get_sigio_count();

enum class topic_t : uint8_t;
/// A sigint_detector_t can be used to check if a SIGINT (or SIGHUP) has been delivered.
class sigchecker_t {
    const topic_t topic_;
    uint64_t gen_{0};

   public:
    sigchecker_t(topic_t signal);

    /// Check if a sigint has been delivered since the last call to check(), or since the detector
    /// was created.
    bool check();

    /// Wait until a sigint is delivered.
    void wait() const;
};

#endif
