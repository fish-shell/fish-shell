/** \file complete.c Functions related to tab-completion.

	These functions are used for storing and retrieving tab-completion data, as well as for performing tab-completion.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <wchar.h>
#include <pthread.h>
#include <algorithm>

#include "fallback.h"
#include "util.h"

#include "tokenizer.h"
#include "wildcard.h"
#include "proc.h"
#include "parser.h"
#include "function.h"
#include "complete.h"
#include "builtin.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "common.h"
#include "reader.h"
#include "history.h"
#include "intern.h"
#include "parse_util.h"
#include "parser_keywords.h"
#include "wutil.h"
#include "path.h"
#include "builtin_scripts.h"

/*
  Completion description strings, mostly for different types of files, such as sockets, block devices, etc.

  There are a few more completion description strings defined in
  expand.c. Maybe all completion description strings should be defined
  in the same file?
*/

/**
   Description for ~USER completion
*/
#define COMPLETE_USER_DESC _( L"Home for %ls" )

/**
   Description for short variables. The value is concatenated to this description
*/
#define COMPLETE_VAR_DESC_VAL _( L"Variable: %ls" )

/**
   The maximum number of commands on which to perform description
   lookup. The lookup process is quite time consuming, so this should
   be set to a pretty low number.
*/
#define MAX_CMD_DESC_LOOKUP 10

/**
   Condition cache value returned from hashtable when this condition
   has not yet been tested. This value is NULL, so that when the hash
   table returns NULL, this wil be seen as an untested condition.
*/
#define CC_NOT_TESTED 0

/**
   Condition cache value returned from hashtable when the condition is
   met. This can be any value, that is a valid pointer, and that is
   different from CC_NOT_TESTED and CC_FALSE.
*/
#define CC_TRUE L"true"

/**
   Condition cache value returned from hashtable when the condition is
   not met. This can be any value, that is a valid pointer, and that
   is different from CC_NOT_TESTED and CC_TRUE.

*/
#define CC_FALSE L"false"

/**
   The special cased translation macro for completions. The empty
   string needs to be special cased, since it can occur, and should
   not be translated. (Gettext returns the version information as the
   response)
*/
#ifdef USE_GETTEXT
#define C_(wstr) ((wcscmp(wstr, L"")==0)?L"":wgettext(wstr))
#else
#define C_(string) (string)
#endif


/**
   The maximum amount of time that we're willing to spend doing
   username tilde completion. This special limit has been coded in
   because user lookup can be extremely slow in cases of a humongous
   LDAP database. (Google, I'm looking at you)
 */
#define MAX_USER_LOOKUP_TIME 0.2

/**
   Struct describing a completion option entry.

   If short_opt and long_opt are both zero, the comp field must not be
   empty and contains a list of arguments to the command.

   If either short_opt or long_opt are non-zero, they specify a switch
   for the command. If \c comp is also not empty, it contains a list
   of non-switch arguments that may only follow directly after the
   specified switch.   
*/
typedef struct complete_entry_opt
{
	/** Short style option */
	wchar_t short_opt;
	/** Long style option */
	wcstring long_opt;
	/** Arguments to the option */
	wcstring comp;
	/** Description of the completion */
	wcstring desc;
	/** Condition under which to use the option */
	wcstring condition;
	/** Must be one of the values SHARED, NO_FILES, NO_COMMON,
		EXCLUSIVE, and determines how completions should be performed
		on the argument after the switch. */
	int result_mode;
	/** True if old style long options are used */
	int old_mode;
	/** Completion flags */
	complete_flags_t flags;
    
    const wchar_t *localized_desc() const
    {
        const wchar_t *tmp = desc.c_str();
        return C_(tmp);
    }
} complete_entry_opt_t;

/* Last value used in the order field of completion_entry_t */
static unsigned int kCompleteOrder = 0;

/**
   Struct describing a command completion
*/
typedef std::list<complete_entry_opt_t> option_list_t;
class completion_entry_t
{    
public:
	/** List of all options */
	option_list_t options;
    
	/** String containing all short option characters */
	wcstring short_opt_str;
    
    public:
    
	/** Command string */
	const wcstring cmd;
    
	/** True if command is a path */
	const bool cmd_is_path;
                    
	/** True if no other options than the ones supplied are possible */
	bool authoritative;
    
    /** Order for when this completion was created. This aids in outputting completions sorted by time. */
    const unsigned int order;
    
    /** Getters for option list. */
    const option_list_t &get_options() const;
    
    /** Adds or removes an option. */
    void add_option(const complete_entry_opt_t &opt);
    bool remove_option(wchar_t short_opt, const wchar_t *long_opt);
    
    /** Getter for short_opt_str. */
    wcstring &get_short_opt_str();    
    const wcstring &get_short_opt_str() const;
    
    completion_entry_t(const wcstring &c, bool type, const wcstring &options, bool author) :
        short_opt_str(options),
        cmd(c),
        cmd_is_path(type),
        authoritative(author),
        order(++kCompleteOrder)
    {
    }
};

/** Set of all completion entries */
struct completion_entry_set_comparer {
    /** Comparison for std::set */
    bool operator()(completion_entry_t *p1, completion_entry_t *p2) const {
        /* Paths always come last for no particular reason */
        if (p1->cmd_is_path != p2->cmd_is_path) {
            return p1->cmd_is_path < p2->cmd_is_path;
        } else {
            return p1->cmd < p2->cmd;
        }
    }
};
typedef std::set<completion_entry_t *, completion_entry_set_comparer> completion_entry_set_t;
static completion_entry_set_t completion_set;

// Comparison function to sort completions by their order field
static bool compare_completions_by_order(const completion_entry_t *p1, const completion_entry_t *p2) {
    return p1->order < p2->order;
}

/** The lock that guards the list of completion entries */
static pthread_mutex_t completion_lock = PTHREAD_MUTEX_INITIALIZER;

/** The lock that guards the options list of individual completion entries. If both completion_lock and completion_entry_lock are to be taken, completion_lock must be taken first. */
static pthread_mutex_t completion_entry_lock = PTHREAD_MUTEX_INITIALIZER;


void completion_entry_t::add_option(const complete_entry_opt_t &opt) {
    ASSERT_IS_LOCKED(completion_entry_lock);
    options.push_front(opt);
}

const option_list_t &completion_entry_t::get_options() const {
    ASSERT_IS_LOCKED(completion_entry_lock);
    return options;
}

wcstring &completion_entry_t::get_short_opt_str() {
    ASSERT_IS_LOCKED(completion_entry_lock);
    return short_opt_str;
}

const wcstring &completion_entry_t::get_short_opt_str() const {
    ASSERT_IS_LOCKED(completion_entry_lock);
    return short_opt_str;    
}

