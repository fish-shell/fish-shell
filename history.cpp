/** \file history.c
	History functions, part of the user interface.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "fallback.h"
#include "util.h"
#include "sanity.h"
#include "tokenizer.h"

#include "wutil.h"
#include "history.h"
#include "common.h"
#include "intern.h"
#include "path.h"
#include "signal.h"
#include "autoload.h"
#include "iothread.h"
#include <map>
#include <algorithm>

/*

Our history format is intended to be valid YAML. Here it is:

  - cmd: ssh blah blah blah
    when: 2348237
    paths:
      - /path/to/something
      - /path/to/something_else
    
  Newlines are replaced by \n. Backslashes are replaced by \\.
*/



/** When we rewrite the history, the number of items we keep */
#define HISTORY_SAVE_MAX (1024 * 256)

/** Interval in seconds between automatic history save */
#define SAVE_INTERVAL (5*60)

/** Number of new history entries to add before automatic history save */
#define SAVE_COUNT 5

/** Whether we print timing information */
#define LOG_TIMES 0

class time_profiler_t {
    const char *what;
    double start;
    public:
    
    time_profiler_t(const char *w) {
        if (LOG_TIMES) {
            what = w;
            start = timef();
        }
    }
    
    ~time_profiler_t() {
        if (LOG_TIMES) {
            double end = timef();
            printf("(LOG_TIMES %s: %02f msec)\n", what, (end - start) * 1000);
        }
    }
};

/* Our LRU cache is used for restricting the amount of history we have, and limiting how long we order it. */
class history_lru_node_t : public lru_node_t {
    public:
    time_t timestamp;
    path_list_t required_paths;
    history_lru_node_t(const history_item_t &item) :
        lru_node_t(item.str()),
        timestamp(item.timestamp()),
        required_paths(item.required_paths)
    {}
    
    bool write_to_file(FILE *f) const;
    bool write_yaml_to_file(FILE *f) const;
};

class history_lru_cache_t : public lru_cache_t<history_lru_node_t> {
    protected:
    
    /* Override to delete evicted nodes */
    virtual void node_was_evicted(history_lru_node_t *node) {
        delete node;
    }

    public:
    history_lru_cache_t(size_t max) : lru_cache_t<history_lru_node_t>(max) { }
    
    /* Function to add a history item */
    void add_item(const history_item_t &item) {
        /* Skip empty items */
        if (item.empty())
            return;
            
        /* See if it's in the cache. If it is, update the timestamp. If not, we create a new node and add it. Note that calling get_node promotes the node to the front. */
        history_lru_node_t *node = this->get_node(item.str());
        if (node != NULL) {
            node->timestamp = std::max(node->timestamp, item.timestamp());
            /* What to do about paths here? Let's just ignore them */
        } else {
            node = new history_lru_node_t(item);
            this->add_node(node);
        }
    }
};

static pthread_mutex_t hist_lock = PTHREAD_MUTEX_INITIALIZER;

static std::map<wcstring, history_t *> histories;

static wcstring history_filename(const wcstring &name, const wcstring &suffix);

/** Replaces newlines with a literal backslash followed by an n, and replaces backslashes with two backslashes. */
static void escape_yaml(std::string &str);

/** Undoes escape_yaml */
static void unescape_yaml(std::string &str);

/* Custom deleter for our shared_ptr */
class history_item_data_deleter_t {
    private:
        const bool free_it;
    public:
    history_item_data_deleter_t(bool flag) : free_it(flag) { }
    void operator()(const wchar_t *data) {
        if (free_it)
            free((void *)data);
    }
};

/* We can merge two items if they are the same command. We use the more recent timestamp and the longer list of required paths. */
bool history_item_t::merge(const history_item_t &item)
{
    bool result = false;
    if (this->contents == item.contents) {
        this->creation_timestamp = std::max(this->creation_timestamp, item.creation_timestamp);
        if (this->required_paths.size() < item.required_paths.size()) {
            this->required_paths = item.required_paths;
        }
        result = true;
    }
    return result;
}

history_item_t::history_item_t(const wcstring &str) : contents(str), creation_timestamp(time(NULL))
{
}

history_item_t::history_item_t(const wcstring &str, time_t when, const path_list_t &paths) : contents(str), creation_timestamp(when), required_paths(paths)
{
}

