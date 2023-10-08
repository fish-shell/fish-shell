// The utility library for universal variables. Used both by the client library and by the daemon.
#include "config.h"  // IWYU pragma: keep

#include <arpa/inet.h>  // IWYU pragma: keep
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
// We need the sys/file.h for the flock() declaration on Linux but not OS X.
#include <sys/file.h>  // IWYU pragma: keep
// We need the ioctl.h header so we can check if SIOCGIFHWADDR is defined by it so we know if we're
// on a Linux system.
#include <netinet/in.h>  // IWYU pragma: keep
#include <sys/ioctl.h>   // IWYU pragma: keep
#ifdef __CYGWIN__
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>  // IWYU pragma: keep
#endif
#if !defined(__APPLE__) && !defined(__CYGWIN__)
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>   // IWYU pragma: keep
#include <sys/types.h>  // IWYU pragma: keep

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "common.h"
#include "env.h"
#include "env_universal_common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fd_readable_set.rs.h"
#include "flog.h"
#include "path.h"
#include "utf8.h"
#include "util.h"  // IWYU pragma: keep
#include "wcstringutil.h"
#include "wutil.h"

#ifdef __APPLE__
#define FISH_NOTIFYD_AVAILABLE
#include <notify.h>
#endif

#ifdef __HAIKU__
#define _BSD_SOURCE
#include <bsd/ifaddrs.h>
#endif  // Haiku

namespace {
class universal_notifier_shmem_poller_t final : public universal_notifier_t {
#ifdef __CYGWIN__
    // This is what our shared memory looks like. Everything here is stored in network byte order
    // (big-endian).
    struct universal_notifier_shmem_t {
        uint32_t magic;
        uint32_t version;
        uint32_t universal_variable_seed;
    };

#define SHMEM_MAGIC_NUMBER 0xF154
#define SHMEM_VERSION_CURRENT 1000

   private:
    long long last_change_time{0};
    uint32_t last_seed{0};
    volatile universal_notifier_shmem_t *region{nullptr};

    void open_shmem() {
        assert(region == nullptr);

        // Use a path based on our uid to avoid collisions.
        char path[NAME_MAX];
        snprintf(path, sizeof path, "/%ls_shmem_%d", program_name ? program_name : L"fish",
                 getuid());

        autoclose_fd_t fd{shm_open(path, O_RDWR | O_CREAT, 0600)};
        if (!fd.valid()) {
            const char *error = std::strerror(errno);
            FLOGF(error, _(L"Unable to open shared memory with path '%s': %s"), path, error);
            return;
        }

        // Get the size.
        off_t size = 0;
        struct stat buf = {};
        if (fstat(fd.fd(), &buf) < 0) {
            const char *error = std::strerror(errno);
            FLOGF(error, _(L"Unable to fstat shared memory object with path '%s': %s"), path,
                  error);
            return;
        }
        size = buf.st_size;

        // Set the size, if it's too small.
        if (size < (off_t)sizeof(universal_notifier_shmem_t)) {
            if (ftruncate(fd.fd(), sizeof(universal_notifier_shmem_t)) < 0) {
                const char *error = std::strerror(errno);
                FLOGF(error, _(L"Unable to truncate shared memory object with path '%s': %s"), path,
                      error);
                return;
            }
        }

        // Memory map the region.
        void *addr = mmap(nullptr, sizeof(universal_notifier_shmem_t), PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd.fd(), 0);
        if (addr == MAP_FAILED) {
            const char *error = std::strerror(errno);
            FLOGF(error, _(L"Unable to memory map shared memory object with path '%s': %s"), path,
                  error);
            this->region = nullptr;
            return;
        }
        this->region = static_cast<universal_notifier_shmem_t *>(addr);

        // Read the current seed.
        this->poll();
    }

