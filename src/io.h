#ifndef FISH_IO_H
#define FISH_IO_H

#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include "common.h"
#include "env.h"
#include "flog.h"
#include "global_safety.h"
#include "maybe.h"
#include "redirection.h"

using std::shared_ptr;

class job_group_t;

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

/// separated_buffer_t is composed of a sequence of elements, some of which may be explicitly
/// separated (e.g. through string spit0) and some of which the separation is inferred. This enum
/// tracks the type.
enum class separation_type_t {
    /// This element's separation should be inferred, e.g. through IFS.
    inferred,
    /// This element was explicitly separated and should not be separated further.
    explicitly
};

/// A separated_buffer_t contains a list of elements, some of which may be separated explicitly and
/// others which must be separated further by the user (e.g. via IFS).
template <typename StringType>
class separated_buffer_t {
   public:
    struct element_t {
        StringType contents;
        separation_type_t separation;

        element_t(StringType contents, separation_type_t sep)
            : contents(std::move(contents)), separation(sep) {}

        bool is_explicitly_separated() const { return separation == separation_type_t::explicitly; }
    };

   private:
    /// Limit on how much data we'll buffer. Zero means no limit.
    size_t buffer_limit_;

    /// Current size of all contents.
    size_t contents_size_{0};

    /// List of buffer elements.
    std::vector<element_t> elements_;

    /// True if we're discarding input because our buffer_limit has been exceeded.
    bool discard = false;

    /// Mark that we are about to add the given size \p delta to the buffer. \return true if we
    /// succeed, false if we exceed buffer_limit.
    bool try_add_size(size_t delta) {
        if (discard) return false;
        contents_size_ += delta;
        if (contents_size_ < delta) {
            // Overflow!
            set_discard();
            return false;
        }
        if (buffer_limit_ > 0 && contents_size_ > buffer_limit_) {
            set_discard();
            return false;
        }
        return true;
    }

    /// separated_buffer_t may not be copied.
    separated_buffer_t(const separated_buffer_t &) = delete;
    void operator=(const separated_buffer_t &) = delete;

   public:
    /// Construct a separated_buffer_t with the given buffer limit \p limit, or 0 for no limit.
    separated_buffer_t(size_t limit) : buffer_limit_(limit) {}

    /// \return the buffer limit size, or 0 for no limit.
    size_t limit() const { return buffer_limit_; }

    /// \return the contents size.
    size_t size() const { return contents_size_; }

    /// \return whether the output has been discarded.
    bool discarded() const { return discard; }

    /// Mark the contents as discarded.
    void set_discard() {
        elements_.clear();
        contents_size_ = 0;
        discard = true;
    }

    /// Serialize the contents to a single string, where explicitly separated elements have a
    /// newline appended.
    StringType newline_serialized() const {
        StringType result;
        result.reserve(size());
        for (const auto &elem : elements_) {
            result.append(elem.contents);
            if (elem.is_explicitly_separated()) {
                result.push_back('\n');
            }
        }
        return result;
    }

    /// \return the list of elements.
    const std::vector<element_t> &elements() const { return elements_; }

    /// Append an element with range [begin, end) and the given separation type \p sep.
    template <typename Iterator>
    void append(Iterator begin, Iterator end, separation_type_t sep = separation_type_t::inferred) {
        if (!try_add_size(std::distance(begin, end))) return;
        // Try merging with the last element.
        if (sep == separation_type_t::inferred && !elements_.empty() &&
            !elements_.back().is_explicitly_separated()) {
            elements_.back().contents.append(begin, end);
        } else {
            elements_.emplace_back(StringType(begin, end), sep);
        }
    }

    /// Append a string \p str with the given separation type \p sep.
    void append(const StringType &str, separation_type_t sep = separation_type_t::inferred) {
        append(str.begin(), str.end(), sep);
    }

    // Given that this is a narrow stream, convert a wide stream \p rhs to narrow and then append
    // it.
    template <typename RHSStringType>
    void append_wide_buffer(const separated_buffer_t<RHSStringType> &rhs) {
        for (const auto &rhs_elem : rhs.elements()) {
            append(wcs2string(rhs_elem.contents), rhs_elem.separation);
        }
    }
};

