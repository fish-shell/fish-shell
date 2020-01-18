// Utilities for io redirection.
#include "config.h"  // IWYU pragma: keep

#include "io.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <cwchar>

#include "common.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "iothread.h"
#include "path.h"
#include "redirection.h"
#include "wutil.h"  // IWYU pragma: keep

/// File redirection error message.
#define FILE_ERROR _(L"An error occurred while redirecting file '%ls'")
#define NOCLOB_ERROR _(L"The file '%ls' already exists")

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

io_data_t::~io_data_t() = default;
io_pipe_t::~io_pipe_t() = default;
io_fd_t::~io_fd_t() = default;
io_close_t::~io_close_t() = default;
io_file_t::~io_file_t() = default;
io_bufferfill_t::~io_bufferfill_t() = default;

void io_close_t::print() const { std::fwprintf(stderr, L"close %d\n", fd); }

void io_fd_t::print() const { std::fwprintf(stderr, L"FD map %d -> %d\n", source_fd, fd); }

void io_file_t::print() const { std::fwprintf(stderr, L"file (%d)\n", file_fd_.fd()); }

void io_pipe_t::print() const {
    std::fwprintf(stderr, L"pipe {%d} (input: %s)\n", source_fd, is_input_ ? "yes" : "no");
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
        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
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
    assert(!fillthread_running() && "Already have a fillthread");

    // We want our background thread to own the fd but it's not easy to move into a std::function.
    // Use a shared_ptr.
    auto fdref = move_to_sharedptr(std::move(fd));

    // Construct a promise that can go into our background thread.
    auto promise = std::make_shared<std::promise<void>>();

    // Get the future associated with our promise.
    // Note this should only ever be called once.
    fillthread_waiter_ = promise->get_future();

    // Run our function to read until the receiver is closed.
    // It's OK to capture 'this' by value because 'this' owns the background thread and waits for it
    // before dtor.
    iothread_perform_cantwait([this, promise, fdref]() {
        this->run_background_fillthread(std::move(*fdref));
        promise->set_value();
    });
}

void io_buffer_t::complete_background_fillthread() {
    ASSERT_IS_MAIN_THREAD();
    assert(fillthread_running() && "Should have a fillthread");
    shutdown_fillthread_ = true;

    // Wait for the fillthread to fulfill its promise, and then clear the future so we know we no
    // longer have one.
    fillthread_waiter_.wait();
    fillthread_waiter_ = {};
}

shared_ptr<io_bufferfill_t> io_bufferfill_t::create(const fd_set_t &conflicts,
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

io_buffer_t::~io_buffer_t() {
    assert(!fillthread_running() && "io_buffer_t destroyed with outstanding fillthread");
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

bool io_chain_t::append_from_specs(const redirection_spec_list_t &specs, const wcstring &pwd) {
    for (const auto &spec : specs) {
        switch (spec.mode) {
            case redirection_mode_t::fd: {
                if (spec.is_close()) {
                    this->push_back(make_unique<io_close_t>(spec.fd));
                } else {
                    auto target_fd = spec.get_target_as_fd();
                    assert(target_fd.has_value() &&
                           "fd redirection should have been validated already");
                    this->push_back(make_unique<io_fd_t>(spec.fd, *target_fd));
                }
                break;
            }
            default: {
                // We have a path-based redireciton. Resolve it to a file.
                // Mark it as CLO_EXEC because we don't want it to be open in any child.
                wcstring path = path_apply_working_directory(spec.target, pwd);
                int oflags = spec.oflags();
                autoclose_fd_t file{wopen_cloexec(path, oflags, OPEN_MASK)};
                if (!file.valid()) {
                    if ((oflags & O_EXCL) && (errno == EEXIST)) {
                        debug(1, NOCLOB_ERROR, spec.target.c_str());
                    } else {
                        debug(1, FILE_ERROR, spec.target.c_str());
                        if (should_debug(1)) wperror(L"open");
                    }
                    return false;
                }
                this->push_back(std::make_shared<io_file_t>(spec.fd, std::move(file)));
                break;
            }
        }
    }
    return true;
}

void io_chain_t::print() const {
    if (this->empty()) {
        std::fwprintf(stderr, L"Empty chain %p\n", this);
        return;
    }

    std::fwprintf(stderr, L"Chain %p (%ld items):\n", this, (long)this->size());
    for (size_t i = 0; i < this->size(); i++) {
        const auto &io = this->at(i);
        if (io == nullptr) {
            std::fwprintf(stderr, L"\t(null)\n");
        } else {
            std::fwprintf(stderr, L"\t%lu: fd:%d, ", (unsigned long)i, io->fd);
            io->print();
        }
    }
}

fd_set_t io_chain_t::fd_set() const {
    fd_set_t result;
    for (const auto &io : *this) {
        result.add(io->fd);
    }
    return result;
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
        debug(1, PIPE_ERROR);
        wperror(L"pipe");
        return none();
    }
    set_cloexec(pipes[0]);
    set_cloexec(pipes[1]);

    auto read = move_fd_to_unused(autoclose_fd_t{pipes[0]}, fdset);
    if (!read.valid()) return none();

    auto write = move_fd_to_unused(autoclose_fd_t{pipes[1]}, fdset);
    if (!write.valid()) return none();

    return autoclose_pipes_t(std::move(read), std::move(write));
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
