#include "config.h"

#include "history_file.h"

#include <cstring>

#include "history.h"

// Some forward declarations.
static history_item_t decode_item_fish_2_0(const char *base, size_t len);
static history_item_t decode_item_fish_1_x(const char *begin, size_t length);

static size_t offset_of_next_item_fish_2_0(const history_file_contents_t &contents,
                                           size_t *inout_cursor, time_t cutoff_timestamp);
static size_t offset_of_next_item_fish_1_x(const char *begin, size_t mmap_length,
                                           size_t *inout_cursor);

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
    std::memset(ptr, 0, remaining);
    return true;
}

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

static void replace_all(std::string *str, const char *needle, const char *replacement) {
    size_t needle_len = std::strlen(needle), replacement_len = std::strlen(replacement);
    size_t offset = 0;
    while ((offset = str->find(needle, offset)) != std::string::npos) {
        str->replace(offset, needle_len, replacement);
        offset += replacement_len;
    }
}

// Support for escaping and unescaping the nonstandard "yaml" format introduced in fish 2.0.
static void escape_yaml_fish_2_0(std::string *str) {
    replace_all(str, "\\", "\\\\");  // replace one backslash with two
    replace_all(str, "\n", "\\n");   // replace newline with backslash + literal n
}

/// This function is called frequently, so it ought to be fast.
static void unescape_yaml_fish_2_0(std::string *str) {
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

history_file_contents_t::~history_file_contents_t() { munmap(const_cast<char *>(start_), length_); }

history_file_contents_t::history_file_contents_t(const char *mmap_start, size_t mmap_length,
                                                 history_file_type_t type)
    : start_(mmap_start), length_(mmap_length), type_(type) {
    assert(mmap_start != MAP_FAILED && "Invalid mmap address");
}

std::unique_ptr<history_file_contents_t> history_file_contents_t::create(int fd) {
    // Check that the file is seekable, and its size.
    off_t len = lseek(fd, 0, SEEK_END);
    if (len <= 0 || static_cast<unsigned long>(len) >= SIZE_MAX) return nullptr;
    if (lseek(fd, 0, SEEK_SET) != 0) return nullptr;

    // Read the file, possibly using mmap.
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
#ifdef MAP_ANON
            mmap(0, size_t(len), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
            mmap(0, size_t(len), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
        if (mmap_start == MAP_FAILED) return nullptr;
        if (!read_from_fd(fd, mmap_start, len)) return nullptr;
    }

    // Check the file type.
    auto mtype = infer_file_type(mmap_start, len);
    if (!mtype) return nullptr;

    return std::unique_ptr<history_file_contents_t>(
        new history_file_contents_t(static_cast<const char *>(mmap_start), len, *mtype));
}

history_item_t history_file_contents_t::decode_item(size_t offset) const {
    const char *base = address_at(offset);
    size_t len = this->length() - offset;
    switch (this->type()) {
        case history_type_fish_2_0:
            return decode_item_fish_2_0(base, len);
        case history_type_fish_1_x:
            return decode_item_fish_1_x(base, len);
    }
    return history_item_t{};
}

maybe_t<size_t> history_file_contents_t::offset_of_next_item(size_t *cursor, time_t cutoff) {
    size_t offset = size_t(-1);
    switch (this->type()) {
        case history_type_fish_2_0:
            offset = offset_of_next_item_fish_2_0(*this, cursor, cutoff);
            break;
        case history_type_fish_1_x:
            offset = offset_of_next_item_fish_1_x(this->begin(), this->length(), cursor);
            break;
    }
    if (offset == size_t(-1)) {
        return none();
    }
    return offset;
}

/// Read one line, stripping off any newline, and updating cursor. Note that our input string is NOT
/// null terminated; it's just a memory mapped file.
static size_t read_line(const char *base, size_t cursor, size_t len, std::string &result) {
    // Locate the newline.
    assert(cursor <= len);
    const char *start = base + cursor;
    const char *a_newline = static_cast<const char *>(std::memchr(start, '\n', len - cursor));
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
    size_t where = line.find(':');
    if (where != std::string::npos) {
        key->assign(line, 0, where);

        // Skip a space after the : if necessary.
        size_t val_start = where + 1;
        if (val_start < line.size() && line.at(val_start) == ' ') val_start++;
        value->assign(line, val_start, line.size() - val_start);

        unescape_yaml_fish_2_0(key);
        unescape_yaml_fish_2_0(value);
    }
    return where != std::string::npos;
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

                if (std::strncmp(line.c_str(), "- ", 2)) break;

                // We're going to consume this line.
                cursor += advance;

                // Skip the leading dash-space and then store this path it.
                line.erase(0, 2);
                unescape_yaml_fish_2_0(&line);
                paths.push_back(str2wcstring(line));
            }
        }
    }

