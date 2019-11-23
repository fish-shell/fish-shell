// Utilities for io redirection.
#include "config.h"  // IWYU pragma: keep

#include "io.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <cwchar>

#include "common.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "iothread.h"
#include "redirection.h"
#include "wutil.h"  // IWYU pragma: keep

io_data_t::~io_data_t() = default;

void io_close_t::print() const { std::fwprintf(stderr, L"close %d\n", fd); }

void io_fd_t::print() const { std::fwprintf(stderr, L"FD map %d -> %d\n", old_fd, fd); }

void io_file_t::print() const { std::fwprintf(stderr, L"file (%ls)\n", filename.c_str()); }

void io_pipe_t::print() const {
    std::fwprintf(stderr, L"pipe {%d} (input: %s)\n", pipe_fd(), is_input_ ? "yes" : "no");
}

void io_bufferfill_t::print() const { std::fwprintf(stderr, L"bufferfill {%d}\n", write_fd_.fd()); }

void io_buffer_t::append_from_stream(const output_stream_t &stream) {
    const separated_buffer_t<wcstring> &input = stream.buffer();
    if (input.elements().empty()) return;
    scoped_lock locker(append_lock_);
    if (buffer_.discarded()) return;
    if (input.discarded()) {
        buffer_.set_discard();
        return;
    }
    buffer_.append_wide_buffer(input);
}

void io_buffer_t::run_background_fillthread(autoclose_fd_t readfd) {
    // Here we are running the background fillthread, executing in a background thread.
    // Our plan is:
    // 1. poll via select() until the fd is readable.
    // 2. Acquire the append lock.
    // 3. read until EAGAIN (would block), appending
    // 4. release the lock
    // The purpose of holding the lock around the read calls is to ensure that data from background
    // processes isn't weirdly interspersed with data directly transferred (from a builtin to a
    // buffer).

    const int fd = readfd.fd();

    // 100 msec poll rate. Note that in most cases, the write end of the pipe will be closed so
    // select() will return; the polling is important only for weird cases like a background process
    // launched in a command substitution.
    const long poll_timeout_usec = 100000;
    struct timeval tv = {};
    tv.tv_usec = poll_timeout_usec;

    bool shutdown = false;
    while (!shutdown) {
        bool readable = false;

        // Poll if our fd is readable.
        // Do this even if the shutdown flag is set. It's important we wait for the fd at least
        // once. For short-lived processes, it's possible for the process to execute, produce output
        // (fits in the pipe buffer) and be reaped before we are even scheduled. So always wait at
        // least once on the fd. Note that doesn't mean we will wait for the full poll duration;
        // typically what will happen is our pipe will be widowed and so this will return quickly.
        // It's only for weird cases (e.g. a background process launched inside a command
        // substitution) that we'll wait out the entire poll time.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int ret = select(fd + 1, &fds, NULL, NULL, &tv);
        // select(2) is allowed to (and does) update `tv` to indicate how much time was left, so we
        // need to restore the desired value each time.
        tv.tv_usec = poll_timeout_usec;
        readable = ret > 0;
        if (ret < 0 && errno != EINTR) {
            // Surprising error.
            wperror(L"select");
            return;
        }

        // Only check the shutdown flag if we timed out.
        // It's important that if select() indicated we were readable, that we call select() again
        // allowing it to time out. Note the typical case is that the fd will be closed, in which
        // case select will return immediately.
        if (!readable) {
            shutdown = this->shutdown_fillthread_;
        }

        if (readable || shutdown) {
            // Now either our fd is readable, or we have set the shutdown flag.
            // Either way acquire the lock and read until we reach EOF, or EAGAIN / EINTR.
            scoped_lock locker(append_lock_);
            ssize_t ret;
            do {
                errno = 0;
                char buff[4096];
                ret = read(fd, buff, sizeof buff);
                if (ret > 0) {
                    buffer_.append(&buff[0], &buff[ret]);
                } else if (ret == 0) {
                    shutdown = true;
                } else if (ret == -1 && errno == 0) {
                    // No specific error. We assume we just return,
                    // since that's what we do in read_blocked.
                    return;
                } else if (errno != EINTR && errno != EAGAIN) {
                    wperror(L"read");
                    return;
                }
            } while (ret > 0);
        }
    }
    assert(shutdown && "Should only exit loop if shutdown flag is set");
}

