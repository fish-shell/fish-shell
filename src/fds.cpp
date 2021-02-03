/** Facilities for working with file descriptors. */

#include "config.h"  // IWYU pragma: keep

#include "fds.h"

#include <errno.h>
#include <unistd.h>

#include "flog.h"
#include "wutil.h"

void autoclose_fd_t::close() {
    if (fd_ < 0) return;
    exec_close(fd_);
    fd_ = -1;
}

autoclose_fd_t move_fd_to_unused(autoclose_fd_t fd, const fd_set_t &fdset) {
    if (!fd.valid() || !fdset.contains(fd.fd())) {
        return fd;
    }

    // We have fd >= 0, and it's a conflict. dup it and recurse. Note that we recurse before
    // anything is closed; this forces the kernel to give us a new one (or report fd exhaustion).
    int tmp_fd;
    do {
        tmp_fd = dup(fd.fd());
    } while (tmp_fd < 0 && errno == EINTR);

    assert(tmp_fd != fd.fd());
    if (tmp_fd < 0) {
        // Likely fd exhaustion.
        return autoclose_fd_t{};
    }
    // Ok, we have a new candidate fd. Recurse.
    set_cloexec(tmp_fd);
    return move_fd_to_unused(autoclose_fd_t{tmp_fd}, fdset);
}

maybe_t<autoclose_pipes_t> make_autoclose_pipes(const fd_set_t &fdset) {
    int pipes[2] = {-1, -1};

    if (pipe(pipes) < 0) {
        FLOGF(warning, PIPE_ERROR);
        wperror(L"pipe");
        return none();
    }
    set_cloexec(pipes[0]);
    set_cloexec(pipes[1]);

    autoclose_fd_t read_end{pipes[0]};
    autoclose_fd_t write_end{pipes[1]};

    // Ensure we have no conflicts.
    if (!fdset.empty()) {
        read_end = move_fd_to_unused(std::move(read_end), fdset);
        if (!read_end.valid()) return none();

        write_end = move_fd_to_unused(std::move(write_end), fdset);
        if (!write_end.valid()) return none();
    }
    return autoclose_pipes_t(std::move(read_end), std::move(write_end));
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