/* completion_t functions */
completion_t::completion_t(const wcstring &comp, const wcstring &desc, int flags_val) : completion(comp), description(desc), flags(flags_val)
{
    if( flags & COMPLETE_AUTO_SPACE )
    {
        flags = flags & ~COMPLETE_AUTO_SPACE;
        size_t len = completion.size();
        if (len > 0  && ( wcschr( L"/=@:", comp.at(len-1)) != 0 ))
            flags |= COMPLETE_NO_SPACE;
    }

}

completion_t::completion_t(const completion_t &him) : completion(him.completion), description(him.description), flags(him.flags)
{
}

completion_t &completion_t::operator=(const completion_t &him)
{
    if (this != &him)
    {
        this->completion = him.completion;
        this->description = him.description;
        this->flags = him.flags;
    }
    return *this;
}

bool completion_t::operator < (const completion_t& rhs) const
{
    return this->completion < rhs.completion;
}

bool completion_t::operator == (const completion_t& rhs) const
{
    return this->completion == rhs.completion;
}

bool completion_t::operator != (const completion_t& rhs) const
{
    return ! (*this == rhs);
}

wcstring_list_t completions_to_wcstring_list( const std::vector<completion_t> &list )
{
    wcstring_list_t strings;
    strings.reserve(list.size());
    for (std::vector<completion_t>::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
        strings.push_back(iter->completion);
    }
    return strings;
}

void sort_completions( std::vector<completion_t> &completions)
{
    std::sort(completions.begin(), completions.end());
}

/** Class representing an attempt to compute completions */
class completer_t {
    const complete_type_t type;
    const wcstring initial_cmd;
    std::vector<completion_t> completions;
    wcstring_list_t commands_to_load;
    
    /** Table of completions conditions that have already been tested and the corresponding test results */
    typedef std::map<wcstring, bool> condition_cache_t;
    condition_cache_t condition_cache;
    
    public:
    completer_t(const wcstring &c, complete_type_t t) :
        type(t),
        initial_cmd(c)
    {
    }
    
    bool empty() const { return completions.empty(); }
    const std::vector<completion_t> &get_completions(void) { return completions; }
    
    bool try_complete_variable( const wcstring &str );
    bool try_complete_user( const wcstring &str );

    bool complete_param( const wcstring &cmd_orig,
                         const wcstring &popt,
                         const wcstring &str,
                         bool use_switches);
                         
    void complete_param_expand(const wcstring &str, bool do_file);
    
    void debug_print_completions();
    
    void complete_cmd( const wcstring &str,
                       bool use_function,
                       bool use_builtin,
                       bool use_command);
                       
    void complete_from_args( const wcstring &str,
                             const wcstring &args,
                             const wcstring &desc,
                             complete_flags_t flags );
                       
    void complete_cmd_desc( const wcstring &str );
                          
    bool complete_variable(const wcstring &str, size_t start_offset);
    
    bool condition_test( const wcstring &condition );
    
    void complete_strings( const wcstring &wc_escaped,
                           const wchar_t *desc,
                           wcstring (*desc_func)(const wcstring &),
                           std::vector<completion_t> &possible_comp,
                           complete_flags_t flags );
    
    expand_flags_t expand_flags() const {
        /* Never do command substitution in autosuggestions. Sadly, we also can't yet do job expansion because it's not thread safe. */
        expand_flags_t result = 0;
        if (type == COMPLETE_AUTOSUGGEST)
            result |= EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_JOBS;
        return result;
    }
    
    void get_commands_to_load(wcstring_list_t *lst) {
        if (lst)
            lst->insert(lst->end(), commands_to_load.begin(), commands_to_load.end());
    }

};

/* Autoloader for completions */
class completion_autoload_t : public autoload_t {
public:
    completion_autoload_t();
    virtual void command_removed(const wcstring &cmd);
};

static completion_autoload_t completion_autoloader;

/** Constructor */
completion_autoload_t::completion_autoload_t() : autoload_t(L"fish_complete_path",
                                                            internal_completion_scripts,
                                                            sizeof internal_completion_scripts / sizeof *internal_completion_scripts)
{
}

/** Callback when an autoloaded completion is removed */
void completion_autoload_t::command_removed(const wcstring &cmd) {
    complete_remove( cmd.c_str(), COMMAND, 0, 0 );
}


/**
   Create a new completion entry

*/
void append_completion(std::vector<completion_t> &completions, const wcstring &comp, const wcstring &desc, complete_flags_t flags)
{
    completions.push_back(completion_t(comp, desc, flags));
}

/**
   Test if the specified script returns zero. The result is cached, so
   that if multiple completions use the same condition, it needs only
   be evaluated once. condition_cache_clear must be called after a
   completion run to make sure that there are no stale completions.
*/
bool completer_t::condition_test( const wcstring &condition )
{    
	if( condition.empty() )
	{
//		fwprintf( stderr, L"No condition specified\n" );
		return 1;
	}
 
    if (this->type == COMPLETE_AUTOSUGGEST)
    {
        /* Autosuggestion can't support conditions */
        return 0;
    }

    ASSERT_IS_MAIN_THREAD();
    
    bool test_res;
    condition_cache_t::iterator cached_entry = condition_cache.find(condition);
    if (cached_entry == condition_cache.end()) {
        /* Compute new value and reinsert it */
        test_res = (0 == exec_subshell( condition));
        condition_cache[condition] = test_res;
    } else {
        /* Use the old value */
        test_res = cached_entry->second;
    }
    return test_res;
}


/** Search for an exactly matching completion entry. Must be called while locked. */
static completion_entry_t *complete_find_exact_entry( const wcstring &cmd, const bool cmd_is_path )
{
    ASSERT_IS_LOCKED(completion_lock);
    completion_entry_t *result = NULL;
    completion_entry_t tmp_entry(cmd, cmd_is_path, L"", false);
    completion_entry_set_t::iterator iter = completion_set.find(&tmp_entry);
    if (iter != completion_set.end()) {
        result = *iter;
    }
	return result;
}

/** Locate the specified entry. Create it if it doesn't exist. Must be called while locked. */
static completion_entry_t *complete_get_exact_entry( const wcstring &cmd, bool cmd_is_path )
{
    ASSERT_IS_LOCKED(completion_lock);
	completion_entry_t *c;

	c = complete_find_exact_entry( cmd, cmd_is_path );

	if( c == NULL )
	{
        c = new completion_entry_t(cmd, cmd_is_path, L"", false);
        completion_set.insert(c);
	}

	return c;
}


void complete_set_authoritative( const wchar_t *cmd, bool cmd_is_path, bool authoritative )
{
	completion_entry_t *c;

	CHECK( cmd, );
    scoped_lock lock(completion_lock);
	c = complete_get_exact_entry( cmd, cmd_is_path );
	c->authoritative = authoritative;
}


