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
#include "fd_monitor.h"
#include "iothread.h"
#include "path.h"
#include "redirection.h"
#include "wutil.h"  // IWYU pragma: keep

/// File redirection error message.
#define FILE_ERROR _(L"An error occurred while redirecting file '%ls'")
#define NOCLOB_ERROR _(L"The file '%ls' already exists")

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

/// Provide the fd monitor used for background fillthread operations.
static fd_monitor_t &fd_monitor() {
    // Deliberately leaked to avoid shutdown dtors.
    static auto fdm = new fd_monitor_t();
    return *fdm;
}

io_data_t::~io_data_t() = default;
io_pipe_t::~io_pipe_t() = default;
io_fd_t::~io_fd_t() = default;
io_close_t::~io_close_t() = default;
io_file_t::~io_file_t() = default;
io_bufferfill_t::~io_bufferfill_t() = default;

void io_close_t::print() const { std::fwprintf(stderr, L"close %d\n", fd); }

void io_fd_t::print() const { std::fwprintf(stderr, L"FD map %d -> %d\n", source_fd, fd); }

void io_file_t::print() const { std::fwprintf(stderr, L"file %d -> %d\n", file_fd_.fd(), fd); }

void io_pipe_t::print() const {
    std::fwprintf(stderr, L"pipe {%d} (input: %s) -> %d\n", source_fd, is_input_ ? "yes" : "no",
                  fd);
}

void io_bufferfill_t::print() const {
    std::fwprintf(stderr, L"bufferfill %d -> %d\n", write_fd_.fd(), fd);
}

void io_buffer_t::append_from_stream(const output_stream_t &stream) {
    const separated_buffer_t<wcstring> &input = stream.buffer();
    if (input.elements().empty() && !input.discarded()) return;
    scoped_lock locker(append_lock_);
    if (buffer_.discarded()) return;
    if (input.discarded()) {
        buffer_.set_discard();
        return;
    }
    buffer_.append_wide_buffer(input);
}

ssize_t io_buffer_t::read_once(int fd) {
    assert(fd >= 0 && "Invalid fd");
    ASSERT_IS_LOCKED(append_lock_);
    errno = 0;
    char buff[4096 * 4];

    // We want to swallow EINTR only; in particular EAGAIN needs to be returned back to the caller.
    ssize_t ret;
    do {
        ret = read(fd, buff, sizeof buff);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0 && errno != EAGAIN) {
        wperror(L"read");
    } else if (ret > 0) {
        buffer_.append(&buff[0], &buff[ret]);
    }
    return ret;
}