/// Describes what type of IO operation an io_data_t represents.
enum class io_mode_t { file, pipe, fd, close, bufferfill };

/// Represents an FD redirection.
class io_data_t {
    // No assignment or copying allowed.
    io_data_t(const io_data_t &rhs) = delete;
    void operator=(const io_data_t &rhs) = delete;

   protected:
    io_data_t(io_mode_t m, int fd, int source_fd) : io_mode(m), fd(fd), source_fd(source_fd) {}

   public:
    /// Type of redirect.
    const io_mode_t io_mode;

    /// FD to redirect.
    const int fd;

    /// Source fd. This is dup2'd to fd, or if it is -1, then fd is closed.
    /// That is, we call dup2(source_fd, fd).
    const int source_fd;

    virtual void print() const = 0;
    virtual ~io_data_t() = 0;
};

class io_close_t : public io_data_t {
   public:
    explicit io_close_t(int f) : io_data_t(io_mode_t::close, f, -1) {}

    void print() const override;
    ~io_close_t() override;
};

class io_fd_t : public io_data_t {
   public:
    void print() const override;

    ~io_fd_t() override;

    /// fd to redirect specified fd to. For example, in 2>&1, source_fd is 1, and io_data_t::fd
    /// is 2.
    io_fd_t(int f, int source_fd) : io_data_t(io_mode_t::fd, f, source_fd) {}
};

/// Represents a redirection to or from an opened file.
class io_file_t : public io_data_t {
   public:
    void print() const override;

    io_file_t(int fd, autoclose_fd_t file)
        : io_data_t(io_mode_t::file, fd, file.fd()), file_fd_(std::move(file)) {
        // Invalid file redirections are replaced with a closed fd, so the following
        // assertion isn't guaranteed to pass:
        // assert(file_fd_.valid() && "File is not valid");
    }

    ~io_file_t() override;

   private:
    // The fd for the file which we are writing to or reading from.
    autoclose_fd_t file_fd_;
};

/// Represents (one end) of a pipe.
class io_pipe_t : public io_data_t {
    // The pipe's fd. Conceptually this is dup2'd to io_data_t::fd.
    autoclose_fd_t pipe_fd_;

    /// Whether this is an input pipe. This is used only for informational purposes.
    const bool is_input_;

   public:
    void print() const override;

    io_pipe_t(int fd, bool is_input, autoclose_fd_t pipe_fd)
        : io_data_t(io_mode_t::pipe, fd, pipe_fd.fd()),
          pipe_fd_(std::move(pipe_fd)),
          is_input_(is_input) {
        assert(pipe_fd_.valid() && "Pipe is not valid");
    }

    ~io_pipe_t() override;
};

class io_buffer_t;
class io_chain_t;

/// Represents filling an io_buffer_t. Very similar to io_pipe_t.
class io_bufferfill_t : public io_data_t {
    /// Write end. The other end is connected to an io_buffer_t.
    const autoclose_fd_t write_fd_;

    /// The receiving buffer.
    const std::shared_ptr<io_buffer_t> buffer_;

   public:
    void print() const override;

    // The ctor is public to support make_shared() in the static create function below.
    // Do not invoke this directly.
    io_bufferfill_t(int target, autoclose_fd_t write_fd, std::shared_ptr<io_buffer_t> buffer)
        : io_data_t(io_mode_t::bufferfill, target, write_fd.fd()),
          write_fd_(std::move(write_fd)),
          buffer_(std::move(buffer)) {
        assert(write_fd_.valid() && "fd is not valid");
    }

    ~io_bufferfill_t() override;

    std::shared_ptr<io_buffer_t> buffer() const { return buffer_; }

    /// Create an io_bufferfill_t which, when written from, fills a buffer with the contents.
    /// \returns nullptr on failure, e.g. too many open fds.
    ///
    /// \param target the fd which this will be dup2'd to - typically stdout.
    /// \param conflicts A set of fds. The function ensures that any pipe it makes does
    /// not conflict with an fd redirection in this list.
    static shared_ptr<io_bufferfill_t> create(const fd_set_t &conflicts, size_t buffer_limit = 0,
                                              int target = STDOUT_FILENO);

