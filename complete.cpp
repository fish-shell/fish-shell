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
	int flags;
    
    const wchar_t *localized_desc() const
    {
        const wchar_t *tmp = desc.c_str();
        return C_(tmp);
    }
} complete_entry_opt_t;

/**
   Struct describing a command completion
*/
typedef std::list<complete_entry_opt_t> option_list_t;
struct completion_entry_t
{
	/** True if command is a path */
	int cmd_type;

	/** Command string */
	wcstring cmd;
	/** String containing all short option characters */
	wcstring short_opt_str;
    
	/** List of all options */
	option_list_t options;
    
	/** True if no other options than the ones supplied are possible */
	int authoritative;
};

/** Linked list of all completion entries */
typedef std::list<completion_entry_t *> completion_entry_list_t;
static completion_entry_list_t completion_entries;

/**
   Table of completions conditions that have already been tested and
   the corresponding test results
*/
static std::map<wcstring, bool> condition_cache;

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
void completion_allocate(std::vector<completion_t> &completions, const wcstring &comp, const wcstring &desc, int flags)
{
    completions.push_back(completion_t(comp, desc, flags));
}

/**
   The init function for the completion code. Does nothing.
*/
static void complete_init()
{
}

/**
   This command clears the cache of condition tests created by \c condition_test().
*/
static void condition_cache_clear()
{
	condition_cache.clear();
}

/**
   Test if the specified script returns zero. The result is cached, so
   that if multiple completions use the same condition, it needs only
   be evaluated once. condition_cache_clear must be called after a
   completion run to make sure that there are no stale completions.
*/
static int condition_test( const wcstring &condition )
{
    ASSERT_IS_MAIN_THREAD();
    
	if( condition.empty() )
	{
//		fwprintf( stderr, L"No condition specified\n" );
		return 1;
	}
    
    bool test_res;
    std::map<wcstring, bool>::iterator cached_entry = condition_cache.find(condition);
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


/**
   Search for an exactly matching completion entry
*/
static completion_entry_t *complete_find_exact_entry( const wchar_t *cmd,
													const int cmd_type )
{
    for (completion_entry_list_t::iterator iter = completion_entries.begin(); iter != completion_entries.end(); iter++)
	{
        completion_entry_t *entry = *iter;
        if (entry->cmd == cmd && cmd_type == entry->cmd_type)
			return entry;
	}
	return NULL;
}

/**
   Locate the specified entry. Create it if it doesn't exist.
*/
static completion_entry_t *complete_get_exact_entry( const wchar_t *cmd,
												   int cmd_type )
{
	completion_entry_t *c;

	complete_init();

	c = complete_find_exact_entry( cmd, cmd_type );

	if( c == NULL )
	{
        c = new completion_entry_t();
        completion_entries.push_front(c);
        c->cmd = cmd;
		c->cmd_type = cmd_type;
		c->short_opt_str = L"";
		c->authoritative = 1;
	}

	return c;
}


void complete_set_authoritative( const wchar_t *cmd,
								 int cmd_type,
								 int authoritative )
{
	completion_entry_t *c;

	CHECK( cmd, );
	c = complete_get_exact_entry( cmd, cmd_type );
	c->authoritative = authoritative;
}


void complete_add( const wchar_t *cmd,
				   int cmd_type,
				   wchar_t short_opt,
				   const wchar_t *long_opt,
				   int old_mode,
				   int result_mode,
				   const wchar_t *condition,
				   const wchar_t *comp,
				   const wchar_t *desc,
				   int flags )
{
	CHECK( cmd, );

	completion_entry_t *c;
	c = complete_get_exact_entry( cmd, cmd_type );
    
    c->options.push_front(complete_entry_opt_t());
    complete_entry_opt_t &opt = c->options.front();
    
	if( short_opt != L'\0' )
	{
		int len = 1 + ((result_mode & NO_COMMON) != 0);
        
        c->short_opt_str.push_back(short_opt);
		if( len == 2 )
		{
            c->short_opt_str.push_back(L':');
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
}

/**
   Remove all completion options in the specified entry that match the
   specified short / long option strings. Returns true if it is now
   empty and should be deleted, false if it's not empty.
*/
static bool complete_remove_entry( completion_entry_t *e, wchar_t short_opt, const wchar_t *long_opt )
{

	if(( short_opt == 0 ) && (long_opt == 0 ) )
	{
        e->options.clear();
	}
	else
	{
        for (option_list_t::iterator iter = e->options.begin(); iter != e->options.end(); )
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
                    size_t idx = e->short_opt_str.find(o.short_opt);
                    if (idx != wcstring::npos)
                    {
                        /* Consume all colons */
                        size_t first_non_colon = idx + 1;
                        while (first_non_colon < e->short_opt_str.size() && e->short_opt_str.at(first_non_colon) == L':')
                            first_non_colon++;
                        e->short_opt_str.erase(idx, first_non_colon - idx);
                    }
				}
                
                /* Destroy this option and go to the next one */
				iter = e->options.erase(iter);
			}
			else
			{
                /* Just go to the next one */
				iter++;
			}
		}
	}
    return e->options.empty();
}


