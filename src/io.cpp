// Utilities for io redirection.
#include "config.h"  // IWYU pragma: keep

#include "io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cwchar>
#include <functional>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fd_monitor.rs.h"
#include "fds.h"
#include "fds.rs.h"
#include "flog.h"
#include "maybe.h"
#include "path.h"
#include "redirection.h"
#include "threads.rs.h"
#include "wutil.h"  // IWYU pragma: keep

/// File redirection error message.
#define FILE_ERROR _(L"An error occurred while redirecting file '%ls'")
#define NOCLOB_ERROR _(L"The file '%ls' already exists")

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

/// Provide the fd monitor used for background fillthread operations.
static fd_monitor_t &fd_monitor() {
    // Deliberately leaked to avoid shutdown dtors.
    static auto fdm = make_fd_monitor_t();
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

ssize_t io_buffer_t::read_once(int fd, acquired_lock<separated_buffer_t> &buffer) {
    assert(fd >= 0 && "Invalid fd");
    errno = 0;
    char bytes[4096 * 4];

    // We want to swallow EINTR only; in particular EAGAIN needs to be returned back to the caller.
    ssize_t amt;
    do {
        amt = read(fd, bytes, sizeof bytes);
    } while (amt < 0 && errno == EINTR);
    if (amt < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        wperror(L"read");
    } else if (amt > 0) {
        buffer->append(bytes, static_cast<size_t>(amt));
    }
    return amt;
}

struct callback_args_t {
    io_buffer_t *instance;
    std::shared_ptr<std::promise<void>> promise;
};

extern "C" {
static void item_callback_trampoline(autoclose_fd_t2 &fd, item_wake_reason_t reason,
                                     callback_args_t *args) {
    (args->instance)->item_callback(fd, (uint8_t)reason, args);
}
}

void io_buffer_t::begin_filling(autoclose_fd_t fd) {
    assert(!fillthread_running() && "Already have a fillthread");

    // We want to fill buffer_ by reading from fd. fd is the read end of a pipe; the write end is
    // owned by another process, or something else writing in fish.
    // Pass fd to an fd_monitor. It will add fd to its select() loop, and give us a callback when
    // the fd is readable, or when our item is poked. The usual path is that we will get called
    // back, read a bit from the fd, and append it to the buffer. Eventually the write end of the
    // pipe will be closed - probably the other process exited - and fd will be widowed; read() will
    // then return 0 and we will stop reading.
    // In exotic circumstances the write end of the pipe will not be closed; this may happen in
    // e.g.:
    //   cmd ( background & ; echo hi )
    // Here the background process will inherit the write end of the pipe and hold onto it forever.
    // In this case, when complete_background_fillthread() is called, the callback will be invoked
    // with item_wake_reason_t::poke, and we will notice that the shutdown flag is set (this
    // indicates that the command substitution is done); in this case we will read until we get
    // EAGAIN and then give up.

    // Construct a promise. We will fulfill it in our fill thread, and wait for it in
    // complete_background_fillthread(). Note that TSan complains if the promise's dtor races with
    // the future's call to wait(), so we store the promise, not just its future (#7681).
    auto promise = std::make_shared<std::promise<void>>();
    this->fill_waiter_ = promise;

    // Run our function to read until the receiver is closed.
    // It's OK to capture 'this' by value because 'this' waits for the promise in its dtor.
    auto args = new callback_args_t;
    args->instance = this;
    args->promise = std::move(promise);

    item_id_ = fd_monitor().add_item(fd.acquire(), kNoTimeout, (uint8_t *)item_callback_trampoline,
                                     (uint8_t *)args);
}

/// This is a hack to work around the difficulties in passing a capturing lambda across FFI
/// boundaries. A static function that takes a generic/untyped callback parameter is easy to
/// marshall with the basic C ABI.
void io_buffer_t::item_callback(autoclose_fd_t2 &fd, uint8_t r, callback_args_t *args) {
    item_wake_reason_t reason = (item_wake_reason_t)r;
    auto &promise = *args->promise;

    // Only check the shutdown flag if we timed out or were poked.
    // It's important that if select() indicated we were readable, that we call select() again
    // allowing it to time out. Note the typical case is that the fd will be closed, in which
    // case select will return immediately.
    bool done = false;
    if (reason == item_wake_reason_t::Readable) {
        // select() reported us as readable; read a bit.
        auto buffer = buffer_.acquire();
        ssize_t ret = read_once(fd.fd(), buffer);
        done = (ret == 0 || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK));
    } else if (shutdown_fillthread_) {
        // Here our caller asked us to shut down; read while we keep getting data.
        // This will stop when the fd is closed or if we get EAGAIN.
        auto buffer = buffer_.acquire();
        ssize_t ret;
        do {
            ret = read_once(fd.fd(), buffer);
        } while (ret > 0);
        done = true;
    }
    if (done) {
        fd.close();
        promise.set_value();
        // When we close the fd, we signal to the caller that the fd should be removed from its set
        // and that this callback should never be called again.
        // Manual memory management is not nice but this is just during the cpp-to-rust transition.
        delete args;
    }
};

