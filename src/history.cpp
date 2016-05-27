// History functions, part of the user interface.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <algorithm>
#include <iterator>
#include <map>

#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "iothread.h"
#include "lru.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "path.h"
#include "reader.h"
#include "sanity.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

// Our history format is intended to be valid YAML. Here it is:
//
//   - cmd: ssh blah blah blah
//     when: 2348237
//     paths:
//       - /path/to/something
//       - /path/to/something_else
//
//   Newlines are replaced by \n. Backslashes are replaced by \\.

// When we rewrite the history, the number of items we keep.
#define HISTORY_SAVE_MAX (1024 * 256)

// Default buffer size for flushing to the history file.
#define HISTORY_OUTPUT_BUFFER_SIZE (16 * 1024)

namespace {

/// Helper class for certain output. This is basically a string that allows us to ensure we only
/// flush at record boundaries, and avoids the copying of ostringstream. Have you ever tried to
/// implement your own streambuf? Total insanity.
static size_t safe_strlen(const char *s) { return s ? strlen(s) : 0; }
class history_output_buffer_t {
    // A null-terminated C string.
    std::vector<char> buffer;
    // Offset is the offset of the null terminator.
    size_t offset;

   public:
    /// Add a bit more to HISTORY_OUTPUT_BUFFER_SIZE because we flush once we've exceeded that size.
    history_output_buffer_t() : buffer(HISTORY_OUTPUT_BUFFER_SIZE + 128, '\0'), offset(0) {}

    /// Append one or more strings.
    void append(const char *s1, const char *s2 = NULL, const char *s3 = NULL) {
        const char *ptrs[4] = {s1, s2, s3, NULL};
        const size_t lengths[4] = {safe_strlen(s1), safe_strlen(s2), safe_strlen(s3), 0};

        // Determine the additional size we'll need.
        size_t additional_length = 0;
        for (size_t i = 0; i < sizeof lengths / sizeof *lengths; i++) {
            additional_length += lengths[i];
        }

        // Allocate that much, plus a null terminator.
        size_t required_size = offset + additional_length + 1;
        if (required_size > buffer.size()) {
            buffer.resize(required_size, '\0');
        }

        // Copy.
        for (size_t i = 0; ptrs[i] != NULL; i++) {
            memmove(&buffer.at(offset), ptrs[i], lengths[i]);
            offset += lengths[i];
        }

        // Null terminator was appended by virtue of the resize() above (or in a previous
        // invocation).
        assert(buffer.at(buffer.size() - 1) == '\0');
    }

    /// Output to a given fd, resetting our buffer. Returns true on success, false on error.
    bool flush_to_fd(int fd) {
        bool result = write_loop(fd, &buffer.at(0), offset) >= 0;
        offset = 0;
        return result;
    }

    /// Return how much data we've accumulated.
    size_t output_size() const { return offset; }
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

/// Lock a file via fcntl; returns true on success, false on failure.
static bool history_file_lock(int fd, short type) {
    assert(type == F_RDLCK || type == F_WRLCK);
    struct flock flk = {};
    flk.l_type = type;
    flk.l_whence = SEEK_SET;
    int ret = fcntl(fd, F_SETLKW, (void *)&flk);
    return ret != -1;
}

/// Our LRU cache is used for restricting the amount of history we have, and limiting how long we
/// order it.
class history_lru_node_t : public lru_node_t {
   public:
    time_t timestamp;
    path_list_t required_paths;
    explicit history_lru_node_t(const history_item_t &item)
        : lru_node_t(item.str()),
          timestamp(item.timestamp()),
          required_paths(item.get_required_paths()) {}
};

class history_lru_cache_t : public lru_cache_t<history_lru_node_t> {
   protected:
    /// Override to delete evicted nodes.
    virtual void node_was_evicted(history_lru_node_t *node) { delete node; }

   public:
    explicit history_lru_cache_t(size_t max) : lru_cache_t<history_lru_node_t>(max) {}

    /// Function to add a history item.
    void add_item(const history_item_t &item) {
        // Skip empty items.
        if (item.empty()) return;

        // See if it's in the cache. If it is, update the timestamp. If not, we create a new node
        // and add it. Note that calling get_node promotes the node to the front.
        history_lru_node_t *node = this->get_node(item.str());
        if (node == NULL) {
            node = new history_lru_node_t(item);
            this->add_node(node);
        } else {
            node->timestamp = std::max(node->timestamp, item.timestamp());
            // What to do about paths here? Let's just ignore them.
        }
    }
};

class history_collection_t {
    pthread_mutex_t m_lock;
    std::map<wcstring, history_t *> m_histories;