void complete_remove( const wchar_t *cmd,
					  int cmd_type,
					  wchar_t short_opt,
					  const wchar_t *long_opt )
{
	CHECK( cmd, );
    for (completion_entry_list_t::iterator iter = completion_entries.begin(); iter != completion_entries.end(); ) {
        completion_entry_t *e = *iter;
        bool delete_it = false;
		if(cmd_type == e->cmd_type && cmd == e->cmd) {
            delete_it = complete_remove_entry( e, short_opt, long_opt );
        }
        
        if (delete_it) {
            /* Delete this entry */
            iter = completion_entries.erase(iter);
            delete e;
        } else {
            /* Don't delete it */
            iter++;
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
    if (! path_get_path_string(str, path)) {
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

int complete_is_valid_option( const wchar_t *str,
							  const wchar_t *opt,
							  wcstring_list_t *errors,
							  bool allow_autoload )
{
    wcstring cmd, path;
	int found_match = 0;
	int authoritative = 1;
	int opt_found=0;
	std::set<wcstring> gnu_match_set;
	int is_gnu_opt=0;
	int is_old_opt=0;
	int is_short_opt=0;
	int is_gnu_exact=0;
	int gnu_opt_len=0;
    
    std::vector<char> short_validated;

	
	CHECK( str, 0 );
	CHECK( opt, 0 );
		
	/*
	  Check some generic things like -- and - options.
	*/
	switch( wcslen(opt ) )
	{

		case 0:
		case 1:
		{
			return 1;
		}
		
		case 2:
		{
			if( wcscmp( L"--", opt ) == 0 )
			{
				return 1;
			}
			break;
		}
	}
	
	if( opt[0] != L'-' )
	{
		if( errors )
            errors->push_back(L"Option does not begin with a '-'");
		return 0;
	}


    short_validated.resize(wcslen(opt), 0);	
    
	is_gnu_opt = opt[1]==L'-';
	if( is_gnu_opt )
	{
		const wchar_t *opt_end = wcschr(opt, L'=' );
		if( opt_end )
		{
			gnu_opt_len = (opt_end-opt)-2;
		}
		else
		{
			gnu_opt_len = wcslen(opt)-2;
		}
	}
	
	parse_cmd_string( str, path, cmd );

	/*
	  Make sure completions are loaded for the specified command
	*/
	if (allow_autoload) complete_load( cmd, false );
	
    for (completion_entry_list_t::iterator iter = completion_entries.begin(); iter != completion_entries.end(); iter++)
	{
        const completion_entry_t *i = *iter;
		const wcstring &match = i->cmd_type?path:cmd;
		const wchar_t *a;

		if( !wildcard_match( match, i->cmd ) )
		{
			continue;
		}
		
		found_match = 1;

		if( !i->authoritative )
		{
			authoritative = 0;
			break;
		}


		if( is_gnu_opt )
		{

            for (option_list_t::const_iterator iter = i->options.begin(); iter != i->options.end(); iter++)
            {
                const complete_entry_opt_t &o = *iter;
				if( o.old_mode )
				{
					continue;
				}
				
				if( &opt[2] == o.long_opt )
				{
                    gnu_match_set.insert(o.long_opt);
					if( (wcsncmp( &opt[2],
								  o.long_opt.c_str(),
                                  o.long_opt.size())==0) )
					{
						is_gnu_exact=1;
					}
				}
			}
		}
		else
		{
			/* Check for old style options */
            for (option_list_t::const_iterator iter = i->options.begin(); iter != i->options.end(); iter++)
			{
                const complete_entry_opt_t &o = *iter;
                
				if( !o.old_mode )
					continue;


				if( wcscmp( &opt[1], o.long_opt.c_str() )==0)
				{
					opt_found = 1;
					is_old_opt = 1;
					break;
				}

			}

			if( is_old_opt )
				break;


			for( a = &opt[1]; *a; a++ )
			{

                //PCA Rewrite this
				wchar_t *str_pos = const_cast<wchar_t*>(wcschr(i->short_opt_str.c_str(), *a));

				if  (str_pos )
				{
					if( *(str_pos +1)==L':' )
					{
						/*
						  This is a short option with an embedded argument,
						  call complete_is_valid_argument on the argument.
						*/
						wchar_t nopt[3];
						nopt[0]=L'-';
						nopt[1]=opt[1];
						nopt[2]=L'\0';

						short_validated.at(a-opt) =
							complete_is_valid_argument( str, nopt, &opt[2]);
					}
					else
					{
						short_validated.at(a-opt)=1;
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
			size_t j;

			opt_found=1;
			for( j=1; j<wcslen(opt); j++)
			{
				if ( !short_validated.at(j))
				{
					if( errors )
					{
						wchar_t str[2];
						str[0] = opt[j];
						str[1]=0;
                        errors->push_back(format_error(_(L"Unknown option: "), str));
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

    return (authoritative && found_match)?opt_found:1;
}

int complete_is_valid_argument( const wchar_t *str,
								const wchar_t *opt,
								const wchar_t *arg )
{
	return 1;
}


/**
   Copy any strings in possible_comp which have the specified prefix
   to the list comp_out. The prefix may contain wildcards. The output
   will consist of completion_t structs.

   There are three ways to specify descriptions for each
   completion. Firstly, if a description has already been added to the
   completion, it is _not_ replaced. Secondly, if the desc_func
   function is specified, use it to determine a dynamic
   completion. Thirdly, if none of the above are available, the desc
   string is used as a description.

   \param comp_out the destination list
   \param wc_escaped the prefix, possibly containing wildcards. The wildcard should not have been unescaped, i.e. '*' should be used for any string, not the ANY_STRING character.
   \param desc the default description, used for completions with no embedded description. The description _may_ contain a COMPLETE_SEP character, if not, one will be prefixed to it
   \param desc_func the function that generates a description for those completions witout an embedded description
   \param possible_comp the list of possible completions to iterate over
*/

static void complete_strings( std::vector<completion_t> &comp_out,
							  const wchar_t *wc_escaped,
							  const wchar_t *desc,
							  const wchar_t *(*desc_func)(const wcstring &),
							  std::vector<completion_t> &possible_comp,
							  int flags )
{
    wcstring tmp = wc_escaped;
    if (! expand_one(tmp, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_WILDCARDS))
        return;
    
	const wchar_t *wc = parse_util_unescape_wildcards( tmp.c_str() );
	
	for( size_t i=0; i< possible_comp.size(); i++ )
	{
		wcstring temp = possible_comp.at( i ).completion;
		const wchar_t *next_str = temp.empty()?NULL:temp.c_str();

		if( next_str )
		{
			wildcard_complete( next_str, wc, desc, desc_func, comp_out, flags );
		}
	}

	free( (void *)wc );
}

/**
   If command to complete is short enough, substitute
   the description with the whatis information for the executable.
*/
static void complete_cmd_desc( const wchar_t *cmd, std::vector<completion_t> &comp )
{
	const wchar_t *cmd_start;
	int cmd_len;
	int skip;
	
	if( !cmd )
		return;

	cmd_start=wcsrchr(cmd, L'/');

	if( cmd_start )
		cmd_start++;
	else
		cmd_start = cmd;

	cmd_len = wcslen(cmd_start);

	/*
	  Using apropos with a single-character search term produces far
	  to many results - require at least two characters if we don't
	  know the location of the whatis-database.
	*/
	if(cmd_len < 2 )
		return;

	if( wildcard_has( cmd_start, 0 ) )
	{
		return;
	}

	skip = 1;
	
	for( size_t i=0; i< comp.size(); i++ )
	{
		const completion_t &c = comp.at ( i );
			
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
		for( size_t i=0; i<comp.size(); i++ )
		{
            completion_t &completion = comp.at(i);
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
   Returns a description for the specified function
*/
static const wchar_t *complete_function_desc( const wcstring &fn )
{
	const wchar_t *res = function_get_desc( fn );

	if( !res )
		res = function_get_definition( fn );

	return res;
}


/**
   Complete the specified command name. Search for executables in the
   path, executables defined using an absolute path, functions,
   builtins and directories for implicit cd commands.

   \param cmd the command string to find completions for

   \param comp the list to add all completions to
*/
static void complete_cmd( const wchar_t *cmd,
						  std::vector<completion_t> &comp,
						  int use_function,
						  int use_builtin,
						  int use_command )
{
	wchar_t *path_cpy;
	wchar_t *nxt_path;
	wchar_t *state;
	std::vector<completion_t> possible_comp;
	wchar_t *nxt_completion;

	wchar_t *cdpath_cpy = wcsdup(L".");

	if( (wcschr( cmd, L'/') != 0) || (cmd[0] == L'~' ) )
	{

		if( use_command )
		{
			
			if( expand_string(cmd, comp, ACCEPT_INCOMPLETE | EXECUTABLES_ONLY ) != EXPAND_ERROR )
			{
				complete_cmd_desc( cmd, comp );
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
			
				path_cpy = wcsdup( path.c_str() );
			
				for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
				     nxt_path != 0;
				     nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
				{
					size_t prev_count;
					int path_len = wcslen(nxt_path);
					int add_slash;
					
					if( !path_len )
					{
						continue;
					}

					add_slash = nxt_path[path_len-1]!=L'/';
					nxt_completion = wcsdupcat( nxt_path,
												add_slash?L"/":L"",
												cmd );
					if( ! nxt_completion )
						continue;
					
					prev_count =  comp.size() ;
					
					if( expand_string(
									   nxt_completion,
									   comp,
									   ACCEPT_INCOMPLETE |
									   EXECUTABLES_ONLY ) != EXPAND_ERROR )
					{
						for( size_t i=prev_count; i< comp.size(); i++ )
						{
							completion_t &c =  comp.at( i );
							if(c.flags & COMPLETE_NO_CASE )
							{
								c.completion += add_slash ;
							}
						}
					}
				}
				free( path_cpy );
				complete_cmd_desc( cmd, comp );
			}
		}
		
		/*
		  These return the original strings - don't free them
		*/

		if( use_function )
		{
			//function_get_names( &possible_comp, cmd[0] == L'_' );
            wcstring_list_t names = function_get_names(cmd[0] == L'_' );
            for (size_t i=0; i < names.size(); i++) {
                possible_comp.push_back(completion_t(names.at(i)));
            }
            
			complete_strings( comp, cmd, 0, &complete_function_desc, possible_comp, 0 );
		}

		possible_comp.clear();
		
		if( use_builtin )
		{
			builtin_get_names( possible_comp );
			complete_strings( comp, cmd, 0, &builtin_get_desc, possible_comp, 0 );
		}
//		al_destroy( &possible_comp );

	}

	if( use_builtin || (use_function && function_exists( L"cd") ) )
	{
		/*
		  Tab complete implicit cd for directories in CDPATH
		*/
		if( cmd[0] != L'/' && ( wcsncmp( cmd, L"./", 2 )!=0) )
		{
			for( nxt_path = wcstok( cdpath_cpy, ARRAY_SEP_STR, &state );
				 nxt_path != 0;
				 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
			{
				wchar_t *nxt_completion=
					wcsdupcat( nxt_path,
							   (nxt_path[wcslen(nxt_path)-1]==L'/'?L"":L"/"),
							   cmd );
				if( ! nxt_completion )
				{
					continue;
				}
			
				if( expand_string(		   nxt_completion,
								   comp,
								   ACCEPT_INCOMPLETE | DIRECTORIES_ONLY ) != EXPAND_ERROR )
				{
				}
			}
		}
	}

	free( cdpath_cpy );
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
static void complete_from_args( const wcstring &str,
								const wcstring &args,
								const wcstring &desc,
								std::vector<completion_t> &comp_out,
								int flags )
{

	std::vector<completion_t> possible_comp;

    parser_t parser(PARSER_TYPE_COMPLETIONS_ONLY);
	proc_push_interactive(0);
	parser.eval_args( args.c_str(), possible_comp );

	proc_pop_interactive();
	
	complete_strings( comp_out, str.c_str(), desc.c_str(), 0, possible_comp, flags );

//	al_foreach( &possible_comp, &free );
//	al_destroy( &possible_comp );
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
static int complete_param( const wchar_t *cmd_orig,
						   const wchar_t *popt,
						   const wchar_t *str,
						   int use_switches,
						   std::vector<completion_t> &comp_out )
{

	int use_common=1, use_files=1;

    wcstring cmd, path;
    parse_cmd_string(cmd_orig, path, cmd);

	complete_load( cmd, true );

    for (completion_entry_list_t::iterator iter = completion_entries.begin(); iter != completion_entries.end(); iter++)
	{
        completion_entry_t *i = *iter;
		const wcstring &match = i->cmd_type?path:cmd;

		if( ( (!wildcard_match( match, i->cmd ) ) ) )
		{
			continue;
		}
		
		use_common=1;
		if( use_switches )
		{
			
			if( str[0] == L'-' )
			{
				/* Check if we are entering a combined option and argument
				   (like --color=auto or -I/usr/include) */
                for (option_list_t::const_iterator oiter = i->options.begin(); oiter != i->options.end(); oiter++)
				{
                	const complete_entry_opt_t *o = &*oiter;
					wchar_t *arg;
					if( (arg=param_match2( o, str ))!=0 && condition_test( o->condition ))
					{
						use_common &= ((o->result_mode & NO_COMMON )==0);
						use_files &= ((o->result_mode & NO_FILES )==0);
						complete_from_args( arg, o->comp, o->localized_desc(), comp_out, o->flags );
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
                for (option_list_t::const_iterator oiter = i->options.begin(); oiter != i->options.end(); oiter++)
				{
                    const complete_entry_opt_t *o = &*oiter;
					if( o->old_mode )
					{
						if( param_match_old( o, popt ) && condition_test( o->condition ))
						{
							old_style_match = 1;
							use_common &= ((o->result_mode & NO_COMMON )==0);
							use_files &= ((o->result_mode & NO_FILES )==0);
							complete_from_args( str, o->comp, o->localized_desc(), comp_out, o->flags );
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
                    for (option_list_t::const_iterator oiter = i->options.begin(); oiter != i->options.end(); oiter++)
                    {
                        const complete_entry_opt_t *o = &*oiter;
						/*
						  Gnu-style options with _optional_ arguments must
						  be specified as a single token, so that it can
						  be differed from a regular argument.
						*/
						if( !o->old_mode && ! o->long_opt.empty() && !(o->result_mode & NO_COMMON) )
							continue;

						if( param_match( o, popt ) && condition_test( o->condition  ))
						{
							use_common &= ((o->result_mode & NO_COMMON )==0);
							use_files &= ((o->result_mode & NO_FILES )==0);
							complete_from_args( str, o->comp.c_str(), o->localized_desc(), comp_out, o->flags );

						}
					}
				}
			}
		}
		
		if( use_common )
		{

            for (option_list_t::const_iterator oiter = i->options.begin(); oiter != i->options.end(); oiter++)
            {
                const complete_entry_opt_t *o = &*oiter;
				/*
				  If this entry is for the base command,
				  check if any of the arguments match
				*/

				if( !condition_test( o->condition ))
					continue;


				if( (o->short_opt == L'\0' ) && (o->long_opt[0]==L'\0'))
				{
					use_files &= ((o->result_mode & NO_FILES )==0);
					complete_from_args( str, o->comp, o->localized_desc(), comp_out, o->flags );
				}
				
				if( wcslen(str) > 0 && use_switches )
				{
					/*
					  Check if the short style option matches
					*/
					if( o->short_opt != L'\0' &&
						short_ok( str, o->short_opt, i->short_opt_str ) )
					{
						const wchar_t *desc = o->localized_desc();
						wchar_t completion[2];
						completion[0] = o->short_opt;
						completion[1] = 0;
						
						completion_allocate( comp_out, completion, desc, 0 );

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

							int offset = 0;
							int flags = 0;

															
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
								completion_allocate( comp_out,
													 completion,
													 C_(o->desc.c_str()),
													 flags );									
								
							}
							
							completion_allocate( comp_out,
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
static void complete_param_expand( const wchar_t *str,
								   std::vector<completion_t> &comp_out,
								   int do_file )
{
	const wchar_t *comp_str;
	int flags;
	
	if( (wcsncmp( str, L"--", 2 )) == 0 && (comp_str = wcschr(str, L'=' ) ) )
	{
		comp_str++;
	}
	else
	{
		comp_str = str;
	}

	flags = EXPAND_SKIP_CMDSUBST | 
		ACCEPT_INCOMPLETE | 
		(do_file?0:EXPAND_SKIP_WILDCARDS);
	
	if( expand_string( comp_str,
					   comp_out,
					   flags ) == EXPAND_ERROR )
	{
		debug( 3, L"Error while expanding string '%ls'", comp_str );
	}
	
}


/**
   Complete the specified string as an environment variable
*/
static int complete_variable( const wchar_t *whole_var,
							  int start_offset,
							  std::vector<completion_t> &comp_list )
{
	const wchar_t *var = &whole_var[start_offset];
	int varlen = wcslen( var );
	int res = 0;
    
    const wcstring_list_t names = env_get_names(0);
	for( size_t i=0; i<names.size(); i++ )
	{
		const wcstring & env_name = names.at(i);
		int namelen = env_name.size();
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
			const env_var_t value_unescaped = env_get_string( env_name );
			if( !value_unescaped.missing() )
			{
				wcstring comp;
				int flags = 0;
				int offset = 0;
				
				if( match )
				{
                    comp.append(env_name.c_str() + varlen);
					offset = varlen;
				}
				else
				{
                    comp.append(whole_var, start_offset);
                    comp.append(env_name);
					flags = COMPLETE_NO_CASE | COMPLETE_DONT_ESCAPE;
				}
				
				wcstring value = expand_escape_variable( value_unescaped );
                
                wcstring desc = format_string(COMPLETE_VAR_DESC_VAL, value.c_str());				
				completion_allocate( comp_list, 
									 comp.c_str(),
									 desc.c_str(),
									 flags );
				res =1;
			}
			
		}
	}

	return res;
}

/**
   Search the specified string for the \$ sign. If found, try to
   complete as an environment variable. 

   \return 0 if unable to complete, 1 otherwise
*/
static int try_complete_variable( const wchar_t *cmd,
								  std::vector<completion_t> &comp )
{
	int len = wcslen( cmd );
	int i;

	for( i=len-1; i>=0; i-- )
	{
		if( cmd[i] == L'$' )
		{
/*			wprintf( L"Var prefix \'%ls\'\n", &cmd[i+1] );*/
			return complete_variable( cmd, i+1, comp );
		}
		if( !isalnum(cmd[i]) && cmd[i]!=L'_' )
		{
			return 0;
		}
	}
	return 0;
}

/**
   Try to complete the specified string as a username. This is used by
   ~USER type expansion.

   \return 0 if unable to complete, 1 otherwise
*/
static int try_complete_user( const wchar_t *cmd,
							  std::vector<completion_t> &comp )
{
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
			int name_len = wcslen( user_name );
			
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
						completion_allocate( comp, 
											 &pw_name[name_len],
											 desc,
											 COMPLETE_NO_SPACE );
						
						res=1;
					}
					else if( wcsncasecmp( user_name, pw_name, name_len )==0 )
					{
						wcstring name = format_string(L"~%ls", pw_name);
                        wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
							
						completion_allocate( comp, 
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

void complete( const wchar_t *cmd,
			   std::vector<completion_t> &comp )
{

	const wchar_t *tok_begin, *tok_end, *cmdsubst_begin, *cmdsubst_end, *prev_begin, *prev_end;
	wcstring buff;
	tokenizer tok;
	const wchar_t *current_token=0, *current_command=0, *prev_token=0;
	int on_command=0;
	int pos;
	int done=0;
	int cursor_pos;
	int use_command = 1;
	int use_function = 1;
	int use_builtin = 1;
	int had_ddash = 0;

	CHECK( cmd, );
//	CHECK( comp, );

	complete_init();

//	debug( 1, L"Complete '%ls'", cmd );

	cursor_pos = wcslen(cmd );

	parse_util_cmdsubst_extent( cmd, cursor_pos, &cmdsubst_begin, &cmdsubst_end );
	parse_util_token_extent( cmd, cursor_pos, &tok_begin, &tok_end, &prev_begin, &prev_end );

	if( !cmdsubst_begin )
		done=1;

	/**
	   If we are completing a variable name or a tilde expansion user
	   name, we do that and return. No need for any other competions.
	*/

	if( !done )
	{
		if( try_complete_variable( tok_begin, comp ) ||  try_complete_user( tok_begin, comp ))
		{
			done=1;
		}
	}

	if( !done )
	{
		pos = cursor_pos-(cmdsubst_begin-cmd);
		
		buff = wcstring( cmdsubst_begin, cmdsubst_end-cmdsubst_begin );
	}

	if( !done )
	{
		int had_cmd=0;
		int end_loop=0;

		tok_init( &tok, buff.c_str(), TOK_ACCEPT_UNFINISHED );

		while( tok_has_next( &tok) && !end_loop )
		{

			switch( tok_last_type( &tok ) )
			{

				case TOK_STRING:
				{

					const wcstring ncmd = tok_last( &tok );
					int is_ddash = (ncmd == L"--") && ( (tok_get_pos( &tok )+2) < pos );
					
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
							int token_end;
							
							free( (void *)current_command );
							current_command = wcsdup( ncmd.c_str() );
							
							token_end = tok_get_pos( &tok ) + ncmd.size();
							
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

			if( tok_get_pos( &tok ) >= pos )
			{
				end_loop=1;
			}
			
			tok_next( &tok );

		}

		tok_destroy( &tok );

		/*
		  Get the string to complete
		*/

		current_token = wcsndup( tok_begin, cursor_pos-(tok_begin-cmd) );

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
			free( (void *)current_command );
			if( use_command == 0 )
				current_command = wcsdup( L"builtin" );
			else
				current_command = wcsdup( L"command" );
			
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
		
		if( !current_command )
		{
			current_command = wcsdup(L"");
		}
		
		if( !prev_token )
		{
			prev_token = wcsdup(L"");
		}

		if( current_token && current_command && prev_token )
		{
			if( on_command )
			{
				/* Complete command filename */
				complete_cmd( current_token,
							  comp, use_function, use_builtin, use_command );
			}
			else
			{
				int do_file=0;
				
				wchar_t *current_command_unescape = unescape( current_command, 0 );
				wchar_t *prev_token_unescape = unescape( prev_token, 0 );
				wchar_t *current_token_unescape = unescape( current_token, UNESCAPE_INCOMPLETE );
				
				if( current_token_unescape && prev_token_unescape && current_token_unescape )
				{
					do_file = complete_param( current_command_unescape, 
											  prev_token_unescape, 
											  current_token_unescape, 
											  !had_ddash, 
											  comp );
				}
				
				free( current_command_unescape );
				free( prev_token_unescape );
				free( current_token_unescape );
				
				/*
				  If we have found no command specific completions at
				  all, fall back to using file completions.
				*/
				if( comp.empty() )
					do_file = 1;
				
				/*
				  This function wants the unescaped string
				*/
				complete_param_expand( current_token, comp, do_file );
			}
		}
	}
	
	free( (void *)current_token );
	free( (void *)current_command );
	free( (void *)prev_token );

	condition_cache_clear();



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
    for (completion_entry_list_t::const_iterator iter = completion_entries.begin(); iter != completion_entries.end(); iter++)
    {
        const completion_entry_t *e = *iter;
        for (option_list_t::const_iterator oiter = e->options.begin(); oiter != e->options.end(); oiter++)
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
						   e->cmd_type?L"path":L"command",
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