   public:
    // Our notification involves changing the value in our shared memory. In practice, all clients
    // will be in separate processes, so it suffices to set the value to a pid. For testing
    // purposes, however, it's useful to keep them in the same process, so we increment the value.
    // This isn't "safe" in the sense that multiple simultaneous increments may result in one being
    // lost, but it should always result in the value being changed, which is sufficient.
    void post_notification() override {
        if (region != nullptr) {
            /* Read off the seed */
            uint32_t seed = ntohl(region->universal_variable_seed);  //!OCLINT(constant cond op)

            // Increment it. Don't let it wrap to zero.
            do {
                seed++;
            } while (seed == 0);

            // Write out our data.
            region->magic = htonl(SHMEM_MAGIC_NUMBER);       //!OCLINT(constant cond op)
            region->version = htonl(SHMEM_VERSION_CURRENT);  //!OCLINT(constant cond op)
            region->universal_variable_seed = htonl(seed);   //!OCLINT(constant cond op)

            FLOGF(uvar_notifier, "posting notification: seed %u -> %u", last_seed, seed);
            last_seed = seed;
        }
    }

    universal_notifier_shmem_poller_t() { open_shmem(); }

    ~universal_notifier_shmem_poller_t() {
        if (region != nullptr) {
            void *address = const_cast<void *>(static_cast<volatile void *>(region));
            if (munmap(address, sizeof(universal_notifier_shmem_t)) < 0) {
                wperror(L"munmap");
            }
        }
    }

    bool poll() override {
        bool result = false;
        if (region != nullptr) {
            uint32_t seed = ntohl(region->universal_variable_seed);  //!OCLINT(constant cond op)
            if (seed != last_seed) {
                result = true;
                FLOGF(uvar_notifier, "polled true: shmem seed change %u -> %u", last_seed, seed);
                last_seed = seed;
                last_change_time = get_time();
            }
        }
        return result;
    }

    unsigned long usec_delay_between_polls() const override {
        // If it's been less than five seconds since the last change, we poll quickly Otherwise we
        // poll more slowly. Note that a poll is a very cheap shmem read. The bad part about making
        // this high is the process scheduling/wakeups it produces.
        long long usec_per_sec = 1000000;
        if (get_time() - last_change_time < 5LL * usec_per_sec) {
            return usec_per_sec / 10;  // 10 times a second
        }
        return usec_per_sec / 3;  // 3 times a second
    }
#else  // this class isn't valid on this system
   public:
    [[noreturn]] universal_notifier_shmem_poller_t() {
        DIE("universal_notifier_shmem_poller_t cannot be used on this system");
    }
#endif
};

/// A notifyd-based notifier. Very straightforward.
class universal_notifier_notifyd_t final : public universal_notifier_t {
#ifdef FISH_NOTIFYD_AVAILABLE
    // Note that we should not use autoclose_fd_t, as notify_cancel() takes responsibility for
    // closing it.
    int notify_fd{-1};
    int token{-1};  // NOTIFY_TOKEN_INVALID
    std::string name{};

    void setup_notifyd() {
        // Per notify(3), the user.uid.%d style is only accessible to processes with that uid.
        char local_name[256];
        snprintf(local_name, sizeof local_name, "user.uid.%d.%ls.uvars", getuid(),
                 program_name ? program_name : L"fish");
        name.assign(local_name);

        uint32_t status =
            notify_register_file_descriptor(name.c_str(), &this->notify_fd, 0, &this->token);

        if (status != NOTIFY_STATUS_OK) {
            FLOGF(warning, "notify_register_file_descriptor() failed with status %u.", status);
            FLOGF(warning, "Universal variable notifications may not be received.");
        }
        if (notify_fd >= 0) {
            // Mark us for non-blocking reads, and CLO_EXEC.
            int flags = fcntl(notify_fd, F_GETFL, 0);
            if (flags >= 0 && !(flags & O_NONBLOCK)) {
                fcntl(notify_fd, F_SETFL, flags | O_NONBLOCK);
            }

            (void)set_cloexec(notify_fd);
            // Serious hack: notify_fd is likely the read end of a pipe. The other end is owned by
            // libnotify, which does not mark it as CLO_EXEC (it should!). The next fd is probably
            // notify_fd + 1. Do it ourselves. If the implementation changes and some other FD gets
            // marked as CLO_EXEC, that's probably a good thing.
            (void)set_cloexec(notify_fd + 1);
        }
    }

   public:
    universal_notifier_notifyd_t() { setup_notifyd(); }

