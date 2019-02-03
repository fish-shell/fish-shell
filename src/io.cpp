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
    fwprintf(stderr, L"pipe {%d, %d} (input: %s)\n", pipe_fd[0], pipe_fd[1],
             is_input ? "yes" : "no");
}

void io_buffer_t::print() const {
    fwprintf(stderr, L"buffer (input: %s, size %lu)\n",
             is_input ? "yes" : "no", (unsigned long)buffer_.size());
}

void io_buffer_t::append_from_stream(const output_stream_t &stream) {
    if (buffer_.discarded()) return;
    if (stream.buffer().discarded()) {
        buffer_.set_discard();
        return;
    }
    buffer_.append_wide_buffer(stream.buffer());
}

void io_buffer_t::read() {
    exec_close(pipe_fd[1]);

    if (io_mode == io_mode_t::buffer) {
        debug(4, L"io_buffer_t::read: blocking read on fd %d", pipe_fd[0]);
        while (1) {
            char b[4096];
            long len = read_blocked(pipe_fd[0], b, 4096);
            if (len == 0) {
                break;
            } else if (len < 0) {
                // exec_read_io_buffer is only called on jobs that have exited, and will therefore
                // never block. But a broken pipe seems to cause some flags to reset, causing the
                // EOF flag to not be set. Therefore, EAGAIN is ignored and we exit anyway.
                if (errno != EAGAIN) {
                    const wchar_t *fmt =
                        _(L"An error occured while reading output from code block on fd %d");
                    debug(1, fmt, pipe_fd[0]);
                    wperror(L"io_buffer_t::read");
                }

                break;
            } else {
                buffer_.append(&b[0], &b[len]);
            }
        }
    }
}

bool io_buffer_t::avoid_conflicts_with_io_chain(const io_chain_t &ios) {
    bool result = pipe_avoid_conflicts_with_io_chain(this->pipe_fd, ios);
    if (!result) {
        wperror(L"dup");
    }
    return result;
}

shared_ptr<io_buffer_t> io_buffer_t::create(int fd, const io_chain_t &conflicts,
                                            size_t buffer_limit) {
    bool success = true;
    assert(fd >= 0);
    shared_ptr<io_buffer_t> buffer_redirect(new io_buffer_t(fd, buffer_limit));

    if (exec_pipe(buffer_redirect->pipe_fd) == -1) {
        debug(1, PIPE_ERROR);
        wperror(L"pipe");
        success = false;
    } else if (!buffer_redirect->avoid_conflicts_with_io_chain(conflicts)) {
        // The above call closes the fds on error.
        success = false;
    } else if (make_fd_nonblocking(buffer_redirect->pipe_fd[0]) != 0) {
        debug(1, PIPE_ERROR);
        wperror(L"fcntl");
        success = false;
    }

    if (!success) {
        buffer_redirect.reset();
    }
    return buffer_redirect;
}

io_buffer_t::~io_buffer_t() {
    if (pipe_fd[0] >= 0) {
        exec_close(pipe_fd[0]);
    }
    // Dont free fd for writing. This should already be free'd before calling exec_read_io_buffer on
    // the buffer.
}

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

bool pipe_avoid_conflicts_with_io_chain(int fds[2], const io_chain_t &ios) {
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