void complete_add( const wchar_t *cmd,
				   bool cmd_is_path,
				   wchar_t short_opt,
				   const wchar_t *long_opt,
				   int old_mode,
				   int result_mode,
				   const wchar_t *condition,
				   const wchar_t *comp,
				   const wchar_t *desc,
				   complete_flags_t flags )
{
	CHECK( cmd, );
    
    /* Lock the lock that allows us to edit the completion entry list */
    scoped_lock lock(completion_lock);
    
    /* Lock the lock that allows us to edit individual completion entries */
    scoped_lock lock2(completion_entry_lock);
    
	completion_entry_t *c;
	c = complete_get_exact_entry( cmd, cmd_is_path );
        
    /* Create our new option */
    complete_entry_opt_t opt;
	if( short_opt != L'\0' )
	{
		int len = 1 + ((result_mode & NO_COMMON) != 0);
        
        c->get_short_opt_str().push_back(short_opt);
		if( len == 2 )
		{
            c->get_short_opt_str().push_back(L':');
		}
	}
	
	opt.short_opt = short_opt;
	opt.result_mode = result_mode;
	opt.old_mode=old_mode;

    if (comp) opt.comp = comp;
    if (condition) opt.condition = condition;
    if (long_opt) opt.long_opt = long_opt;
    if (desc) opt.desc = desc;
	opt.flags = flags;
    
    c->add_option(opt);
}

/**
   Remove all completion options in the specified entry that match the
   specified short / long option strings. Returns true if it is now
   empty and should be deleted, false if it's not empty. Must be called while locked.
*/
bool completion_entry_t::remove_option( wchar_t short_opt, const wchar_t *long_opt )
{
    ASSERT_IS_LOCKED(completion_lock);
    ASSERT_IS_LOCKED(completion_entry_lock);
	if(( short_opt == 0 ) && (long_opt == 0 ) )
	{
        this->options.clear();
	}
	else
	{
        for (option_list_t::iterator iter = this->options.begin(); iter != this->options.end(); )
		{
            complete_entry_opt_t &o = *iter;
			if(short_opt==o.short_opt || long_opt == o.long_opt)
			{
				/*			fwprintf( stderr,
							L"remove option -%lc --%ls\n",
							o->short_opt?o->short_opt:L' ',
							o->long_opt );
				*/
				if( o.short_opt )
				{
                    wcstring &short_opt_str = this->get_short_opt_str();
                    size_t idx = short_opt_str.find(o.short_opt);
                    if (idx != wcstring::npos)
                    {
                        /* Consume all colons */
                        size_t first_non_colon = idx + 1;
                        while (first_non_colon < short_opt_str.size() && short_opt_str.at(first_non_colon) == L':')
                            first_non_colon++;
                        short_opt_str.erase(idx, first_non_colon - idx);
                    }
				}
                
                /* Destroy this option and go to the next one */
				iter = this->options.erase(iter);
			}
			else
			{
                /* Just go to the next one */
				++iter;
			}
		}
	}
    return this->options.empty();
}


void complete_remove( const wchar_t *cmd,
					  bool cmd_is_path,
					  wchar_t short_opt,
					  const wchar_t *long_opt )
{
	CHECK( cmd, );
    scoped_lock lock(completion_lock);
    scoped_lock lock2(completion_entry_lock);
    
    completion_entry_t tmp_entry(cmd, cmd_is_path, L"", false);
    completion_entry_set_t::iterator iter = completion_set.find(&tmp_entry);
    if (iter != completion_set.end()) {
        completion_entry_t *entry = *iter;
        bool delete_it = entry->remove_option(short_opt, long_opt);
        if (delete_it) {
            /* Delete this entry */
            completion_set.erase(iter);
            delete entry;
        }
    }
}

/* Formats an error string by prepending the prefix and then appending the str in single quotes */
static wcstring format_error(const wchar_t *prefix, const wcstring &str) {
    wcstring result = prefix;
    result.push_back(L'\'');
    result.append(str);
    result.push_back(L'\'');
    return result;
}

/**
   Find the full path and commandname from a command string 'str'.
*/
static void parse_cmd_string(const wcstring &str, wcstring &path, wcstring &cmd) {
    if (! path_get_path(str, &path)) {
		/** Use the empty string as the 'path' for commands that can not be found. */
        path = L"";
    }
    
    /* Make sure the path is not included in the command */
    size_t last_slash = str.find_last_of(L'/');
    if (last_slash != wcstring::npos) {
        cmd = str.substr(last_slash + 1);
    } else {
        cmd = str;
    }
}

int complete_is_valid_option( const wcstring &str,
							  const wcstring &opt,
							  wcstring_list_t *errors,
							  bool allow_autoload )
{
    wcstring cmd, path;
	bool found_match = false;
	bool authoritative = true;
	int opt_found=0;
	std::set<wcstring> gnu_match_set;
	bool is_gnu_opt=false;
	bool is_old_opt=false;
	bool is_short_opt=false;
	bool is_gnu_exact=false;
	size_t gnu_opt_len=0;
    
    if (opt.empty())
        return false;
    
    std::vector<bool> short_validated;
	/*
	  Check some generic things like -- and - options.
	*/
	switch( opt.size() )
	{

		case 0:
		case 1:
		{
			return true;
		}
		
		case 2:
		{
			if( opt == L"--" )
			{
				return true;
			}
			break;
		}
	}
	
	if( opt.at(0) != L'-' )
	{
		if( errors )
            errors->push_back(L"Option does not begin with a '-'");
		return false;
	}


    short_validated.resize(opt.size(), 0);
    
	is_gnu_opt = opt.at(1) == L'-';
	if( is_gnu_opt )
	{
        size_t opt_end = opt.find(L'=');
		if( opt_end != wcstring::npos )
		{
			gnu_opt_len = opt_end-2;
		}
		else
		{
			gnu_opt_len = opt.size() - 2;
		}
	}
	
	parse_cmd_string( str, path, cmd );

	/*
	  Make sure completions are loaded for the specified command
	*/
	if (allow_autoload) {
        complete_load( cmd, false );
    }

	scoped_lock lock(completion_lock);
    scoped_lock lock2(completion_entry_lock);
    for (completion_entry_set_t::const_iterator iter = completion_set.begin(); iter != completion_set.end(); ++iter)
	{
        const completion_entry_t *i = *iter;
		const wcstring &match = i->cmd_is_path ? path : cmd;

		if( !wildcard_match( match, i->cmd ) )
		{
			continue;
		}
		
		found_match = true;

		if (! i->authoritative)
		{
			authoritative = false;
			break;
		}

        const option_list_t &options = i->get_options();
		if( is_gnu_opt )
		{
            for (option_list_t::const_iterator iter = options.begin(); iter != options.end(); ++iter)
            {
                const complete_entry_opt_t &o = *iter;
				if (o.old_mode )
				{
					continue;
				}
				
                if (opt.compare(2, gnu_opt_len, o.long_opt) == 0)
				{
                    gnu_match_set.insert(o.long_opt);
					if (opt.compare(2, o.long_opt.size(), o.long_opt))
					{
						is_gnu_exact = true;
					}
				}
			}
		}
		else
		{
			/* Check for old style options */
            for (option_list_t::const_iterator iter = options.begin(); iter != options.end(); ++iter)
			{
                const complete_entry_opt_t &o = *iter;
                
				if( !o.old_mode )
					continue;


				if( opt.compare(1, wcstring::npos, o.long_opt )==0)
				{
					opt_found = true;
					is_old_opt = true;
					break;
				}

			}

			if( is_old_opt )
				break;

            for (size_t opt_idx = 1; opt_idx < opt.size(); opt_idx++)
			{
                const wcstring &short_opt_str = i->get_short_opt_str();
                size_t str_idx = short_opt_str.find(opt.at(opt_idx));
				if  (str_idx != wcstring::npos )
				{
					if (str_idx + 1 < short_opt_str.size() && short_opt_str.at(str_idx + 1) == L':' )
					{
						/*
						  This is a short option with an embedded argument,
						  call complete_is_valid_argument on the argument.
						*/
                        const wcstring nopt = L"-" + opt.substr(1, 1);
						short_validated.at(opt_idx) =
							complete_is_valid_argument( str, nopt, opt.substr(2));
					}
					else
					{
						short_validated.at(opt_idx) = true;
					}
				}
			}
		}
	}

	if( authoritative )
	{

		if( !is_gnu_opt && !is_old_opt )
			is_short_opt = 1;


		if( is_short_opt )
		{
			opt_found=1;
			for( size_t j=1; j<opt.size(); j++)
			{
				if ( !short_validated.at(j))
				{
					if( errors )
					{
                        const wcstring str = opt.substr(j, 1);
                        errors->push_back(format_error(_(L"Unknown option: "), str.c_str()));
					}

					opt_found = 0;
					break;
				}

			}
		}

		if( is_gnu_opt )
		{
			opt_found = is_gnu_exact || (gnu_match_set.size() == 1);
			if( errors && !opt_found )
			{
                const wchar_t *prefix;
                if( gnu_match_set.empty())
				{
                    prefix = _(L"Unknown option: ");
				}
				else
				{
                    prefix = _(L"Multiple matches for option: ");
				}
                errors->push_back(format_error(prefix, opt));
			}
		}
	}

    return (authoritative && found_match)?opt_found:true;
}