    /// Reset the receiver (possibly closing the write end of the pipe), and complete the fillthread
    /// of the buffer. \return the buffer.
    static std::shared_ptr<io_buffer_t> finish(std::shared_ptr<io_bufferfill_t> &&filler);
};

class output_stream_t;

/// An io_buffer_t is a buffer which can populate itself by reading from an fd.
/// It is not an io_data_t.
class io_buffer_t {
   private:
    friend io_bufferfill_t;

    /// Buffer storing what we have read.
    separated_buffer_t<std::string> buffer_;

    /// Atomic flag indicating our fillthread should shut down.
    relaxed_atomic_bool_t shutdown_fillthread_{false};

    /// The future allowing synchronization with the background fillthread, if the fillthread is
    /// running. The fillthread fulfills the corresponding promise when it exits.
    std::future<void> fillthread_waiter_{};

    /// Lock for appending.
    std::mutex append_lock_{};

    /// Read some, filling the buffer. The append lock must be held.
    /// \return positive on success, 0 if closed, -1 on error (in which case errno will be set).
    ssize_t read_once(int fd);

    /// Begin the fill operation, reading from the given fd in the background.
    void begin_filling(autoclose_fd_t readfd);

    /// End the background fillthread operation.
    void complete_background_fillthread();

    /// Helper to return whether the fillthread is running.
    bool fillthread_running() const { return fillthread_waiter_.valid(); }

   public:
    explicit io_buffer_t(size_t limit) : buffer_(limit) {}

    ~io_buffer_t();

    /// Access the underlying buffer.
    /// This requires that the background fillthread be none.
    const separated_buffer_t<std::string> &buffer() const {
        assert(!fillthread_running() && "Cannot access buffer during background fill");
        return buffer_;
    }

    /// Function to append to the buffer.
    void append(const char *ptr, size_t count) {
        scoped_lock locker(append_lock_);
        buffer_.append(ptr, ptr + count);
    }

    /// Appends data from a given separated buffer.
    /// Marks the receiver as discarded if the buffer was discarded.
    void append_from_wide_buffer(const separated_buffer_t<wcstring> &input);
};

using io_data_ref_t = std::shared_ptr<const io_data_t>;

class io_chain_t : public std::vector<io_data_ref_t> {
   public:
    using std::vector<io_data_ref_t>::vector;
    // user-declared ctor to allow const init. Do not default this, it will break the build.
    io_chain_t() {}

    void remove(const io_data_ref_t &element);
    void push_back(io_data_ref_t element);
    void append(const io_chain_t &chain);

    /// \return the last io redirection in the chain for the specified file descriptor, or nullptr
    /// if none.
    io_data_ref_t io_for_fd(int fd) const;

    /// Attempt to resolve a list of redirection specs to IOs, appending to 'this'.
    /// \return true on success, false on error, in which case an error will have been printed.
    bool append_from_specs(const redirection_spec_list_t &specs, const wcstring &pwd);

    /// Output debugging information to stderr.
    void print() const;

    /// \return the set of redirected FDs.
    fd_set_t fd_set() const;
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
/// Call pipe(), populating autoclose fds, avoiding conflicts.
/// The pipes are marked CLO_EXEC.
/// \return pipes on success, none() on error.
maybe_t<autoclose_pipes_t> make_autoclose_pipes(const fd_set_t &fdset);

/// If the given fd is present in \p fdset, duplicates it repeatedly until an fd not used in the set
/// is found or we run out. If we return a new fd or an error, closes the old one. Marks the fd as
/// cloexec. \returns invalid fd on failure (in which case the given fd is still closed).
autoclose_fd_t move_fd_to_unused(autoclose_fd_t fd, const fd_set_t &fdset);

/// Base class representing the output that a builtin can generate.
/// This has various subclasses depending on the ultimate output destination.
class output_stream_t {
   public:
    /// Required override point. The output stream receives a string \p s with \p amt chars.
    virtual void append(const wchar_t *s, size_t amt) = 0;

    /// \return the separated buffer if this holds one, otherwise nullptr.
    virtual const separated_buffer_t<wcstring> *get_separated_buffer() const { return nullptr; }