bool history_item_t::matches_search(const wcstring &term, enum history_search_type_t type) const {
    switch (type) {
    
        case HISTORY_SEARCH_TYPE_CONTAINS:
        /* We consider equal strings to NOT match a contains search (so that you don't have to see history equal to what you typed). The length check ensures that. */
            return contents.size() > term.size() && contents.find(term) != wcstring::npos;
            
        case HISTORY_SEARCH_TYPE_PREFIX:
            /* We consider equal strings to match a prefix search, so that autosuggest will allow suggesting what you've typed */
            return string_prefixes_string(term, contents);
            
        default:
            sanity_lose();
            return false;
    }
}

/* Output our YAML to a file */
bool history_lru_node_t::write_yaml_to_file(FILE *f) const {
    std::string cmd = wcs2string(key);
    escape_yaml(cmd);
    if (fprintf(f, "- cmd: %s\n", cmd.c_str()) < 0)
        return false;
        
    if (fprintf(f, "   when: %ld\n", (long)timestamp) < 0)
        return false;
    
    if (! required_paths.empty()) {
        if (fputs("   paths:\n", f) < 0)
            return false;
            
        for (path_list_t::const_iterator iter = required_paths.begin(); iter != required_paths.end(); ++iter) {
            std::string path = wcs2string(*iter);
            escape_yaml(path);
            
            if (fprintf(f, "    - %s\n", path.c_str()) < 0)
                return false;
        }
    }
    return true;
}


// Parse a timestamp line that looks like this: spaces, "when:", spaces, timestamp, newline
// The string is NOT null terminated; however we do know it contains a newline, so stop when we reach it
static bool parse_timestamp(const char *str, time_t *out_when) {
    const char *cursor = str;
    /* Advance past spaces */
    while (*cursor == ' ')
        cursor++;
        
    /* Look for "when:" */
    size_t when_len = 5;
    if (strncmp(cursor, "when:", when_len) != 0)
        return false;
    cursor += when_len;
    
    /* Advance past spaces */
    while (*cursor == ' ')
        cursor++;
    
    /* Try to parse a timestamp. */
    long timestamp = 0;
    if (isdigit(*cursor) && (timestamp = strtol(cursor, NULL, 0)) > 0) {
        *out_when = (time_t)timestamp;
        return true;
    }
    return false;
}

// Returns a pointer to the start of the next line, or NULL
// The next line must itself end with a newline
// Note that the string is not null terminated
static const char *next_line(const char *start, size_t length) {
    /* Handle the hopeless case */
    if (length < 1)
        return NULL;
    
    /* Get a pointer to the end, that we must not pass */
    const char * const end = start + length;
    
    /* Skip past the next newline */
    const char *nextline = (const char *)memchr(start, '\n', length);
    if (! nextline || nextline >= end) {
        return NULL;
    }
    /* Skip past the newline character itself */
    if (++nextline >= end) {
        return NULL;
    }
    
    /* Make sure this new line is itself "newline terminated". If it's not, return NULL; */
    const char *next_newline = (const char *)memchr(nextline, '\n', end - nextline);
    if (! next_newline) {
        return NULL;
    }
    
    /* Done */
    return nextline;
}