bool complete_is_valid_argument( const wcstring &str, const wcstring &opt, const wcstring &arg )
{
	return true;
}


/**
   Copy any strings in possible_comp which have the specified prefix
   to the completer's completion array. The prefix may contain wildcards.
   The output will consist of completion_t structs.

   There are three ways to specify descriptions for each
   completion. Firstly, if a description has already been added to the
   completion, it is _not_ replaced. Secondly, if the desc_func
   function is specified, use it to determine a dynamic
   completion. Thirdly, if none of the above are available, the desc
   string is used as a description.

   \param wc_escaped the prefix, possibly containing wildcards. The wildcard should not have been unescaped, i.e. '*' should be used for any string, not the ANY_STRING character.
   \param desc the default description, used for completions with no embedded description. The description _may_ contain a COMPLETE_SEP character, if not, one will be prefixed to it
   \param desc_func the function that generates a description for those completions witout an embedded description
   \param possible_comp the list of possible completions to iterate over
*/

void completer_t::complete_strings( const wcstring &wc_escaped,
                                    const wchar_t *desc,
                                    wcstring (*desc_func)(const wcstring &),
                                    std::vector<completion_t> &possible_comp,
                                    complete_flags_t flags )
{
    wcstring tmp = wc_escaped;
    if (! expand_one(tmp, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_WILDCARDS | this->expand_flags()))
        return;
    
	const wchar_t *wc = parse_util_unescape_wildcards( tmp.c_str() );
	
	for( size_t i=0; i< possible_comp.size(); i++ )
	{
		wcstring temp = possible_comp.at( i ).completion;
		const wchar_t *next_str = temp.empty()?NULL:temp.c_str();

		if( next_str )
		{
			wildcard_complete( next_str, wc, desc, desc_func, this->completions, flags );
		}
	}

	free( (void *)wc );
}

/**
   If command to complete is short enough, substitute
   the description with the whatis information for the executable.
*/
void completer_t::complete_cmd_desc( const wcstring &str )
{
    ASSERT_IS_MAIN_THREAD();
    
	const wchar_t *cmd_start;
	int skip;
	
    const wchar_t * const cmd = str.c_str();
	cmd_start=wcsrchr(cmd, L'/');

	if( cmd_start )
		cmd_start++;
	else
		cmd_start = cmd;

	/*
	  Using apropos with a single-character search term produces far
	  to many results - require at least two characters if we don't
	  know the location of the whatis-database.
	*/
	if (wcslen(cmd_start) < 2)
		return;

	if( wildcard_has( cmd_start, 0 ) )
	{
		return;
	}

	skip = 1;
	
	for( size_t i=0; i< this->completions.size(); i++ )
	{
		const completion_t &c = this->completions.at ( i );
			
		if( c.completion.empty() || (c.completion[c.completion.size()-1] != L'/' )) 
		{
			skip = 0;
			break;
		}
		
	}
	
	if( skip )
	{
		return;
	}
	

    wcstring lookup_cmd(L"__fish_describe_command ");
    lookup_cmd.append(escape_string(cmd_start, 1));

    std::map<wcstring, wcstring> lookup;

	/*
	  First locate a list of possible descriptions using a single
	  call to apropos or a direct search if we know the location
	  of the whatis database. This can take some time on slower
	  systems with a large set of manuals, but it should be ok
	  since apropos is only called once.
	*/
    wcstring_list_t list;
	if( exec_subshell( lookup_cmd, list ) != -1 )
	{
	
		/*
		  Then discard anything that is not a possible completion and put
		  the result into a hashtable with the completion as key and the
		  description as value.

		  Should be reasonably fast, since no memory allocations are needed.
		*/
		for( size_t i=0; i < list.size(); i++ )
		{
            const wcstring &elstr = list.at(i);
            
            const wcstring fullkey(elstr, wcslen(cmd_start));
            
            size_t tab_idx = fullkey.find(L'\t');
			if( tab_idx == wcstring::npos )
				continue;
            
            const wcstring key(fullkey, 0, tab_idx);
            wcstring val(fullkey, tab_idx + 1);

			/*
			  And once again I make sure the first character is uppercased
			  because I like it that way, and I get to decide these
			  things.
			*/
            if (! val.empty())
                val[0]=towupper(val[0]);
		
            lookup[key] = val;
		}

		/*
		  Then do a lookup on every completion and if a match is found,
		  change to the new description.

		  This needs to do a reallocation for every description added, but
		  there shouldn't be that many completions, so it should be ok.
		*/
		for( size_t i=0; i<this->completions.size(); i++ )
		{
            completion_t &completion = this->completions.at(i);
            const wcstring &el = completion.completion;
            if (el.empty())
                continue;
            
            std::map<wcstring, wcstring>::iterator new_desc_iter = lookup.find(el);
            if (new_desc_iter != lookup.end())
                completion.description = new_desc_iter->second;
		}
	}
	
}

