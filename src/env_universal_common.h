#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H
#include "config.h"  // IWYU pragma: keep

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.h"
#include "env.h"
#include "fds.h"
#include "maybe.h"
#include "wutil.h"

#if INCLUDE_RUST_HEADERS
#include "env_universal_common.rs.h"
#endif

/// The "universal notifier" is an object responsible for broadcasting and receiving universal
/// variable change notifications. These notifications do not contain the change, but merely
/// indicate that the uvar file has changed. It is up to the uvar subsystem to re-read the file.
///
/// We support a few notification strategies. Not all strategies are supported on all platforms.
///
/// Notifiers may request polling, and/or provide a file descriptor to be watched for readability in
/// select().
///
/// To request polling, the notifier overrides usec_delay_between_polls() to return a positive
/// value. That value will be used as the timeout in select(). When select returns, the loop invokes
/// poll(). poll() should return true to indicate that the file may have changed.
///
/// To provide a file descriptor, the notifier overrides notification_fd() to return a non-negative
/// fd. This will be added to the "read" file descriptor list in select(). If the fd is readable,
/// notification_fd_became_readable() will be called; that function should be overridden to return
/// true if the file may have changed.
class universal_notifier_t {
   public:
    enum notifier_strategy_t {
        // Poll on shared memory.
        strategy_shmem_polling,

        // Mac-specific notify(3) implementation.
        strategy_notifyd,

        // Strategy that uses a named pipe. Somewhat complex, but portable and doesn't require
        // polling most of the time.
        strategy_named_pipe,
    };

    universal_notifier_t(const universal_notifier_t &) = delete;
    universal_notifier_t &operator=(const universal_notifier_t &) = delete;

   protected:
    universal_notifier_t();

   public:
    static notifier_strategy_t resolve_default_strategy();
    virtual ~universal_notifier_t();

    // Factory constructor.
    static std::unique_ptr<universal_notifier_t> new_notifier_for_strategy(
        notifier_strategy_t strat, const wchar_t *test_path = nullptr);

    // Default instance. Other instances are possible for testing.
    static universal_notifier_t &default_notifier();

    // FFI helper so autocxx can "deduce" the lifetime.
    static universal_notifier_t &default_notifier_ffi(int &) { return default_notifier(); }

    // Does a fast poll(). Returns true if changed.
    virtual bool poll();

    // Triggers a notification.
    virtual void post_notification();

    // Recommended delay between polls. A value of 0 means no polling required (so no timeout).
    virtual unsigned long usec_delay_between_polls() const;

    // Returns the fd from which to watch for events, or -1 if none.
    virtual int notification_fd() const;

    // The notification_fd is readable; drain it. Returns true if a notification is considered to
    // have been posted.
    virtual bool notification_fd_became_readable(int fd);
};

wcstring get_runtime_path();

void env_universal_notifier_t_default_notifier_post_notification_ffi();

#endif
