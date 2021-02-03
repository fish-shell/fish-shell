/** Facilities for working with file descriptors. */

#include "config.h"  // IWYU pragma: keep

#include "fds.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"
#include "flog.h"
#include "wutil.h"

#if defined(__linux__)
#include <sys/statfs.h>
#endif

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
    if (fd >= 0 && !set_cloexec(fd)) {
        exec_close(fd);
        fd = -1;
    }
#endif
    return fd;
}

int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode) {
    return open_cloexec(wcs2string(pathname), flags, mode);
}

int fd_check_is_remote(int fd) {
    UNUSED(fd);
#if defined(__linux__)
    struct statfs buf {};
    if (fstatfs(fd, &buf) < 0) {
        return -1;
    }
    // Linux has constants for these like NFS_SUPER_MAGIC, SMB_SUPER_MAGIC, CIFS_MAGIC_NUMBER but
    // these are in varying headers. Simply hard code them.
    // NOTE: The cast is necessary for 32-bit systems because of the 4-byte CIFS_MAGIC_NUMBER
    switch (static_cast<unsigned int>(buf.f_type)) {
        case 0x6969:       // NFS_SUPER_MAGIC
        case 0x517B:       // SMB_SUPER_MAGIC
        case 0xFE534D42U:  // SMB2_MAGIC_NUMBER - not in the manpage
        case 0xFF534D42U:  // CIFS_MAGIC_NUMBER
            return 1;
        default:
            // Other FSes are assumed local.
            return 0;
    }
#elif defined(ST_LOCAL)
    // ST_LOCAL is a flag to statvfs, which is itself standardized.
    // In practice the only system to use this path is NetBSD.
    struct statvfs buf {};
    if (fstatvfs(fd, &buf) < 0) return -1;
    return (buf.f_flag & ST_LOCAL) ? 0 : 1;
#elif defined(MNT_LOCAL)
    struct statfs buf {};
    if (fstatfs(fd, &buf) < 0) return -1;
    return (buf.f_flags & MNT_LOCAL) ? 0 : 1;
#else
    return -1;
#endif
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