void io_buffer_t::begin_filling(autoclose_fd_t fd) {
    ASSERT_IS_MAIN_THREAD();
    assert(!fillthread_running() && "Already have a fillthread");

    // We want to fill buffer_ by reading from fd. fd is the read end of a pipe; the write end is
    // owned by another process, or something else writing in fish.
    // Pass fd to an fd_monitor. It will add fd to its select() loop, and give us a callback when
    // the fd is readable, or when our timeout is hit. The usual path is that we will get called
    // back, read a bit from the fd, and append it to the buffer. Eventually the write end of the
    // pipe will be closed - probably the other process exited - and fd will be widowed; read() will
    // then return 0 and we will stop reading.
    // In exotic circumstances the write end of the pipe will not be closed; this may happen in
    // e.g.:
    //   cmd ( background & ; echo hi )
    // Here the background process will inherit the write end of the pipe and hold onto it forever.
    // In this case, we will hit the timeout on waiting for more data and notice that the shutdown
    // flag is set (this indicates that the command substitution is done); in this case we will read
    // until we get EAGAIN and then give up.

    // Construct a promise that can go into our background thread.
    auto promise = std::make_shared<std::promise<void>>();

    // Get the future associated with our promise.
    // Note this should only ever be called once.
    fillthread_waiter_ = promise->get_future();

    // 100 msec poll rate. Note that in most cases, the write end of the pipe will be closed so
    // select() will return; the polling is important only for weird cases like a background process
    // launched in a command substitution.
    constexpr uint64_t usec_per_msec = 1000;
    uint64_t poll_usec = 100 * usec_per_msec;

    // Run our function to read until the receiver is closed.
    // It's OK to capture 'this' by value because 'this' waits for the promise in its dtor.
    fd_monitor_item_t item;
    item.fd = std::move(fd);
    item.timeout_usec = poll_usec;
    item.callback = [this, promise](autoclose_fd_t &fd, bool timed_out) {
        ASSERT_IS_BACKGROUND_THREAD();
        // Only check the shutdown flag if we timed out.
        // It's important that if select() indicated we were readable, that we call select() again
        // allowing it to time out. Note the typical case is that the fd will be closed, in which
        // case select will return immediately.
        bool done = false;
        if (!timed_out) {
            // select() reported us as readable; read a bit.
            scoped_lock locker(append_lock_);
            ssize_t ret = read_once(fd.fd());
            done = (ret == 0 || (ret < 0 && errno != EAGAIN));
        } else if (shutdown_fillthread_) {
            // Here our caller asked us to shut down; read while we keep getting data.
            // This will stop when the fd is closed or if we get EAGAIN.
            scoped_lock locker(append_lock_);
            ssize_t ret;
            do {
                ret = read_once(fd.fd());
            } while (ret > 0);
            done = true;
        }
        if (done) {
            fd.close();
            promise->set_value();
        }
    };
    fd_monitor().add(std::move(item));
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

shared_ptr<io_bufferfill_t> io_bufferfill_t::create(const fd_set_t &conflicts, size_t buffer_limit,
                                                    int target) {
    assert(target >= 0 && "Invalid target fd");

    // Construct our pipes.
    auto pipes = make_autoclose_pipes(conflicts);
    if (!pipes) {
        return nullptr;
    }
    // Our buffer will read from the read end of the pipe. This end must be non-blocking. This is
    // because our fillthread needs to poll to decide if it should shut down, and also accept input
    // from direct buffer transfers.
    if (make_fd_nonblocking(pipes->read.fd())) {
        FLOGF(warning, PIPE_ERROR);
        wperror(L"fcntl");
        return nullptr;
    }
    // Our fillthread gets the read end of the pipe; out_pipe gets the write end.
    auto buffer = std::make_shared<io_buffer_t>(buffer_limit);
    buffer->begin_filling(std::move(pipes->read));
    return std::make_shared<io_bufferfill_t>(target, std::move(pipes->write), buffer);
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
    for (auto iter = this->begin(); iter != this->end(); ++iter) {
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
                        FLOGF(warning, NOCLOB_ERROR, spec.target.c_str());
                    } else {
                        FLOGF(warning, FILE_ERROR, spec.target.c_str());
                        if (should_flog(warning)) wperror(L"open");
                    }
                    // If opening a file fails, insert a closed FD instead of the file redirection
                    // and return false. This lets execution potentially recover and at least gives
                    // the shell a chance to gracefully regain control of the shell (see #7038).
                    this->push_back(make_unique<io_close_t>(spec.fd));
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

    std::fwprintf(stderr, L"Chain %p (%ld items):\n", this, static_cast<long>(this->size()));
    for (size_t i = 0; i < this->size(); i++) {
        const auto &io = this->at(i);
        if (io == nullptr) {
            std::fwprintf(stderr, L"\t(null)\n");
        } else {
            std::fwprintf(stderr, L"\t%lu: fd:%d, ", static_cast<unsigned long>(i), io->fd);
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
        FLOGF(warning, PIPE_ERROR);
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

void output_stream_t::append_narrow_buffer(const separated_buffer_t<std::string> &buffer) {
    for (const auto &rhs_elem : buffer.elements()) {
        buffer_.append(str2wcstring(rhs_elem.contents), rhs_elem.separation);
    }
}
