#ifndef FISH_HISTORY_FILE_H
#define FISH_HISTORY_FILE_H

#include "config.h"

#include <sys/mman.h>

#include <cassert>
#include <ctime>
#include <memory>

#include "maybe.h"

class history_item_t;

// History file types.
enum history_file_type_t { history_type_fish_2_0, history_type_fish_1_x };

/// history_file_contents_t holds the read-only contents of a file.
class history_file_contents_t {
   public:
    /// Construct a history file contents from a file descriptor. The file descriptor is not closed.
    static std::unique_ptr<history_file_contents_t> create(int fd);

    /// Decode an item at a given offset.
    history_item_t decode_item(size_t offset) const;

    /// Get the file type.
    history_file_type_t type() const { return type_; }

    /// Get the size of the contents.
    size_t length() const { return length_; }

    /// Return a pointer to the beginning.
    const char *begin() const { return address_at(0); }

    /// Return a pointer to one-past-the-end.
    const char *end() const { return address_at(length_); }

    /// Access the address at a given offset.
    const char *address_at(size_t offset) const {
        assert(offset <= length_ && "Invalid offset");
        return start_ + offset;
    }

    ~history_file_contents_t();

   private:
    // The memory mapped pointer.
    const char *start_;

    // The mapped length.
    const size_t length_;

    // The type of the mapped file.
    const history_file_type_t type_;

    // Private constructor; use the static create() function.
    history_file_contents_t(const char *mmap_start, size_t mmap_length, history_file_type_t type);

    /// Support for iterating item offsets.
    /// The cursor should initially be 0.
    /// If cutoff is nonzero, skip items whose timestamp is newer than cutoff.
    /// \return the offset of the next item, or none() on end.
    maybe_t<size_t> offset_of_next_item(size_t *cursor, time_t cutoff) const;

    history_file_contents_t(history_file_contents_t &&) = delete;
    void operator=(history_file_contents_t &&) = delete;

    friend class history_file_reader_t;
};

/// A class which knows how to produce history items from a history file contents.
class history_file_reader_t {
   public:
    /// \return the offset in the file contents of the next history item, or none() if exhausted.
    /// If \p out is set, then return the item as well by reference.
    maybe_t<size_t> next(history_item_t *out);

    /// Construct from the file contents. The contents are held by reference.
    /// If \p cutoff is not zero, then ignore items whose timestamp is larger than cutoff.
    history_file_reader_t(const history_file_contents_t &contents, time_t cutoff);

    ~history_file_reader_t();

   private:
    struct impl_t;
    const history_file_contents_t &contents_;
    time_t cutoff_;
    std::unique_ptr<impl_t> impl_;
};

/// Append a history item to a buffer, in preparation for outputting it to the history file.
void append_history_item_to_buffer(const history_item_t &item, std::string *buffer);

#endif