// Support for iteratively locating the offsets of history items
// Pass the address and length of a mapped region.
// Pass a pointer to a cursor size_t, initially 0
// If custoff_timestamp is nonzero, skip items created at or after that timestamp
// Returns (size_t)(-1) when done
static size_t offset_of_next_item(const char *begin, size_t mmap_length, size_t *inout_cursor, time_t cutoff_timestamp)
{
    size_t cursor = *inout_cursor;
    size_t result = (size_t)(-1);
    while (cursor < mmap_length) {
        const char * const line_start = begin + cursor;
        
        /* Advance the cursor to the next line */
        const char *newline = (const char *)memchr(line_start, '\n', mmap_length - cursor);
        if (newline == NULL)
            break;
        
        /* Advance the cursor past this line. +1 is for the newline */
        size_t line_len = newline - line_start;
        cursor += line_len + 1;

        /* Skip lines with a leading space, since these are in the interior of one of our items */
        if (line_start[0] == ' ')
            continue;
        
        /* Skip very short lines to make one of the checks below easier */
        if (line_len < 3)
            continue;
        
        /* Try to be a little YAML compatible. Skip lines with leading %, ---, or ... */
        if (! memcmp(line_start, "%", 1) ||
            ! memcmp(line_start, "---", 3) ||
            ! memcmp(line_start, "...", 3))
            continue;

        /* At this point, we know line_start is at the beginning of an item. But maybe we want to skip this item because of timestamps. A 0 cutoff means we don't care; if we do care, then try parsing out a timestamp. */
        if (cutoff_timestamp != 0) {
            /* Hackish fast way to skip items created after our timestamp. This is the mechanism by which we avoid "seeing" commands from other sessions that started after we started. We try hard to ensure that our items are sorted by their timestamps, so in theory we could just break, but I don't think that works well if (for example) the clock changes. So we'll read all subsequent items.
             */
            const char * const end = begin + mmap_length;
            
            /* Walk over lines that we think are interior. These lines are not null terminated, but are guaranteed to contain a newline. */
            bool has_timestamp = false;
            time_t timestamp;
            const char *interior_line;
            for (interior_line = next_line(line_start, end - line_start);
                 interior_line != NULL && ! has_timestamp;
                 interior_line = next_line(interior_line, end - interior_line)) {
                 
                /* If the first character is not a space, it's not an interior line, so we're done */
                if (interior_line[0] != ' ')
                    break;
                    
                /* Hackish optimization: since we just stepped over some interior line, update the cursor so we don't have to look at these lines next time */
                cursor = interior_line - begin;
                
                /* Try parsing a timestamp from this line. If we succeed, the loop will break. */
                has_timestamp = parse_timestamp(interior_line, &timestamp);
            }
                 
            /* Skip this item if the timestamp is at or after our cutoff. */
            if (has_timestamp && timestamp >= cutoff_timestamp) {
                continue;
            }
        }

        /* We made it through the gauntlet. */
        result = line_start - begin;
        break;
    }
    *inout_cursor = cursor;
    return result;
}


history_t & history_t::history_with_name(const wcstring &name) {
    /* Note that histories are currently never deleted, so we can return a reference to them without using something like shared_ptr */
    scoped_lock locker(hist_lock);
    history_t *& current = histories[name];
    if (current == NULL)
        current = new history_t(name);
    return *current;
}

history_t::history_t(const wcstring &pname) :
    name(pname),
    unsaved_item_count(0),
    mmap_start(NULL),
    mmap_length(0),
    birth_timestamp(time(NULL)),
    save_timestamp(0),
    loaded_old(false)
{
    pthread_mutex_init(&lock, NULL);
}

history_t::~history_t()
{
    pthread_mutex_destroy(&lock);
}

void history_t::add(const history_item_t &item)
{
    scoped_lock locker(lock);
    
    /* Try merging with the last item */
    if (! new_items.empty() && new_items.back().merge(item)) {
        /* We merged, so we don't have to add anything */
    }
    else
    {
        /* We have to add a new item */
        new_items.push_back(item);
        unsaved_item_count++;
    }
    
    /* Prevent the first write from always triggering a save */
    time_t now = time(NULL);
    if (! save_timestamp)
        save_timestamp = now;
    
    /* This might be a good candidate for moving to a background thread */
    if((now > save_timestamp + SAVE_INTERVAL) || (unsaved_item_count >= SAVE_COUNT)) {
        time_profiler_t profiler("save_internal");
		this->save_internal();
    }

}

void history_t::add(const wcstring &str, const path_list_t &valid_paths)
{
    this->add(history_item_t(str, time(NULL), valid_paths));
}

void history_t::get_string_representation(wcstring &result, const wcstring &separator)
{
    scoped_lock locker(lock);
    
    bool first = true;
    
    /* Append new items */
    for (size_t i=0; i < new_items.size(); i++) {
        if (! first)
            result.append(separator);
        result.append(new_items.at(i).str());
        first = false;
    }
    
    /* Append old items */
    load_old_if_needed();
    for (std::deque<size_t>::const_reverse_iterator iter = old_item_offsets.rbegin(); iter != old_item_offsets.rend(); ++iter) {        
        size_t offset = *iter;
        const history_item_t item = history_t::decode_item(mmap_start + offset, mmap_length - offset);
        if (! first)
            result.append(separator);
        result.append(item.str());
        first = false;
    }
}

