// History functions, part of the user interface.
#include "config.h"  // IWYU pragma: keep

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
// We need the sys/file.h for the flock() declaration on Linux but not OS X.
#include <sys/file.h>  // IWYU pragma: keep
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <cwchar>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <type_traits>
#include <unordered_set>

#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "io.h"
#include "iothread.h"
#include "lru.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "path.h"
#include "reader.h"
#include "tnode.h"
#include "wcstringutil.h"
#include "wildcard.h"  // IWYU pragma: keep
#include "wutil.h"     // IWYU pragma: keep

// Our history format is intended to be valid YAML. Here it is:
//
//   - cmd: ssh blah blah blah
//     when: 2348237
//     paths:
//       - /path/to/something
//       - /path/to/something_else
//
//   Newlines are replaced by \n. Backslashes are replaced by \\.

// This is the history session ID we use by default if the user has not set env var fish_history.
#define DFLT_FISH_HISTORY_SESSION_ID L"fish"

// When we rewrite the history, the number of items we keep.
#define HISTORY_SAVE_MAX (1024 * 256)

// Default buffer size for flushing to the history file.
#define HISTORY_OUTPUT_BUFFER_SIZE (64 * 1024)

// The file access mode we use for creating history files
static constexpr int history_file_mode = 0644;

// How many times we retry to save
// Saving may fail if the file is modified in between our opening
// the file and taking the lock
static constexpr int max_save_tries = 1024;

namespace {

/// Helper class for certain output. This is basically a string that allows us to ensure we only
/// flush at record boundaries, and avoids the copying of ostringstream. Have you ever tried to
/// implement your own streambuf? Total insanity.
static size_t safe_strlen(const char *s) { return s ? strlen(s) : 0; }
class history_output_buffer_t {
    std::vector<char> buffer;

   public:
    /// Add a bit more to HISTORY_OUTPUT_BUFFER_SIZE because we flush once we've exceeded that size.
    explicit history_output_buffer_t(size_t reserve_amt = HISTORY_OUTPUT_BUFFER_SIZE + 128) {
        buffer.reserve(reserve_amt);
    }

    /// Append one or more strings.
    void append(const char *s1, const char *s2 = NULL, const char *s3 = NULL) {
        constexpr size_t ptr_count = 3;
        const char *ptrs[ptr_count] = {s1, s2, s3};
        size_t lengths[ptr_count] = {safe_strlen(s1), safe_strlen(s2), safe_strlen(s3)};

        // Determine the additional size we'll need.
        size_t additional_length = std::accumulate(std::begin(lengths), std::end(lengths), 0);
        buffer.reserve(buffer.size() + additional_length);

        // Append
        for (size_t i = 0; i < ptr_count; i++) {
            if (lengths[i] > 0) {
                buffer.insert(buffer.end(), ptrs[i], ptrs[i] + lengths[i]);
            }
        }
    }

    /// Output to a given fd, resetting our buffer. Returns true on success, false on error.
    bool flush_to_fd(int fd) {
        if (buffer.empty()) {
            return true;
        }
        bool result = write_loop(fd, &buffer.at(0), buffer.size()) >= 0;
        buffer.clear();
        return result;
    }

    /// Return how much data we've accumulated.
    size_t output_size() const { return buffer.size(); }
};

class time_profiler_t {
    const char *what;
    double start;

   public:
    explicit time_profiler_t(const char *w) {
        what = w;
        start = timef();
    }

    ~time_profiler_t() {
        double end = timef();
        debug(2, "%s: %.0f ms", what, (end - start) * 1000);
    }
};

/// Lock the history file.
/// Returns true on success, false on failure.
static bool history_file_lock(int fd, int lock_type) {
    static std::atomic<bool> do_locking(true);
    if (!do_locking) return false;

    double start_time = timef();
    int retval = flock(fd, lock_type);
    double duration = timef() - start_time;
    if (duration > 0.25) {
        debug(1, _(L"Locking the history file took too long (%.3f seconds)."), duration);
        // we've decided to stop doing any locking behavior
        // but make sure we don't leave the file locked!
        if (retval == 0 && lock_type != LOCK_UN) {
            flock(fd, LOCK_UN);
        }
        do_locking = false;
        return false;
    }
    return retval != -1;
}

// History file types.
enum history_file_type_t { history_type_fish_2_0, history_type_fish_1_x };

/// Try to infer the history file type based on inspecting the data.
static maybe_t<history_file_type_t> infer_file_type(const void *data, size_t len) {
    maybe_t<history_file_type_t> result{};
    if (len > 0) {  // old fish started with a #
        if (static_cast<const char *>(data)[0] == '#') {
            result = history_type_fish_1_x;
        } else {  // assume new fish
            result = history_type_fish_2_0;
        }
    }
    return result;
}

/// Our LRU cache is used for restricting the amount of history we have, and limiting how long we
/// order it.
class history_lru_item_t {
   public:
    wcstring text;
    time_t timestamp;
    path_list_t required_paths;
    explicit history_lru_item_t(const history_item_t &item)
        : text(item.str()),
          timestamp(item.timestamp()),
          required_paths(item.get_required_paths()) {}
};

class history_lru_cache_t : public lru_cache_t<history_lru_cache_t, history_lru_item_t> {
    typedef lru_cache_t<history_lru_cache_t, history_lru_item_t> super;

   public:
    using super::super;

    /// Function to add a history item.
    void add_item(const history_item_t &item) {
        // Skip empty items.
        if (item.empty()) return;

        // See if it's in the cache. If it is, update the timestamp. If not, we create a new node
        // and add it. Note that calling get_node promotes the node to the front.
        wcstring key = item.str();
        history_lru_item_t *node = this->get(key);
        if (node == NULL) {
            this->insert(std::move(key), history_lru_item_t(item));
        } else {
            node->timestamp = std::max(node->timestamp, item.timestamp());
            // What to do about paths here? Let's just ignore them.
        }
    }
};

// The set of histories
// Note that histories are currently immortal
class history_collection_t {
    owning_lock<std::map<wcstring, std::unique_ptr<history_t>>> histories;

   public:
    history_t &get_creating(const wcstring &name);
    void save();
};

}  // anonymous namespace

// history_file_contents_t holds the read-only contents of a file.
class history_file_contents_t {
    // The memory mapped pointer.
    void *start_;

    // The mapped length.
    size_t length_;

    // The type of the mapped file.
    history_file_type_t type_;

    // Private constructor; use the static create() function.
    history_file_contents_t(void *mmap_start, size_t mmap_length, history_file_type_t type)
        : start_(mmap_start), length_(mmap_length), type_(type) {
        assert(mmap_start != MAP_FAILED && "Invalid mmap address");
    }

    history_file_contents_t(history_file_contents_t &&) = delete;
    void operator=(history_file_contents_t &&) = delete;

    // Check if we should mmap the fd.
    // Don't try mmap() on non-local filesystems.
    static bool should_mmap(int fd) {
        if (history_t::never_mmap) return false;

        // mmap only if we are known not-remote (return is 0).
        int ret = fd_check_is_remote(fd);
        return ret == 0;
    }

    // Read up to len bytes from fd into address, zeroing the rest.
    // Return true on success, false on failure.
    static bool read_from_fd(int fd, void *address, size_t len) {
        size_t remaining = len;
        char *ptr = static_cast<char *>(address);
        while (remaining > 0) {
            ssize_t amt = read(fd, ptr, remaining);
            if (amt < 0) {
                if (errno != EINTR) {
                    return false;
                }
            } else if (amt == 0) {
                break;
            } else {
                remaining -= amt;
                ptr += amt;
            }
        }
        bzero(ptr, remaining);
        return true;
    }

   public:
    // Access the address at a given offset.
    const char *address_at(size_t offset) const {
        assert(offset <= length_ && "Invalid offset");
        auto base = static_cast<const char *>(start_);
        return base + offset;
    }

    // Return a pointer to the beginning.
    const char *begin() const { return address_at(0); }

    // Return a pointer to one-past-the-end.
    const char *end() const { return address_at(length_); }

    // Get the size of the contents.
    size_t length() const { return length_; }

