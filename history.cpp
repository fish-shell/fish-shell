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

#include "wutil.h"
#include "history.h"
#include "common.h"
#include "halloc.h"
#include "halloc_util.h"
#include "intern.h"
#include "path.h"
#include "signal.h"
#include "autoload.h"
#include <map>
#include <algorithm>

/** When we rewrite the history, the number of items we keep */
#define HISTORY_SAVE_MAX (1024 * 256)

/** Interval in seconds between automatic history save */
#define SAVE_INTERVAL (5*60)

/** Number of new history entries to add before automatic history save */
#define SAVE_COUNT 5

/* Our LRU cache is used for restricting the amount of history we have, and limiting how long we order it. */
class history_lru_node_t : public lru_node_t {
    public:
    time_t timestamp;
    history_lru_node_t(const history_item_t &item) : lru_node_t(item.str()), timestamp(item.timestamp()) {}
    
    bool write_to_file(FILE *f) const;
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
        } else {
            node = new history_lru_node_t(item);
            this->add_node(node);
        }
    }
};

static pthread_mutex_t hist_lock = PTHREAD_MUTEX_INITIALIZER;

static std::map<wcstring, history_t *> histories;

static wcstring history_filename(const wcstring &name, const wcstring &suffix);

/* Unescapes newlines in-place */
static void unescape_newlines(wcstring &str);

/* Escapes newlines in-place */
static void escape_newlines(wcstring &str);

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

history_item_t::history_item_t(const wcstring &str) : contents(str), creation_timestamp(time(NULL))
{
}

history_item_t::history_item_t(const wcstring &str, time_t when) : contents(str), creation_timestamp(when)
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

