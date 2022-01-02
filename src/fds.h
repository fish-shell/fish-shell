/** Facilities for working with file descriptors. */

#ifndef FISH_FDS_H
#define FISH_FDS_H

#include "config.h"  // IWYU pragma: keep

#include <poll.h>
#include <sys/select.h>
#include <sys/types.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "common.h"
#include "maybe.h"

/// Pipe redirection error message.
#define PIPE_ERROR _(L"An error occurred while setting up pipe")

/// The first "high fd", which is considered outside the range of valid user-specified redirections
/// (like >&5).
extern const int k_first_high_fd;

/// A helper class for managing and automatically closing a file descriptor.
class autoclose_fd_t : noncopyable_t {
    int fd_;

   public:
    // Closes the fd if not already closed.
    void close();

    // Returns the fd.
    int fd() const { return fd_; }

    // Returns the fd, transferring ownership to the caller.
    int acquire() {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }

    // Resets to a new fd, taking ownership.
    void reset(int fd) {
        if (fd == fd_) return;
        close();
        fd_ = fd;
    }

    // \return if this has a valid fd.
    bool valid() const { return fd_ >= 0; }

    autoclose_fd_t(autoclose_fd_t &&rhs) : fd_(rhs.fd_) { rhs.fd_ = -1; }

    void operator=(autoclose_fd_t &&rhs) {
        close();
        std::swap(this->fd_, rhs.fd_);
    }

    explicit autoclose_fd_t(int fd = -1) : fd_(fd) {}
    ~autoclose_fd_t() { close(); }
};

// Resolve whether to use poll() or select().
#ifndef FISH_READABLE_SET_USE_POLL
#ifdef __APPLE__
//  Apple's `man poll`: "The poll() system call currently does not support devices."
#define FISH_READABLE_SET_USE_POLL 0
#else
// Use poll other places so we can support unlimited fds.
#define FISH_READABLE_SET_USE_POLL 1
#endif
#endif

/// A modest wrapper around select() or poll(), according to FISH_READABLE_SET_USE_POLL.
/// This allows accumulating a set of fds and then seeing if they are readable.
/// This only handles readability.
struct fd_readable_set_t {
    /// Construct an empty set.
    fd_readable_set_t();

    /// Reset back to an empty set.
    void clear();

    /// Add an fd to the set. The fd is ignored if negative (for convenience).
    void add(int fd);

    /// \return true if the given fd is marked as set, in our set. \returns false if negative.
    bool test(int fd) const;

    /// Call select() or poll(), according to FISH_READABLE_SET_USE_POLL. Note this destructively
    /// modifies the set. \return the result of select() or poll().
    int check_readable(uint64_t timeout_usec = fd_readable_set_t::kNoTimeout);

    /// Check if a single fd is readable, with a given timeout.
    /// \return true if readable, false if not.
    static bool is_fd_readable(int fd, uint64_t timeout_usec);

    /// Check if a single fd is readable, without blocking.
    /// \return true if readable, false if not.
    static bool poll_fd_readable(int fd);

    /// A special timeout value which may be passed to indicate no timeout.
    static constexpr uint64_t kNoTimeout = std::numeric_limits<uint64_t>::max();

   private:
#if FISH_READABLE_SET_USE_POLL
    // Our list of FDs, sorted by fd.
    std::vector<struct pollfd> pollfds_{};

    // Helper function.
    static int do_poll(struct pollfd *fds, size_t count, uint64_t timeout_usec);
#else
    // The underlying fdset and nfds value to pass to select().
    fd_set fdset_;
    int nfds_{0};
#endif
};

/// Helper type returned from making autoclose pipes.
struct autoclose_pipes_t {
    /// Read end of the pipe.
    autoclose_fd_t read;

    /// Write end of the pipe.
    autoclose_fd_t write;

    autoclose_pipes_t() = default;
    autoclose_pipes_t(autoclose_fd_t r, autoclose_fd_t w)
        : read(std::move(r)), write(std::move(w)) {}
};

/// Call pipe(), populating autoclose fds.
/// The pipes are marked CLO_EXEC and are placed in the high fd range.
/// \return pipes on success, none() on error.
maybe_t<autoclose_pipes_t> make_autoclose_pipes();

/// An event signaller implemented using a file descriptor, so it can plug into select().
/// This is like a binary semaphore. A call to post() will signal an event, making the fd readable.
/// Multiple calls to post() may be coalesced. On Linux this uses eventfd(); on other systems this
/// uses a pipe. try_consume() may be used to consume the event. Importantly this is async signal
/// safe. Of course it is CLO_EXEC as well.
class fd_event_signaller_t {
   public:
    /// \return the fd to read from, for notification.
    int read_fd() const { return fd_.fd(); }

    /// If an event is signalled, consume it; otherwise return.
    /// This does not block.
    /// This retries on EINTR.
    bool try_consume() const;

    /// Mark that an event has been received. This may be coalesced.
    /// This retries on EINTR.
    void post();

    /// Perform a poll to see if an event is received.
    /// If \p wait is set, wait until it is readable; this does not consume the event
    /// but guarantees that the next call to wait() will not block.
    /// \return true if readable, false if not readable, or not interrupted by a signal.
    bool poll(bool wait = false) const;

    // The default constructor will abort on failure (fd exhaustion).
    // This should only be used during startup.
    fd_event_signaller_t();
    ~fd_event_signaller_t();

   private:
    // \return the fd to write to.
    int write_fd() const;

    // Always the read end of the fd; maybe the write end as well.
    autoclose_fd_t fd_;

#ifndef HAVE_EVENTFD
    // If using a pipe, then this is its write end.
    autoclose_fd_t write_;
#endif
};

/// Sets CLO_EXEC on a given fd according to the value of \p should_set.
int set_cloexec(int fd, bool should_set = true);

/// Wide character version of open() that also sets the close-on-exec flag (atomically when
/// possible).
int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode = 0);

/// Narrow versions of wopen_cloexec.
int open_cloexec(const std::string &path, int flags, mode_t mode = 0);
int open_cloexec(const char *path, int flags, mode_t mode = 0);

/// Mark an fd as nonblocking; returns errno or 0 on success.
int make_fd_nonblocking(int fd);

/// Mark an fd as blocking; returns errno or 0 on success.
int make_fd_blocking(int fd);

/// Close a file descriptor \p fd, retrying on EINTR.
void exec_close(int fd);

#endif