/**
   Returns a description for the specified function, or an empty string if none
*/
static wcstring complete_function_desc( const wcstring &fn )
{
    wcstring result;
    bool has_description = function_get_desc(fn, &result);
    if (! has_description) {
        function_get_definition(fn, &result);
    }
	return result;
}


/**
   Complete the specified command name. Search for executables in the
   path, executables defined using an absolute path, functions,
   builtins and directories for implicit cd commands.

   \param cmd the command string to find completions for

   \param comp the list to add all completions to
*/
void completer_t::complete_cmd( const wcstring &str_cmd, bool use_function, bool use_builtin, bool use_command)
{
    /* Paranoia */
    if (str_cmd.empty())
        return;
        
	std::vector<completion_t> possible_comp;

    env_var_t cdpath = env_get_string(L"CDPATH");
    if (cdpath.missing_or_empty())
        cdpath = L".";
    
    const bool wants_description = (type == COMPLETE_DEFAULT);
    
    if (str_cmd.find(L'/') != wcstring::npos || str_cmd.at(0) == L'~')
	{

		if( use_command )
		{
			
			if( expand_string(str_cmd, this->completions, ACCEPT_INCOMPLETE | EXECUTABLES_ONLY | this->expand_flags() ) != EXPAND_ERROR )
			{
                if (wants_description) {
                    this->complete_cmd_desc( str_cmd );
                }
			}
		}
	}
	else
	{
		if( use_command )
		{
			
			const env_var_t path = env_get_string(L"PATH");
			if( !path.missing() )
			{
                wcstring base_path;
                wcstokenizer tokenizer(path, ARRAY_SEP_STR);
				while (tokenizer.next(base_path))
				{
                    if (base_path.empty())
                        continue;
                        
                    /* Make sure the base path ends with a slash */
                    if (base_path.at(base_path.size() - 1) != L'/')
                        base_path.push_back(L'/');

                    wcstring nxt_completion = base_path;
                    nxt_completion.append(str_cmd);

                    size_t prev_count =  this->completions.size();
					if( expand_string( nxt_completion,
									   this->completions,
									   ACCEPT_INCOMPLETE | EXECUTABLES_ONLY | this->expand_flags()  ) != EXPAND_ERROR )
					{
                        /* For all new completions, if COMPLETE_NO_CASE is set, then use only the last path component */
						for( size_t i=prev_count; i< this->completions.size(); i++ )
						{
							completion_t &c =  this->completions.at( i );
							if(c.flags & COMPLETE_NO_CASE )
							{
                                
								c.completion.erase(0, base_path.size());
							}
						}
					}
				}
				if (wants_description)
                    this->complete_cmd_desc( str_cmd );
			}
		}
		
		/*
		  These return the original strings - don't free them
		*/

		if( use_function )
		{
			//function_get_names( &possible_comp, cmd[0] == L'_' );
            wcstring_list_t names = function_get_names(str_cmd.at(0) == L'_' );
            for (size_t i=0; i < names.size(); i++) {
                possible_comp.push_back(completion_t(names.at(i)));
            }
            
			this->complete_strings( str_cmd, 0, &complete_function_desc, possible_comp, 0 );
		}

		possible_comp.clear();
		
		if( use_builtin )
		{
			builtin_get_names( possible_comp );
			this->complete_strings( str_cmd, 0, &builtin_get_desc, possible_comp, 0 );
		}

	}
}


/**
   Evaluate the argument list (as supplied by complete -a) and insert
   any return matching completions. Matching is done using \c
   copy_strings_with_prefix, meaning the completion may contain
   wildcards. Logically, this is not always the right thing to do, but
   I have yet to come up with a case where this matters.

   \param str The string to complete.
   \param args The list of option arguments to be evaluated.
   \param desc Description of the completion
   \param comp_out The list into which the results will be inserted
*/
void completer_t::complete_from_args( const wcstring &str,
                                      const wcstring &args,
                                      const wcstring &desc,
                                      complete_flags_t flags )
{
    
	std::vector<completion_t> possible_comp;

    bool is_autosuggest = (this->type == COMPLETE_AUTOSUGGEST);
    parser_t parser(is_autosuggest ? PARSER_TYPE_COMPLETIONS_ONLY : PARSER_TYPE_GENERAL, false);

    /* If type is COMPLETE_AUTOSUGGEST, it means we're on a background thread, so don't call proc_push_interactive */    
    if (! is_autosuggest)
        proc_push_interactive(0);

	parser.eval_args( args.c_str(), possible_comp );

    if (! is_autosuggest)
        proc_pop_interactive();
	
	this->complete_strings( str, desc.c_str(), 0, possible_comp, flags );
}

/**
   Match against an old style long option
*/
static int param_match_old( const complete_entry_opt_t *e,
							const wchar_t *optstr )
{
	return  (optstr[0] == L'-') && (e->long_opt == &optstr[1]);
}

/**
   Match a parameter
*/
static int param_match( const complete_entry_opt_t *e,
						const wchar_t *optstr )
{
	if( e->short_opt != L'\0' &&
		e->short_opt == optstr[1] )
		return 1;

	if( !e->old_mode && (wcsncmp( L"--", optstr, 2 ) == 0 ))
	{
		if( e->long_opt == &optstr[2])
		{
			return 1;
		}
	}

	return 0;
}

/**
   Test if a string is an option with an argument, like --color=auto or -I/usr/include
*/
static wchar_t *param_match2( const complete_entry_opt_t *e,
							  const wchar_t *optstr )
{
	if( e->short_opt != L'\0' && e->short_opt == optstr[1] )
		return (wchar_t *)&optstr[2];
	if( !e->old_mode && (wcsncmp( L"--", optstr, 2 ) == 0) )
	{
		size_t len = e->long_opt.size();

		if( wcsncmp( e->long_opt.c_str(), &optstr[2],len ) == 0 )
		{
			if( optstr[len+2] == L'=' )
				return (wchar_t *)&optstr[len+3];
		}
	}
	return 0;
}

/**
   Tests whether a short option is a viable completion
*/
static int short_ok( const wcstring &arg_str, wchar_t nextopt, const wcstring &allopt_str )
{
    const wchar_t *arg = arg_str.c_str();
    const wchar_t *allopt = allopt_str.c_str();
	const wchar_t *ptr;

	if( arg[0] != L'-')
		return arg[0] == L'\0';
	if( arg[1] == L'-' )
		return 0;

	if( wcschr( arg, nextopt ) != 0 )
		return 0;

	for( ptr = arg+1; *ptr; ptr++ )
	{
		const wchar_t *tmp = wcschr( allopt, *ptr );
		/* Unknown option */
		if( tmp == 0 )
		{
			/*fwprintf( stderr, L"Unknown option %lc", *ptr );*/

			return 0;
		}

		if( *(tmp+1) == L':' )
		{
/*			fwprintf( stderr, L"Woot %ls", allopt );*/
			return 0;
		}

	}

	return 1;
}