history_item_t history_t::item_at_index(size_t idx) {
    scoped_lock locker(lock);
    
    /* 0 is considered an invalid index */
    assert(idx > 0);
    idx--;
    
    /* idx=0 corresponds to last item in new_items */
    size_t new_item_count = new_items.size();
    if (idx < new_item_count) {
        return new_items.at(new_item_count - idx - 1);
    }
    
    /* Now look in our old items */
    idx -= new_item_count;
    load_old_if_needed();
    size_t old_item_count = old_item_offsets.size();
    if (idx < old_item_count) {
        /* idx=0 corresponds to last item in old_item_offsets */
        size_t offset = old_item_offsets.at(old_item_count - idx - 1);
        return history_t::decode_item(mmap_start + offset, mmap_length - offset);
    }
    
    /* Index past the valid range, so return an empty history item */
    return history_item_t(wcstring(), 0);
}

/* Read one line, stripping off any newline, and updating cursor. Note that our input string is NOT null terminated; it's just a memory mapped file. */
static size_t read_line(const char *base, size_t cursor, size_t len, std::string &result) {
    /* Locate the newline */
    assert(cursor <= len);
    const char *start = base + cursor;
    const char *newline = (char *)memchr(start, '\n', len - cursor);
    if (newline != NULL) {
        /* We found a newline. */
        result.assign(start, newline - start);
        
        /* Return the amount to advance the cursor; skip over the newline */
        return newline - start + 1;
    } else {
        /* We ran off the end */
        result.clear();
        return len - cursor;
    }
}

/* Trims leading spaces in the given string, returning how many there were */
static size_t trim_leading_spaces(std::string &str) {
    size_t i = 0, max = str.size();
    while (i < max && str[i] == ' ')
        i++;
    str.erase(0, i);
    return i;
}

static bool extract_prefix(std::string &key, std::string &value, const std::string &line) {
    size_t where = line.find(":");
    if (where != std::string::npos) {
        key = line.substr(0, where);
        
        // skip a space after the : if necessary
        size_t val_start = where + 1;
        if (val_start < line.size() && line.at(val_start) == ' ')
            val_start++;
        value = line.substr(val_start);
        
        unescape_yaml(key);
        unescape_yaml(value);
    }
    return where != std::string::npos;
}

history_item_t history_t::decode_item(const char *base, size_t len) {
    wcstring cmd;
    time_t when = 0;
    path_list_t paths;
    
    size_t indent = 0, cursor = 0;
    std::string key, value, line;
    
    /* Read the "- cmd:" line */
    size_t advance = read_line(base, cursor, len, line);
    trim_leading_spaces(line);
    if (! extract_prefix(key, value, line) || key != "- cmd")
        goto done;
    
    cursor += advance;
    cmd = str2wcstring(value);

    /* Read the remaining lines */
    for (;;) {
        /* Read a line */
        size_t advance = read_line(base, cursor, len, line);
        
        /* Count and trim leading spaces */
        size_t this_indent = trim_leading_spaces(line);
        if (indent == 0)
            indent = this_indent;
        
        if (this_indent == 0 || indent != this_indent)
            break;
        
        if (! extract_prefix(key, value, line))
            break;
            
        /* We are definitely going to consume this line */
        unescape_yaml(value);
        cursor += advance;
        
        if (key == "when") {
            /* Parse an int from the timestamp */
            long tmp = 0;
            if (sscanf(value.c_str(), "%ld", &tmp) > 0) {
                when = tmp;
            }
        } else if (key == "paths") {
            /* Read lines starting with " - " until we can't read any more */
            for (;;) {
                size_t advance = read_line(base, cursor, len, line);
                if (trim_leading_spaces(line) <= indent)
                    break;
                    
                if (strncmp(line.c_str(), "- ", 2))
                    break;
                    
                /* We're going to consume this line */
                cursor += advance;
                

                /* Skip the leading dash-space and then store this path it */
                line.erase(0, 2);
                unescape_yaml(line);
                paths.push_front(str2wcstring(line));
            }
        }
    }
    /* Reverse the paths, since we pushed them to the front each time */
done:
    paths.reverse();
    return history_item_t(cmd, when, paths);
    
}