    /// An optional override point. This is for explicit separation.
    virtual void append_with_separation(const wchar_t *s, size_t len, separation_type_t type);

    /// The following are all convenience overrides.
    void append_with_separation(const wcstring &s, separation_type_t type) {
        append_with_separation(s.data(), s.size(), type);
    }

    /// Append a string.
    void append(const wcstring &s) { append(s.data(), s.size()); }
    void append(const wchar_t *s) { append(s, std::wcslen(s)); }

    /// Append a char.
    void append(wchar_t s) { append(&s, 1); }
    void push_back(wchar_t c) { append(c); }

    // Append data from a narrow buffer, widening it.
    void append_narrow_buffer(const separated_buffer_t<std::string> &buffer);

    /// Append a format string.
    void append_format(const wchar_t *format, ...) {
        va_list va;
        va_start(va, format);
        append_formatv(format, va);
        va_end(va);
    }

    void append_formatv(const wchar_t *format, va_list va) { append(vformat_string(format, va)); }

    // No copying.
    output_stream_t(const output_stream_t &s) = delete;
    void operator=(const output_stream_t &s) = delete;

    output_stream_t() = default;
    virtual ~output_stream_t() = default;
};

/// A null output stream which ignores all writes.
class null_output_stream_t final : public output_stream_t {
    virtual void append(const wchar_t *s, size_t amt) override;
};

/// An output stream for builtins which outputs to an fd.
/// Note the fd may be something like stdout; there is no ownership implied here.
class fd_output_stream_t final : public output_stream_t {
   public:
    /// Construct from a file descriptor, which must be nonegative.
    explicit fd_output_stream_t(int fd) : fd_(fd) { assert(fd_ >= 0 && "Invalid fd"); }

    void append(const wchar_t *s, size_t amt) override;

   private:
    /// The file descriptor to write to.
    const int fd_;

    /// Whether we have received an error.
    bool errored_{false};
};

/// An output stream for builtins which buffers into a separated buffer.
class buffered_output_stream_t final : public output_stream_t {
   public:
    explicit buffered_output_stream_t(size_t buffer_limit) : buffer_(buffer_limit) {}

    void append(const wchar_t *s, size_t amt) override;
    void append_with_separation(const wchar_t *s, size_t len, separation_type_t type) override;

    wcstring contents() const { return buffer_.newline_serialized(); }

    /// Access the buffer.
    separated_buffer_t<wcstring> &buffer() { return buffer_; }
    const separated_buffer_t<wcstring> &buffer() const { return buffer_; }

    const separated_buffer_t<wcstring> *get_separated_buffer() const override { return &buffer_; }

   private:
    /// Storage for our data.
    separated_buffer_t<wcstring> buffer_;
};

struct io_streams_t {
    // Streams for out and err.
    output_stream_t &out;
    output_stream_t &err;

    // fd representing stdin. This is not closed by the destructor.
    int stdin_fd{-1};

    // Whether stdin is "directly redirected," meaning it is the recipient of a pipe (foo | cmd) or
    // direct redirection (cmd < foo.txt). An "indirect redirection" would be e.g. begin ; cmd ; end
    // < foo.txt
    bool stdin_is_directly_redirected{false};

    // Indicates whether stdout and stderr are specifically piped.
    // If this is set, then the is_redirected flags must also be set.
    bool out_is_piped{false};
    bool err_is_piped{false};

    // Indicates whether stdout and stderr are at all redirected (e.g. to a file or piped).
    bool out_is_redirected{false};
    bool err_is_redirected{false};

    // Actual IO redirections. This is only used by the source builtin. Unowned.
    const io_chain_t *io_chain{nullptr};

    // The job group of the job, if any. This enables builtins which run more code like eval() to
    // share pgid.
    // FIXME: this is awkwardly placed.
    std::shared_ptr<job_group_t> job_group{};

    // io_streams_t cannot be copied.
    io_streams_t(const io_streams_t &) = delete;
    void operator=(const io_streams_t &) = delete;

    io_streams_t(output_stream_t &out, output_stream_t &err) : out(out), err(err) {}
};

#endif
