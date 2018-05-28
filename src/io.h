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

/// Describes what type of IO operation an io_data_t represents.
enum io_mode_t { IO_FILE, IO_PIPE, IO_FD, IO_BUFFER, IO_CLOSE };

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
    explicit io_close_t(int f) : io_data_t(IO_CLOSE, f) {}

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

    io_fd_t(int f, int old, bool us) : io_data_t(IO_FD, f), old_fd(old), user_supplied(us) {}
};

class io_file_t : public io_data_t {
   public:
    /// Filename, malloc'd. This needs to be used after fork, so don't use wcstring here.
    const char *const filename_cstr;
    /// file creation flags to send to open.
    const int flags;

    void print() const override;

    io_file_t(int f, const wcstring &fname, int fl = 0)
        : io_data_t(IO_FILE, f), filename_cstr(wcs2str(fname)), flags(fl) {}

    ~io_file_t() override { free((void *)filename_cstr); }
};

class io_pipe_t : public io_data_t {
   protected:
    io_pipe_t(io_mode_t m, int f, bool i) : io_data_t(m, f), is_input(i) {
        pipe_fd[0] = pipe_fd[1] = -1;
    }

   public:
    int pipe_fd[2];
    const bool is_input;

    void print() const override;

    io_pipe_t(int f, bool i) : io_data_t(IO_PIPE, f), is_input(i) { pipe_fd[0] = pipe_fd[1] = -1; }
};

class io_chain_t;
class io_buffer_t : public io_pipe_t {
   private:
    /// True if we're discarding input.
    bool discard{false};
    /// Limit on how much data we'll buffer. Zero means no limit.
    size_t buffer_limit;
    /// Buffer to save output in.
    std::vector<char> out_buffer;

    explicit io_buffer_t(int f, size_t limit)
        : io_pipe_t(IO_BUFFER, f, false /* not input */),
          buffer_limit(limit),
          out_buffer() {}

   public:
    void print() const override;

    ~io_buffer_t() override;

    /// Function to append to the buffer.
    void out_buffer_append(const char *ptr, size_t count) {
        if (discard) return;
        if (buffer_limit && out_buffer.size() + count > buffer_limit) {
            discard = true;
            out_buffer.clear();
            return;
        }
        out_buffer.insert(out_buffer.end(), ptr, ptr + count);
    }

    /// Function to get a pointer to the buffer.
    const char *out_buffer_ptr() const { return out_buffer.empty() ? NULL : &out_buffer.at(0); }

    /// Function to get the size of the buffer.
    size_t out_buffer_size() const { return out_buffer.size(); }

    /// Function that returns true if we discarded the input because there was too much data.
    bool output_discarded() { return discard; }

    /// Function to explicitly put the object in discard mode. Meant to be used when moving
    /// the results from an output_stream_t to an io_buffer_t.
    void set_discard() {
        discard = true;
        out_buffer.clear();
    }

    /// This is used to transfer the buffer limit for this object to a output_stream_t object.
    size_t get_buffer_limit() { return buffer_limit; }

    /// Ensures that the pipes do not conflict with any fd redirections in the chain.
    bool avoid_conflicts_with_io_chain(const io_chain_t &ios);

    /// Close output pipe, and read from input pipe until eof.
    void read();

    /// Create a IO_BUFFER type io redirection, complete with a pipe and a vector<char> for output.
    /// The default file descriptor used is STDOUT_FILENO for buffering.
    ///
    /// \param fd the fd that will be mapped in the child process, typically STDOUT_FILENO
    /// \param conflicts A set of IO redirections. The function ensures that any pipe it makes does
    /// not conflict with an fd redirection in this list.
    static shared_ptr<io_buffer_t> create(int fd, const io_chain_t &conflicts,
                                          size_t buffer_limit = 0);
};

class io_chain_t : public std::vector<shared_ptr<io_data_t> > {
   public:
    io_chain_t();
    explicit io_chain_t(const shared_ptr<io_data_t> &);

    void remove(const shared_ptr<const io_data_t> &element);
    void push_back(const shared_ptr<io_data_t> &element);
    void push_front(const shared_ptr<io_data_t> &element);
    void append(const io_chain_t &chain);

    shared_ptr<const io_data_t> get_io_for_fd(int fd) const;
    shared_ptr<io_data_t> get_io_for_fd(int fd);
};

/// Return the last io redirection in the chain for the specified file descriptor.
shared_ptr<const io_data_t> io_chain_get(const io_chain_t &src, int fd);
shared_ptr<io_data_t> io_chain_get(io_chain_t &src, int fd);

/// Given a pair of fds, if an fd is used by the given io chain, duplicate that fd repeatedly until
/// we find one that does not conflict, or we run out of fds. Returns the new fds by reference,
/// closing the old ones. If we get an error, returns false (in which case both fds are closed and
/// set to -1).
bool pipe_avoid_conflicts_with_io_chain(int fds[2], const io_chain_t &ios);

/// Class representing the output that a builtin can generate.
class output_stream_t {
   private:
    /// Limit on how much data we'll buffer. Zero means no limit.
    size_t buffer_limit;
    /// True if we're discarding input.
    bool discard;
    // No copying.
    output_stream_t(const output_stream_t &s);
    void operator=(const output_stream_t &s);

    wcstring buffer_;

    void check_for_overflow() {
        if (buffer_limit && buffer_.size() > buffer_limit) {
            discard = true;
            buffer_.clear();
        }
    }

   public:
    output_stream_t(size_t buffer_limit_) : buffer_limit(buffer_limit_), discard(false) {}

    void append(const wcstring &s) {
        if (discard) return;
        buffer_.append(s);
        check_for_overflow();
    }

    void append(const wchar_t *s) {
        if (discard) return;
        buffer_.append(s);
        check_for_overflow();
    }

    void append(wchar_t s) {
        if (discard) return;
        buffer_.push_back(s);
        check_for_overflow();
    }

    void append(const wchar_t *s, size_t amt) {
        if (discard) return;
        buffer_.append(s, amt);
        check_for_overflow();
    }

    void push_back(wchar_t c) {
        if (discard) return;
        buffer_.push_back(c);
        check_for_overflow();
    }

    void append_format(const wchar_t *format, ...) {
        if (discard) return;
        va_list va;
        va_start(va, format);
        ::append_formatv(buffer_, format, va);
        va_end(va);
        check_for_overflow();
    }

    void append_formatv(const wchar_t *format, va_list va_orig) {
        if (discard) return;
        ::append_formatv(buffer_, format, va_orig);
        check_for_overflow();
    }

    const wcstring &buffer() const { return buffer_; }

    /// Function that returns true if we discarded the input because there was too much data.
    bool output_discarded() { return discard; }

    bool empty() const { return buffer_.empty(); }
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