done:
    history_item_t result(cmd, when);
    result.set_required_paths(paths);
    return result;
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
    if (std::strncmp(cursor, "when:", when_len) != 0) return false;
    cursor += when_len;

    // Advance past spaces.
    while (*cursor == ' ') cursor++;

    // Try to parse a timestamp.
    long timestamp = 0;
    if (isdigit(*cursor) && (timestamp = strtol(cursor, NULL, 0)) > 0) {
        *out_when = static_cast<time_t>(timestamp);
        return true;
    }
    return false;
}

/// Returns a pointer to the start of the next line, or NULL. The next line must itself end with a
/// newline. Note that the string is not null terminated.
static const char *next_line(const char *start, const char *end) {
    // Handle the hopeless case.
    if (end == start) return NULL;

    // Skip past the next newline.
    const char *nextline = std::find(start, end, '\n');
    if (nextline == end) {
        return NULL;
    }

    // Skip past the newline character itself.
    if (++nextline >= end) {
        return NULL;
    }

    // Make sure this new line is itself "newline terminated". If it's not, return NULL.
    const char *next_newline = std::find(nextline, end, '\n');
    if (next_newline == end) {
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
        const char *a_newline =
            static_cast<const char *>(std::memchr(line_start, '\n', length - cursor));
        if (a_newline == NULL) break;

        // Advance the cursor past this line. +1 is for the newline.
        cursor = a_newline - begin + 1;

        // Skip lines with a leading space, since these are in the interior of one of our items.
        if (line_start[0] == ' ') continue;

        // Skip very short lines to make one of the checks below easier.
        if (a_newline - line_start < 3) continue;

        // Try to be a little YAML compatible. Skip lines with leading %, ---, or ...
        if (!std::memcmp(line_start, "%", 1) || !std::memcmp(line_start, "---", 3) ||
            !std::memcmp(line_start, "...", 3))
            continue;

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce lines with lots of
        // leading "- cmd: - cmd: - cmd:". Trim all but one leading "- cmd:".
        const char *double_cmd = "- cmd: - cmd: ";
        const size_t double_cmd_len = std::strlen(double_cmd);
        while (static_cast<size_t>(a_newline - line_start) > double_cmd_len &&
               !std::memcmp(line_start, double_cmd, double_cmd_len)) {
            // Skip over just one of the - cmd. In the end there will be just one left.
            line_start += std::strlen("- cmd: ");
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce commands like "when:
        // 123456". Ignore those.
        const char *cmd_when = "- cmd:    when:";
        const size_t cmd_when_len = std::strlen(cmd_when);
        if (static_cast<size_t>(a_newline - line_start) >= cmd_when_len &&
            !std::memcmp(line_start, cmd_when, cmd_when_len)) {
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

            for (interior_line = next_line(line_start, end);
                 interior_line != NULL && !has_timestamp;
                 interior_line = next_line(interior_line, end)) {
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

void append_history_item_to_buffer(const history_item_t &item, std::string *buffer) {
    auto append = [=](const char *a, const char *b = nullptr, const char *c = nullptr) {
        if (a) buffer->append(a);
        if (b) buffer->append(b);
        if (c) buffer->append(c);
    };

    std::string cmd = wcs2string(item.str());
    escape_yaml_fish_2_0(&cmd);
    append("- cmd: ", cmd.c_str(), "\n");
    append("  when: ", std::to_string(item.timestamp()).c_str(), "\n");
    const path_list_t &paths = item.get_required_paths();
    if (!paths.empty()) {
        append("  paths:\n");

        for (const auto &wpath : paths) {
            std::string path = wcs2string(wpath);
            escape_yaml_fish_2_0(&path);
            append("    - ", path.c_str(), "\n");
        }
    }
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
            c = static_cast<unsigned char>(*pos);
            res = 1;
        } else {
            res = std::mbrtowc(&c, pos, end - pos, &state);
        }

        if (res == static_cast<size_t>(-1)) {
            pos++;
            continue;
        } else if (res == static_cast<size_t>(-2)) {
            break;
        } else if (res == static_cast<size_t>(0)) {
            pos++;
            continue;
        }
        pos += res;

        if (c == L'\n') {
            if (timestamp_mode) {
                const wchar_t *time_string = out.c_str();
                while (*time_string && !iswdigit(*time_string)) time_string++;

                if (*time_string) {
                    time_t tm = static_cast<time_t>(fish_wcstol(time_string));
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

/// Same as offset_of_next_item_fish_2_0, but for fish 1.x (pre fishfish).
/// Adapted from history_populate_from_mmap in history.c
static size_t offset_of_next_item_fish_1_x(const char *begin, size_t mmap_length,
                                           size_t *inout_cursor) {
    if (mmap_length == 0 || *inout_cursor >= mmap_length) return static_cast<size_t>(-1);

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
