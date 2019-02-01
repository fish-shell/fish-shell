// Utilities for io redirection.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <wchar.h>

#include "common.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wutil.h"  // IWYU pragma: keep

io_data_t::~io_data_t() = default;

void io_close_t::print() const { fwprintf(stderr, L"close %d\n", fd); }

void io_fd_t::print() const { fwprintf(stderr, L"FD map %d -> %d\n", old_fd, fd); }

void io_file_t::print() const { fwprintf(stderr, L"file (%s)\n", filename_cstr); }

void io_pipe_t::print() const {
    fwprintf(stderr, L"pipe {%d} (input: %s)\n", pipe_fd(), is_input_ ? "yes" : "no");
}

void io_bufferfill_t::print() const { fwprintf(stderr, L"bufferfill {%d}\n", write_fd_.fd()); }

void io_buffer_t::append_from_stream(const output_stream_t &stream) {
    if (buffer_.discarded()) return;
    if (stream.buffer().discarded()) {
        buffer_.set_discard();
        return;
    }
    buffer_.append_wide_buffer(stream.buffer());
}

long io_buffer_t::read_some() {
    int fd = read_.fd();
    assert(fd >= 0 && "Should have a valid fd");
    debug(4, L"io_buffer_t::read: blocking read on fd %d", fd);
    long len;
    char b[4096];
    do {
        len = read(fd, b, sizeof b);
    } while (len < 0 && errno == EINTR);
    if (len > 0) {
        buffer_.append(&b[0], &b[len]);
    }
    return len;
}

void io_buffer_t::read_to_wouldblock() {
    long len;
    do {
        len = read_some();
    } while (len > 0);
    if (len < 0 && errno != EAGAIN) {
        debug(1, _(L"An error occured while reading output from code block on fd %d"), read_.fd());
        wperror(L"io_buffer_t::read");
    }
}

bool io_buffer_t::try_read(unsigned long timeout_usec) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_usec;
    int fd = read_.fd();
    assert(fd >= 0 && "Should have a valid fd");
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int ret = select(fd + 1, &fds, nullptr, nullptr, &timeout);
    if (ret < 0) {
        // Treat EINTR as timeout.
        if (errno != EINTR) {
            debug(1, _(L"An error occured inside select on fd %d"), fd);
            wperror(L"io_buffer_t::try_read");
        }
        return false;
    }
    if (ret > 0) {
        read_some();
    }
    return ret > 0;
}

shared_ptr<io_bufferfill_t> io_bufferfill_t::create(const io_chain_t &conflicts,
                                                    size_t buffer_limit) {
    // Construct our pipes.
    auto pipes = make_autoclose_pipes(conflicts);
    if (!pipes) {
        return nullptr;
    }

    // Our buffer will read from the read end of the pipe. This end must be non-blocking. This is
    // because we retain the write end of the pipe in this process (even after handing it off to a
    // child process); therefore a read on the pipe may block forever. What we should do is arrange
    // for the write end of the pipe to be closed at the right time; then the read could just block.
    if (make_fd_nonblocking(pipes->read.fd())) {
        debug(1, PIPE_ERROR);
        wperror(L"fcntl");
        return nullptr;
    }

    // Our buffer gets the read end of the pipe; out_pipe gets the write end.
    auto buffer = std::make_shared<io_buffer_t>(std::move(pipes->read), buffer_limit);
    return std::make_shared<io_bufferfill_t>(std::move(pipes->write), buffer);
}

io_pipe_t::~io_pipe_t() = default;

io_bufferfill_t::~io_bufferfill_t() = default;

io_buffer_t::~io_buffer_t() = default;

void io_chain_t::remove(const shared_ptr<const io_data_t> &element) {
    // See if you can guess why std::find doesn't work here.
    for (io_chain_t::iterator iter = this->begin(); iter != this->end(); ++iter) {
        if (*iter == element) {
            this->erase(iter);
            break;
        }
    }
}

void io_chain_t::push_back(shared_ptr<io_data_t> element) {
    // Ensure we never push back NULL.
    assert(element.get() != nullptr);
    std::vector<shared_ptr<io_data_t> >::push_back(std::move(element));
}

void io_chain_t::push_front(shared_ptr<io_data_t> element) {
    assert(element.get() != nullptr);
    this->insert(this->begin(), std::move(element));
}

void io_chain_t::append(const io_chain_t &chain) {
    this->insert(this->end(), chain.begin(), chain.end());
}