   public:
    history_collection_t() { VOMIT_ON_FAILURE_NO_ERRNO(pthread_mutex_init(&m_lock, NULL)); }
    ~history_collection_t() {
        for (std::map<wcstring, history_t *>::const_iterator i = m_histories.begin();
             i != m_histories.end(); ++i) {
            delete i->second;
        }
        pthread_mutex_destroy(&m_lock);
    }
    history_t &alloc(const wcstring &name);
    void save();
};

}  // anonymous namespace

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
    const char *newline = (char *)memchr(start, '\n', len - cursor);
    if (newline != NULL) {  // we found a newline
        result.assign(start, newline - start);
        // Return the amount to advance the cursor; skip over the newline.
        return newline - start + 1;
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

/// Try to infer the history file type based on inspecting the data.
static history_file_type_t infer_file_type(const char *data, size_t len) {
    history_file_type_t result = history_type_unknown;
    if (len > 0) {  // old fish started with a #
        if (data[0] == '#') {
            result = history_type_fish_1_x;
        } else {  // assume new fish
            result = history_type_fish_2_0;
        }
    }
    return result;
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
                errno = 0;

                if (*time_string) {
                    time_t tm;
                    wchar_t *end;

                    errno = 0;
                    tm = (time_t)wcstol(time_string, &end, 10);

                    if (tm && !errno && !*end) {
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

static history_item_t decode_item(const char *base, size_t len, history_file_type_t type) {
    if (type == history_type_fish_2_0) return decode_item_fish_2_0(base, len);
    if (type == history_type_fish_1_x) return decode_item_fish_1_x(base, len);
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

history_item_t::history_item_t(const wcstring &str)
    : contents(str), creation_timestamp(time(NULL)), identifier(0) {}

history_item_t::history_item_t(const wcstring &str, time_t when, history_identifier_t ident)
    : contents(str), creation_timestamp(when), identifier(ident) {}

bool history_item_t::matches_search(const wcstring &term, enum history_search_type_t type) const {
    switch (type) {
        case HISTORY_SEARCH_TYPE_CONTAINS: {
            // We consider equal strings to NOT match a contains search (so that you don't have to
            // see history equal to what you typed). The length check ensures that.
            return contents.size() > term.size() && contents.find(term) != wcstring::npos;
        }
        case HISTORY_SEARCH_TYPE_PREFIX: {
            // We consider equal strings to match a prefix search, so that autosuggest will allow
            // suggesting what you've typed.
            return string_prefixes_string(term, contents);
        }
        default: {
            sanity_lose();
            return false;
        }
    }
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
/// Pass the address and length of a mapped region.
/// Pass a pointer to a cursor size_t, initially 0.
/// If custoff_timestamp is nonzero, skip items created at or after that timestamp.
/// Returns (size_t)-1 when done.
static size_t offset_of_next_item_fish_2_0(const char *begin, size_t mmap_length,
                                           size_t *inout_cursor, time_t cutoff_timestamp) {
    size_t cursor = *inout_cursor;
    size_t result = (size_t)-1;
    while (cursor < mmap_length) {
        const char *line_start = begin + cursor;

        // Advance the cursor to the next line.
        const char *newline = (const char *)memchr(line_start, '\n', mmap_length - cursor);
        if (newline == NULL) break;

        // Advance the cursor past this line. +1 is for the newline.
        cursor = newline - begin + 1;

        // Skip lines with a leading space, since these are in the interior of one of our items.
        if (line_start[0] == ' ') continue;

        // Skip very short lines to make one of the checks below easier.
        if (newline - line_start < 3) continue;

        // Try to be a little YAML compatible. Skip lines with leading %, ---, or ...
        if (!memcmp(line_start, "%", 1) || !memcmp(line_start, "---", 3) ||
            !memcmp(line_start, "...", 3))
            continue;

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce lines with lots of
        // leading "- cmd: - cmd: - cmd:". Trim all but one leading "- cmd:".
        const char *double_cmd = "- cmd: - cmd: ";
        const size_t double_cmd_len = strlen(double_cmd);
        while (newline - line_start > double_cmd_len &&
               !memcmp(line_start, double_cmd, double_cmd_len)) {
            // Skip over just one of the - cmd. In the end there will be just one left.
            line_start += strlen("- cmd: ");
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce commands like "when:
        // 123456". Ignore those.
        const char *cmd_when = "- cmd:    when:";
        const size_t cmd_when_len = strlen(cmd_when);
        if (newline - line_start >= cmd_when_len && !memcmp(line_start, cmd_when, cmd_when_len))
            continue;

        // At this point, we know line_start is at the beginning of an item. But maybe we want to
        // skip this item because of timestamps. A 0 cutoff means we don't care; if we do care, then
        // try parsing out a timestamp.
        if (cutoff_timestamp != 0) {
            // Hackish fast way to skip items created after our timestamp. This is the mechanism by
            // which we avoid "seeing" commands from other sessions that started after we started.
            // We try hard to ensure that our items are sorted by their timestamps, so in theory we
            // could just break, but I don't think that works well if (for example) the clock
            // changes. So we'll read all subsequent items.
            const char *const end = begin + mmap_length;

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
static size_t offset_of_next_item(const char *begin, size_t mmap_length,
                                  history_file_type_t mmap_type, size_t *inout_cursor,
                                  time_t cutoff_timestamp) {
    size_t result;
    switch (mmap_type) {
        case history_type_fish_2_0: {
            result =
                offset_of_next_item_fish_2_0(begin, mmap_length, inout_cursor, cutoff_timestamp);
            break;
        }
        case history_type_fish_1_x: {
            result = offset_of_next_item_fish_1_x(begin, mmap_length, inout_cursor);
            break;
        }
        case history_type_unknown: {
            // Oh well.
            result = (size_t)-1;
            break;
        }
    }
    return result;
}

history_t &history_collection_t::alloc(const wcstring &name) {
    // Note that histories are currently never deleted, so we can return a reference to them without
    // using something like shared_ptr.
    scoped_lock locker(m_lock);  //!OCLINT(side-effect)
    history_t *&current = m_histories[name];
    if (current == NULL) current = new history_t(name);
    return *current;
}

history_t &history_t::history_with_name(const wcstring &name) { return histories.alloc(name); }

history_t::history_t(const wcstring &pname)
    : name(pname),
      first_unwritten_new_item_index(0),
      has_pending_item(false),
      disable_automatic_save_counter(0),
      mmap_start(NULL),
      mmap_length(0),
      mmap_type(history_file_type_t(-1)),
      mmap_file_id(kInvalidFileID),
      boundary_timestamp(time(NULL)),
      countdown_to_vacuum(-1),
      loaded_old(false),
      chaos_mode(false) {
    pthread_mutex_init(&lock, NULL);
}

history_t::~history_t() { pthread_mutex_destroy(&lock); }

void history_t::add(const history_item_t &item, bool pending) {
    scoped_lock locker(lock);  //!OCLINT(side-effect)

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
    time_profiler_t profiler(vacuum ? "save_internal vacuum"
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

void history_t::remove(const wcstring &str) {
    // Add to our list of deleted items.
    deleted_items.insert(str);

    // Remove from our list of new items.
    size_t idx = new_items.size();
    while (idx--) {
        if (new_items.at(idx).str() == str) {
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

    scoped_lock locker(lock);  //!OCLINT(side-effect)

    // Look for an item with the given identifier. It is likely to be at the end of new_items.
    for (history_item_list_t::reverse_iterator iter = new_items.rbegin(); iter != new_items.rend();
         ++iter) {
        if (iter->identifier == ident) {  // found it
            iter->required_paths = valid_file_paths;
            break;
        }
    }
}

void history_t::get_string_representation(wcstring *result, const wcstring &separator) {
    scoped_lock locker(lock);  //!OCLINT(side-effect)

    bool first = true;

    std::set<wcstring> seen;

    // If we have a pending item, we skip the first encountered (i.e. last) new item.
    bool next_is_pending = this->has_pending_item;

    // Append new items. Note that in principle we could use const_reverse_iterator, but we do not
    // because reverse_iterator is not convertible to const_reverse_iterator. See
    // http://github.com/fish-shell/fish-shell/issues/431.
    for (history_item_list_t::reverse_iterator iter = new_items.rbegin(); iter < new_items.rend();
         ++iter) {
        // Skip a pending item if we have one.
        if (next_is_pending) {
            next_is_pending = false;
            continue;
        }

        // Skip duplicates.
        if (!seen.insert(iter->str()).second) continue;

        if (!first) result->append(separator);
        result->append(iter->str());
        first = false;
    }

    // Append old items.
    load_old_if_needed();
    for (std::deque<size_t>::reverse_iterator iter = old_item_offsets.rbegin();
         iter != old_item_offsets.rend(); ++iter) {
        size_t offset = *iter;
        const history_item_t item =
            decode_item(mmap_start + offset, mmap_length - offset, mmap_type);

        // Skip duplicates.
        if (!seen.insert(item.str()).second) continue;

        if (!first) result->append(separator);
        result->append(item.str());
        first = false;
    }
}

history_item_t history_t::item_at_index(size_t idx) {
    scoped_lock locker(lock);  //!OCLINT(side-effect)

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
        return decode_item(mmap_start + offset, mmap_length - offset, mmap_type);
    }

    // Index past the valid range, so return an empty history item.
    return history_item_t(wcstring(), 0);
}

void history_t::populate_from_mmap(void) {
    mmap_type = infer_file_type(mmap_start, mmap_length);
    size_t cursor = 0;
    for (;;) {
        size_t offset =
            offset_of_next_item(mmap_start, mmap_length, mmap_type, &cursor, boundary_timestamp);
        // If we get back -1, we're done.
        if (offset == (size_t)-1) break;

        // Remember this item.
        old_item_offsets.push_back(offset);
    }
}

/// Do a private, read-only map of the entirety of a history file with the given name. Returns true
/// if successful. Returns the mapped memory region by reference.
bool history_t::map_file(const wcstring &name, const char **out_map_start, size_t *out_map_len,
                         file_id_t *file_id) {
    bool result = false;
    wcstring filename = history_filename(name, L"");
    if (!filename.empty()) {
        int fd = wopen_cloexec(filename, O_RDONLY);
        if (fd >= 0) {
            // Get the file ID if requested.
            if (file_id != NULL) *file_id = file_id_for_fd(fd);

            // Take a read lock to guard against someone else appending. This is released when the
            // file is closed (below). We will read the file after releasing the lock, but that's
            // not a problem, because we never modify already written data. In short, the purpose of
            // this lock is to ensure we don't see the file size change mid-update.
            //
            //    We may fail to lock (e.g. on lockless NFS - see
            //    https://github.com/fish-shell/fish-shell/issues/685 ). In that case, we proceed as
            //    if it did not fail. The risk is that we may get an incomplete history item; this
            //    is unlikely because we only treat an item as valid if it has a terminating
            //    newline.
            //
            //  Simulate a failing lock in chaos_mode.
            if (!chaos_mode) history_file_lock(fd, F_RDLCK);
            off_t len = lseek(fd, 0, SEEK_END);
            if (len != (off_t)-1) {
                size_t mmap_length = (size_t)len;
                if (lseek(fd, 0, SEEK_SET) == 0) {
                    char *mmap_start;
                    if ((mmap_start = (char *)mmap(0, mmap_length, PROT_READ, MAP_PRIVATE, fd,
                                                   0)) != MAP_FAILED) {
                        result = true;
                        *out_map_start = mmap_start;
                        *out_map_len = mmap_length;
                    }
                }
            }
            close(fd);
        }
    }
    return result;
}

bool history_t::load_old_if_needed(void) {
    if (loaded_old) return true;
    loaded_old = true;

    // PCA not sure why signals were blocked here
    // signal_block();

    bool ok = false;
    if (map_file(name, &mmap_start, &mmap_length, &mmap_file_id)) {
        // Here we've mapped the file.
        ok = true;
        time_profiler_t profiler("populate_from_mmap");  //!OCLINT(side-effect)
        this->populate_from_mmap();
    }

    // signal_unblock();
    return ok;
}

void history_search_t::skip_matches(const wcstring_list_t &skips) {
    external_skips = skips;
    std::sort(external_skips.begin(), external_skips.end());
}

bool history_search_t::should_skip_match(const wcstring &str) const {
    return std::binary_search(external_skips.begin(), external_skips.end(), str);
}

bool history_search_t::go_forwards() {
    // Pop the top index (if more than one) and return if we have any left.
    if (prev_matches.size() > 1) {
        prev_matches.pop_back();
        return true;
    }
    return false;
}

bool history_search_t::go_backwards() {
    // Backwards means increasing our index.
    const size_t max_idx = (size_t)-1;

    size_t idx = 0;
    if (!prev_matches.empty()) idx = prev_matches.back().first;

    if (idx == max_idx) return false;

    const bool main_thread = is_main_thread();

    while (++idx < max_idx) {
        if (main_thread ? reader_interrupted() : reader_thread_job_is_stale()) {
            return false;
        }

        const history_item_t item = history->item_at_index(idx);
        // We're done if it's empty or we cancelled.
        if (item.empty()) {
            return false;
        }

        // Look for a term that matches and that we haven't seen before.
        const wcstring &str = item.str();
        if (item.matches_search(term, search_type) && !match_already_made(str) &&
            !should_skip_match(str)) {
            prev_matches.push_back(prev_match_t(idx, item));
            return true;
        }
    }
    return false;
}

/// Goes to the end (forwards).
void history_search_t::go_to_end(void) { prev_matches.clear(); }

/// Returns if we are at the end, which is where we start.
bool history_search_t::is_at_end(void) const { return prev_matches.empty(); }

/// Goes to the beginning (backwards).
void history_search_t::go_to_beginning(void) {
    // Go backwards as far as we can.
    while (go_backwards()) {  //!OCLINT(empty while statement)
        // Do nothing.
    }
}

history_item_t history_search_t::current_item() const {
    assert(!prev_matches.empty());  //!OCLINT(double negative)
    return prev_matches.back().second;
}

wcstring history_search_t::current_string() const {
    history_item_t item = this->current_item();
    return item.str();
}

bool history_search_t::match_already_made(const wcstring &match) const {
    for (std::vector<prev_match_t>::const_iterator iter = prev_matches.begin();
         iter != prev_matches.end(); ++iter) {
        if (iter->second.str() == match) return true;
    }
    return false;
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

static wcstring history_filename(const wcstring &name, const wcstring &suffix) {
    wcstring path;
    if (!path_get_data(path)) return L"";

    wcstring result = path;
    result.append(L"/");
    result.append(name);
    result.append(L"_history");
    result.append(suffix);
    return result;
}

void history_t::clear_file_state() {
    ASSERT_IS_LOCKED(lock);
    // Erase everything we know about our file.
    if (mmap_start != NULL && mmap_start != MAP_FAILED) {
        munmap((void *)mmap_start, mmap_length);
    }
    mmap_start = NULL;
    mmap_length = 0;
    loaded_old = false;
    old_item_offsets.clear();
}

void history_t::compact_new_items() {
    // Keep only the most recent items with the given contents. This algorithm could be made more
    // efficient, but likely would consume more memory too.
    std::set<wcstring> seen;
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

bool history_t::save_internal_via_rewrite() {
    // This must be called while locked.
    ASSERT_IS_LOCKED(lock);
    bool ok = false;

    wcstring tmp_name_template = history_filename(name, L".XXXXXX");
    if (!tmp_name_template.empty()) {
        // Make an LRU cache to save only the last N elements.
        history_lru_cache_t lru(HISTORY_SAVE_MAX);

        // Insert old items in, from old to new. Merge them with our new items, inserting items with
        // earlier timestamps first.
        history_item_list_t::const_iterator new_item_iter = new_items.begin();

        // Map in existing items (which may have changed out from underneath us, so don't trust our
        // old mmap'd data).
        const char *local_mmap_start = NULL;
        size_t local_mmap_size = 0;
        if (map_file(name, &local_mmap_start, &local_mmap_size, NULL)) {
            const history_file_type_t local_mmap_type =
                infer_file_type(local_mmap_start, local_mmap_size);
            size_t cursor = 0;
            for (;;) {
                size_t offset = offset_of_next_item(local_mmap_start, local_mmap_size,
                                                    local_mmap_type, &cursor, 0);
                // If we get back -1, we're done.
                if (offset == (size_t)-1) break;

                // Try decoding an old item.
                const history_item_t old_item = decode_item(
                    local_mmap_start + offset, local_mmap_size - offset, local_mmap_type);
                if (old_item.empty() || deleted_items.count(old_item.str()) > 0) {
                    //                    debug(0, L"Item is deleted : %s\n",
                    //                    old_item.str().c_str());
                    continue;
                }
                // The old item may actually be more recent than our new item, if it came from
                // another session. Insert all new items at the given index with an earlier
                // timestamp.
                for (; new_item_iter != new_items.end(); ++new_item_iter) {
                    if (new_item_iter->timestamp() < old_item.timestamp()) {
                        // This "new item" is in fact older.
                        lru.add_item(*new_item_iter);
                    } else {
                        // The new item is not older.
                        break;
                    }
                }

                // Now add this old item.
                lru.add_item(old_item);
            }
            munmap((void *)local_mmap_start, local_mmap_size);
        }

        // Insert any remaining new items.
        for (; new_item_iter != new_items.end(); ++new_item_iter) {
            lru.add_item(*new_item_iter);
        }

        signal_block();

        // Try to create a temporary file, up to 10 times. We don't use mkstemps because we want to
        // open it CLO_EXEC. This should almost always succeed on the first try.
        int out_fd = -1;
        wcstring tmp_name;
        for (size_t attempt = 0; attempt < 10 && out_fd == -1; attempt++) {
            char *narrow_str = wcs2str(tmp_name_template.c_str());
#if HAVE_MKOSTEMP
            out_fd = mkostemp(narrow_str, O_CLOEXEC);
            if (out_fd >= 0) {
                tmp_name = str2wcstring(narrow_str);
            }
#else
            if (narrow_str && mktemp(narrow_str)) {
                // It was successfully templated; try opening it atomically.
                tmp_name = str2wcstring(narrow_str);
                out_fd = wopen_cloexec(tmp_name, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0600);
            }
#endif
            free(narrow_str);
        }

        if (out_fd >= 0) {
            // Write them out.
            bool errored = false;
            history_output_buffer_t buffer;
            for (history_lru_cache_t::iterator iter = lru.begin(); iter != lru.end(); ++iter) {
                const history_lru_node_t *node = *iter;
                append_yaml_to_buffer(node->key, node->timestamp, node->required_paths, &buffer);
                if (buffer.output_size() >= HISTORY_OUTPUT_BUFFER_SIZE &&
                    !buffer.flush_to_fd(out_fd)) {
                    errored = true;
                    break;
                }
            }

            if (!errored && buffer.flush_to_fd(out_fd)) {
                ok = true;
            }

            if (!ok) {
                // This message does not have high enough priority to be shown by default.
                debug(2, L"Error when writing history file");
            } else {
                wcstring new_name = history_filename(name, wcstring());

                // Ensure we maintain the ownership and permissions of the original (#2355). If the
                // stat fails, we assume (hope) our default permissions are correct. This
                // corresponds to e.g. someone running sudo -E as the very first command. If they
                // did, it would be tricky to set the permissions correctly. (bash doesn't get this
                // case right either).
                struct stat sbuf;
                if (wstat(new_name, &sbuf) >= 0) {  // success
                    if (fchown(out_fd, sbuf.st_uid, sbuf.st_gid) == -1) {
                        debug(2, L"Error %d when changing ownership of history file", errno);
                    }
                    if (fchmod(out_fd, sbuf.st_mode) == -1) {
                        debug(2, L"Error %d when changing mode of history file", errno);
                    }
                }

                if (wrename(tmp_name, new_name) == -1) {
                    debug(2, L"Error %d when renaming history file", errno);
                }
            }
            close(out_fd);
        }

        signal_unblock();

        // Make sure we clear all nodes, since this doesn't happen automatically.
        lru.evict_all_nodes();
    }

    if (ok) {
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

    signal_block();

    // Open the file.
    int out_fd = wopen_cloexec(history_path, O_WRONLY | O_APPEND);
    if (out_fd >= 0) {
        // Check to see if the file changed.
        if (file_id_for_fd(out_fd) != mmap_file_id) file_changed = true;

        // Exclusive lock on the entire file. This is released when we close the file (below). This
        // may fail on (e.g.) lockless NFS. If so, proceed as if it did not fail; the risk is that
        // we may get interleaved history items, which is considered better than no history, or
        // forcing everything through the slow copy-move mode. We try to minimize this possibility
        // by writing with O_APPEND.
        //
        // Simulate a failing lock in chaos_mode
        if (!chaos_mode) history_file_lock(out_fd, F_WRLCK);

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
        bool errored = false;
        history_output_buffer_t buffer;
        while (first_unwritten_new_item_index < new_items.size()) {
            const history_item_t &item = new_items.at(first_unwritten_new_item_index);
            append_yaml_to_buffer(item.str(), item.timestamp(), item.get_required_paths(), &buffer);
            if (buffer.output_size() >= HISTORY_OUTPUT_BUFFER_SIZE) {
                errored = !buffer.flush_to_fd(out_fd);
                if (errored) break;
            }

            // We wrote this item, hooray.
            first_unwritten_new_item_index++;
        }

        if (!errored && buffer.flush_to_fd(out_fd)) {
            ok = true;
        }

        close(out_fd);
    }

    signal_unblock();

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
        ok = this->save_internal_via_rewrite();
    }
}

void history_t::save(void) {
    scoped_lock locker(lock);  //!OCLINT(side-effect)
    this->save_internal(false);
}

void history_t::disable_automatic_saving() {
    scoped_lock locker(lock);  //!OCLINT(side-effect)
    disable_automatic_save_counter++;
    assert(disable_automatic_save_counter != 0);  // overflow!
}

void history_t::enable_automatic_saving() {
    scoped_lock locker(lock);                    //!OCLINT(side-effect)
    assert(disable_automatic_save_counter > 0);  // underflow
    disable_automatic_save_counter--;
    save_internal_unless_disabled();
}

void history_t::clear(void) {
    scoped_lock locker(lock);  //!OCLINT(side-effect)
    new_items.clear();
    deleted_items.clear();
    first_unwritten_new_item_index = 0;
    old_item_offsets.clear();
    wcstring filename = history_filename(name, L"");
    if (!filename.empty()) wunlink(filename);
    this->clear_file_state();
}

bool history_t::is_empty(void) {
    scoped_lock locker(lock);  //!OCLINT(side-effect)

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
    wcstring old_file;
    if (path_get_config(old_file)) {
        old_file.append(L"/");
        old_file.append(name);
        old_file.append(L"_history");
        int src_fd = wopen_cloexec(old_file, O_RDONLY, 0);
        if (src_fd != -1) {
            wcstring new_file = history_filename(name, wcstring());

            // Clear must come after we've retrieved the new_file name, and before we open
            // destination file descriptor, since it destroys the name and the file.
            this->clear();

            int dst_fd = wopen_cloexec(new_file, O_WRONLY | O_CREAT, 0644);
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

/// Indicate whether we ought to import the bash history file into fish.
static bool should_import_bash_history_line(const std::string &line) {
    if (line.empty()) return false;

    // Very naive tests! Skip export; probably should skip others.
    const char *const ignore_prefixes[] = {"export ", "#"};

    for (size_t i = 0; i < sizeof ignore_prefixes / sizeof *ignore_prefixes; i++) {
        const char *prefix = ignore_prefixes[i];
        if (!line.compare(0, strlen(prefix), prefix)) {
            return false;
        }
    }

    // Skip lines with backticks.
    if (line.find('`') != std::string::npos) return false;

    return true;
}

void history_t::populate_from_bash(FILE *stream) {
    // Bash's format is very simple: just lines with #s for comments. Ignore a few commands that are
    // bash-specific. This list ought to be expanded.
    std::string line;
    for (;;) {
        line.clear();
        bool success = false;
        bool has_newline = false;

        // Loop until we've read a line.
        do {
            char buff[128];
            success = (bool)fgets(buff, sizeof buff, stream);
            if (success) {
                // Skip the newline.
                char *newline = strchr(buff, '\n');
                if (newline) *newline = '\0';
                has_newline = (newline != NULL);

                // Append what we've got.
                line.append(buff);
            }
        } while (success && !has_newline);

        // Maybe add this line.
        if (should_import_bash_history_line(line)) {
            this->add(str2wcstring(line));
        }

        if (line.empty()) break;
    }
}

void history_t::incorporate_external_changes() {
    // To incorporate new items, we simply update our timestamp to now, so that items from previous
    // instances get added. We then clear the file state so that we remap the file. Note that this
    // is somehwhat expensive because we will be going back over old items. An optimization would be
    // to preserve old_item_offsets so that they don't have to be recomputed. (However, then items
    // *deleted* in other instances would not show up here).
    time_t new_timestamp = time(NULL);
    scoped_lock locker(lock);  //!OCLINT(side-effect)

    // If for some reason the clock went backwards, we don't want to start dropping items; therefore
    // we only do work if time has progressed. This also makes multiple calls cheap.
    if (new_timestamp > this->boundary_timestamp) {
        this->boundary_timestamp = new_timestamp;
        this->clear_file_state();
    }
}

void history_init() {}

void history_collection_t::save() {
    // Save all histories.
    for (std::map<wcstring, history_t *>::iterator iter = m_histories.begin();
         iter != m_histories.end(); ++iter) {
        history_t *hist = iter->second;
        hist->save();
    }
}

void history_destroy() { histories.save(); }

void history_sanity_check() {
    // No sanity checking implemented yet...
}

int file_detection_context_t::perform_file_detection(bool test_all) {
    ASSERT_IS_BACKGROUND_THREAD();
    valid_paths.clear();
    // TODO: Figure out why this bothers to return a variable result since the only consumer,
    // perform_file_detection_done(), ignores the result. It seems like either this should always
    // return a constant or perform_file_detection_done() should use our return value.
    int result = 1;
    for (path_list_t::const_iterator iter = potential_paths.begin(); iter != potential_paths.end();
         ++iter) {
        if (path_is_valid(*iter, working_directory)) {
            // Push the original (possibly relative) path.
            valid_paths.push_back(*iter);
        } else {
            // Not a valid path.
            result = 0;
            if (!test_all) break;
        }
    }
    return result;
}

bool file_detection_context_t::paths_are_valid(const path_list_t &paths) {
    this->potential_paths = paths;
    return perform_file_detection(false) > 0;
}

file_detection_context_t::file_detection_context_t(history_t *hist, history_identifier_t ident)
    : history(hist), working_directory(env_get_pwd_slash()), history_item_identifier(ident) {}

static int threaded_perform_file_detection(file_detection_context_t *ctx) {
    ASSERT_IS_BACKGROUND_THREAD();
    assert(ctx != NULL);
    return ctx->perform_file_detection(true /* test all */);
}

static void perform_file_detection_done(file_detection_context_t *ctx,
                                        int success) {  //!OCLINT(success is ignored)
    ASSERT_IS_MAIN_THREAD();

    // Now that file detection is done, update the history item with the valid file paths.
    ctx->history->set_valid_file_paths(ctx->valid_paths, ctx->history_item_identifier);

    // Allow saving again.
    ctx->history->enable_automatic_saving();

    // Done with the context.
    delete ctx;
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
    path_list_t potential_paths;

    // Find all arguments that look like they could be file paths.
    bool impending_exit = false;
    parse_node_tree_t tree;
    parse_tree_from_string(str, parse_flag_none, &tree, NULL);
    size_t count = tree.size();

    for (size_t i = 0; i < count; i++) {
        const parse_node_t &node = tree.at(i);
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
            if (tree.decoration_for_plain_statement(node) == parse_statement_decoration_exec) {
                impending_exit = true;
            }

            wcstring command;
            tree.command_for_plain_statement(node, str, &command);
            unescape_string_in_place(&command, UNESCAPE_DEFAULT);
            if (contains(command, L"exit", L"reboot")) {
                impending_exit = true;
            }
        }
    }

    // If we got a path, we'll perform file detection for autosuggestion hinting.
    history_identifier_t identifier = 0;
    if (!potential_paths.empty() && !impending_exit) {
        // Grab the next identifier.
        static history_identifier_t sLastIdentifier = 0;
        identifier = ++sLastIdentifier;

        // Create a new detection context.
        file_detection_context_t *context = new file_detection_context_t(this, identifier);
        context->potential_paths.swap(potential_paths);

        // Prevent saving until we're done, so we have time to get the paths.
        this->disable_automatic_saving();

        // Kick it off. Even though we haven't added the item yet, it updates the item on the main
        // thread, so we can't race.
        iothread_perform(threaded_perform_file_detection, perform_file_detection_done, context);
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
    scoped_lock locker(lock);  //!OCLINT(side-effect)
    this->has_pending_item = false;
}
