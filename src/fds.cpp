/** Facilities for working with file descriptors. */

#include "config.h"  // IWYU pragma: keep

#include "fds.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#include "flog.h"
#include "wutil.h"

#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

// The first fd in the "high range." fds below this are allowed to be used directly by users in
// redirections, e.g. >&3
const int k_first_high_fd = 10;
static constexpr uint64_t kUsecPerMsec = 1000;
static constexpr uint64_t kUsecPerSec [[gnu::unused]] = 1000 * kUsecPerMsec;

void autoclose_fd_t::close() {
    if (fd_ < 0) return;
    exec_close(fd_);
    fd_ = -1;
}

std::shared_ptr<fd_event_signaller_t> ffi_new_fd_event_signaller_t() {
    return std::make_shared<fd_event_signaller_t>();
}

#ifdef HAVE_EVENTFD
// Note we do not want to use EFD_SEMAPHORE because we are binary (not counting) semaphore.
fd_event_signaller_t::fd_event_signaller_t() {
    int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd < 0) {
        wperror(L"eventfd");
        exit_without_destructors(1);
    }
    fd_.reset(fd);
};

int fd_event_signaller_t::write_fd() const { return fd_.fd(); }

#else
// Implementation using pipes.
fd_event_signaller_t::fd_event_signaller_t() {
    auto pipes = make_autoclose_pipes();
    if (!pipes) {
        wperror(L"pipe");
        exit_without_destructors(1);
    }
    DIE_ON_FAILURE(make_fd_nonblocking(pipes->read.fd()));
    DIE_ON_FAILURE(make_fd_nonblocking(pipes->write.fd()));
    fd_ = std::move(pipes->read);
    write_ = std::move(pipes->write);
}

int fd_event_signaller_t::write_fd() const { return write_.fd(); }
#endif

bool fd_event_signaller_t::try_consume() const {
    // If we are using eventfd, we want to read a single uint64.
    // If we are using pipes, read a lot; note this may leave data on the pipe if post has been
    // called many more times. In no case do we care about the data which is read.
#ifdef HAVE_EVENTFD
    uint64_t buff[1];
#else
    uint8_t buff[1024];
#endif
    ssize_t ret;
    do {
        ret = read(read_fd(), buff, sizeof buff);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        wperror(L"read");
    }
    return ret > 0;
}

void fd_event_signaller_t::post() const {
    // eventfd writes uint64; pipes write 1 byte.
#ifdef HAVE_EVENTFD
    const uint64_t c = 1;
#else
    const uint8_t c = 1;
#endif
    ssize_t ret;
    do {
        ret = write(write_fd(), &c, sizeof c);
    } while (ret < 0 && errno == EINTR);
    // EAGAIN occurs if either the pipe buffer is full or the eventfd overflows (very unlikely).
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        wperror(L"write");
    }
}

bool fd_event_signaller_t::poll(bool wait) const {
    struct timeval timeout = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(read_fd(), &fds);
    int res = select(read_fd() + 1, &fds, nullptr, nullptr, wait ? nullptr : &timeout);
    return res > 0;
}

fd_event_signaller_t::~fd_event_signaller_t() = default;

/// If the given fd is in the "user range", move it to a new fd in the "high range".
/// zsh calls this movefd().
/// \p input_has_cloexec describes whether the input has CLOEXEC already set, so we can avoid
/// setting it again.
/// \return the fd, which always has CLOEXEC set; or an invalid fd on failure, in
/// which case an error will have been printed, and the input fd closed.
static autoclose_fd_t heightenize_fd(autoclose_fd_t fd, bool input_has_cloexec) {
    // Check if the fd is invalid or already in our high range.
    if (!fd.valid()) {
        return fd;
    }
    if (fd.fd() >= k_first_high_fd) {
        if (!input_has_cloexec) set_cloexec(fd.fd());
        return fd;
    }
#if defined(F_DUPFD_CLOEXEC)
    // Here we are asking the kernel to give us a
    int newfd = fcntl(fd.fd(), F_DUPFD_CLOEXEC, k_first_high_fd);
    if (newfd < 0) {
        wperror(L"fcntl");
        return autoclose_fd_t{};
    }
    return autoclose_fd_t(newfd);
#elif defined(F_DUPFD)
    int newfd = fcntl(fd.fd(), F_DUPFD, k_first_high_fd);
    if (newfd < 0) {
        wperror(L"fcntl");
        return autoclose_fd_t{};
    }
    set_cloexec(newfd);
    return autoclose_fd_t(newfd);
#else
    // We have fd >= 0, and it's in the user range. dup it and recurse. Note that we recurse before
    // anything is closed; this forces the kernel to give us a new one (or report fd exhaustion).
    int tmp_fd;
    do {
        tmp_fd = dup(fd.fd());
    } while (tmp_fd < 0 && errno == EINTR);
    // Ok, we have a new candidate fd. Recurse.
    return heightenize_fd(autoclose_fd_t{tmp_fd}, false);
#endif
}

maybe_t<autoclose_pipes_t> make_autoclose_pipes() {
    int pipes[2] = {-1, -1};

    bool already_cloexec = false;
#ifdef HAVE_PIPE2
    if (pipe2(pipes, O_CLOEXEC) < 0) {
        FLOGF(warning, PIPE_ERROR);
        wperror(L"pipe2");
        return none();
    }
    already_cloexec = true;
#else
    if (pipe(pipes) < 0) {
        FLOGF(warning, PIPE_ERROR);
        wperror(L"pipe");
        return none();
    }
#endif

    autoclose_fd_t read_end{pipes[0]};
    autoclose_fd_t write_end{pipes[1]};

    // Ensure our fds are out of the user range.
    read_end = heightenize_fd(std::move(read_end), already_cloexec);
    if (!read_end.valid()) return none();

    write_end = heightenize_fd(std::move(write_end), already_cloexec);
    if (!write_end.valid()) return none();

    return autoclose_pipes_t(std::move(read_end), std::move(write_end));
}

pipes_ffi_t make_pipes_ffi() {
    pipes_ffi_t res = {-1, -1};
    if (auto pipes = make_autoclose_pipes()) {
        res.read = pipes->read.acquire();
        res.write = pipes->write.acquire();
    }
    return res;
}

int set_cloexec(int fd, bool should_set) {
    // Note we don't want to overwrite existing flags like O_NONBLOCK which may be set. So fetch the
    // existing flags and modify them.
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags < 0) {
        return -1;
    }
    int new_flags = flags;
    if (should_set) {
        new_flags |= FD_CLOEXEC;
    } else {
        new_flags &= ~FD_CLOEXEC;
    }
    if (flags == new_flags) {
        return 0;
    } else {
        return fcntl(fd, F_SETFD, new_flags);
    }
}

int open_cloexec(const std::string &path, int flags, mode_t mode) {
    return open_cloexec(path.c_str(), flags, mode);
}

int open_cloexec(const char *path, int flags, mode_t mode) {
    int fd;

    // Prefer to use O_CLOEXEC.
#ifdef O_CLOEXEC
    fd = open(path, flags | O_CLOEXEC, mode);
#else
    fd = open(path, flags, mode);
    if (fd >= 0 && set_cloexec(fd)) {
        exec_close(fd);
        fd = -1;
    }
#endif
    return fd;
}

int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode) {
    return open_cloexec(wcs2zstring(pathname), flags, mode);
}

void exec_close(int fd) {
    assert(fd >= 0 && "Invalid fd");
    while (close(fd) == -1) {
        if (errno != EINTR) {
            wperror(L"close");
            break;
        }
    }
}
