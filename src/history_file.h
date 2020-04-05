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

    /// Support for iterating item offsets.
    /// The cursor should initially be 0.
    /// If cutoff is nonzero, skip items whose timestamp is newer than cutoff.
    /// \return the offset of the next item, or none() on end.
    maybe_t<size_t> offset_of_next_item(size_t *cursor, time_t cutoff) const;

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

    history_file_contents_t(history_file_contents_t &&) = delete;
    void operator=(history_file_contents_t &&) = delete;
};

/// Append a history item to a buffer, in preparation for outputting it to the history file.
void append_history_item_to_buffer(const history_item_t &item, std::string *buffer);

#endif