void history_t::populate_from_mmap(void)
{
    size_t cursor = 0;
    for (;;) {
        size_t offset = offset_of_next_item(mmap_start, mmap_length, &cursor, birth_timestamp);
        // If we get back -1, we're done
        if (offset == (size_t)(-1))
            break;

        // Remember this item
        old_item_offsets.push_back(offset);        
    }
}

// Do a private, read-only map of the entirety of a history file with the given name. Returns true if successful. Returns the mapped memory region by reference.
static bool map_file(const wcstring &name, const char **out_map_start, size_t *out_map_len)
{
    bool result = false;
    wcstring filename = history_filename(name, L"");
    if (! filename.empty())
    {
        int fd;
		if((fd = wopen_cloexec(filename, O_RDONLY)) > 0)
		{
			off_t len = lseek( fd, 0, SEEK_END );
			if(len != (off_t)-1)
			{
				size_t mmap_length = (size_t)len;
				if(lseek(fd, 0, SEEK_SET) == 0)
				{
                    char *mmap_start;
					if ((mmap_start = (char *)mmap(0, mmap_length, PROT_READ, MAP_PRIVATE, fd, 0)) != MAP_FAILED)
					{
						result = true;
                        *out_map_start = mmap_start;
                        *out_map_len = mmap_length;
					}
				}
			}
			close( fd );
		}
    }
    return result;
}

bool history_t::load_old_if_needed(void)
{
    if (loaded_old) return true;
    loaded_old = true;
    
    
    // PCA not sure why signals were blocked here
	//signal_block();
    
	bool ok = false;    
    if (map_file(name, &mmap_start, &mmap_length)) {
        // Here we've mapped the file
        ok = true;
        time_profiler_t profiler("populate_from_mmap");
        this->populate_from_mmap();
    } 
    
	//signal_unblock();
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
    /* Pop the top index (if more than one) and return if we have any left */
    if (prev_matches.size() > 1) {
        prev_matches.pop_back();
        return true;
    }
    return false;
}

bool history_search_t::go_backwards() {
    /* Backwards means increasing our index */
    const size_t max_idx = (size_t)(-1);

    size_t idx = 0;
    if (! prev_matches.empty())
        idx = prev_matches.back().first;
    
    if (idx == max_idx)
        return false;
        
    while (++idx < max_idx) {
        const history_item_t item = history->item_at_index(idx);
        /* We're done if it's empty */
        if (item.empty()) {
            return false;
        }

        /* Look for a term that matches and that we haven't seen before */
        const wcstring &str = item.str();
        if (item.matches_search(term, search_type) && ! match_already_made(str) && ! should_skip_match(str)) {
            prev_matches.push_back(prev_match_t(idx, item));
            return true;
        }
    }
    return false;
}

/** Goes to the end (forwards) */
void history_search_t::go_to_end(void) {
    prev_matches.clear();
}

/** Returns if we are at the end, which is where we start. */
bool history_search_t::is_at_end(void) const {
    return prev_matches.empty();
}


/** Goes to the beginning (backwards) */
void history_search_t::go_to_beginning(void) {
    /* Just go backwards as far as we can */
    while (go_backwards())
        ;
}


history_item_t history_search_t::current_item() const {
    assert(! prev_matches.empty()); 
    return prev_matches.back().second;
}

wcstring history_search_t::current_string() const {
    history_item_t item = this->current_item();
    return item.str();
}

bool history_search_t::match_already_made(const wcstring &match) const {
    for (std::deque<prev_match_t>::const_iterator iter = prev_matches.begin(); iter != prev_matches.end(); ++iter) {
        if (iter->second.str() == match)
            return true;
    }
    return false;
}

static void replace_all(std::string &str, const char *needle, const char *replacement)
{
    size_t needle_len = strlen(needle), replacement_len = strlen(replacement);
    size_t offset = 0;
    while((offset = str.find(needle, offset)) != std::string::npos)
    {
        str.replace(offset, needle_len, replacement);
        offset += replacement_len;
    }
}

static void escape_yaml(std::string &str) {
    replace_all(str, "\\", "\\\\"); //replace one backslash with two
    replace_all(str, "\n", "\\n"); //replace newline with backslash + literal n
}