    // Get the file type.
    history_file_type_t type() const { return type_; }

    ~history_file_contents_t() { munmap(start_, length_); }

    // Construct a history file contents from a file descriptor. The file descriptor is not closed.
    static std::unique_ptr<history_file_contents_t> create(int fd) {
        // Check that the file is seekable, and its size.
        off_t len = lseek(fd, 0, SEEK_END);
        if (len <= 0 || static_cast<unsigned long>(len) >= SIZE_MAX) return nullptr;
        if (lseek(fd, 0, SEEK_SET) != 0) return nullptr;

        // Read the file, possibly ussing mmap.
        void *mmap_start = nullptr;
        if (should_mmap(fd)) {
            // We feel confident to map the file directly. Note this is still risky: if another
            // process truncates the file we risk SIGBUS.
            mmap_start = mmap(0, size_t(len), PROT_READ, MAP_PRIVATE, fd, 0);
            if (mmap_start == MAP_FAILED) return nullptr;
        } else {
            // We don't want to map the file. mmap some private memory and then read into it. We use
            // mmap instead of malloc so that the destructor can always munmap().
            mmap_start =
                mmap(0, size_t(len), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mmap_start == MAP_FAILED) return nullptr;
            if (!read_from_fd(fd, mmap_start, len)) return nullptr;
        }

        // Check the file type.
        auto mtype = infer_file_type(mmap_start, len);
        if (!mtype) return nullptr;

        return std::unique_ptr<history_file_contents_t>(
            new history_file_contents_t(mmap_start, len, *mtype));
    }
};

static history_collection_t histories;

static wcstring history_filename(const wcstring &name, const wcstring &suffix);

/// Replaces newlines with a literal backslash followed by an n, and replaces backslashes with two
/// backslashes.
static void escape_yaml(std::string *str);

/// Inverse of escape_yaml.
static void unescape_yaml(std::string *str);

/// Read one line, stripping off any newline, and updating cursor. Note that our input string is NOT
/// null terminated; it's just a memory mapped file.
static size_t read_line(const char *base, size_t cursor, size_t len, std::string &result) {
    // Locate the newline.
    assert(cursor <= len);
    const char *start = base + cursor;
    const char *a_newline = (char *)memchr(start, '\n', len - cursor);
    if (a_newline != NULL) {  // we found a newline
        result.assign(start, a_newline - start);
        // Return the amount to advance the cursor; skip over the newline.
        return a_newline - start + 1;
    }

    // We ran off the end.
    result.clear();
    return len - cursor;
}

/// Trims leading spaces in the given string, returning how many there were.
static size_t trim_leading_spaces(std::string &str) {
    size_t i = 0, max = str.size();
    while (i < max && str[i] == ' ') i++;
    str.erase(0, i);
    return i;
}

static bool extract_prefix_and_unescape_yaml(std::string *key, std::string *value,
                                             const std::string &line) {
    size_t where = line.find(":");
    if (where != std::string::npos) {
        key->assign(line, 0, where);

        // Skip a space after the : if necessary.
        size_t val_start = where + 1;
        if (val_start < line.size() && line.at(val_start) == ' ') val_start++;
        value->assign(line, val_start, line.size() - val_start);

        unescape_yaml(key);
        unescape_yaml(value);
    }
    return where != std::string::npos;
}

/// Remove backslashes from all newlines. This makes a string from the history file better formated
/// for on screen display.
static wcstring history_unescape_newlines_fish_1_x(const wcstring &in_str) {
    wcstring out;
    for (const wchar_t *in = in_str.c_str(); *in; in++) {
        if (*in == L'\\') {
            if (*(in + 1) != L'\n') {
                out.push_back(*in);
            }
        } else {
            out.push_back(*in);
        }
    }
    return out;
}

/// Decode an item via the fish 1.x format. Adapted from fish 1.x's item_get().
static history_item_t decode_item_fish_1_x(const char *begin, size_t length) {
    const char *end = begin + length;
    const char *pos = begin;
    wcstring out;
    bool was_backslash = false;
    bool first_char = true;
    bool timestamp_mode = false;
    time_t timestamp = 0;

    while (1) {
        wchar_t c;
        size_t res;
        mbstate_t state = {};

        if (MB_CUR_MAX == 1) {  // single-byte locale
            c = (unsigned char)*pos;
            res = 1;
        } else {
            res = mbrtowc(&c, pos, end - pos, &state);
        }

        if (res == (size_t)-1) {
            pos++;
            continue;
        } else if (res == (size_t)-2) {
            break;
        } else if (res == (size_t)0) {
            pos++;
            continue;
        }
        pos += res;

        if (c == L'\n') {
            if (timestamp_mode) {
                const wchar_t *time_string = out.c_str();
                while (*time_string && !iswdigit(*time_string)) time_string++;

                if (*time_string) {
                    time_t tm = (time_t)fish_wcstol(time_string);
                    if (!errno && tm >= 0) {
                        timestamp = tm;
                    }
                }

                out.clear();
                timestamp_mode = false;
                continue;
            }
            if (!was_backslash) break;
        }

        if (first_char) {
            first_char = false;
            if (c == L'#') timestamp_mode = true;
        }

        out.push_back(c);
        was_backslash = (c == L'\\') && !was_backslash;
    }

    out = history_unescape_newlines_fish_1_x(out);
    return history_item_t(out, timestamp);
}

/// Decode an item via the fish 2.0 format.
static history_item_t decode_item_fish_2_0(const char *base, size_t len) {
    wcstring cmd;
    time_t when = 0;
    path_list_t paths;

    size_t indent = 0, cursor = 0;
    std::string key, value, line;

    // Read the "- cmd:" line.
    size_t advance = read_line(base, cursor, len, line);
    trim_leading_spaces(line);
    if (!extract_prefix_and_unescape_yaml(&key, &value, line) || key != "- cmd") {
        goto done;  //!OCLINT(goto is the cleanest way to handle bad input)
    }

    cursor += advance;
    cmd = str2wcstring(value);

    // Read the remaining lines.
    for (;;) {
        size_t advance = read_line(base, cursor, len, line);

        size_t this_indent = trim_leading_spaces(line);
        if (indent == 0) indent = this_indent;

        if (this_indent == 0 || indent != this_indent) break;

        if (!extract_prefix_and_unescape_yaml(&key, &value, line)) break;

        // We are definitely going to consume this line.
        cursor += advance;

        if (key == "when") {
            // Parse an int from the timestamp. Should this fail, strtol returns 0; that's
            // acceptable.
            char *end = NULL;
            long tmp = strtol(value.c_str(), &end, 0);
            when = tmp;
        } else if (key == "paths") {
            // Read lines starting with " - " until we can't read any more.
            for (;;) {
                size_t advance = read_line(base, cursor, len, line);
                if (trim_leading_spaces(line) <= indent) break;

                if (strncmp(line.c_str(), "- ", 2)) break;

                // We're going to consume this line.
                cursor += advance;

                // Skip the leading dash-space and then store this path it.
                line.erase(0, 2);
                unescape_yaml(&line);
                paths.push_back(str2wcstring(line));
            }
        }
    }

done:
    history_item_t result(cmd, when);
    result.set_required_paths(paths);
    return result;
}

static history_item_t decode_item(const history_file_contents_t &contents, size_t offset) {
    const char *base = contents.address_at(offset);
    size_t len = contents.length() - offset;
    switch (contents.type()) {
        case history_type_fish_2_0:
            return decode_item_fish_2_0(base, len);
        case history_type_fish_1_x:
            return decode_item_fish_1_x(base, len);
    }
    return history_item_t(L"");
}

/// We can merge two items if they are the same command. We use the more recent timestamp, more
/// recent identifier, and the longer list of required paths.
bool history_item_t::merge(const history_item_t &item) {
    bool result = false;
    if (this->contents == item.contents) {
        this->creation_timestamp = std::max(this->creation_timestamp, item.creation_timestamp);
        if (this->required_paths.size() < item.required_paths.size()) {
            this->required_paths = item.required_paths;
        }
        if (this->identifier < item.identifier) {
            this->identifier = item.identifier;
        }
        result = true;
    }
    return result;
}