void complete_load( const wcstring &name, bool reload )
{
	completion_autoloader.load( name, reload );
}

/**
   Find completion for the argument str of command cmd_orig with
   previous option popt. Insert results into comp_out. Return 0 if file
   completion should be disabled, 1 otherwise.
*/
struct local_options_t {
    wcstring short_opt_str;
    option_list_t options;
};
bool completer_t::complete_param( const wcstring &scmd_orig, const wcstring &spopt, const wcstring &sstr, bool use_switches)
{

    const wchar_t * const cmd_orig = scmd_orig.c_str(), * const popt = spopt.c_str(), * const str = sstr.c_str();

	bool use_common=1, use_files=1;

    wcstring cmd, path;
    parse_cmd_string(cmd_orig, path, cmd);

    if (this->type == COMPLETE_DEFAULT)
    {
        complete_load( cmd, true );
    }
    else if (this->type == COMPLETE_AUTOSUGGEST)
    {
        /* Maybe indicate we should try loading this on the main thread */
        if (! list_contains_string(this->commands_to_load, cmd) && ! completion_autoloader.has_tried_loading(cmd))
        {
            this->commands_to_load.push_back(cmd);
        }
    }
    
    /* Make a list of lists of all options that we care about */
    std::vector<local_options_t> all_options;
    {
        scoped_lock lock(completion_lock);
        scoped_lock lock2(completion_entry_lock);
        for (completion_entry_set_t::const_iterator iter = completion_set.begin(); iter != completion_set.end(); ++iter)
        {
            const completion_entry_t *i = *iter;
            const wcstring &match = i->cmd_is_path ? path : cmd;
            if (! wildcard_match(match, i->cmd))
            {
                continue;
            }
            
            /* Copy all of their options into our list */
            all_options.push_back(local_options_t());
            all_options.back().short_opt_str = i->get_short_opt_str();
            all_options.back().options = i->get_options(); //Oof, this is a lot of copying
        }
    }
    
    /* Now release the lock and test each option that we captured above.
       We have to do this outside the lock because callouts (like the condition) may add or remove completions.
       See https://github.com/ridiculousfish/fishfish/issues/2 */
    for (std::vector<local_options_t>::const_iterator iter = all_options.begin(); iter != all_options.end(); iter++)
    {
        const option_list_t &options = iter->options;
		use_common=1;
		if( use_switches )
		{
			
			if( str[0] == L'-' )
			{
				/* Check if we are entering a combined option and argument
				   (like --color=auto or -I/usr/include) */
                for (option_list_t::const_iterator oiter = options.begin(); oiter != options.end(); ++oiter)
				{
                	const complete_entry_opt_t *o = &*oiter;
					wchar_t *arg;
					if( (arg=param_match2( o, str ))!=0 && this->condition_test( o->condition ))
					{
                        if (o->result_mode & NO_COMMON) use_common = false;
                        if (o->result_mode & NO_FILES) use_files = false;
						complete_from_args( arg, o->comp, o->localized_desc(), o->flags );
					}

				}
			}
			else if( popt[0] == L'-' )
			{
				/* Set to true if we found a matching old-style switch */
				int old_style_match = 0;
			
				/*
				  If we are using old style long options, check for them
				  first
				*/
                for (option_list_t::const_iterator oiter = options.begin(); oiter != options.end(); ++oiter)
				{
                    const complete_entry_opt_t *o = &*oiter;
					if( o->old_mode )
					{
						if( param_match_old( o, popt ) && this->condition_test( o->condition ))
						{
							old_style_match = 1;
                            if (o->result_mode & NO_COMMON) use_common = false;
                            if (o->result_mode & NO_FILES) use_files = false;
							complete_from_args( str, o->comp, o->localized_desc(), o->flags );
						}
					}
				}
						
				/*
				  No old style option matched, or we are not using old
				  style options. We check if any short (or gnu style
				  options do.
				*/
				if( !old_style_match )
				{
                    for (option_list_t::const_iterator oiter = options.begin(); oiter != options.end(); ++oiter)
                    {
                        const complete_entry_opt_t *o = &*oiter;
						/*
						  Gnu-style options with _optional_ arguments must
						  be specified as a single token, so that it can
						  be differed from a regular argument.
						*/
						if( !o->old_mode && ! o->long_opt.empty() && !(o->result_mode & NO_COMMON) )
							continue;

						if( param_match( o, popt ) && this->condition_test( o->condition  ))
						{
                            if (o->result_mode & NO_COMMON) use_common = false;
                            if (o->result_mode & NO_FILES) use_files = false;
							complete_from_args( str, o->comp.c_str(), o->localized_desc(), o->flags );

						}
					}
				}
			}
		}
		
		if( use_common )
		{

            for (option_list_t::const_iterator oiter = options.begin(); oiter != options.end(); ++oiter)
            {
                const complete_entry_opt_t *o = &*oiter;
				/*
				  If this entry is for the base command,
				  check if any of the arguments match
				*/

				if( !this->condition_test( o->condition ))
					continue;


				if( (o->short_opt == L'\0' ) && (o->long_opt[0]==L'\0'))
				{
					use_files &= ((o->result_mode & NO_FILES )==0);
					complete_from_args( str, o->comp, o->localized_desc(), o->flags );
				}
				
				if( wcslen(str) > 0 && use_switches )
				{
					/*
					  Check if the short style option matches
					*/
					if( o->short_opt != L'\0' &&
						short_ok(str, o->short_opt, iter->short_opt_str))
					{
						const wchar_t *desc = o->localized_desc();
						wchar_t completion[2];
						completion[0] = o->short_opt;
						completion[1] = 0;
						
						append_completion( this->completions, completion, desc, 0 );

					}

					/*
					  Check if the long style option matches
					*/
					if( o->long_opt[0] != L'\0' )
					{
						int match=0, match_no_case=0;
						
                        wcstring whole_opt;
                        whole_opt.append(o->old_mode?L"-":L"--");
                        whole_opt.append(o->long_opt);

                        match = string_prefixes_string(str, whole_opt);

						if( !match )
						{
							match_no_case = wcsncasecmp( str, whole_opt.c_str(), wcslen(str) )==0;
						}
						
						if( match || match_no_case )
						{
							int has_arg=0; /* Does this switch have any known arguments  */
							int req_arg=0; /* Does this switch _require_ an argument */

							size_t offset = 0;
							complete_flags_t flags = 0;

															
							if( match )
								offset = wcslen( str );
							else
								flags = COMPLETE_NO_CASE;

							has_arg = ! o->comp.empty();
							req_arg = (o->result_mode & NO_COMMON );

							if( !o->old_mode && ( has_arg && !req_arg ) )
							{
								
								/*
								  Optional arguments to a switch can
								  only be handled using the '=', so we
								  add it as a completion. By default
								  we avoid using '=' and instead rely
								  on '--switch switch-arg', since it
								  is more commonly supported by
								  homebrew getopt-like functions.
								*/
								wcstring completion = format_string(L"%ls=", whole_opt.c_str()+offset);						
								append_completion( this->completions,
													 completion,
													 C_(o->desc.c_str()),
													 flags );									
								
							}
							
							append_completion( this->completions,
												 whole_opt.c_str() + offset,
												 C_(o->desc.c_str()),
												 flags );
						}					
					}
				}
			}
		}
	}
	
	return use_files;
}