bool history_lru_node_t::write_to_file(FILE *f) const {
    wcstring escaped = key;
    escape_newlines(escaped);
	return fwprintf( f, L"# %d\n%ls\n", timestamp, escaped.c_str());
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
    mmap_start(NULL),
    mmap_length(0),
    save_timestamp(0),
    loaded_old(false)
{
    pthread_mutex_init(&lock, NULL);
}

history_t::~history_t()
{
    pthread_mutex_destroy(&lock);
}

void history_t::add(const wcstring &str)
{
    scoped_lock locker(lock);
    
    new_items.push_back(history_item_t(str.c_str(), true));
    
    /* This might be a good candidate for moving to a background thread */
    if((time(0) > save_timestamp + SAVE_INTERVAL) || (new_items.size() >= SAVE_COUNT))
		this->save_internal();
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

history_item_t history_t::decode_item(const char *begin, size_t len)
{
    const char *pos = begin, *end = begin + len;
    
    int was_backslash = 0;
    
    wcstring output;
    time_t timestamp = 0;
    
    int first_char = 1;	
    bool timestamp_mode = false;
    
    while( 1 )
    {
        wchar_t c;
        mbstate_t state;
        bzero(&state, sizeof state);
        
        size_t res;
        
        res = mbrtowc( &c, pos, end-pos, &state );
        
        if( res == (size_t)-1 )
        {
            pos++;
            continue;
        }
        else if( res == (size_t)-2 )
        {
            break;
        }
        else if( res == (size_t)0 )
        {
            pos++;
            continue;
        }
        pos += res;
        
        if( c == L'\n' )
        {
            if( timestamp_mode )
            {
                const wchar_t *time_string = output.c_str();
                while( *time_string && !iswdigit(*time_string))
                    time_string++;
                errno=0;
                
                if( *time_string )
                {
                    time_t tm;
                    wchar_t *end;
                    
                    errno = 0;
                    tm = (time_t)wcstol( time_string, &end, 10 );
                    
                    if( tm && !errno && !*end )
                    {
                        timestamp = tm;
                    }
                    
                }					
                
                output.clear();
                timestamp_mode = 0;
                continue;
            }
            if( !was_backslash )
                break;
        }
        
        if( first_char )
        {
            if( c == L'#' ) 
                timestamp_mode = 1;
        }
        
        first_char = 0;
        
        output.push_back(c);
        
        was_backslash = ( (c == L'\\') && !was_backslash);
        
    }
    
    unescape_newlines(output);
    return history_item_t(output, timestamp);
}

void history_t::populate_from_mmap(void)
{
	const char *begin = mmap_start;
	const char *end = begin + mmap_length;
	const char *pos;
	
	int ignore_newline = 0;
	int do_push = 1;
	
	for( pos = begin; pos <end; pos++ )
	{
		
		if( do_push )
		{
			ignore_newline = *pos == '#';
            /* Need to unique-ize */
            old_item_offsets.push_back(pos - begin);
			do_push = 0;
		}
		
		switch( *pos )
		{
			case '\\':
			{
				pos++;
				break;
			}
			
			case '\n':
			{
				if( ignore_newline )
				{
					ignore_newline = 0;
				}
				else
				{
					do_push = 1;
				}				
				break;
			}
		}
	}	
}

void history_t::load_old_if_needed(void)
{
    if (loaded_old) return;
    loaded_old = true;
    
	int fd;
	int ok=0;
	
	signal_block();
    wcstring filename = history_filename(name, L"");
	
	if( ! filename.empty() )
	{
		if( ( fd = wopen( filename.c_str(), O_RDONLY ) ) > 0 )
		{
			off_t len = lseek( fd, 0, SEEK_END );
			if( len != (off_t)-1)
			{
				mmap_length = (size_t)len;
				if( lseek( fd, 0, SEEK_SET ) == 0 )
				{
					if( (mmap_start = (char *)mmap( 0, mmap_length, PROT_READ, MAP_PRIVATE, fd, 0 )) != MAP_FAILED )
					{
						ok = 1;
						this->populate_from_mmap();
					}
				}
			}
			close( fd );
		}
	}
	signal_unblock();
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
        if (item.matches_search(term, search_type) && ! match_already_made(item.str())) {
            prev_matches.push_back(prev_match_t(idx, item.str()));
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


wcstring history_search_t::current_item() const {
    assert(! prev_matches.empty()); 
    return prev_matches.back().second;
}

bool history_search_t::match_already_made(const wcstring &match) const {
    for (std::deque<prev_match_t>::const_iterator iter = prev_matches.begin(); iter != prev_matches.end(); iter++) {
        if (iter->second == match)
            return true;
    }
    return false;
}


static void replace_all(wcstring &str, const wchar_t *needle, const wchar_t *replacement)
{
    size_t needle_len = wcslen(needle);
    size_t offset = 0;
    while((offset = str.find(needle, offset)) != wcstring::npos)
    {
        str.replace(offset, needle_len, replacement);
        offset += needle_len;
    }
}

static void unescape_newlines(wcstring &str)
{
    /* Replace instances of backslash + newline with just the newline */
    replace_all(str, L"\\\n", L"\n");
}

static void escape_newlines(wcstring &str)
{
    /* Replace instances of newline with backslash + newline with newline */
    replace_all(str, L"\\\n", L"\n");
    
    /* If the string ends with a backslash, we'll combine it with the next line. Hack around that by appending a newline. */
    if (! str.empty() && str.at(str.size() - 1) == L'\\')
        str.push_back('\n');
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

/** Save the specified mode to file */
void history_t::save_internal()
{
    /* This must be called while locked */
    
    /* Nothing to do if there's no new items */
    if (new_items.empty())
        return;
    
	bool ok = true;
	
	signal_block();
    
    wcstring tmp_name = history_filename(name, L".tmp");
	if( ! tmp_name.empty() )
	{
        FILE *out;
		if( (out=wfopen( tmp_name.c_str(), "w" ) ) )
		{
                
            /* Load old */
            load_old_if_needed();
            
            /* Make an LRU cache to save only the last N elements */
            history_lru_cache_t lru(HISTORY_SAVE_MAX);
            
            /* Insert old items in, from old to new */
            for (std::vector<size_t>::iterator iter = old_item_offsets.begin(); iter != old_item_offsets.end(); iter++) {
                size_t offset = *iter;
                history_item_t item = history_t::decode_item(mmap_start + offset, mmap_length - offset);
                lru.add_item(item);
            }
            
            /* Insert new items */
            for (std::vector<history_item_t>::iterator iter = new_items.begin(); iter != new_items.end(); iter++) {
                lru.add_item(*iter);
            }
            
            /* Write them out */
            for (history_lru_cache_t::iterator iter = lru.begin(); iter != lru.end(); iter++) {
                if (! (*iter)->write_to_file(out)) {
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
				wrename(tmp_name.c_str(), new_name.c_str());
			}
		}
	}	

	if( ok )
	{
		/* Our history has been written to the file, so clear our state so we can re-reference the file. */
		if (mmap_start != NULL && mmap_start != MAP_FAILED) {
			munmap((void *)mmap_start, mmap_length);
        }
        mmap_start = 0;
        mmap_length = 0;
        loaded_old = false;
        new_items.clear();
        old_item_offsets.clear();
		save_timestamp=time(0);
	}
	
	signal_unblock();
}

void history_t::save(void) {
    scoped_lock locker(lock);
    this->save_internal();
}

void history_init()
{
}


void history_destroy()
{
    /* Save all histories */
    for (std::map<wcstring, history_t *>::iterator iter = histories.begin(); iter != histories.end(); iter++) {
        iter->second->save();
    }
}


void history_sanity_check()
{
	/*
	  No sanity checking implemented yet...
	*/
}

