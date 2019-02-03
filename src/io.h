#ifndef FISH_IO_H
#define FISH_IO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include <vector>
// Note that we have to include something to get any _LIBCPP_VERSION defined so we can detect libc++
// So it's key that vector go above. If we didn't need vector for other reasons, we might include
// ciso646, which does nothing
#if defined(_LIBCPP_VERSION) || __cplusplus > 199711L
// C++11 or libc++ (which is a C++11-only library, but the memory header works OK in C++03)
#include <memory>
using std::shared_ptr;
#else
// C++03 or libstdc++
#include <tr1/memory>
using std::tr1::shared_ptr;
#endif

#include "common.h"
#include "env.h"

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

 void reset_discard() {
     discard = false;
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
        if (sep == separation_type_t::inferred && !elements_.empty() && !elements_.back().is_explicitly_separated()) {
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
   private:
    // No assignment or copying allowed.
    io_data_t(const io_data_t &rhs);
    void operator=(const io_data_t &rhs);

   protected:
    io_data_t(io_mode_t m, int f) : io_mode(m), fd(f) {}

   public:
    /// Type of redirect.
    const io_mode_t io_mode;
    /// FD to redirect.
    const int fd;

    virtual void print() const = 0;
    virtual ~io_data_t() = 0;
};

class io_close_t : public io_data_t {
   public:
    explicit io_close_t(int f) : io_data_t(io_mode_t::close, f) {}

    void print() const override;
};

class io_fd_t : public io_data_t {
   public:
    /// fd to redirect specified fd to. For example, in 2>&1, old_fd is 1, and io_data_t::fd is 2.
    const int old_fd;

    /// Whether this redirection was supplied by a script. For example, 'cmd <&3' would have
    /// user_supplied set to true. But a redirection that comes about through transmogrification
    /// would not.
    const bool user_supplied;

    void print() const override;

    io_fd_t(int f, int old, bool us)
        : io_data_t(io_mode_t::fd, f), old_fd(old), user_supplied(us) {}
};

class io_file_t : public io_data_t {
   public:
    /// Filename, malloc'd. This needs to be used after fork, so don't use wcstring here.
    const char *const filename_cstr;
    /// file creation flags to send to open.
    const int flags;

    void print() const override;

    io_file_t(int f, const wcstring &fname, int fl = 0)
        : io_data_t(io_mode_t::file, f), filename_cstr(wcs2str(fname)), flags(fl) {}

    ~io_file_t() override { free((void *)filename_cstr); }
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
        : io_data_t(io_mode_t::pipe, fd), pipe_fd_(std::move(pipe_fd)), is_input_(is_input) {}

    ~io_pipe_t();

    int pipe_fd() const { return pipe_fd_.fd(); }
};

class io_buffer_t;
class io_chain_t;

/// Represents filling an io_buffer_t. Very similar to io_pipe_t.
/// Bufferfills always target stdout.
class io_bufferfill_t : public io_data_t {
    /// Write end. The other end is connected to an io_buffer_t.
    const autoclose_fd_t write_fd_;

    /// The receiving buffer.
    const std::shared_ptr<io_buffer_t> buffer_;

   public:
    void print() const override;

    io_bufferfill_t(autoclose_fd_t write_fd, std::shared_ptr<io_buffer_t> buffer)
        : io_data_t(io_mode_t::bufferfill, STDOUT_FILENO),
          write_fd_(std::move(write_fd)),
          buffer_(std::move(buffer)) {}

    ~io_bufferfill_t();

    std::shared_ptr<io_buffer_t> buffer() const { return buffer_; }

    int write_fd() const { return write_fd_.fd(); }

    /// Create an io_bufferfill_t which, when written from, populates a buffer (also created).
    /// \returns nullptr on failure, e.g. too many open fds.
    ///
    /// \param conflicts A set of IO redirections. The function ensures that any pipe it makes does
    /// not conflict with an fd redirection in this list.
    static shared_ptr<io_bufferfill_t> create(const io_chain_t &conflicts, size_t buffer_limit = 0);
};

class output_stream_t;

/// An io_buffer_t is a buffer which can populate itself by reading from an fd.
/// It is not an io_data_t.
class io_buffer_t {
   private:
    /// fd from which to read.
    autoclose_fd_t read_;

    /// Buffer storing what we have read.
    separated_buffer_t<std::string> buffer_;

    /// Read some. Append it to our buffer.
    /// \return positive if we read, 0 on EOF, -1 on error.
    long read_some();

   public:
    explicit io_buffer_t(autoclose_fd_t read, size_t limit)
        : read_(std::move(read)), buffer_(limit) {
        // Explicitly reset the discard flag because we share this buffer.
        buffer_.reset_discard();
    }

    ~io_buffer_t();

    /// Access the underlying buffer.
    const separated_buffer_t<std::string> &buffer() const { return buffer_; }

    /// Function to append to the buffer.
    void append(const char *ptr, size_t count) { buffer_.append(ptr, ptr + count); }

    /// Read from input pipe until EOF or EAGAIN (i.e. would block).
    void read_to_wouldblock();

    /// Read a bit, if our fd is readable, with the given timeout.
    /// \return true if we read some, false on timeout.
    bool try_read(unsigned long timeout_usec);

    /// Appends data from a given output_stream_t.
    /// Marks the receiver as discarded if the stream was discarded.
    void append_from_stream(const output_stream_t &stream);
};

class io_chain_t : public std::vector<shared_ptr<io_data_t>> {
   public:
    using std::vector<shared_ptr<io_data_t>>::vector;
    // user-declared ctor to allow const init. Do not default this, it will break the build.
    io_chain_t() {}

    void remove(const shared_ptr<const io_data_t> &element);
    void push_back(shared_ptr<io_data_t> element);
    void push_front(shared_ptr<io_data_t> element);
    void append(const io_chain_t &chain);

    shared_ptr<const io_data_t> get_io_for_fd(int fd) const;
    shared_ptr<io_data_t> get_io_for_fd(int fd);
};

/// Return the last io redirection in the chain for the specified file descriptor.
shared_ptr<const io_data_t> io_chain_get(const io_chain_t &src, int fd);
shared_ptr<io_data_t> io_chain_get(io_chain_t &src, int fd);

/// Helper type returned from making autoclose pipes.
struct autoclose_pipes_t {
    /// Read end of the pipe.
    autoclose_fd_t read;

    /// Write end of the pipe.
    autoclose_fd_t write;
};
/// Call pipe(), populating autoclose fds, avoiding conflicts.
/// The pipes are marked CLO_EXEC.
/// \return pipes on success, none() on error.
maybe_t<autoclose_pipes_t> make_autoclose_pipes(const io_chain_t &ios);

/// If the given fd is used by the io chain, duplicates it repeatedly until an fd not used in the io
/// chain is found, or we run out. If we return a new fd or an error, closes the old one.
/// If \p cloexec is set, any fd created is marked close-on-exec.
/// \returns -1 on failure (in which case the given fd is still closed).
int move_fd_to_unused(int fd, const io_chain_t &io_chain, bool cloexec = true);

/// Class representing the output that a builtin can generate.
class output_stream_t {
   private:
    /// Storage for our data.
    separated_buffer_t<wcstring> buffer_;

    // No copying.
    output_stream_t(const output_stream_t &s) = delete;
    void operator=(const output_stream_t &s) = delete;

   public:
    output_stream_t(size_t buffer_limit) : buffer_(buffer_limit) {}

    void append(const wcstring &s) { buffer_.append(s.begin(), s.end()); }

    separated_buffer_t<wcstring> &buffer() { return buffer_; }

    const separated_buffer_t<wcstring> &buffer() const { return buffer_; }

    void append(const wchar_t *s) { append(s, wcslen(s)); }

    void append(wchar_t s) { append(&s, 1); }

    void append(const wchar_t *s, size_t amt) { buffer_.append(s, s + amt); }

    void push_back(wchar_t c) { append(c); }

    void append_format(const wchar_t *format, ...) {
        va_list va;
        va_start(va, format);
        append_formatv(format, va);
        va_end(va);
    }

    void append_formatv(const wchar_t *format, va_list va) { append(vformat_string(format, va)); }

    bool empty() const { return buffer_.size() == 0; }

    wcstring contents() const { return buffer_.newline_serialized(); }
};

struct io_streams_t {
    output_stream_t out;
    output_stream_t err;

    // fd representing stdin. This is not closed by the destructor.
    int stdin_fd;

    // Whether stdin is "directly redirected," meaning it is the recipient of a pipe (foo | cmd) or
    // direct redirection (cmd < foo.txt). An "indirect redirection" would be e.g. begin ; cmd ; end
    // < foo.txt
    bool stdin_is_directly_redirected;

    // Indicates whether stdout and stderr are redirected (e.g. to a file or piped).
    bool out_is_redirected;
    bool err_is_redirected;

    // Actual IO redirections. This is only used by the source builtin. Unowned.
    const io_chain_t *io_chain;

    io_streams_t(size_t read_limit)
        : out(read_limit),
          err(read_limit),
          stdin_fd(-1),
          stdin_is_directly_redirected(false),
          out_is_redirected(false),
          err_is_redirected(false),
          io_chain(NULL) {}
};

#if 0
// Print debug information about the specified IO redirection chain to stderr.
void io_print(const io_chain_t &chain);
#endif

#endif