void io_buffer_t::begin_background_fillthread(autoclose_fd_t fd) {
    ASSERT_IS_MAIN_THREAD();
    assert(!fillthread_ && "Already have a fillthread");

    // We want our background thread to own the fd but it's not easy to move into a std::function.
    // Use a shared_ptr.
    auto fdref = move_to_sharedptr(std::move(fd));

    // Our function to read until the receiver is closed.
    // It's OK to capture 'this' by value because 'this' owns the background thread and joins it
    // before dtor.
    std::function<void(void)> func = [this, fdref]() {
        this->run_background_fillthread(std::move(*fdref));
    };

    pthread_t fillthread{};
    if (!make_pthread(&fillthread, std::move(func))) {
        wperror(L"make_pthread");
    }
    fillthread_ = fillthread;
}

void io_buffer_t::complete_background_fillthread() {
    ASSERT_IS_MAIN_THREAD();
    assert(fillthread_ && "Should have a fillthread");
    shutdown_fillthread_ = true;
    void *ignored = nullptr;
    int err = pthread_join(*fillthread_, &ignored);
    DIE_ON_FAILURE(err);
    fillthread_.reset();
}

shared_ptr<io_bufferfill_t> io_bufferfill_t::create(const io_chain_t &conflicts,
                                                    size_t buffer_limit) {
    // Construct our pipes.
    auto pipes = make_autoclose_pipes(conflicts);
    if (!pipes) {
        return nullptr;
    }
    // Our buffer will read from the read end of the pipe. This end must be non-blocking. This is
    // because our fillthread needs to poll to decide if it should shut down, and also accept input
    // from direct buffer transfers.
    if (make_fd_nonblocking(pipes->read.fd())) {
        debug(1, PIPE_ERROR);
        wperror(L"fcntl");
        return nullptr;
    }
    // Our fillthread gets the read end of the pipe; out_pipe gets the write end.
    auto buffer = std::make_shared<io_buffer_t>(buffer_limit);
    buffer->begin_background_fillthread(std::move(pipes->read));
    return std::make_shared<io_bufferfill_t>(std::move(pipes->write), buffer);
}

std::shared_ptr<io_buffer_t> io_bufferfill_t::finish(std::shared_ptr<io_bufferfill_t> &&filler) {
    // The io filler is passed in. This typically holds the only instance of the write side of the
    // pipe used by the buffer's fillthread (except for that side held by other processes). Get the
    // buffer out of the bufferfill and clear the shared_ptr; this will typically widow the pipe.
    // Then allow the buffer to finish.
    assert(filler && "Null pointer in finish");
    auto buffer = filler->buffer();
    filler.reset();
    buffer->complete_background_fillthread();
    return buffer;
}

io_pipe_t::~io_pipe_t() = default;

io_bufferfill_t::~io_bufferfill_t() = default;

io_buffer_t::~io_buffer_t() {
    assert(!fillthread_ && "io_buffer_t destroyed with outstanding fillthread");
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

void io_chain_t::push_back(io_data_ref_t element) {
    // Ensure we never push back NULL.
    assert(element.get() != nullptr);
    std::vector<io_data_ref_t>::push_back(std::move(element));
}

void io_chain_t::append(const io_chain_t &chain) {
    assert(&chain != this && "Cannot append self to self");
    this->insert(this->end(), chain.begin(), chain.end());
}

#if 0
// This isn't used so the lint tools were complaining about its presence. I'm keeping it in the
// source because it could be useful for debugging.
void io_print(const io_chain_t &chain)
{
    if (chain.empty())
    {
        std::fwprintf(stderr, L"Empty chain %p\n", &chain);
        return;
    }

    std::fwprintf(stderr, L"Chain %p (%ld items):\n", &chain, (long)chain.size());
    for (size_t i=0; i < chain.size(); i++)
    {
        const shared_ptr<io_data_t> &io = chain.at(i);
        if (io.get() == NULL)
        {
            std::fwprintf(stderr, L"\t(null)\n");
        }
        else
        {
            std::fwprintf(stderr, L"\t%lu: fd:%d, ", (unsigned long)i, io->fd);
            io->print();
        }
    }
}
#endif

int move_fd_to_unused(int fd, const io_chain_t &io_chain, bool cloexec) {
    if (fd < 0 || io_chain.io_for_fd(fd).get() == nullptr) {
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

shared_ptr<const io_data_t> io_chain_t::io_for_fd(int fd) const {
    for (auto iter = rbegin(); iter != rend(); ++iter) {
        const auto &data = *iter;
        if (data->fd == fd) {
            return data;
        }
    }
    return nullptr;
}