    ~universal_notifier_notifyd_t() {
        if (token != -1 /* NOTIFY_TOKEN_INVALID */) {
            // Note this closes notify_fd.
            notify_cancel(token);
        }
    }

    int notification_fd() const override { return notify_fd; }

    bool notification_fd_became_readable(int fd) override {
        // notifyd notifications come in as 32 bit values. We don't care about the value. We set
        // ourselves as non-blocking, so just read until we can't read any more.
        assert(fd == notify_fd);
        bool read_something = false;
        unsigned char buff[64];
        ssize_t amt_read;
        do {
            amt_read = read(notify_fd, buff, sizeof buff);
            read_something = (read_something || amt_read > 0);
        } while (amt_read == sizeof buff);
        FLOGF(uvar_notifier, "notify fd %s readable", read_something ? "was" : "was not");
        return read_something;
    }

    void post_notification() override {
        FLOG(uvar_notifier, "posting notification");
        uint32_t status = notify_post(name.c_str());
        if (status != NOTIFY_STATUS_OK) {
            FLOGF(warning,
                  "notify_post() failed with status %u. Uvar notifications may not be sent.",
                  status);
        }
    }
#else  // this class isn't valid on this system
   public:
    [[noreturn]] universal_notifier_notifyd_t() {
        DIE("universal_notifier_notifyd_t cannot be used on this system");
    }
#endif
};

/// Returns a "variables" file in the appropriate runtime directory. This is called infrequently and
/// so does not need to be cached.
static wcstring default_named_pipe_path() {
    wcstring result = env_get_runtime_path();
    if (!result.empty()) {
        result.append(L"/fish_universal_variables");
    }
    return result;
}

/// Create a fifo (named pipe) at \p test_path if non-null, or a default runtime path if null.
/// Open the fifo for both reading and writing, in non-blocking mode.
/// \return the fifo, or an invalid fd on failure.
static autoclose_fd_t make_fifo(const wchar_t *test_path, const wchar_t *suffix) {
    wcstring vars_path = test_path ? wcstring(test_path) : default_named_pipe_path();
    vars_path.append(suffix);
    const std::string narrow_path = wcs2zstring(vars_path);

    int mkfifo_status = mkfifo(narrow_path.c_str(), 0600);
    if (mkfifo_status == -1 && errno != EEXIST) {
        const char *error = std::strerror(errno);
        const wchar_t *errmsg = _(L"Unable to make a pipe for universal variables using '%ls': %s");
        FLOGF(error, errmsg, vars_path.c_str(), error);
        return autoclose_fd_t{};
    }

    autoclose_fd_t res{wopen_cloexec(vars_path, O_RDWR | O_NONBLOCK, 0600)};
    if (!res.valid()) {
        const char *error = std::strerror(errno);
        const wchar_t *errmsg = _(L"Unable to open a pipe for universal variables using '%ls': %s");
        FLOGF(error, errmsg, vars_path.c_str(), error);
    }
    return res;
}

// Named-pipe based notifier. All clients open the same named pipe for reading and writing. The
// pipe's readability status is a trigger to enter polling mode.
//
// To post a notification, write some data to the pipe, wait a little while, and then read it back.
//
// To receive a notification, watch for the pipe to become readable. When it does, enter a polling
// mode until the pipe is no longer readable, where we poll based on the modification date of the
// pipe. To guard against the possibility of a shell exiting when there is data remaining in the
// pipe, if the pipe is kept readable too long, clients will attempt to read data out of it (to
// render it no longer readable).
class universal_notifier_named_pipe_t final : public universal_notifier_t {
#if !defined(__CYGWIN__)
    // We operate a state machine.
    enum state_t{
        // The pipe is not yet readable. There is nothing to do in poll.
        // If the pipe becomes readable we will enter the polling state.
        waiting_for_readable,

        // The pipe is readable. In poll, check if the pipe is still readable,
        // and whether its timestamp has changed.
        polling_during_readable,

        // We have written to the pipe (so we expect it to be readable).
        // We may read back from it in poll().
        waiting_to_drain,
    };

    // The state we are currently in.
    state_t state{waiting_for_readable};

    // When we entered that state, in microseconds since epoch.
    long long state_start_usec{-1};