separated_buffer_t io_buffer_t::complete_background_fillthread_and_take_buffer() {
    // Mark that our fillthread is done, then wake it up.
    assert(fillthread_running() && "Should have a fillthread");
    assert(this->item_id_ > 0 && "Should have a valid item ID");
    shutdown_fillthread_ = true;
    fd_monitor().poke_item(this->item_id_);

    // Wait for the fillthread to fulfill its promise, and then clear the future so we know we no
    // longer have one.
    fill_waiter_->get_future().wait();
    fill_waiter_.reset();

    // Return our buffer, transferring ownership.
    auto locked_buff = buffer_.acquire();
    separated_buffer_t result = std::move(*locked_buff);
    locked_buff->clear();
    return result;
}

shared_ptr<io_bufferfill_t> io_bufferfill_t::create(size_t buffer_limit, int target) {
    assert(target >= 0 && "Invalid target fd");

    // Construct our pipes.
    auto pipes = make_autoclose_pipes();
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

separated_buffer_t io_bufferfill_t::finish(std::shared_ptr<io_bufferfill_t> &&filler) {
    // The io filler is passed in. This typically holds the only instance of the write side of the
    // pipe used by the buffer's fillthread (except for that side held by other processes). Get the
    // buffer out of the bufferfill and clear the shared_ptr; this will typically widow the pipe.
    // Then allow the buffer to finish.
    assert(filler && "Null pointer in finish");
    auto buffer = filler->buffer();
    filler.reset();
    return buffer->complete_background_fillthread_and_take_buffer();
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

bool io_chain_t::append(const io_chain_t &chain) {
    assert(&chain != this && "Cannot append self to self");
    this->insert(this->end(), chain.begin(), chain.end());
    return true;
}

bool io_chain_t::append_from_specs(const redirection_spec_list_t &specs, const wcstring &pwd) {
    bool have_error = false;
    for (size_t i = 0; i < specs.size(); i++) {
        const redirection_spec_t *spec = specs.at(i);
        switch (spec->mode()) {
            case redirection_mode_t::fd: {
                if (spec->is_close()) {
                    this->push_back(make_unique<io_close_t>(spec->fd()));
                } else {
                    auto target_fd = spec->get_target_as_fd();
                    assert(target_fd && "fd redirection should have been validated already");
                    this->push_back(make_unique<io_fd_t>(spec->fd(), *target_fd));
                }
                break;
            }
            default: {
                // We have a path-based redireciton. Resolve it to a file.
                // Mark it as CLO_EXEC because we don't want it to be open in any child.
                wcstring path = path_apply_working_directory(*spec->target(), pwd);
                int oflags = spec->oflags();
                autoclose_fd_t file{wopen_cloexec(path, oflags, OPEN_MASK)};
                if (!file.valid()) {
                    if ((oflags & O_EXCL) && (errno == EEXIST)) {
                        FLOGF(warning, NOCLOB_ERROR, spec->target()->c_str());
                    } else {
                        if (should_flog(warning)) {
                            FLOGF(warning, FILE_ERROR, spec->target()->c_str());
                            auto err = errno;
                            // If the error is that the file doesn't exist
                            // or there's a non-directory component,
                            // find the first problematic component for a better message.
                            if (err == ENOENT || err == ENOTDIR) {
                                auto dname = *spec->target();
                                struct stat buf;

                                while (!dname.empty()) {
                                    auto next = wdirname(dname);
                                    if (!wstat(next, &buf)) {
                                        if (!S_ISDIR(buf.st_mode)) {
                                            FLOGF(warning, _(L"Path '%ls' is not a directory"),
                                                  next.c_str());
                                        } else {
                                            FLOGF(warning, _(L"Path '%ls' does not exist"),
                                                  dname.c_str());
                                        }
                                        break;
                                    }
                                    dname = next;
                                }
                            } else {
                                wperror(L"open");
                            }
                        }
                    }
                    // If opening a file fails, insert a closed FD instead of the file redirection
                    // and return false. This lets execution potentially recover and at least gives
                    // the shell a chance to gracefully regain control of the shell (see #7038).
                    this->push_back(make_unique<io_close_t>(spec->fd()));
                    have_error = true;
                    break;
                }
                this->push_back(std::make_shared<io_file_t>(spec->fd(), std::move(file)));
                break;
            }
        }
    }
    return !have_error;
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

shared_ptr<const io_data_t> io_chain_t::io_for_fd(int fd) const {
    for (auto iter = rbegin(); iter != rend(); ++iter) {
        const auto &data = *iter;
        if (data->fd == fd) {
            return data;
        }
    }
    return nullptr;
}

dup2_list_t dup2_list_resolve_chain_shim(const io_chain_t &io_chain) {
    ASSERT_IS_NOT_FORKED_CHILD();
    std::vector<dup2_action_t> chain;
    for (const auto &io_data : io_chain) {
        chain.push_back(dup2_action_t{io_data->source_fd, io_data->fd});
    }
    return dup2_list_resolve_chain(chain);
}

bool output_stream_t::append_narrow_buffer(const separated_buffer_t &buffer) {
    for (const auto &rhs_elem : buffer.elements()) {
        if (!append_with_separation(str2wcstring(rhs_elem.contents), rhs_elem.separation, false)) {
            return false;
        }
    }
    return true;
}

bool output_stream_t::append_with_separation(const wchar_t *s, size_t len, separation_type_t type,
                                             bool want_newline) {
    if (type == separation_type_t::explicitly && want_newline) {
        // Try calling "append" less - it might write() to an fd
        wcstring buf{s, len};
        buf.push_back(L'\n');
        return append(buf);
    } else {
        return append(s, len);
    }
}

const wcstring &output_stream_t::contents() const { return g_empty_string; }

int output_stream_t::flush_and_check_error() { return STATUS_CMD_OK; }

fd_output_stream_t::fd_output_stream_t(int fd) : fd_(fd), sigcheck_(topic_t::sighupint) {
    assert(fd_ >= 0 && "Invalid fd");
}

bool fd_output_stream_t::append(const wchar_t *s, size_t amt) {
    if (errored_) return false;
    int res = wwrite_to_fd(s, amt, this->fd_);
    if (res < 0) {
        // Some of our builtins emit multiple screens worth of data sent to a pager (the primary
        // example being the `history` builtin) and receiving SIGINT should be considered normal and
        // non-exceptional (user request to abort via Ctrl-C), meaning we shouldn't print an error.
        if (errno == EINTR && sigcheck_.check()) {
            // We have two options here: we can either return false without setting errored_ to
            // true (*this* write will be silently aborted but the onus is on the caller to check
            // the return value and skip future calls to `append()`) or we can flag the entire
            // output stream as errored, causing us to both return false and skip any future writes.
            // We're currently going with the latter, especially seeing as no callers currently
            // check the result of `append()` (since it was always a void function before).
        } else if (errno != EPIPE) {
            wperror(L"write");
        }
        errored_ = true;
    }
    return !errored_;
}

int fd_output_stream_t::flush_and_check_error() {
    // Return a generic 1 on any write failure.
    return errored_ ? STATUS_CMD_ERROR : STATUS_CMD_OK;
}

bool null_output_stream_t::append(const wchar_t *, size_t) { return true; }

std::unique_ptr<io_streams_t> make_null_io_streams_ffi() {
    // Temporary test helper.
    static null_output_stream_t *null = new null_output_stream_t();
    return std::make_unique<io_streams_t>(*null, *null);
}

std::unique_ptr<io_streams_t> make_test_io_streams_ffi() {
    // Temporary test helper.
    auto streams = std::make_unique<owning_io_streams_t>();
    streams->stdin_is_directly_redirected = false;  // read from argv instead of stdin
    return streams;
}

wcstring get_test_output_ffi(const io_streams_t &streams) {
    string_output_stream_t *out = static_cast<string_output_stream_t *>(&streams.out);
    if (out == nullptr) {
        return wcstring();
    }
    return out->contents();
}

bool string_output_stream_t::append(const wchar_t *s, size_t amt) {
    contents_.append(s, amt);
    return true;
}

const wcstring &string_output_stream_t::contents() const { return contents_; }

bool buffered_output_stream_t::append(const wchar_t *s, size_t amt) {
    return buffer_->append(wcs2string(s, amt));
}

bool buffered_output_stream_t::append_with_separation(const wchar_t *s, size_t len,
                                                      separation_type_t type, bool want_newline) {
    UNUSED(want_newline);
    return buffer_->append(wcs2string(s, len), type);
}

int buffered_output_stream_t::flush_and_check_error() {
    if (buffer_->discarded()) {
        return STATUS_READ_TOO_MUCH;
    }
    return 0;
}
