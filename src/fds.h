/** Facilities for working with file descriptors. */

#ifndef FISH_FDS_H
#define FISH_FDS_H

#include <algorithm>
#include <string>
#include <vector>

#include "maybe.h"

using wcstring = std::wstring;

/// Pipe redirection error message.
#define PIPE_ERROR _(L"An error occurred while setting up pipe")

/// A helper class for managing and automatically closing a file descriptor.
class autoclose_fd_t {
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

    autoclose_fd_t(const autoclose_fd_t &) = delete;
    void operator=(const autoclose_fd_t &) = delete;
    autoclose_fd_t(autoclose_fd_t &&rhs) : fd_(rhs.fd_) { rhs.fd_ = -1; }

    void operator=(autoclose_fd_t &&rhs) {
        close();
        std::swap(this->fd_, rhs.fd_);
    }

    explicit autoclose_fd_t(int fd = -1) : fd_(fd) {}
    ~autoclose_fd_t() { close(); }
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

/// A simple set of FDs.
struct fd_set_t {
    std::vector<bool> fds;

    void add(int fd) {
        assert(fd >= 0 && "Invalid fd");
        if (static_cast<size_t>(fd) >= fds.size()) {
            fds.resize(fd + 1);
        }
        fds[fd] = true;
    }

    bool contains(int fd) const {
        assert(fd >= 0 && "Invalid fd");
        return static_cast<size_t>(fd) < fds.size() && fds[fd];
    }

    bool empty() const { return fds.empty(); }
};

/// Call pipe(), populating autoclose fds, avoiding conflicts.
/// The pipes are marked CLO_EXEC.
/// \return pipes on success, none() on error.
maybe_t<autoclose_pipes_t> make_autoclose_pipes(const fd_set_t &fdset);

/// If the given fd is present in \p fdset, duplicates it repeatedly until an fd not used in the set
/// is found or we run out. If we return a new fd or an error, closes the old one. Marks the fd as
/// cloexec. \returns invalid fd on failure (in which case the given fd is still closed).
autoclose_fd_t move_fd_to_unused(autoclose_fd_t fd, const fd_set_t &fdset);

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

/// Check if an fd is on a remote filesystem (NFS, SMB, CFS)
/// Return 1 if remote, 0 if local, -1 on error or if not implemented on this platform.
int fd_check_is_remote(int fd);

/// Close a file descriptor \p fd, retrying on EINTR.
void exec_close(int fd);

#endif