static void unescape_yaml(std::string &str) {
    bool prev_escape = false;
    for (size_t idx = 0; idx < str.size(); idx++) {
        char c = str.at(idx);
        if (prev_escape) {
            if (c == '\\') {
                /* Two backslashes in a row. Delete this one */
                str.erase(idx, 1);
                idx--;
            } else if (c == 'n') {
                /* Replace backslash + n with an actual newline */
                str.replace(idx - 1, 2, "\n");
                idx--;
            }
            prev_escape = false;
        } else {
            prev_escape = (c == '\\');
        }
    }
}

static wcstring history_filename(const wcstring &name, const wcstring &suffix)
{
    wcstring path;
    if (! path_get_config(path))
        return L"";
    
    wcstring result = path;
    result.append(L"/");
    result.append(name);
    result.append(L"_history");
    result.append(suffix);
    return result;
}

void history_t::clear_file_state()
{
    /* Erase everything we know about our file */
    if (mmap_start != NULL && mmap_start != MAP_FAILED) {
        munmap((void *)mmap_start, mmap_length);
    }
    mmap_start = NULL;
    mmap_length = 0;
    loaded_old = false;
    old_item_offsets.clear();
    save_timestamp=time(0);
}

void history_t::compact_new_items() {
    /* Keep only the most recent items with the given contents. This algorithm could be made more efficient, but likely would consume more memory too. */
    std::set<wcstring> seen;
    size_t idx = new_items.size();
    while (idx--) {
        const history_item_t &item = new_items[idx];
        if (! seen.insert(item.contents).second) {
            // This item was not inserted because it was already in the set, so delete the item at this index
            new_items.erase(new_items.begin() + idx);
        }
    }
}

/** Save the specified mode to file */
void history_t::save_internal()
{
    /* This must be called while locked */
    ASSERT_IS_LOCKED(lock);
    
    /* Nothing to do if there's no new items */
    if (new_items.empty())
        return;
        
    /* Compact our new items so we don't have duplicates */
    this->compact_new_items();
    
	bool ok = true;
    
    wcstring tmp_name = history_filename(name, L".tmp");    
	if( ! tmp_name.empty() )
	{        
        /* Make an LRU cache to save only the last N elements */
        history_lru_cache_t lru(HISTORY_SAVE_MAX);
        
        /* Insert old items in, from old to new. Merge them with our new items, inserting items with earlier timestamps first. */
        std::vector<history_item_t>::const_iterator new_item_iter = new_items.begin();
        
        /* Map in existing items (which may have changed out from underneath us, so don't trust our old mmap'd data) */
        const char *local_mmap_start = NULL;
        size_t local_mmap_size = 0;
        if (map_file(name, &local_mmap_start, &local_mmap_size)) {
            size_t cursor = 0;
            for (;;) {
                size_t offset = offset_of_next_item(local_mmap_start, local_mmap_size, &cursor, 0);
                /* If we get back -1, we're done */
                if (offset == (size_t)(-1))
                    break;

                /* Try decoding an old item */
                const history_item_t old_item = history_t::decode_item(local_mmap_start + offset, local_mmap_size - offset);
                if (old_item.empty())
                    continue;
                    
                /* The old item may actually be more recent than our new item, if it came from another session. Insert all new items at the given index with an earlier timestamp. */
                for (; new_item_iter != new_items.end(); ++new_item_iter) {
                    if (new_item_iter->timestamp() < old_item.timestamp()) {
                        /* This "new item" is in fact older. */
                        lru.add_item(*new_item_iter);
                    } else {
                        /* The new item is not older. */
                        break;
                    }
                }
                
                /* Now add this old item */
                lru.add_item(old_item);
            }
            munmap((void *)local_mmap_start, local_mmap_size);
        }
        
        /* Insert any remaining new items */
        for (; new_item_iter != new_items.end(); ++new_item_iter) {
            lru.add_item(*new_item_iter);
        }
                
        signal_block();
    
        FILE *out;
		if( (out=wfopen( tmp_name, "w" ) ) )
		{
            /* Write them out */
            for (history_lru_cache_t::iterator iter = lru.begin(); iter != lru.end(); ++iter) {
                const history_lru_node_t *node = *iter;
                if (! node->write_yaml_to_file(out)) {
                    ok = false;
                    break;
                }
            }
            
			if( fclose( out ) || !ok )
			{
				/*
				  This message does not have high enough priority to
				  be shown by default.
				*/
				debug( 2, L"Error when writing history file" );
			}
			else
			{
                wcstring new_name = history_filename(name, wcstring());
				wrename(tmp_name, new_name);
			}
		}
        
        signal_unblock();
        
        /* Make sure we clear all nodes, since this doesn't happen automatically */
        lru.evict_all_nodes();
        
        /* We've saved everything, so we have no more unsaved items */
        unsaved_item_count = 0;
	}	

	if( ok )
	{
		/* Our history has been written to the file, so clear our state so we can re-reference the file. */
		this->clear_file_state();
	}
}