history_item_t::history_item_t(const wcstring &str, time_t when, history_identifier_t ident)
    : creation_timestamp(when), identifier(ident) {

    contents = trim(str);
    contents_lower.reserve(contents.size());
    for (const auto &c : contents) {
        contents_lower.push_back(towlower(c));
    }
}

bool history_item_t::matches_search(const wcstring &term, enum history_search_type_t type,
                                    bool case_sensitive) const {
    // Note that 'term' has already been lowercased when constructing the
    // search object if we're doing a case insensitive search.
    const wcstring &content_to_match = case_sensitive ? contents : contents_lower;

    switch (type) {
        case HISTORY_SEARCH_TYPE_EXACT: {
            return term == content_to_match;
        }
        case HISTORY_SEARCH_TYPE_CONTAINS: {
            return content_to_match.find(term) != wcstring::npos;
        }
        case HISTORY_SEARCH_TYPE_PREFIX: {
            return string_prefixes_string(term, content_to_match);
        }
        case HISTORY_SEARCH_TYPE_CONTAINS_GLOB: {
            wcstring wcpattern1 = parse_util_unescape_wildcards(term);
            if (wcpattern1.front() != ANY_STRING) wcpattern1.insert(0, 1, ANY_STRING);
            if (wcpattern1.back() != ANY_STRING) wcpattern1.push_back(ANY_STRING);
            return wildcard_match(content_to_match, wcpattern1);
        }
        case HISTORY_SEARCH_TYPE_PREFIX_GLOB: {
            wcstring wcpattern2 = parse_util_unescape_wildcards(term);
            if (wcpattern2.back() != ANY_STRING) wcpattern2.push_back(ANY_STRING);
            return wildcard_match(content_to_match, wcpattern2);
        }
    }
    DIE("unexpected history_search_type_t value");
}

/// Append our YAML history format to the provided vector at the given offset, updating the offset.
static void append_yaml_to_buffer(const wcstring &wcmd, time_t timestamp,
                                  const path_list_t &required_paths,
                                  history_output_buffer_t *buffer) {
    std::string cmd = wcs2string(wcmd);
    escape_yaml(&cmd);
    buffer->append("- cmd: ", cmd.c_str(), "\n");

    char timestamp_str[96];
    snprintf(timestamp_str, sizeof timestamp_str, "%ld", (long)timestamp);
    buffer->append("  when: ", timestamp_str, "\n");

    if (!required_paths.empty()) {
        buffer->append("  paths:\n");

        for (path_list_t::const_iterator iter = required_paths.begin();
             iter != required_paths.end(); ++iter) {
            std::string path = wcs2string(*iter);
            escape_yaml(&path);
            buffer->append("    - ", path.c_str(), "\n");
        }
    }
}

/// Parse a timestamp line that looks like this: spaces, "when:", spaces, timestamp, newline
/// The string is NOT null terminated; however we do know it contains a newline, so stop when we
/// reach it.
static bool parse_timestamp(const char *str, time_t *out_when) {
    const char *cursor = str;
    // Advance past spaces.
    while (*cursor == ' ') cursor++;

    // Look for "when:".
    size_t when_len = 5;
    if (strncmp(cursor, "when:", when_len) != 0) return false;
    cursor += when_len;

    // Advance past spaces.
    while (*cursor == ' ') cursor++;

    // Try to parse a timestamp.
    long timestamp = 0;
    if (isdigit(*cursor) && (timestamp = strtol(cursor, NULL, 0)) > 0) {
        *out_when = (time_t)timestamp;
        return true;
    }
    return false;
}

/// Returns a pointer to the start of the next line, or NULL. The next line must itself end with a
/// newline. Note that the string is not null terminated.
static const char *next_line(const char *start, size_t length) {
    // Handle the hopeless case.
    if (length < 1) return NULL;

    // Get a pointer to the end, that we must not pass.
    const char *const end = start + length;

    // Skip past the next newline.
    const char *nextline = (const char *)memchr(start, '\n', length);
    if (!nextline || nextline >= end) {
        return NULL;
    }
    // Skip past the newline character itself.
    if (++nextline >= end) {
        return NULL;
    }

    // Make sure this new line is itself "newline terminated". If it's not, return NULL.
    const char *next_newline = (const char *)memchr(nextline, '\n', end - nextline);
    if (!next_newline) {
        return NULL;
    }

    return nextline;
}