/**
   Perform file completion on the specified string
*/
void completer_t::complete_param_expand( const wcstring &sstr, bool do_file)
{
    const wchar_t * const str = sstr.c_str();
	const wchar_t *comp_str;
	
	if (string_prefixes_string( L"--", sstr) && (comp_str = wcschr(str, L'=' ) ) )
	{
		comp_str++;
	}
	else
	{
		comp_str = str;
	}
    
    expand_flags_t flags = EXPAND_SKIP_CMDSUBST | ACCEPT_INCOMPLETE;
    
    if (! do_file)
        flags |= EXPAND_SKIP_WILDCARDS;
    
    /* Squelch file descriptions per issue 254 */
    if (type == COMPLETE_AUTOSUGGEST || do_file)
        flags |= EXPAND_NO_DESCRIPTIONS;
	
	if( expand_string( comp_str,
					   this->completions,
					   flags | this->expand_flags() ) == EXPAND_ERROR )
	{
		debug( 3, L"Error while expanding string '%ls'", comp_str );
	}	
}

void completer_t::debug_print_completions()
{
    for (size_t i=0; i < completions.size(); i++) {
        printf("- Completion: %ls\n", completions.at(i).completion.c_str());
    }
}

/**
   Complete the specified string as an environment variable
*/
bool completer_t::complete_variable(const wcstring &str, size_t start_offset)
{
    const wchar_t * const whole_var = str.c_str();
	const wchar_t *var = &whole_var[start_offset];
	size_t varlen = wcslen( var );
	int res = 0;
    bool wants_description = (type != COMPLETE_AUTOSUGGEST);
    
    const wcstring_list_t names = env_get_names(0);
	for( size_t i=0; i<names.size(); i++ )
	{
		const wcstring & env_name = names.at(i);
		size_t namelen = env_name.size();
		int match=0, match_no_case=0;	

		if( varlen > namelen )
			continue;

		match = string_prefixes_string(var, env_name);
		
		if( !match )
		{
			match_no_case = ( wcsncasecmp( var, env_name.c_str(), varlen) == 0 );
		}

		if( match || match_no_case )
		{
            wcstring comp;
            int flags = 0;
            
            if( match )
            {
                comp.append(env_name.c_str() + varlen);
            }
            else
            {
                comp.append(whole_var, start_offset);
                comp.append(env_name);
                flags = COMPLETE_NO_CASE | COMPLETE_DONT_ESCAPE;
            }
            
            wcstring desc;
            if (wants_description)
            {
                env_var_t value_unescaped = env_get_string( env_name );
                if (value_unescaped.missing())
                    continue;
                
                wcstring value = expand_escape_variable( value_unescaped );
                if (type != COMPLETE_AUTOSUGGEST)
                    desc = format_string(COMPLETE_VAR_DESC_VAL, value.c_str());
            }
                            
            append_completion( this->completions,  comp.c_str(), desc.c_str(), flags );
            res =1;
			
		}
	}

	return res;
}

/**
   Search the specified string for the \$ sign. If found, try to
   complete as an environment variable. 

   \return 0 if unable to complete, 1 otherwise
*/
bool completer_t::try_complete_variable( const wcstring &str )
{
	size_t i = str.size();
	while (i--)
	{
        wchar_t c = str.at(i);
		if( c == L'$' )
		{
/*			wprintf( L"Var prefix \'%ls\'\n", &cmd[i+1] );*/
			return this->complete_variable( str, i+1 );
		}
		if( !isalnum(c) && c != L'_' )
		{
			return false;
		}
	}
	return false;
}

/**
   Try to complete the specified string as a username. This is used by
   ~USER type expansion.

   \return 0 if unable to complete, 1 otherwise
*/
bool completer_t::try_complete_user( const wcstring &str )
{
    const wchar_t *cmd = str.c_str();
	const wchar_t *first_char=cmd;
	int res=0;
	double start_time = timef();
	
	if( *first_char ==L'~' && !wcschr(first_char, L'/'))
	{
		const wchar_t *user_name = first_char+1;
		const wchar_t *name_end = wcschr( user_name, L'~' );
		if( name_end == 0 )
		{
			struct passwd *pw;
			size_t name_len = wcslen( user_name );
			
			setpwent();
			
			while((pw=getpwent()) != 0)
			{
				double current_time = timef();
				wchar_t *pw_name;

				if( current_time - start_time > 0.2 ) 
				{
					return 1;
				}
			
				pw_name = str2wcs( pw->pw_name );

				if( pw_name )
				{
					if( wcsncmp( user_name, pw_name, name_len )==0 )
					{
                        wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);						
						append_completion( this->completions, 
											 &pw_name[name_len],
											 desc,
											 COMPLETE_NO_SPACE );
						
						res=1;
					}
					else if( wcsncasecmp( user_name, pw_name, name_len )==0 )
					{
						wcstring name = format_string(L"~%ls", pw_name);
                        wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
							
						append_completion( this->completions, 
											 name,
											 desc,
											 COMPLETE_NO_CASE | COMPLETE_DONT_ESCAPE | COMPLETE_NO_SPACE );
						res=1;							
					}
					free( pw_name );
				}
			}
			endpwent();
		}
	}

	return res;
}