    // The pipe itself; this is opened read/write.
    autoclose_fd_t pipe_fd;

    // The pipe's file ID containing the last modified timestamp.
    file_id_t pipe_timestamps{};

    // If we are in waiting_to_drain state, how much we have written and therefore are responsible
    // for draining.
    size_t drain_amount{0};

    // We "flash" the pipe to make it briefly readable, for this many usec.
    static constexpr long long k_flash_duration_usec = 1e4;

    // If the pipe remains readable for this many usec, we drain it.
    static constexpr long long k_readable_too_long_duration_usec = 1e6;

    /// \return the name of a state.
    static const char *state_name(state_t s) {
        switch (s) {
            case waiting_for_readable:
                return "waiting";
            case polling_during_readable:
                return "polling";
            case waiting_to_drain:
                return "draining";
        }
        DIE("Unreachable");
    }

    // Switch to a state (may or may not be new).
    void set_state(state_t new_state) {
        FLOGF(uvar_notifier, "changing from %s to %s", state_name(state), state_name(new_state));
        state = new_state;
        state_start_usec = get_time();
    }

    // Called when the pipe has been readable for too long.
    void drain_excess() const {
        // The pipe seems to have data on it, that won't go away. Read a big chunk out of it. We
        // don't read until it's exhausted, because if someone were to pipe say /dev/null, that
        // would cause us to hang!
        FLOG(uvar_notifier, "pipe was full, draining it");
        char buff[512];
        ignore_result(read(pipe_fd.fd(), buff, sizeof buff));
    }

    // Called when we want to read back data we have written, to mark the pipe as non-readable.
    void drain_written() {
        while (this->drain_amount > 0) {
            char buff[64];
            size_t amt = std::min(this->drain_amount, sizeof buff);
            ignore_result(read(this->pipe_fd.fd(), buff, amt));
            this->drain_amount -= amt;
        }
    }

    /// Check if the pipe's file ID (aka struct stat) is different from what we have stored.
    /// If it has changed, it indicates that someone has modified the pipe; update our stored id.
    /// \return true if changed, false if not.
    bool update_pipe_timestamps() {
        if (!pipe_fd.valid()) return false;
        file_id_t timestamps = file_id_for_fd(pipe_fd.fd());
        if (timestamps == this->pipe_timestamps) {
            return false;
        }
        this->pipe_timestamps = timestamps;
        return true;
    }

   public:
    explicit universal_notifier_named_pipe_t(const wchar_t *test_path)
        : pipe_fd(make_fifo(test_path, L".notifier")) {}

    ~universal_notifier_named_pipe_t() override = default;

    int notification_fd() const override {
        if (!pipe_fd.valid()) return -1;
        // If we are waiting for the pipe to be readable, return it for select.
        // Otherwise we expect it to be readable already; return invalid.
        switch (state) {
            case waiting_for_readable:
                return pipe_fd.fd();
            case polling_during_readable:
            case waiting_to_drain:
                return -1;
        }
        DIE("unreachable");
    }

    bool notification_fd_became_readable(int fd) override {
        assert(fd == pipe_fd.fd() && "Wrong fd");
        UNUSED(fd);
        switch (state) {
            case waiting_for_readable:
                // We are now readable.
                // Grab the timestamp and return true indicating that we received a notification.
                set_state(polling_during_readable);
                update_pipe_timestamps();
                return true;

            case polling_during_readable:
            case waiting_to_drain:
                // We did not return an fd to wait on, so should not be called.
                DIE("should not be called in this state");
        }
        DIE("unreachable");
    }

    void post_notification() override {
        if (!pipe_fd.valid()) return;
        // We need to write some data (any data) to the pipe, then wait for a while, then read
        // it back. Nobody is expected to read it except us.
        FLOGF(uvar_notifier, "writing to pipe (written %lu)", (unsigned long)drain_amount);
        char c[1] = {'\0'};
        ssize_t amt_written = write(pipe_fd.fd(), c, sizeof c);
        if (amt_written < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            // Very unusual: the pipe is full! Try to read some and repeat once.
            drain_excess();
            amt_written = write(pipe_fd.fd(), c, sizeof c);
            if (amt_written < 0) {
                FLOG(uvar_notifier, "pipe could not be drained, skipping notification");
                return;
            }
            FLOG(uvar_notifier, "pipe drained");
        }
        assert(amt_written >= 0 && "Amount should not be negative");
        this->drain_amount += amt_written;
        // We unconditionally set our state to waiting to drain.
        set_state(waiting_to_drain);
        update_pipe_timestamps();
    }