#if 0
// This isn't used so the lint tools were complaining about its presence. I'm keeping it in the
// source because it could be useful for debugging.
void io_print(const io_chain_t &chain)
{
    if (chain.empty())
    {
        fwprintf(stderr, L"Empty chain %p\n", &chain);
        return;
    }

    fwprintf(stderr, L"Chain %p (%ld items):\n", &chain, (long)chain.size());
    for (size_t i=0; i < chain.size(); i++)
    {
        const shared_ptr<io_data_t> &io = chain.at(i);
        if (io.get() == NULL)
        {
            fwprintf(stderr, L"\t(null)\n");
        }
        else
        {
            fwprintf(stderr, L"\t%lu: fd:%d, ", (unsigned long)i, io->fd);
            io->print();
        }
    }
}
#endif


int move_fd_to_unused(int fd, const io_chain_t &io_chain, bool cloexec) {
    if (fd < 0 || io_chain.get_io_for_fd(fd).get() == NULL) {
        return fd;
    }

    // We have fd >= 0, and it's a conflict. dup it and recurse. Note that we recurse before
    // anything is closed; this forces the kernel to give us a new one (or report fd exhaustion).
    int new_fd = fd;
    int tmp_fd;
    do {
        tmp_fd = dup(fd);
    } while (tmp_fd < 0 && errno == EINTR);

    assert(tmp_fd != fd);
    if (tmp_fd < 0) {
        // Likely fd exhaustion.
        new_fd = -1;
    } else {
        // Ok, we have a new candidate fd. Recurse. If we get a valid fd, either it's the same as
        // what we gave it, or it's a new fd and what we gave it has been closed. If we get a
        // negative value, the fd also has been closed.
        if (cloexec) set_cloexec(tmp_fd);
        new_fd = move_fd_to_unused(tmp_fd, io_chain);
    }

    // We're either returning a new fd or an error. In both cases, we promise to close the old one.
    assert(new_fd != fd);
    int saved_errno = errno;
    exec_close(fd);
    errno = saved_errno;
    return new_fd;
}

static bool pipe_avoid_conflicts_with_io_chain(int fds[2], const io_chain_t &ios) {
    bool success = true;
    for (int i = 0; i < 2; i++) {
        fds[i] = move_fd_to_unused(fds[i], ios);
        if (fds[i] < 0) {
            success = false;
            break;
        }
    }

    // If any fd failed, close all valid fds.
    if (!success) {
        int saved_errno = errno;
        for (int i = 0; i < 2; i++) {
            if (fds[i] >= 0) {
                exec_close(fds[i]);
                fds[i] = -1;
            }
        }
        errno = saved_errno;
    }
    return success;
}

maybe_t<autoclose_pipes_t> make_autoclose_pipes(const io_chain_t &ios) {
    int pipes[2] = {-1, -1};

    if (pipe(pipes) < 0) {
        debug(1, PIPE_ERROR);
        wperror(L"pipe");
        return none();
    }
    set_cloexec(pipes[0]);
    set_cloexec(pipes[1]);

    if (!pipe_avoid_conflicts_with_io_chain(pipes, ios)) {
        // The pipes are closed on failure here.
        return none();
    }
    autoclose_pipes_t result;
    result.read = autoclose_fd_t(pipes[0]);
    result.write = autoclose_fd_t(pipes[1]);
    return {std::move(result)};
}

/// Return the last IO for the given fd.
shared_ptr<const io_data_t> io_chain_t::get_io_for_fd(int fd) const {
    size_t idx = this->size();
    while (idx--) {
        const shared_ptr<io_data_t> &data = this->at(idx);
        if (data->fd == fd) {
            return data;
        }
    }
    return shared_ptr<const io_data_t>();
}

shared_ptr<io_data_t> io_chain_t::get_io_for_fd(int fd) {
    size_t idx = this->size();
    while (idx--) {
        const shared_ptr<io_data_t> &data = this->at(idx);
        if (data->fd == fd) {
            return data;
        }
    }
    return shared_ptr<io_data_t>();
}

/// The old function returned the last match, so we mimic that.
shared_ptr<const io_data_t> io_chain_get(const io_chain_t &src, int fd) {
    return src.get_io_for_fd(fd);
}

shared_ptr<io_data_t> io_chain_get(io_chain_t &src, int fd) { return src.get_io_for_fd(fd); }