void complete( const wcstring &cmd, std::vector<completion_t> &comps, complete_type_t type, wcstring_list_t *commands_to_load )
{
    /* Make our completer */
    completer_t completer(cmd, type);
    
	const wchar_t *tok_begin, *tok_end, *cmdsubst_begin, *cmdsubst_end, *prev_begin, *prev_end;
	tokenizer tok;
	const wchar_t *current_token=0, *prev_token=0;
    wcstring current_command;    
	int on_command=0;
	size_t pos;
	bool done=false;
	int use_command = 1;
	int use_function = 1;
	int use_builtin = 1;
	int had_ddash = 0;

//	debug( 1, L"Complete '%ls'", cmd );

	size_t cursor_pos = cmd.size();
    
    const wchar_t *cmd_cstr = cmd.c_str();
	parse_util_cmdsubst_extent( cmd_cstr, cursor_pos, &cmdsubst_begin, &cmdsubst_end );
	parse_util_token_extent( cmd_cstr, cursor_pos, &tok_begin, &tok_end, &prev_begin, &prev_end );

	if( !cmdsubst_begin )
		done=1;
        

	/**
	   If we are completing a variable name or a tilde expansion user
	   name, we do that and return. No need for any other competions.
	*/

	if( !done )
	{
        wcstring tmp = tok_begin;
        done = completer.try_complete_variable( tmp ) || completer.try_complete_user( tmp );
	}

	if( !done )
	{
		pos = cursor_pos-(cmdsubst_begin-cmd_cstr);
		
		wcstring buff = wcstring( cmdsubst_begin, cmdsubst_end-cmdsubst_begin );

		int had_cmd=0;
		int end_loop=0;

		tok_init( &tok, buff.c_str(), TOK_ACCEPT_UNFINISHED | TOK_SQUASH_ERRORS );

		while( tok_has_next( &tok) && !end_loop )
		{

			switch( tok_last_type( &tok ) )
			{

				case TOK_STRING:
				{

					const wcstring ncmd = tok_last( &tok );
					int is_ddash = (ncmd == L"--") && ( (tok_get_pos( &tok )+2) < (long)pos );
					
					if( !had_cmd )
					{

						if( parser_keywords_is_subcommand( ncmd ) )
						{
							if (ncmd == L"builtin" )
							{
								use_function = 0;
								use_command  = 0;
								use_builtin  = 1;
							}
							else if (ncmd == L"command")
							{
								use_command  = 1;
								use_function = 0;
								use_builtin  = 0;
							}
							break;
						}

						
						if( !is_ddash ||
						    ( (use_command && use_function && use_builtin ) ) )
						{
							current_command = ncmd;
							
							size_t token_end = tok_get_pos( &tok ) + ncmd.size();
							
							on_command = (pos <= token_end );
							had_cmd=1;
						}

					}
					else
					{
						if( is_ddash )
						{
							had_ddash = 1;
						}
					}
					
					break;
				}
					
				case TOK_END:
				case TOK_PIPE:
				case TOK_BACKGROUND:
				{
					had_cmd=0;
					had_ddash = 0;
					use_command  = 1;
					use_function = 1;
					use_builtin  = 1;
					break;
				}
				
				case TOK_ERROR:
				{
					end_loop=1;
					break;
				}
				
			}

			if( tok_get_pos( &tok ) >= (long)pos )
			{
				end_loop=1;
			}
			
			tok_next( &tok );

		}

		tok_destroy( &tok );

		/*
		  Get the string to complete
		*/

		current_token = wcsndup( tok_begin, cursor_pos-(tok_begin-cmd_cstr) );

		prev_token = prev_begin ? wcsndup( prev_begin, prev_end - prev_begin ): wcsdup(L"");
		
//		debug( 0, L"on_command: %d, %ls %ls\n", on_command, current_command, current_token );

		/*
		  Check if we are using the 'command' or 'builtin' builtins
		  _and_ we are writing a switch instead of a command. In that
		  case, complete using the builtins completions, not using a
		  subcommand.
		*/
		
		if( (on_command || (wcscmp( current_token, L"--" ) == 0 ) ) &&
			(current_token[0] == L'-') && 
			!(use_command && use_function && use_builtin ) )
		{
			if( use_command == 0 )
				current_command = L"builtin";
			else
				current_command = L"command";
			
			had_cmd = 1;
			on_command = 0;
		}
		
		/*
		  Use command completions if in between commands
		*/
		if( !had_cmd )
		{
			on_command=1;
		}
		
		/*
		  We don't want these to be null
		*/

		if( !current_token )
		{
			current_token = wcsdup(L"");
		}
		
		if( !prev_token )
		{
			prev_token = wcsdup(L"");
		}

		if( current_token && prev_token )
		{
			if( on_command )
			{
				/* Complete command filename */
				completer.complete_cmd( current_token, use_function, use_builtin, use_command );
			}
			else
			{
				int do_file=0;
				
                wcstring current_command_unescape = current_command;
                wcstring prev_token_unescape = prev_token;
                wcstring current_token_unescape = current_token;
                
				if( unescape_string( current_command_unescape, 0 ) &&
                    unescape_string( prev_token_unescape, 0 ) &&
                    unescape_string( current_token_unescape, UNESCAPE_INCOMPLETE))
				{
					do_file = completer.complete_param( current_command_unescape, 
                                                          prev_token_unescape, 
                                                          current_token_unescape, 
                                                          !had_ddash );
				}
                
                /*
				  If we have found no command specific completions at
				  all, fall back to using file completions.
				*/
				if( completer.empty() )
					do_file = 1;
				
				/*
				  This function wants the unescaped string
				*/
				completer.complete_param_expand( current_token, do_file );
			}
		}
	}
	
	free( (void *)current_token );
	free( (void *)prev_token );

	comps = completer.get_completions();
    completer.get_commands_to_load(commands_to_load);
}



/**
   Print the GNU longopt style switch \c opt, and the argument \c
   argument to the specified stringbuffer, but only if arguemnt is
   non-null and longer than 0 characters.
*/
static void append_switch( wcstring &out,
						   const wcstring &opt,
						   const wcstring &argument )
{
	if( argument.empty() )
		return;

    wcstring esc = escape_string( argument, 1 );
	append_format( out, L" --%ls %ls", opt.c_str(), esc.c_str() );
}

void complete_print( wcstring &out )
{
    scoped_lock locker(completion_lock);
    scoped_lock locker2(completion_entry_lock);
    
    // Get a list of all completions in a vector, then sort it by order
    std::vector<const completion_entry_t *> all_completions(completion_set.begin(), completion_set.end());
    sort(all_completions.begin(), all_completions.end(), compare_completions_by_order);
    
    for (std::vector<const completion_entry_t *>::const_iterator iter = all_completions.begin(); iter != all_completions.end(); ++iter)
    {
        const completion_entry_t *e = *iter;
        const option_list_t options = e->get_options();
        for (option_list_t::const_iterator oiter = options.begin(); oiter != options.end(); ++oiter)
        {
            const complete_entry_opt_t *o = &*oiter;
			const wchar_t *modestr[] =
				{
					L"",
					L" --no-files",
					L" --require-parameter",
					L" --exclusive"
				}
			;

			append_format( out,
					   L"complete%ls",
					   modestr[o->result_mode] );

			append_switch( out,
						   e->cmd_is_path ? L"path" : L"command",
						   e->cmd );


			if( o->short_opt != 0 )
			{
				append_format( out,
						   L" --short-option '%lc'",
						   o->short_opt );
			}


			append_switch( out,
						   o->old_mode?L"old-option":L"long-option",
						   o->long_opt );

			append_switch( out,
						   L"description",
						   C_(o->desc.c_str()) );

			append_switch( out,
						   L"arguments",
						   o->comp );

			append_switch( out,
						   L"condition",
						   o->condition );

			out.append( L"\n" );
		}
	}
}