    unsigned long usec_delay_between_polls() const override {
        if (!pipe_fd.valid()) return 0;
        switch (state) {
            case waiting_for_readable:
                // No polling necessary until it becomes readable.
                return 0;

            case polling_during_readable:
            case waiting_to_drain:
                return k_flash_duration_usec;
        }
        DIE("Unreachable");
    }

    bool poll() override {
        if (!pipe_fd.valid()) return false;
        switch (state) {
            case waiting_for_readable:
                // Nothing to do until the fd is readable.
                return false;

            case polling_during_readable: {
                // If we're no longer readable, go back to wait mode.
                // Conversely, if we have been readable too long, perhaps some fish died while its
                // written data was still on the pipe; drain some.
                if (!poll_fd_readable(pipe_fd.fd())) {
                    set_state(waiting_for_readable);
                } else if (get_time() >= state_start_usec + k_readable_too_long_duration_usec) {
                    drain_excess();
                }
                // Sync if the pipe's timestamp is different, meaning someone modified the pipe
                // since we last saw it.
                if (update_pipe_timestamps()) {
                    FLOG(uvar_notifier, "pipe changed, will sync uvars");
                    return true;
                }
                return false;
            }

            case waiting_to_drain: {
                // We wrote data to the pipe. Maybe read it back.
                // If we are still readable, then there is still data on the pipe; maybe another
                // change occurred with ours.
                if (get_time() >= state_start_usec + k_flash_duration_usec) {
                    drain_written();
                    if (!poll_fd_readable(pipe_fd.fd())) {
                        set_state(waiting_for_readable);
                    } else {
                        set_state(polling_during_readable);
                    }
                }
                return update_pipe_timestamps();
            }
        }
        DIE("Unreachable");
    }

#else  // this class isn't valid on this system
   public:
    universal_notifier_named_pipe_t(const wchar_t *test_path) {
        static_cast<void>(test_path);
        DIE("universal_notifier_named_pipe_t cannot be used on this system");
    }
#endif
};
}  // namespace

universal_notifier_t::notifier_strategy_t universal_notifier_t::resolve_default_strategy() {
#ifdef FISH_NOTIFYD_AVAILABLE
    return strategy_notifyd;
#elif defined(__CYGWIN__)
    return strategy_shmem_polling;
#else
    return strategy_named_pipe;
#endif
}

universal_notifier_t &universal_notifier_t::default_notifier() {
    static std::unique_ptr<universal_notifier_t> result =
        new_notifier_for_strategy(universal_notifier_t::resolve_default_strategy());
    return *result;
}

std::unique_ptr<universal_notifier_t> universal_notifier_t::new_notifier_for_strategy(
    universal_notifier_t::notifier_strategy_t strat, const wchar_t *test_path) {
    switch (strat) {
        case strategy_notifyd: {
            return make_unique<universal_notifier_notifyd_t>();
        }
        case strategy_shmem_polling: {
            return make_unique<universal_notifier_shmem_poller_t>();
        }
        case strategy_named_pipe: {
            return make_unique<universal_notifier_named_pipe_t>(test_path);
        }
    }
    DIE("should never reach this statement");
    return nullptr;
}

// Default implementations.
universal_notifier_t::universal_notifier_t() = default;

universal_notifier_t::~universal_notifier_t() = default;

int universal_notifier_t::notification_fd() const { return -1; }

bool universal_notifier_t::poll() { return false; }

void universal_notifier_t::post_notification() {}

unsigned long universal_notifier_t::usec_delay_between_polls() const { return 0; }

bool universal_notifier_t::notification_fd_became_readable(int fd) {
    UNUSED(fd);
    return false;
}

void env_universal_notifier_t_default_notifier_post_notification_ffi() {
    return universal_notifier_t::default_notifier().post_notification();
}