void history_t::save(void) {
    scoped_lock locker(lock);
    this->save_internal();
}

void history_t::clear(void) {
    scoped_lock locker(lock);
    new_items.clear();
    unsaved_item_count = 0;
    old_item_offsets.clear();
    wcstring filename = history_filename(name, L"");
    if (! filename.empty())
        wunlink(filename);
    this->clear_file_state();
    
}

void history_init()
{
}


void history_destroy()
{
    /* Save all histories */
    for (std::map<wcstring, history_t *>::iterator iter = histories.begin(); iter != histories.end(); ++iter) {
        iter->second->save();
    }
}


void history_sanity_check()
{
	/*
	  No sanity checking implemented yet...
	*/
}

int file_detection_context_t::perform_file_detection(bool test_all) {
    ASSERT_IS_BACKGROUND_THREAD();
    valid_paths.clear();
    int result = 1;
    for (path_list_t::const_iterator iter = potential_paths.begin(); iter != potential_paths.end(); ++iter) {    
        if (path_is_valid(*iter, working_directory)) {
            /* Push the original (possibly relative) path */
            valid_paths.push_front(*iter);
        } else {
            /* Not a valid path */
            result = 0;
            if (! test_all)
                break;
        }
    }
    valid_paths.reverse();
    return result;
}

bool file_detection_context_t::paths_are_valid(const path_list_t &paths) {
    this->potential_paths = paths;
    return perform_file_detection(false) > 0;
}

file_detection_context_t::file_detection_context_t(history_t *hist, const wcstring &cmd) :
    history(hist),
    command(cmd),
    when(time(NULL)),
    working_directory(get_working_directory())
{
}

static int threaded_perform_file_detection(file_detection_context_t *ctx) {
    ASSERT_IS_BACKGROUND_THREAD();
    assert(ctx != NULL);
    return ctx->perform_file_detection(true /* test all */);
}

static void perform_file_detection_done(file_detection_context_t *ctx, int success) {
    /* Now that file detection is done, create the history item */
    ctx->history->add(ctx->command, ctx->valid_paths);
    
    /* Done with the context. */
    delete ctx;
}

static bool string_could_be_path(const wcstring &potential_path) {
    // Assume that things with leading dashes aren't paths
    if (potential_path.empty() || potential_path.at(0) == L'-')
        return false;
    return true;
}

void history_t::add_with_file_detection(const wcstring &str)
{
    ASSERT_IS_MAIN_THREAD();
    path_list_t potential_paths;
    
    tokenizer tokenizer;
    for( tok_init( &tokenizer, str.c_str(), TOK_SQUASH_ERRORS );
        tok_has_next( &tokenizer );
        tok_next( &tokenizer ) )
    {
        int type = tok_last_type( &tokenizer );
        if (type == TOK_STRING) {
            const wchar_t *token_cstr = tok_last(&tokenizer);
            if (token_cstr) {
                wcstring potential_path = token_cstr;
                if (unescape_string(potential_path, false) && string_could_be_path(potential_path)) {
                    potential_paths.push_front(potential_path);
                }
            }
        }
    }
    tok_destroy(&tokenizer);
    
    if (! potential_paths.empty()) {
        /* We have some paths. Make a context. */
        file_detection_context_t *context = new file_detection_context_t(this, str);
                        
        /* Store the potential paths. Reverse them to put them in the same order as in the command. */
        potential_paths.reverse();
        context->potential_paths.swap(potential_paths);
        iothread_perform(threaded_perform_file_detection, perform_file_detection_done, context);
    }
}