/// Support for iteratively locating the offsets of history items.
/// Pass the file contents and a pointer to a cursor size_t, initially 0.
/// If custoff_timestamp is nonzero, skip items created at or after that timestamp.
/// Returns (size_t)-1 when done.
static size_t offset_of_next_item_fish_2_0(const history_file_contents_t &contents,
                                           size_t *inout_cursor, time_t cutoff_timestamp) {
    size_t cursor = *inout_cursor;
    size_t result = size_t(-1);
    const size_t length = contents.length();
    const char *const begin = contents.begin();
    const char *const end = contents.end();
    while (cursor < length) {
        const char *line_start = contents.address_at(cursor);

        // Advance the cursor to the next line.
        const char *a_newline = (const char *)memchr(line_start, '\n', length - cursor);
        if (a_newline == NULL) break;

        // Advance the cursor past this line. +1 is for the newline.
        cursor = a_newline - begin + 1;

        // Skip lines with a leading space, since these are in the interior of one of our items.
        if (line_start[0] == ' ') continue;

        // Skip very short lines to make one of the checks below easier.
        if (a_newline - line_start < 3) continue;

        // Try to be a little YAML compatible. Skip lines with leading %, ---, or ...
        if (!memcmp(line_start, "%", 1) || !memcmp(line_start, "---", 3) ||
            !memcmp(line_start, "...", 3))
            continue;

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce lines with lots of
        // leading "- cmd: - cmd: - cmd:". Trim all but one leading "- cmd:".
        const char *double_cmd = "- cmd: - cmd: ";
        const size_t double_cmd_len = strlen(double_cmd);
        while ((size_t)(a_newline - line_start) > double_cmd_len &&
               !memcmp(line_start, double_cmd, double_cmd_len)) {
            // Skip over just one of the - cmd. In the end there will be just one left.
            line_start += strlen("- cmd: ");
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce commands like "when:
        // 123456". Ignore those.
        const char *cmd_when = "- cmd:    when:";
        const size_t cmd_when_len = strlen(cmd_when);
        if ((size_t)(a_newline - line_start) >= cmd_when_len &&
            !memcmp(line_start, cmd_when, cmd_when_len)) {
            continue;
        }

        // At this point, we know line_start is at the beginning of an item. But maybe we want to
        // skip this item because of timestamps. A 0 cutoff means we don't care; if we do care, then
        // try parsing out a timestamp.
        if (cutoff_timestamp != 0) {
            // Hackish fast way to skip items created after our timestamp. This is the mechanism by
            // which we avoid "seeing" commands from other sessions that started after we started.
            // We try hard to ensure that our items are sorted by their timestamps, so in theory we
            // could just break, but I don't think that works well if (for example) the clock
            // changes. So we'll read all subsequent items.
            // Walk over lines that we think are interior. These lines are not null terminated, but
            // are guaranteed to contain a newline.
            bool has_timestamp = false;
            time_t timestamp = 0;
            const char *interior_line;

            for (interior_line = next_line(line_start, end - line_start);
                 interior_line != NULL && !has_timestamp;
                 interior_line = next_line(interior_line, end - interior_line)) {
                // If the first character is not a space, it's not an interior line, so we're done.
                if (interior_line[0] != ' ') break;

                // Hackish optimization: since we just stepped over some interior line, update the
                // cursor so we don't have to look at these lines next time.
                cursor = interior_line - begin;

                // Try parsing a timestamp from this line. If we succeed, the loop will break.
                has_timestamp = parse_timestamp(interior_line, &timestamp);
            }

            // Skip this item if the timestamp is past our cutoff.
            if (has_timestamp && timestamp > cutoff_timestamp) {
                continue;
            }
        }

        // We made it through the gauntlet.
        result = line_start - begin;
        break;  //!OCLINT(avoid branching statement as last in loop)
    }

    *inout_cursor = cursor;
    return result;
}

/// Same as offset_of_next_item_fish_2_0, but for fish 1.x (pre fishfish).
/// Adapted from history_populate_from_mmap in history.c
static size_t offset_of_next_item_fish_1_x(const char *begin, size_t mmap_length,
                                           size_t *inout_cursor) {
    if (mmap_length == 0 || *inout_cursor >= mmap_length) return (size_t)-1;

    const char *end = begin + mmap_length;
    const char *pos;
    bool ignore_newline = false;
    bool do_push = true;
    bool all_done = false;
    size_t result = *inout_cursor;

    for (pos = begin + *inout_cursor; pos < end && !all_done; pos++) {
        if (do_push) {
            ignore_newline = (*pos == '#');
            do_push = false;
        }

        if (*pos == '\\') {
            pos++;
        } else if (*pos == '\n') {
            if (!ignore_newline) {
                // pos will be left pointing just after this newline, because of the ++ in the loop.
                all_done = true;
            }
            ignore_newline = false;
        }
    }

    *inout_cursor = (pos - begin);
    return result;
}

/// Returns the offset of the next item based on the given history type, or -1.
static size_t offset_of_next_item(const history_file_contents_t &contents, size_t *inout_cursor,
                                  time_t cutoff_timestamp) {
    switch (contents.type()) {
        case history_type_fish_2_0:
            return offset_of_next_item_fish_2_0(contents, inout_cursor, cutoff_timestamp);
            ;
        case history_type_fish_1_x:
            return offset_of_next_item_fish_1_x(contents.begin(), contents.length(), inout_cursor);
    }
    return size_t(-1);
}

history_t &history_collection_t::get_creating(const wcstring &name) {
    // Return a history for the given name, creating it if necessary
    // Note that histories are currently never deleted, so we can return a reference to them without
    // using something like shared_ptr
    auto hs = histories.acquire();
    std::unique_ptr<history_t> &hist = (*hs)[name];
    if (!hist) {
        hist = make_unique<history_t>(name);
    }
    return *hist;
}

history_t &history_t::history_with_name(const wcstring &name) {
    return histories.get_creating(name);
}

history_t::history_t(wcstring pname)
    : name(std::move(pname)), history_file_id(kInvalidFileID), boundary_timestamp(time(NULL)) {}

history_t::~history_t() = default;

bool history_t::chaos_mode = false;
bool history_t::never_mmap = false;

void history_t::add(const history_item_t &item, bool pending) {
    scoped_lock locker(lock);

    // Try merging with the last item.
    if (!new_items.empty() && new_items.back().merge(item)) {
        // We merged, so we don't have to add anything. Maybe this item was pending, but it just got
        // merged with an item that is not pending, so pending just becomes false.
        this->has_pending_item = false;
    } else {
        // We have to add a new item.
        new_items.push_back(item);
        this->has_pending_item = pending;
        save_internal_unless_disabled();
    }
}

void history_t::save_internal_unless_disabled() {
    // This must be called while locked.
    ASSERT_IS_LOCKED(lock);

    // Respect disable_automatic_save_counter.
    if (disable_automatic_save_counter > 0) {
        return;
    }

    // We may or may not vacuum. We try to vacuum every kVacuumFrequency items, but start the
    // countdown at a random number so that even if the user never runs more than 25 commands, we'll
    // eventually vacuum.  If countdown_to_vacuum is -1, it means we haven't yet picked a value for
    // the counter.
    const int kVacuumFrequency = 25;
    if (countdown_to_vacuum < 0) {
        static unsigned int seed = (unsigned int)time(NULL);
        // Generate a number in the range [0, kVacuumFrequency).
        countdown_to_vacuum = rand_r(&seed) / (RAND_MAX / kVacuumFrequency + 1);
    }

    // Determine if we're going to vacuum.
    bool vacuum = false;
    if (countdown_to_vacuum == 0) {
        countdown_to_vacuum = kVacuumFrequency;
        vacuum = true;
    }

    // This might be a good candidate for moving to a background thread.
    time_profiler_t profiler(vacuum ? "save_internal vacuum"       //!OCLINT(unused var)
                                    : "save_internal no vacuum");  //!OCLINT(side-effect)
    this->save_internal(vacuum);

    // Update our countdown.
    assert(countdown_to_vacuum > 0);
    countdown_to_vacuum--;
}

void history_t::add(const wcstring &str, history_identifier_t ident, bool pending) {
    time_t when = time(NULL);
    // Big hack: do not allow timestamps equal to our boundary date. This is because we include
    // items whose timestamps are equal to our boundary when reading old history, so we can catch
    // "just closed" items. But this means that we may interpret our own items, that we just wrote,
    // as old items, if we wrote them in the same second as our birthdate.
    if (when == this->boundary_timestamp) {
        when++;
    }

    this->add(history_item_t(str, when, ident), pending);
}

// Remove matching history entries from our list of new items. This only supports literal,
// case-sensitive, matches.
void history_t::remove(const wcstring &str_to_remove) {
    // Add to our list of deleted items.
    deleted_items.insert(str_to_remove);

    size_t idx = new_items.size();
    while (idx--) {
        bool matched = new_items.at(idx).str() == str_to_remove;
        if (matched) {
            new_items.erase(new_items.begin() + idx);
            // If this index is before our first_unwritten_new_item_index, then subtract one from
            // that index so it stays pointing at the same item. If it is equal to or larger, then
            // we have not yet writen this item, so we don't have to adjust the index.
            if (idx < first_unwritten_new_item_index) {
                first_unwritten_new_item_index--;
            }
        }
    }
    assert(first_unwritten_new_item_index <= new_items.size());
}

void history_t::set_valid_file_paths(const wcstring_list_t &valid_file_paths,
                                     history_identifier_t ident) {
    // 0 identifier is used to mean "not necessary".
    if (ident == 0) {
        return;
    }

    scoped_lock locker(lock);

    // Look for an item with the given identifier. It is likely to be at the end of new_items.
    for (history_item_list_t::reverse_iterator iter = new_items.rbegin(); iter != new_items.rend();
         ++iter) {
        if (iter->identifier == ident) {  // found it
            iter->required_paths = valid_file_paths;
            break;
        }
    }
}

void history_t::get_history(wcstring_list_t &result) {
    scoped_lock locker(lock);

    // If we have a pending item, we skip the first encountered (i.e. last) new item.
    bool next_is_pending = this->has_pending_item;
    std::unordered_set<wcstring> seen;

    // Append new items.
    for (auto iter = new_items.crbegin(); iter < new_items.crend(); ++iter) {
        // Skip a pending item if we have one.
        if (next_is_pending) {
            next_is_pending = false;
            continue;
        }

        if (seen.insert(iter->str()).second) result.push_back(iter->str());
    }

    // Append old items.
    load_old_if_needed();
    for (auto iter = old_item_offsets.crbegin(); iter != old_item_offsets.crend(); ++iter) {
        size_t offset = *iter;
        const history_item_t item = decode_item(*file_contents, offset);
        if (seen.insert(item.str()).second) result.push_back(item.str());
    }
}

size_t history_t::size() {
    scoped_lock locker(lock);
    size_t new_item_count = new_items.size();
    if (this->has_pending_item && new_item_count > 0) new_item_count -= 1;
    load_old_if_needed();
    size_t old_item_count = old_item_offsets.size();
    return new_item_count + old_item_count;
}

history_item_t history_t::item_at_index_assume_locked(size_t idx) {
    ASSERT_IS_LOCKED(lock);

    // 0 is considered an invalid index.
    assert(idx > 0);
    idx--;

    // Determine how many "resolved" (non-pending) items we have. We can have at most one pending
    // item, and it's always the last one.
    size_t resolved_new_item_count = new_items.size();
    if (this->has_pending_item && resolved_new_item_count > 0) {
        resolved_new_item_count -= 1;
    }

    // idx == 0 corresponds to the last resolved item.
    if (idx < resolved_new_item_count) {
        return new_items.at(resolved_new_item_count - idx - 1);
    }

    // Now look in our old items.
    idx -= resolved_new_item_count;
    load_old_if_needed();
    size_t old_item_count = old_item_offsets.size();
    if (idx < old_item_count) {
        // idx == 0 corresponds to last item in old_item_offsets.
        size_t offset = old_item_offsets.at(old_item_count - idx - 1);
        return decode_item(*file_contents, offset);
    }

    // Index past the valid range, so return an empty history item.
    return history_item_t(wcstring(), 0);
}

history_item_t history_t::item_at_index(size_t idx) {
    scoped_lock locker(lock);
    return item_at_index_assume_locked(idx);
}

std::unordered_map<long, wcstring> history_t::items_at_indexes(const std::vector<long> &idxs) {
    scoped_lock locker(lock);
    std::unordered_map<long, wcstring> result;
    for (long idx : idxs) {
        if (idx <= 0) {
            // Skip non-positive entries.
            continue;
        }
        // Insert an empty string to see if this is the first time the index is encountered. If so,
        // we have to go fetch the item.
        auto iter_inserted = result.emplace(idx, wcstring{});
        if (iter_inserted.second) {
            // New key.
            auto item = item_at_index_assume_locked(size_t(idx));
            iter_inserted.first->second = std::move(item.contents);
        }
    }
    return result;
}

void history_t::populate_from_file_contents() {
    old_item_offsets.clear();
    if (file_contents) {
        size_t cursor = 0;
        for (;;) {
            size_t offset = offset_of_next_item(*file_contents, &cursor, boundary_timestamp);
            // If we get back -1, we're done.
            if (offset == size_t(-1)) break;

            // Remember this item.
            old_item_offsets.push_back(offset);
        }
    }
}

void history_t::load_old_if_needed() {
    if (loaded_old) return;
    loaded_old = true;

    time_profiler_t profiler("load_old");  //!OCLINT(side-effect)
    wcstring filename = history_filename(name, L"");
    if (!filename.empty()) {
        int fd = wopen_cloexec(filename, O_RDONLY);
        if (fd >= 0) {
            // Take a read lock to guard against someone else appending. This is released when the
            // file is closed (below). We will read the file after releasing the lock, but that's
            // not a problem, because we never modify already written data. In short, the purpose of
            // this lock is to ensure we don't see the file size change mid-update.
            //
            // We may fail to lock (e.g. on lockless NFS - see issue #685. In that case, we proceed
            // as if it did not fail. The risk is that we may get an incomplete history item; this
            // is unlikely because we only treat an item as valid if it has a terminating newline.
            //
            // Simulate a failing lock in chaos_mode.
            if (!chaos_mode) history_file_lock(fd, LOCK_SH);
            file_contents = history_file_contents_t::create(fd);
            this->history_file_id = file_contents ? file_id_for_fd(fd) : kInvalidFileID;
            if (!chaos_mode) history_file_lock(fd, LOCK_UN);
            close(fd);

            time_profiler_t profiler("populate_from_file_contents");  //!OCLINT(side-effect)
            this->populate_from_file_contents();
        }
    }
}

bool history_search_t::go_backwards() {
    // Backwards means increasing our index.
    const size_t max_index = (size_t)-1;

    if (current_index_ == max_index) return false;
    const bool main_thread = is_main_thread();

    size_t index = current_index_;
    while (++index < max_index) {
        if (main_thread ? reader_interrupted() : reader_thread_job_is_stale()) {
            return false;
        }

        history_item_t item = history_->item_at_index(index);

        // We're done if it's empty or we cancelled.
        if (item.empty()) {
            return false;
        }

        // Look for an item that matches and (if deduping) that we haven't seen before.
        if (!item.matches_search(canon_term_, search_type_, !ignores_case())) {
            continue;
        }

        // Skip if deduplicating.
        if (dedup() && !deduper_.insert(item.str()).second) {
            continue;
        }

        // This is our new item.
        current_item_ = std::move(item);
        current_index_ = index;
        return true;
    }
    return false;
}

history_item_t history_search_t::current_item() const {
    assert(current_item_ && "No current item");
    return *current_item_;
}

wcstring history_search_t::current_string() const {
    history_item_t item = this->current_item();
    return item.str();
}

static void replace_all(std::string *str, const char *needle, const char *replacement) {
    size_t needle_len = strlen(needle), replacement_len = strlen(replacement);
    size_t offset = 0;
    while ((offset = str->find(needle, offset)) != std::string::npos) {
        str->replace(offset, needle_len, replacement);
        offset += replacement_len;
    }
}

static void escape_yaml(std::string *str) {
    replace_all(str, "\\", "\\\\");  // replace one backslash with two
    replace_all(str, "\n", "\\n");   // replace newline with backslash + literal n
}

/// This function is called frequently, so it ought to be fast.
static void unescape_yaml(std::string *str) {
    size_t cursor = 0, size = str->size();
    while (cursor < size) {
        // Operate on a const version of str, to avoid needless COWs that at() does.
        const std::string &const_str = *str;

        // Look for a backslash.
        size_t backslash = const_str.find('\\', cursor);
        if (backslash == std::string::npos || backslash + 1 >= size) {
            // Either not found, or found as the last character.
            break;
        } else {
            // Backslash found. Maybe we'll do something about it. Be sure to invoke the const
            // version of at().
            char escaped_char = const_str.at(backslash + 1);
            if (escaped_char == '\\') {
                // Two backslashes in a row. Delete the second one.
                str->erase(backslash + 1, 1);
                size--;
            } else if (escaped_char == 'n') {
                // Backslash + n. Replace with a newline.
                str->replace(backslash, 2, "\n");
                size--;
            }
            // The character at index backslash has now been made whole; start at the next
            // character.
            cursor = backslash + 1;
        }
    }
}

static wcstring history_filename(const wcstring &session_id, const wcstring &suffix) {
    if (session_id.empty()) return L"";

    wcstring result;
    if (!path_get_data(result)) return L"";

    result.append(L"/");
    result.append(session_id);
    result.append(L"_history");
    result.append(suffix);
    return result;
}

void history_t::clear_file_state() {
    ASSERT_IS_LOCKED(lock);
    // Erase everything we know about our file.
    file_contents.reset();
    loaded_old = false;
    old_item_offsets.clear();
}

void history_t::compact_new_items() {
    // Keep only the most recent items with the given contents. This algorithm could be made more
    // efficient, but likely would consume more memory too.
    std::unordered_set<wcstring> seen;
    size_t idx = new_items.size();
    while (idx--) {
        const history_item_t &item = new_items[idx];
        if (!seen.insert(item.contents).second) {
            // This item was not inserted because it was already in the set, so delete the item at
            // this index.
            new_items.erase(new_items.begin() + idx);

            if (idx < first_unwritten_new_item_index) {
                // Decrement first_unwritten_new_item_index if we are deleting a previously written
                // item.
                first_unwritten_new_item_index--;
            }
        }
    }
}

// Given the fd of an existing history file, or -1 if none, write
// a new history file to temp_fd. Returns true on success, false
// on error
bool history_t::rewrite_to_temporary_file(int existing_fd, int dst_fd) const {
    // This must be called while locked.
    ASSERT_IS_LOCKED(lock);

    // We are reading FROM existing_fd and writing TO dst_fd
    // dst_fd must be valid; existing_fd does not need to be
    assert(dst_fd >= 0);

    // Make an LRU cache to save only the last N elements.
    history_lru_cache_t lru(HISTORY_SAVE_MAX);

    // Read in existing items (which may have changed out from underneath us, so don't trust our
    // old file contents).
    if (auto local_file = history_file_contents_t::create(existing_fd)) {
        size_t cursor = 0;
        for (;;) {
            size_t offset = offset_of_next_item(*local_file, &cursor, 0);
            // If we get back -1, we're done.
            if (offset == (size_t)-1) break;

            // Try decoding an old item.
            const history_item_t old_item = decode_item(*local_file, offset);

            if (old_item.empty() || deleted_items.count(old_item.str()) > 0) {
                // debug(0, L"Item is deleted : %s\n", old_item.str().c_str());
                continue;
            }
            // Add this old item.
            lru.add_item(old_item);
        }
    }

    // Insert any unwritten new items
    for (auto iter = new_items.cbegin() + this->first_unwritten_new_item_index;
         iter != new_items.cend(); ++iter) {
        lru.add_item(*iter);
    }

    // Stable-sort our items by timestamp
    // This is because we may have read "old" items with a later timestamp than our "new" items
    // This is the essential step that roughly orders items by history
    lru.stable_sort([](const history_lru_item_t &item1, const history_lru_item_t &item2) {
        return item1.timestamp < item2.timestamp;
    });

    // Write them out.
    bool ok = true;
    history_output_buffer_t buffer(HISTORY_OUTPUT_BUFFER_SIZE);
    for (const auto &key_item : lru) {
        const history_lru_item_t &item = key_item.second;
        append_yaml_to_buffer(item.text, item.timestamp, item.required_paths, &buffer);
        if (buffer.output_size() >= HISTORY_OUTPUT_BUFFER_SIZE) {
            ok = buffer.flush_to_fd(dst_fd);
            if (!ok) {
                debug(2, L"Error %d when writing to temporary history file", errno);
                break;
            }
        }
    }

    if (ok) {
        ok = buffer.flush_to_fd(dst_fd);
        if (!ok) {
            debug(2, L"Error %d when writing to temporary history file", errno);
        }
    }
    return ok;
}

// Returns the fd of an opened temporary file, or -1 on failure
static int create_temporary_file(const wcstring &name_template, wcstring *out_path) {
    int out_fd = -1;
    for (size_t attempt = 0; attempt < 10 && out_fd == -1; attempt++) {
        char *narrow_str = wcs2str(name_template);
        out_fd = fish_mkstemp_cloexec(narrow_str);
        if (out_fd >= 0) {
            *out_path = str2wcstring(narrow_str);
        }
        free(narrow_str);
    }
    return out_fd;
}

bool history_t::save_internal_via_rewrite() {
    // This must be called while locked.
    ASSERT_IS_LOCKED(lock);
    bool ok = false;

    // We want to rewrite the file, while holding the lock for as briefly as possible
    // To do this, we speculatively write a file, and then lock and see if our original file changed
    // Repeat until we succeed or give up
    const wcstring target_name = history_filename(name, wcstring());
    const wcstring tmp_name_template = history_filename(name, L".XXXXXX");
    if (target_name.empty() || tmp_name_template.empty()) {
        return false;
    }

    // Make our temporary file
    // Remember that we have to close this fd!
    wcstring tmp_name;
    int tmp_fd = create_temporary_file(tmp_name_template, &tmp_name);
    if (tmp_fd < 0) {
        return false;
    }

    bool done = false;
    for (int i = 0; i < max_save_tries && !done; i++) {
        // Open any target file, but do not lock it right away
        int target_fd_before = wopen_cloexec(target_name, O_RDONLY | O_CREAT, history_file_mode);
        file_id_t orig_file_id = file_id_for_fd(target_fd_before);  // possibly invalid
        bool wrote = this->rewrite_to_temporary_file(target_fd_before, tmp_fd);
        if (target_fd_before >= 0) {
            close(target_fd_before);
        }
        if (!wrote) {
            // Failed to write, no good
            break;
        }

        // The crux! We rewrote the history file; see if the history file changed while we
        // were rewriting it. Make an effort to take the lock before checking, to avoid racing.
        // If the open fails, then proceed; this may be because there is no current history
        file_id_t new_file_id = kInvalidFileID;
        int target_fd_after = wopen_cloexec(target_name, O_RDONLY);
        if (target_fd_after >= 0) {
            // critical to take the lock before checking file IDs,
            // and hold it until after we are done replacing
            // Also critical to check the file at the path, NOT based on our fd
            // It's only OK to replace the file while holding the lock
            history_file_lock(target_fd_after, LOCK_EX);
            new_file_id = file_id_for_path(target_name);
        }
        bool can_replace_file = (new_file_id == orig_file_id || new_file_id == kInvalidFileID);
        if (!can_replace_file) {
            // The file has changed, so we're going to re-read it
            // Truncate our tmp_fd so we can reuse it
            if (ftruncate(tmp_fd, 0) == -1 || lseek(tmp_fd, 0, SEEK_SET) == -1) {
                debug(2, L"Error %d when truncating temporary history file", errno);
            }
        } else {
            // The file is unchanged, or the new file doesn't exist or we can't read it
            // We also attempted to take the lock, so we feel confident in replacing it

            // Ensure we maintain the ownership and permissions of the original (#2355). If the
            // stat fails, we assume (hope) our default permissions are correct. This
            // corresponds to e.g. someone running sudo -E as the very first command. If they
            // did, it would be tricky to set the permissions correctly. (bash doesn't get this
            // case right either).
            struct stat sbuf;
            if (target_fd_after >= 0 && fstat(target_fd_after, &sbuf) >= 0) {
                if (fchown(tmp_fd, sbuf.st_uid, sbuf.st_gid) == -1) {
                    debug(2, L"Error %d when changing ownership of history file", errno);
                }
                if (fchmod(tmp_fd, sbuf.st_mode) == -1) {
                    debug(2, L"Error %d when changing mode of history file", errno);
                }
            }

            // Slide it into place
            if (wrename(tmp_name, target_name) == -1) {
                debug(2, L"Error %d when renaming history file", errno);
            }

            // We did it
            done = true;
        }

        if (target_fd_after >= 0) {
            close(target_fd_after);
        }
    }

    // Ensure we never leave the old file around
    wunlink(tmp_name);
    close(tmp_fd);

    if (done) {
        // We've saved everything, so we have no more unsaved items.
        this->first_unwritten_new_item_index = new_items.size();

        // We deleted our deleted items.
        this->deleted_items.clear();

        // Our history has been written to the file, so clear our state so we can re-reference the
        // file.
        this->clear_file_state();
    }

    return ok;
}

// Function called to save our unwritten history file by appending to the existing history file
// Returns true on success, false on failure.
bool history_t::save_internal_via_appending() {
    // This must be called while locked.
    ASSERT_IS_LOCKED(lock);

    // No deleting allowed.
    assert(deleted_items.empty());

    bool ok = false;

    // If the file is different (someone vacuumed it) then we need to update our mmap.
    bool file_changed = false;

    // Get the path to the real history file.
    wcstring history_path = history_filename(name, wcstring());
    if (history_path.empty()) {
        return true;
    }

    // We are going to open the file, lock it, append to it, and then close it
    // After locking it, we need to stat the file at the path; if there is a new file there, it
    // means the file was replaced and we have to try again.
    // Limit our max tries so we don't do this forever.
    int history_fd = -1;
    for (int i = 0; i < max_save_tries; i++) {
        int fd = wopen_cloexec(history_path, O_WRONLY | O_APPEND);
        if (fd < 0) {
            // can't open, we're hosed
            break;
        }
        // Exclusive lock on the entire file. This is released when we close the file (below). This
        // may fail on (e.g.) lockless NFS. If so, proceed as if it did not fail; the risk is that
        // we may get interleaved history items, which is considered better than no history, or
        // forcing everything through the slow copy-move mode. We try to minimize this possibility
        // by writing with O_APPEND.
        //
        // Simulate a failing lock in chaos_mode
        if (!chaos_mode) history_file_lock(fd, LOCK_EX);
        const file_id_t file_id = file_id_for_fd(fd);
        if (file_id_for_path(history_path) != file_id) {
            // The file has changed, we're going to retry
            close(fd);
        } else {
            // File IDs match, so the file we opened is still at that path
            // We're going to use this fd
            if (file_id != this->history_file_id) {
                file_changed = true;
            }
            history_fd = fd;
            break;
        }
    }

    if (history_fd >= 0) {
        // We (hopefully successfully) took the exclusive lock. Append to the file.
        // Note that this is sketchy for a few reasons:
        //   - Another shell may have appended its own items with a later timestamp, so our file may
        // no longer be sorted by timestamp.
        //   - Another shell may have appended the same items, so our file may now contain
        // duplicates.
        //
        // We cannot modify any previous parts of our file, because other instances may be reading
        // those portions. We can only append.
        //
        // Originally we always rewrote the file on saving, which avoided both of these problems.
        // However, appending allows us to save history after every command, which is nice!
        //
        // Periodically we "clean up" the file by rewriting it, so that most of the time it doesn't
        // have duplicates, although we don't yet sort by timestamp (the timestamp isn't really used
        // for much anyways).

        // So far so good. Write all items at or after first_unwritten_new_item_index. Note that we
        // write even a pending item - pending items are ignored by history within the command
        // itself, but should still be written to the file.
        // TODO: consider filling the buffer ahead of time, so we can just lock, splat, and unlock?
        bool errored = false;
        // Use a small buffer size for appending, we usually only have 1 item
        history_output_buffer_t buffer(64);
        while (first_unwritten_new_item_index < new_items.size()) {
            const history_item_t &item = new_items.at(first_unwritten_new_item_index);
            append_yaml_to_buffer(item.str(), item.timestamp(), item.get_required_paths(), &buffer);
            if (buffer.output_size() >= HISTORY_OUTPUT_BUFFER_SIZE) {
                errored = !buffer.flush_to_fd(history_fd);
                if (errored) break;
            }

            // We wrote this item, hooray.
            first_unwritten_new_item_index++;
        }

        if (!errored && buffer.flush_to_fd(history_fd)) {
            ok = true;
        }

        // Since we just modified the file, update our mmap_file_id to match its current state
        // Otherwise we'll think the file has been changed by someone else the next time we go to
        // write.
        // We don't update the mapping since we only appended to the file, and everything we
        // appended remains in our new_items
        this->history_file_id = file_id_for_fd(history_fd);

        close(history_fd);
    }

    // If someone has replaced the file, forget our file state.
    if (file_changed) {
        this->clear_file_state();
    }

    return ok;
}

/// Save the specified mode to file; optionally also vacuums.
void history_t::save_internal(bool vacuum) {
    ASSERT_IS_LOCKED(lock);

    // Nothing to do if there's no new items.
    if (first_unwritten_new_item_index >= new_items.size() && deleted_items.empty()) return;

    if (history_filename(name, L"").empty()) {
        // We're in the "incognito" mode. Pretend we've saved the history.
        this->first_unwritten_new_item_index = new_items.size();
        this->deleted_items.clear();
        this->clear_file_state();
    }

    // Compact our new items so we don't have duplicates.
    this->compact_new_items();

    // Try saving. If we have items to delete, we have to rewrite the file. If we do not, we can
    // append to it.
    bool ok = false;
    if (!vacuum && deleted_items.empty()) {
        // Try doing a fast append.
        ok = save_internal_via_appending();
    }
    if (!ok) {
        // We did not or could not append; rewrite the file ("vacuum" it).
        this->save_internal_via_rewrite();
    }
}

void history_t::save() {
    scoped_lock locker(lock);
    this->save_internal(false);
}

// Formats a single history record, including a trailing newline.
//
// Returns nothing. The only possible failure involves formatting the timestamp. If that happens we
// simply omit the timestamp from the output.
static void format_history_record(const history_item_t &item, const wchar_t *show_time_format,
                                  bool null_terminate, wcstring &result) {
    if (show_time_format) {
        const time_t seconds = item.timestamp();
        struct tm timestamp;
        if (localtime_r(&seconds, &timestamp)) {
            const int max_tstamp_length = 100;
            wchar_t timestamp_string[max_tstamp_length + 1];
            if (std::wcsftime(timestamp_string, max_tstamp_length, show_time_format, &timestamp) !=
                0) {
                result.append(timestamp_string);
            }
        }
    }

    result.append(item.str());
    if (null_terminate) {
        result.push_back(L'\0');
    } else {
        result.push_back(L'\n');
    }
}

/// This handles the slightly unusual case of someone searching history for
/// specific terms/patterns.
bool history_t::search_with_args(history_search_type_t search_type, wcstring_list_t search_args,
                                 const wchar_t *show_time_format, size_t max_items,
                                 bool case_sensitive, bool null_terminate, bool reverse,
                                 io_streams_t &streams) {
    wcstring_list_t results;
    size_t hist_size = this->size();
    if (max_items > hist_size) max_items = hist_size;

    for (const wcstring &search_string : search_args) {
        if (search_string.empty()) {
            streams.err.append_format(L"Searching for the empty string isn't allowed");
            return false;
        }
        history_search_t searcher = history_search_t(
            *this, search_string, search_type, case_sensitive ? 0 : history_search_ignore_case);
        while (searcher.go_backwards()) {
            wcstring result;
            auto cur_item = searcher.current_item();
            format_history_record(cur_item, show_time_format, null_terminate, result);
            if (reverse) {
                results.push_back(result);
            } else {
                streams.out.append(result);
            }
            if (--max_items == 0) break;
        }
    }

    if (reverse) {
        for (auto it = results.rbegin(); it != results.rend(); it++) {
            streams.out.append(*it);
        }
    }

    return true;
}

bool history_t::search(history_search_type_t search_type, wcstring_list_t search_args,
                       const wchar_t *show_time_format, size_t max_items, bool case_sensitive,
                       bool null_terminate, bool reverse, io_streams_t &streams) {
    if (!search_args.empty()) {
        // User wants the results filtered. This is not the common case so we do it separate
        // from the code below for unfiltered output which is much cheaper.
        return search_with_args(search_type, search_args, show_time_format, max_items,
                                case_sensitive, null_terminate, reverse, streams);
    }

    // scoped_lock locker(lock);
    size_t hist_size = this->size();
    if (max_items > hist_size) max_items = hist_size;

    if (reverse) {
        for (size_t i = max_items; i != 0; --i) {
            auto cur_item = this->item_at_index(i);
            wcstring result;
            format_history_record(cur_item, show_time_format, null_terminate, result);
            streams.out.append(result);
        }
    } else {
        // Start at one because zero is the current command.
        for (size_t i = 1; i < max_items + 1; ++i) {
            auto cur_item = this->item_at_index(i);
            wcstring result;
            format_history_record(cur_item, show_time_format, null_terminate, result);
            streams.out.append(result);
        }
    }

    return true;
}

void history_t::disable_automatic_saving() {
    scoped_lock locker(lock);
    disable_automatic_save_counter++;
    assert(disable_automatic_save_counter != 0);  // overflow!
}

void history_t::enable_automatic_saving() {
    scoped_lock locker(lock);
    assert(disable_automatic_save_counter > 0);  // underflow
    disable_automatic_save_counter--;
    save_internal_unless_disabled();
}

void history_t::clear() {
    scoped_lock locker(lock);
    new_items.clear();
    deleted_items.clear();
    first_unwritten_new_item_index = 0;
    old_item_offsets.clear();
    wcstring filename = history_filename(name, L"");
    if (!filename.empty()) wunlink(filename);
    this->clear_file_state();
}

bool history_t::is_empty() {
    scoped_lock locker(lock);

    // If we have new items, we're not empty.
    if (!new_items.empty()) return false;

    bool empty = false;
    if (loaded_old) {
        // If we've loaded old items, see if we have any offsets.
        empty = old_item_offsets.empty();
    } else {
        // If we have not loaded old items, don't actually load them (which may be expensive); just
        // stat the file and see if it exists and is nonempty.
        const wcstring where = history_filename(name, L"");
        if (where.empty()) {
            return true;
        }

        struct stat buf = {};
        if (wstat(where, &buf) != 0) {
            // Access failed, assume missing.
            empty = true;
        } else {
            // We're empty if the file is empty.
            empty = (buf.st_size == 0);
        }
    }
    return empty;
}

/// Populates from older location (in config path, rather than data path) This is accomplished by
/// clearing ourselves, and copying the contents of the old history file to the new history file.
/// The new contents will automatically be re-mapped later.
void history_t::populate_from_config_path() {
    wcstring new_file = history_filename(name, wcstring());
    if (new_file.empty()) {
        return;
    }

    wcstring old_file;
    if (path_get_config(old_file)) {
        old_file.append(L"/");
        old_file.append(name);
        old_file.append(L"_history");
        int src_fd = wopen_cloexec(old_file, O_RDONLY, 0);
        if (src_fd != -1) {
            // Clear must come after we've retrieved the new_file name, and before we open
            // destination file descriptor, since it destroys the name and the file.
            this->clear();

            int dst_fd = wopen_cloexec(new_file, O_WRONLY | O_CREAT, history_file_mode);
            char buf[BUFSIZ];
            ssize_t size;
            while ((size = read(src_fd, buf, BUFSIZ)) > 0) {
                ssize_t written = write(dst_fd, buf, static_cast<size_t>(size));
                if (written < 0) {
                    // This message does not have high enough priority to be shown by default.
                    debug(2, L"Error when writing history file");
                    break;
                }
            }

            close(src_fd);
            close(dst_fd);
        }
    }
}

/// Decide whether we ought to import a bash history line into fish. This is a very crude heuristic.
static bool should_import_bash_history_line(const std::string &line) {
    if (line.empty()) return false;

    parse_node_tree_t parse_tree;
    wcstring wide_line = str2wcstring(line);
    if (!parse_tree_from_string(wide_line, parse_flag_none, &parse_tree, NULL)) return false;

    // In doing this test do not allow incomplete strings. Hence the "false" argument.
    parse_error_list_t errors;
    parse_util_detect_errors(wide_line, &errors, false);
    if (!errors.empty()) return false;

    // The following are Very naive tests!

    // Skip comments.
    if (line[0] == '#') return false;

    // Skip lines with backticks.
    if (line.find('`') != std::string::npos) return false;

    // Skip lines with [[...]] and ((...)) since we don't handle those constructs.
    if (line.find("[[") != std::string::npos) return false;
    if (line.find("]]") != std::string::npos) return false;
    if (line.find("((") != std::string::npos) return false;
    if (line.find("))") != std::string::npos) return false;

    // Temporarily skip lines with && and ||
    if (line.find("&&") != std::string::npos) return false;
    if (line.find("||") != std::string::npos) return false;

    // Skip lines that end with a backslash. We do not handle multiline commands from bash history.
    if (line.back() == '\\') return false;

    return true;
}

/// Import a bash command history file. Bash's history format is very simple: just lines with #s for
/// comments. Ignore a few commands that are bash-specific. It makes no attempt to handle multiline
/// commands. We can't actually parse bash syntax and the bash history file does not unambiguously
/// encode multiline commands.
void history_t::populate_from_bash(FILE *stream) {
    // We do not import bash history if an alternative fish history file is being used.
    if (history_session_id() != DFLT_FISH_HISTORY_SESSION_ID) return;

    // Process the entire history file until EOF is observed.
    bool eof = false;
    while (!eof) {
        auto line = std::string();

        // Loop until we've read a line or EOF is observed.
        while (true) {
            char buff[128];
            if (!fgets(buff, sizeof buff, stream)) {
                eof = true;
                break;
            }

            // Deal with the newline if present.
            char *a_newline = strchr(buff, '\n');
            if (a_newline) *a_newline = '\0';
            line.append(buff);
            if (a_newline) break;
        }

        // Add this line if it doesn't contain anything we know we can't handle.
        if (should_import_bash_history_line(line)) this->add(str2wcstring(line));
    }
}

void history_t::incorporate_external_changes() {
    // To incorporate new items, we simply update our timestamp to now, so that items from previous
    // instances get added. We then clear the file state so that we remap the file. Note that this
    // is somehwhat expensive because we will be going back over old items. An optimization would be
    // to preserve old_item_offsets so that they don't have to be recomputed. (However, then items
    // *deleted* in other instances would not show up here).
    time_t new_timestamp = time(NULL);
    scoped_lock locker(lock);

    // If for some reason the clock went backwards, we don't want to start dropping items; therefore
    // we only do work if time has progressed. This also makes multiple calls cheap.
    if (new_timestamp > this->boundary_timestamp) {
        this->boundary_timestamp = new_timestamp;
        this->clear_file_state();

        // We also need to erase new_items, since we go through those first, and that means we
        // will not properly interleave them with items from other instances.
        // We'll pick them up from the file (#2312)
        this->save_internal(false);
        this->new_items.clear();
        this->first_unwritten_new_item_index = 0;
    }
}

void history_collection_t::save() {
    // Save all histories
    auto hists = histories.acquire();
    for (auto &p : *hists) {
        p.second->save();
    }
}

void history_save_all() { histories.save(); }

/// Return the prefix for the files to be used for command and read history.
wcstring history_session_id() {
    wcstring result = DFLT_FISH_HISTORY_SESSION_ID;

    const auto var = env_get(L"fish_history");
    if (var) {
        wcstring session_id = var->as_string();
        if (session_id.empty()) {
            result = L"";
        } else if (session_id == L"default") {
            ;  // using the default value
        } else if (valid_var_name(session_id)) {
            result = session_id;
        } else {
            debug(0,
                  _(L"History session ID '%ls' is not a valid variable name. "
                    L"Falling back to `%ls`."),
                  session_id.c_str(), result.c_str());
        }
    }

    return result;
}

path_list_t valid_paths(const path_list_t &paths, const wcstring &working_directory) {
    ASSERT_IS_BACKGROUND_THREAD();
    wcstring_list_t result;
    for (const wcstring &path : paths) {
        if (path_is_valid(path, working_directory)) {
            result.push_back(path);
        }
    }
    return result;
}

bool all_paths_are_valid(const path_list_t &paths, const wcstring &working_directory) {
    ASSERT_IS_BACKGROUND_THREAD();
    for (const wcstring &path : paths) {
        if (!path_is_valid(path, working_directory)) {
            return false;
        }
    }
    return true;
}

static bool string_could_be_path(const wcstring &potential_path) {
    // Assume that things with leading dashes aren't paths.
    if (potential_path.empty() || potential_path.at(0) == L'-') {
        return false;
    }
    return true;
}

void history_t::add_pending_with_file_detection(const wcstring &str) {
    ASSERT_IS_MAIN_THREAD();

    // Find all arguments that look like they could be file paths.
    bool impending_exit = false;
    parse_node_tree_t tree;
    parse_tree_from_string(str, parse_flag_none, &tree, NULL);

    path_list_t potential_paths;
    for (const parse_node_t &node : tree) {
        if (!node.has_source()) {
            continue;
        }

        if (node.type == symbol_argument) {
            wcstring potential_path = node.get_source(str);
            bool unescaped = unescape_string_in_place(&potential_path, UNESCAPE_DEFAULT);
            if (unescaped && string_could_be_path(potential_path)) {
                potential_paths.push_back(potential_path);
            }
        } else if (node.type == symbol_plain_statement) {
            // Hack hack hack - if the command is likely to trigger an exit, then don't do
            // background file detection, because we won't be able to write it to our history file
            // before we exit.
            if (get_decoration({&tree, &node}) == parse_statement_decoration_exec) {
                impending_exit = true;
            }

            if (maybe_t<wcstring> command = command_for_plain_statement({&tree, &node}, str)) {
                unescape_string_in_place(&*command, UNESCAPE_DEFAULT);
                if (*command == L"exit" || *command == L"reboot") {
                    impending_exit = true;
                }
            }
        }
    }

    // If we got a path, we'll perform file detection for autosuggestion hinting.
    history_identifier_t identifier = 0;
    if (!potential_paths.empty() && !impending_exit) {
        // Grab the next identifier.
        static history_identifier_t sLastIdentifier = 0;
        identifier = ++sLastIdentifier;

        // Prevent saving until we're done, so we have time to get the paths.
        this->disable_automatic_saving();

        // Check for which paths are valid on a background thread,
        // then on the main thread update our history item
        const wcstring wd = env_get_pwd_slash();
        iothread_perform([=]() { return valid_paths(potential_paths, wd); },
                         [=](path_list_t validated_paths) {
                             this->set_valid_file_paths(validated_paths, identifier);
                             this->enable_automatic_saving();
                         });
    }

    // Actually add the item to the history.
    this->add(str, identifier, true /* pending */);

    // If we think we're about to exit, save immediately, regardless of any disabling. This may
    // cause us to lose file hinting for some commands, but it beats losing history items.
    if (impending_exit) {
        this->save();
    }
}

/// Very simple, just mark that we have no more pending items.
void history_t::resolve_pending() {
    scoped_lock locker(lock);
    this->has_pending_item = false;
}
