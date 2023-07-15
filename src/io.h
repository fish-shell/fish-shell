#ifndef FISH_IO_H
#define FISH_IO_H

#include <stdarg.h>
#include <unistd.h>

#include <cstdint>
#include <cwchar>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "cxx.h"
#include "fds.h"
#include "global_safety.h"
#include "redirection.h"
#include "signals.h"
#include "topic_monitor.h"
#if INCLUDE_RUST_HEADERS
#include "io.rs.h"
#else
struct IoChain;
struct IoStreams;
struct DynOutputStreamFfi;
#endif
using output_stream_t = DynOutputStreamFfi;
using io_streams_t = IoStreams;

// null_output_stream_t

using std::shared_ptr;

struct job_group_t;

/// separated_buffer_t represents a buffer of output from commands, prepared to be turned into a
/// variable. For example, command substitutions output into one of these. Most commands just
/// produce a stream of bytes, and those get stored directly. However other commands produce
/// explicitly separated output, in particular `string` like `string collect` and `string split0`.
/// The buffer tracks a sequence of elements. Some elements are explicitly separated and should not
/// be further split; other elements have inferred separation and may be split by IFS (or not,
/// depending on its value).
enum class separation_type_t {
    inferred,    // this element should be further separated by IFS
    explicitly,  // this element is explicitly separated and should not be further split
};

/// A separated_buffer_t contains a list of elements, some of which may be separated explicitly and
/// others which must be separated further by the user (e.g. via IFS).
class separated_buffer_t : noncopyable_t {
   public:
    struct element_t {
        std::string contents;
        separation_type_t separation;

        element_t(std::string contents, separation_type_t sep)
            : contents(std::move(contents)), separation(sep) {}

        bool is_explicitly_separated() const { return separation == separation_type_t::explicitly; }
    };

    /// We not be copied but may be moved.
    /// Note this leaves the moved-from value in a bogus state until clear() is called on it.
    separated_buffer_t(separated_buffer_t &&) = default;
    separated_buffer_t &operator=(separated_buffer_t &&) = default;

    /// Construct a separated_buffer_t with the given buffer limit \p limit, or 0 for no limit.
    separated_buffer_t(size_t limit) : buffer_limit_(limit) {}

    /// \return the buffer limit size, or 0 for no limit.
    size_t limit() const { return buffer_limit_; }

    /// \return the contents size.
    size_t size() const { return contents_size_; }

    /// \return whether the output has been discarded.
    bool discarded() const { return discard_; }

    /// Serialize the contents to a single string, where explicitly separated elements have a
    /// newline appended.
    std::string newline_serialized() const {
        std::string result;
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

    /// Append a string \p str of a given length \p len, with separation type \p sep.
    bool append(const char *str, size_t len, separation_type_t sep = separation_type_t::inferred) {
        if (!try_add_size(len)) return false;
        // Try merging with the last element.
        if (sep == separation_type_t::inferred && last_inferred()) {
            elements_.back().contents.append(str, len);
        } else {
            elements_.emplace_back(std::string(str, len), sep);
        }
        return true;
    }

    /// Append a string \p str with separation type \p sep.
    bool append(std::string &&str, separation_type_t sep = separation_type_t::inferred) {
        if (!try_add_size(str.size())) return false;
        // Try merging with the last element.
        if (sep == separation_type_t::inferred && last_inferred()) {
            elements_.back().contents.append(str);
        } else {
            elements_.emplace_back(std::move(str), sep);
        }
        return true;
    }

    /// Remove all elements and unset the discard flag.
    void clear() {
        elements_.clear();
        contents_size_ = 0;
        discard_ = false;
    }

   private:
    /// \return true if our last element has an inferred separation type.
    bool last_inferred() const {
        return !elements_.empty() && !elements_.back().is_explicitly_separated();
    }

    /// If our last element has an inferred separation, return a pointer to it; else nullptr.
    /// This is useful for appending one inferred separation to another.
    element_t *last_if_inferred() {
        if (!elements_.empty() && !elements_.back().is_explicitly_separated()) {
            return &elements_.back();
        }
        return nullptr;
    }

    /// Mark that we are about to add the given size \p delta to the buffer. \return true if we
    /// succeed, false if we exceed buffer_limit.
    bool try_add_size(size_t delta) {
        if (discard_) return false;
        size_t proposed_size = contents_size_ + delta;
        if ((proposed_size < delta) || (buffer_limit_ > 0 && proposed_size > buffer_limit_)) {
            clear();
            discard_ = true;
            return false;
        }
        contents_size_ = proposed_size;
        return true;
    }

    /// Limit on how much data we'll buffer. Zero means no limit.
    size_t buffer_limit_;

    /// Current size of all contents.
    size_t contents_size_{0};

    /// List of buffer elements.
    std::vector<element_t> elements_;

    /// True if we're discarding input because our buffer_limit has been exceeded.
    bool discard_{false};
};

/// Describes what type of IO operation an io_data_t represents.
enum class io_mode_t { file, pipe, fd, close, bufferfill };

/// Represents an FD redirection.
class io_data_t : noncopyable_t, nonmovable_t {
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

class io_close_t final : public io_data_t {
   public:
    explicit io_close_t(int f) : io_data_t(io_mode_t::close, f, -1) {}

    void print() const override;
    ~io_close_t() override;
};

class io_fd_t final : public io_data_t {
   public:
    void print() const override;

    ~io_fd_t() override;

    /// fd to redirect specified fd to. For example, in 2>&1, source_fd is 1, and io_data_t::fd
    /// is 2.
    io_fd_t(int f, int source_fd) : io_data_t(io_mode_t::fd, f, source_fd) {}
};

/// Represents a redirection to or from an opened file.
class io_file_t final : public io_data_t {
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
class io_pipe_t final : public io_data_t {
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

/// Represents filling an io_buffer_t. Very similar to io_pipe_t.
class io_bufferfill_t final : public io_data_t {
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
    static shared_ptr<io_bufferfill_t> create(size_t buffer_limit = 0, int target = STDOUT_FILENO);

    /// Reset the receiver (possibly closing the write end of the pipe), and complete the fillthread
    /// of the buffer. \return the buffer.
    static separated_buffer_t finish(std::shared_ptr<io_bufferfill_t> &&filler);
};

struct callback_args_t;
struct autoclose_fd_t2;

using io_data_ref_t = std::shared_ptr<const io_data_t>;

using io_chain_t = IoChain;

dup2_list_t dup2_list_resolve_chain_shim(const io_chain_t &io_chain);

// /// FFI helper.
// struct owning_io_streams_t : io_streams_t {
//     string_output_stream_t out_storage;
//     null_output_stream_t err_storage;
//     owning_io_streams_t() : io_streams_t(out_storage, err_storage) {}
// };

// std::unique_ptr<io_streams_t> make_null_io_streams_ffi();
// std::unique_ptr<io_streams_t> make_test_io_streams_ffi();
// wcstring get_test_output_ffi(const io_streams_t &streams);

#endif
