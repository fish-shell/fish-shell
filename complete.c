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
#include "halloc.h"
#include "halloc_util.h"
#include "wutil.h"
#include "path.h"


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
	const wchar_t *long_opt;
	/** Arguments to the option */
	const wchar_t *comp;
	/** Description of the completion */
	const wchar_t *desc;
	/** Condition under which to use the option */
	const wchar_t *condition;
	/** Must be one of the values SHARED, NO_FILES, NO_COMMON,
		EXCLUSIVE, and determines how completions should be performed
		on the argument after the switch. */
	int result_mode;
	/** True if old style long options are used */
	int old_mode;
	/** Next option in the linked list */
	struct complete_entry_opt *next;
	/** Completion flags */
	int flags;
}
	complete_entry_opt_t;

/**
   Struct describing a command completion
*/
typedef struct complete_entry
{
	/** True if command is a path */
	int cmd_type;

	/** Command string */
	const wchar_t *cmd;
	/** String containing all short option characters */
	wchar_t *short_opt_str;
	/** Linked list of all options */
	complete_entry_opt_t *first_option;
	/** Next command completion in the linked list */
	struct complete_entry *next;
	/** True if no other options than the ones supplied are possible */
	int authoritative;
}
	complete_entry_t;

/** First node in the linked list of all completion entries */
static complete_entry_t *first_entry=0;

/**
   Table of completions conditions that have already been tested and
   the corresponding test results
*/
static hash_table_t *condition_cache=0;

static void complete_free_entry( complete_entry_t *c );

/**
   Create a new completion entry

*/
void completion_allocate( array_list_t *context,
						  const wchar_t *comp,
						  const wchar_t *desc,
						  int flags )
{
	completion_t *res = halloc( context, sizeof( completion_t) );
	res->completion = halloc_wcsdup( context, comp );
	if( desc )
		res->description = halloc_wcsdup( context, desc );

	if( flags & COMPLETE_AUTO_SPACE )
	{
		int len = wcslen(comp);

		flags = flags & (~COMPLETE_AUTO_SPACE);
	
		if( ( len > 0 ) && ( wcschr( L"/=@:", comp[ len - 1 ] ) != 0 ) )
			flags |= COMPLETE_NO_SPACE;
		
	}
	
	res->flags = flags;
	al_push( context, res );
}

/**
   Destroys various structures used for tab-completion and free()s the memory used by them.
*/
static void complete_destroy()
{
	complete_entry_t *i=first_entry, *prev;
	
	while( i )
	{
		prev = i;
		i=i->next;
		complete_free_entry( prev );
	}
	first_entry = 0;
	
	parse_util_load_reset( L"fish_complete_path", 0 );
	
}

/**
   The init function for the completion code. Currently, all it really
   does is make sure complete_destroy is called on exit.
*/
static void complete_init()
{
	static int is_init = 0;
	if( !is_init )
	{
		is_init = 1;
		halloc_register_function_void( global_context, &complete_destroy );
	}
}

/**
   This command clears the cache of condition tests created by \c condition_test().
*/
static void condition_cache_clear()
{
	if( condition_cache )
	{
		hash_destroy( condition_cache );
		free( condition_cache );
		condition_cache = 0;
	}
}

/**
   Test if the specified script returns zero. The result is cached, so
   that if multiple completions use the same condition, it needs only
   be evaluated once. condition_cache_clear must be called after a
   completion run to make sure that there are no stale completions.
*/
static int condition_test( const wchar_t *condition )
{
	const void *test_res;

	if( !condition || !wcslen(condition) )
	{
//		fwprintf( stderr, L"No condition specified\n" );
		return 1;
	}

	if( !condition_cache )
	{
		condition_cache = malloc( sizeof( hash_table_t ) );
		if( !condition_cache )
		{
			DIE_MEM();

		}

		hash_init( condition_cache,
				   &hash_wcs_func,
				   &hash_wcs_cmp );


	}

	test_res = hash_get( condition_cache, condition );

	if (test_res == CC_NOT_TESTED )
	{
		test_res = exec_subshell( condition, 0 )?CC_FALSE:CC_TRUE;
		hash_put( condition_cache, condition, test_res );

		/*
		  Restore previous status information
		*/
	}

	if( wcscmp( test_res, CC_TRUE ) == 0 )
	{
		return 1;
	}

	return 0;
}


/**
   Recursively free all complete_entry_opt_t structs and their contents
*/
static void complete_free_opt_recursive( complete_entry_opt_t *o )
{
	if( !o )
		return;
	
	complete_free_opt_recursive( o->next );
	halloc_free( o );
}

/**
   Free a complete_entry_t and its contents
*/
static void complete_free_entry( complete_entry_t *c )
{
//	free( c->cmd );
	free( c->short_opt_str );
	complete_free_opt_recursive( c->first_option );
	free( c );
}

/**
   Search for an exactly matching completion entry
*/
static complete_entry_t *complete_find_exact_entry( const wchar_t *cmd,
													const int cmd_type )
{
	complete_entry_t *i;
	for( i=first_entry; i; i=i->next )
	{
		if( ( wcscmp(cmd, i->cmd)==0) && ( cmd_type == i->cmd_type ))
		{
			return i;
		}
	}
	return 0;
}

/**
   Locate the specified entry. Create it if it doesn't exist.
*/
static complete_entry_t *complete_get_exact_entry( const wchar_t *cmd,
												   int cmd_type )
{
	complete_entry_t *c;

	complete_init();

	c = complete_find_exact_entry( cmd, cmd_type );

	if( c == 0 )
	{
		if( !(c = malloc( sizeof(complete_entry_t) )))
		{
			DIE_MEM();
		}
		
		c->next = first_entry;
		first_entry = c;

		c->first_option = 0;

		c->cmd = intern( cmd );
		c->cmd_type = cmd_type;
		c->short_opt_str = wcsdup(L"");
		c->authoritative = 1;
	}

	return c;
}


void complete_set_authoritative( const wchar_t *cmd,
								 int cmd_type,
								 int authoritative )
{
	complete_entry_t *c;

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
	complete_entry_t *c;
	complete_entry_opt_t *opt;

	CHECK( cmd, );

	c = complete_get_exact_entry( cmd, cmd_type );

	opt = halloc( 0, sizeof( complete_entry_opt_t ) );
	
	opt->next = c->first_option;
	c->first_option = opt;
	if( short_opt != L'\0' )
	{
		int len = 1 + ((result_mode & NO_COMMON) != 0);
		c->short_opt_str =
			realloc( c->short_opt_str,
					 sizeof(wchar_t)*(wcslen( c->short_opt_str ) + 1 + len) );
		wcsncat( c->short_opt_str,
				 &short_opt, 1 );
		if( len == 2 )
		{
			wcscat( c->short_opt_str, L":" );
		}
	}
	
	opt->short_opt = short_opt;
	opt->result_mode = result_mode;
	opt->old_mode=old_mode;

	opt->comp      = comp?halloc_wcsdup(opt, comp):L"";
	opt->condition = condition?halloc_wcsdup(opt, condition):L"";
	opt->long_opt  = long_opt?halloc_wcsdup(opt, long_opt):L"" ;
	opt->flags = flags;
	
	if( desc && wcslen( desc ) )
	{
		opt->desc = halloc_wcsdup( opt, desc );
	}
	else
	{
		opt->desc = L"";
	}
	
}

/**
   Remove all completion options in the specified entry that match the
   specified short / long option strings.
*/
static  complete_entry_t *complete_remove_entry( complete_entry_t *e,
												 wchar_t short_opt,
												 const wchar_t *long_opt )
{

	complete_entry_opt_t *o, *oprev=0, *onext=0;
	
	if(( short_opt == 0 ) && (long_opt == 0 ) )
	{
		complete_free_opt_recursive( e->first_option );
		e->first_option=0;
	}
	else
	{
		
		for( o= e->first_option; o; o=onext )
		{
			onext=o->next;
			
			if( ( short_opt==o->short_opt ) ||
				( wcscmp( long_opt, o->long_opt ) == 0 ) )
			{
				wchar_t *pos;
				/*			fwprintf( stderr,
							L"remove option -%lc --%ls\n",
							o->short_opt?o->short_opt:L' ',
							o->long_opt );
				*/
				if( o->short_opt )
				{
					pos = wcschr( e->short_opt_str,
								  o->short_opt );
					if( pos )
					{
						wchar_t *pos2 = pos+1;
						while( *pos2 == L':' )
						{
							pos2++;
						}
						
						memmove( pos,
								 pos2,
								 sizeof(wchar_t)*wcslen(pos2) );
					}
				}
				
				if( oprev == 0 )
				{
					e->first_option = o->next;
				}
				else
				{
					oprev->next = o->next;
				}
				free( o );
			}
			else
			{
				oprev = o;
			}
		}
	}
	
	if( e && (e->first_option == 0) )
	{
		free( e->short_opt_str );
		free( e );
		e=0;
	}
	
	return e;

}


void complete_remove( const wchar_t *cmd,
					  int cmd_type,
					  wchar_t short_opt,
					  const wchar_t *long_opt )
{
	complete_entry_t *e, *eprev=0, *enext=0;

	CHECK( cmd, );
		
	for( e = first_entry; e; e=enext )
	{
		enext=e->next;

		if( (cmd_type == e->cmd_type ) &&
			( wcscmp( cmd, e->cmd) == 0 ) )
		{
			e = complete_remove_entry( e, short_opt, long_opt );
		}

		if( e )
		{
			eprev = e;
		}
		else
		{
			if( eprev )
			{
				eprev->next = enext;
			}
			else
			{
				first_entry = enext;
			}
		}
		
	}
}

/**
   Find the full path and commandname from a command string. Both
   pointers are allocated using halloc and will be free'd when\c
   context is halloc_free'd.
*/
static void parse_cmd_string( void *context, 
							  const wchar_t *str,
							  wchar_t **pathp,
							  wchar_t **cmdp )
{
    wchar_t *cmd, *path;

	/* Get the path of the command */
	path = path_get_path( context, str );
	if( path == 0 )
	{
		/**
		   Use the empty string as the 'path' for commands that can
		   not be found.
		*/
		path = halloc_wcsdup( context, L"");
	}
	
	/* Make sure the path is not included in the command */
	cmd = wcsrchr( str, L'/' );
	if( cmd != 0 )
	{
		cmd++;
	}
	else
	{
		cmd = (wchar_t *)str;
	}
	
	*pathp=path;
	*cmdp=cmd;
}

int complete_is_valid_option( const wchar_t *str,
							  const wchar_t *opt,
							  array_list_t *errors )
{
	complete_entry_t *i;
	complete_entry_opt_t *o;
	wchar_t *cmd, *path;
	int found_match = 0;
	int authoritative = 1;
	int opt_found=0;
	hash_table_t gnu_match_hash;
	int is_gnu_opt=0;
	int is_old_opt=0;
	int is_short_opt=0;
	int is_gnu_exact=0;
	int gnu_opt_len=0;
	char *short_validated;

	void *context;
	
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
		{
			al_push( errors, wcsdup(L"Option does not begin with a '-'") );
		}
		return 0;
	}

	context = halloc( 0, 0 );

	if( !(short_validated = halloc( context, wcslen( opt ) )))
	{
		DIE_MEM();
	}
	


	memset( short_validated, 0, wcslen( opt ) );

	hash_init( &gnu_match_hash,
			   &hash_wcs_func,
			   &hash_wcs_cmp );

	is_gnu_opt = opt[1]==L'-';
	if( is_gnu_opt )
	{
		wchar_t *opt_end = wcschr(opt, L'=' );
		if( opt_end )
		{
			gnu_opt_len = (opt_end-opt)-2;
		}
		else
		{
			gnu_opt_len = wcslen(opt)-2;
		}
	}
	
	parse_cmd_string( context, str, &path, &cmd );

	/*
	  Make sure completions are loaded for the specified command
	*/
	complete_load( cmd, 0 );
	
	for( i=first_entry; i; i=i->next )
	{
		wchar_t *match = i->cmd_type?path:cmd;
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

			for( o = i->first_option; o; o=o->next )
			{
				if( o->old_mode )
				{
					continue;
				}
				
				if( wcsncmp( &opt[2], o->long_opt, gnu_opt_len )==0)
				{
					hash_put( &gnu_match_hash, o->long_opt, L"" );
					if( (wcsncmp( &opt[2],
								  o->long_opt,
								  wcslen( o->long_opt) )==0) )
					{
						is_gnu_exact=1;
					}
				}
			}
		}
		else
		{
			/* Check for old style options */
			for( o = i->first_option; o; o=o->next )
			{
				if( !o->old_mode )
					continue;


				if( wcscmp( &opt[1], o->long_opt )==0)
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

				wchar_t *str_pos = wcschr(i->short_opt_str, *a);

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

						short_validated[a-opt] =
							complete_is_valid_argument( str, nopt, &opt[2]);
					}
					else
					{
						short_validated[a-opt]=1;
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
			int j;

			opt_found=1;
			for( j=1; j<wcslen(opt); j++)
			{
				if ( !short_validated[j])
				{
					if( errors )
					{
						wchar_t str[2];
						str[0] = opt[j];
						str[1]=0;
						al_push( errors,
								 wcsdupcat(_( L"Unknown option: " ), L"'", str, L"'" ) );
					}

					opt_found = 0;
					break;
				}

			}
		}

		if( is_gnu_opt )
		{
			opt_found = is_gnu_exact || (hash_get_count( &gnu_match_hash )==1);
			if( errors && !opt_found )
			{
				if( hash_get_count( &gnu_match_hash )==0)
				{
					al_push( errors,
							 wcsdupcat( _(L"Unknown option: "), L"'", opt, L"\'" ) );
				}
				else
				{
					al_push( errors,
							 wcsdupcat( _(L"Multiple matches for option: "), L"'", opt, L"\'" ) );
				}
			}
		}
	}

	hash_destroy( &gnu_match_hash );

	halloc_free( context );
	
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

static void complete_strings( array_list_t *comp_out,
							  const wchar_t *wc_escaped,
							  const wchar_t *desc,
							  const wchar_t *(*desc_func)(const wchar_t *),
							  array_list_t *possible_comp,
							  int flags )
{
	int i;
	wchar_t *wc, *tmp;

	tmp = expand_one( 0,
					  wcsdup(wc_escaped), EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_WILDCARDS);
	if(!tmp)
		return;

	wc = parse_util_unescape_wildcards( tmp );
	free(tmp);
	
	for( i=0; i<al_get_count( possible_comp ); i++ )
	{
		wchar_t *next_str = (wchar_t *)al_get( possible_comp, i );

		if( next_str )
		{
			wildcard_complete( next_str, wc, desc, desc_func, comp_out, flags );
		}
	}

	free( wc );
}

/**
   If command to complete is short enough, substitute
   the description with the whatis information for the executable.
*/
static void complete_cmd_desc( const wchar_t *cmd, array_list_t *comp )
{
	int i;
	const wchar_t *cmd_start;
	int cmd_len;
	wchar_t *lookup_cmd=0;
	array_list_t list;
	hash_table_t lookup;
	wchar_t *esc;
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
	
	for( i=0; i<al_get_count( comp ); i++ )
	{
		completion_t *c = (completion_t *)al_get( comp, i );
			
		if( !wcslen( c->completion) || (c->completion[wcslen(c->completion)-1] != L'/' )) 
		{
			skip = 0;
			break;
		}
		
	}
	
	if( skip )
	{
		return;
	}
	

	esc = escape( cmd_start, 1 );

	lookup_cmd = wcsdupcat( L"__fish_describe_command ", esc );
	free(esc);

	al_init( &list );
	hash_init( &lookup, &hash_wcs_func, &hash_wcs_cmp );


	/*
	  First locate a list of possible descriptions using a single
	  call to apropos or a direct search if we know the location
	  of the whatis database. This can take some time on slower
	  systems with a large set of manuals, but it should be ok
	  since apropos is only called once.
	*/
	if( exec_subshell( lookup_cmd, &list ) != -1 )
	{
	
		/*
		  Then discard anything that is not a possible completion and put
		  the result into a hashtable with the completion as key and the
		  description as value.

		  Should be reasonably fast, since no memory allocations are needed.
		*/
		for( i=0; i<al_get_count( &list); i++ )
		{
			wchar_t *el = (wchar_t *)al_get( &list, i );
			wchar_t *key, *key_end, *val_begin;
		
			if( !el )
				continue;

			key = el+wcslen(cmd_start);
			key_end = wcschr( key, L'\t' );

			if( !key_end )
				continue;
		
			*key_end = 0;
			val_begin = key_end+1;

			/*
			  And once again I make sure the first character is uppercased
			  because I like it that way, and I get to decide these
			  things.
			*/
			val_begin[0]=towupper(val_begin[0]);
		
			hash_put( &lookup, key, val_begin );
		}

		/*
		  Then do a lookup on every completion and if a match is found,
		  change to the new description.

		  This needs to do a reallocation for every description added, but
		  there shouldn't be that many completions, so it should be ok.
		*/
		for( i=0; i<al_get_count(comp); i++ )
		{
			completion_t *c = (completion_t *)al_get( comp, i );
			const wchar_t *el = c->completion;
			
			wchar_t *new_desc;
			
			new_desc = (wchar_t *)hash_get( &lookup,
											el );
			
			if( new_desc )
			{
				c->description = halloc_wcsdup( comp, new_desc );
			}
		}
	}
	
	hash_destroy( &lookup );
	al_foreach( &list,
				&free );
	al_destroy( &list );
	free( lookup_cmd );
}

/**
   Returns a description for the specified function
*/
static const wchar_t *complete_function_desc( const wchar_t *fn )
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
						  array_list_t *comp,
						  int use_function,
						  int use_builtin,
						  int use_command )
{
	wchar_t *path;
	wchar_t *path_cpy;
	wchar_t *nxt_path;
	wchar_t *state;
	array_list_t possible_comp;
	wchar_t *nxt_completion;

	wchar_t *cdpath = env_get(L"CDPATH");
	wchar_t *cdpath_cpy = wcsdup( cdpath?cdpath:L"." );

	if( (wcschr( cmd, L'/') != 0) || (cmd[0] == L'~' ) )
	{

		if( use_command )
		{
			
			if( expand_string( 0,
							   wcsdup(cmd),
							   comp,
							   ACCEPT_INCOMPLETE | EXECUTABLES_ONLY ) != EXPAND_ERROR )
			{
				complete_cmd_desc( cmd, comp );
			}
		}
	}
	else
	{
		if( use_command )
		{
			
			path = env_get(L"PATH");
			if( path )
			{
			
				path_cpy = wcsdup( path );
			
				for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
				     nxt_path != 0;
				     nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
				{
					int prev_count;
					int i;
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
					
					prev_count = al_get_count( comp );
					
					if( expand_string( 0,
									   nxt_completion,
									   comp,
									   ACCEPT_INCOMPLETE |
									   EXECUTABLES_ONLY ) != EXPAND_ERROR )
					{
						for( i=prev_count; i<al_get_count( comp ); i++ )
						{
							completion_t *c = (completion_t *)al_get( comp, i );
							if(c->flags & COMPLETE_NO_CASE )
							{
								c->completion = halloc_wcsdup( comp, c->completion + path_len + add_slash );
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

		al_init( &possible_comp );

		if( use_function )
		{
			function_get_names( &possible_comp, cmd[0] == L'_' );
			complete_strings( comp, cmd, 0, &complete_function_desc, &possible_comp, 0 );
		}

		al_truncate( &possible_comp, 0 );
		
		if( use_builtin )
		{
			builtin_get_names( &possible_comp );
			complete_strings( comp, cmd, 0, &builtin_get_desc, &possible_comp, 0 );
		}
		al_destroy( &possible_comp );

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
			
				if( expand_string( 0,
								   nxt_completion,
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
static void complete_from_args( const wchar_t *str,
								const wchar_t *args,
								const wchar_t *desc,
								array_list_t *comp_out,
								int flags )
{

	array_list_t possible_comp;

	al_init( &possible_comp );

	proc_push_interactive(0);
	eval_args( args, &possible_comp );
	proc_pop_interactive();
	
	complete_strings( comp_out, str, desc, 0, &possible_comp, flags );

	al_foreach( &possible_comp, &free );
	al_destroy( &possible_comp );
}

/**
   Match against an old style long option
*/
static int param_match_old( complete_entry_opt_t *e,
							const wchar_t *optstr )
{
	return  (optstr[0] == L'-') && (wcscmp( e->long_opt, &optstr[1] ) == 0);
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
		if( wcscmp( e->long_opt, &optstr[2] ) == 0 )
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
		int len = wcslen( e->long_opt );

		if( wcsncmp( e->long_opt, &optstr[2],len ) == 0 )
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
static int short_ok( const wchar_t *arg,
					 wchar_t nextopt,
					 const wchar_t *allopt )
{
	const wchar_t *ptr;

	if( arg[0] != L'-')
		return arg[0] == L'\0';
	if( arg[1] == L'-' )
		return 0;

	if( wcschr( arg, nextopt ) != 0 )
		return 0;

	for( ptr = arg+1; *ptr; ptr++ )
	{
		wchar_t *tmp = wcschr( allopt, *ptr );
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

/**
   This is an event handler triggered when the definition of a
   specifiec function is changed. It automatcally removes the
   specified function.

   This is to make sure that the function disappears if the file is
   removed or if ti contains a syntax error.
*/
static void complete_load_handler( const wchar_t *cmd )
{
	complete_remove( cmd, COMMAND, 0, 0 );
}

void complete_load( const wchar_t *name, int reload )
{
	CHECK( name, );
	parse_util_load( name, 
					 L"fish_complete_path",
					 &complete_load_handler, 
					 reload );	
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
						   array_list_t *comp_out )
{
	complete_entry_t *i;
	complete_entry_opt_t *o;

	array_list_t matches;
	wchar_t *cmd, *path;
	int use_common=1, use_files=1;

	void *context = halloc( 0, 0 );
		
	parse_cmd_string( context, cmd_orig, &path, &cmd );

	complete_load( cmd, 1 );

	al_init( &matches );

	for( i=first_entry; i; i=i->next )
	{
		wchar_t *match = i->cmd_type?path:cmd;

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
				for( o = i->first_option; o; o=o->next )
				{
					wchar_t *arg;
					if( (arg=param_match2( o, str ))!=0 && condition_test( o->condition ))
					{
						use_common &= ((o->result_mode & NO_COMMON )==0);
						use_files &= ((o->result_mode & NO_FILES )==0);
						complete_from_args( arg, o->comp, C_(o->desc), comp_out, o->flags );
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
				for( o = i->first_option; o; o=o->next )
				{
					if( o->old_mode )
					{
						if( param_match_old( o, popt ) && condition_test( o->condition ))
						{
							old_style_match = 1;
							use_common &= ((o->result_mode & NO_COMMON )==0);
							use_files &= ((o->result_mode & NO_FILES )==0);
							complete_from_args( str, o->comp, C_(o->desc), comp_out, o->flags );
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
					for( o = i->first_option; o; o=o->next )
					{
						/*
						  Gnu-style options with _optional_ arguments must
						  be specified as a single token, so that it can
						  be differed from a regular argument.
						*/
						if( !o->old_mode && wcslen(o->long_opt) && !(o->result_mode & NO_COMMON) )
							continue;

						if( param_match( o, popt ) && condition_test( o->condition  ))
						{
							use_common &= ((o->result_mode & NO_COMMON )==0);
							use_files &= ((o->result_mode & NO_FILES )==0);
							complete_from_args( str, o->comp, C_(o->desc), comp_out, o->flags );

						}
					}
				}
			}
		}
		
		if( use_common )
		{

			for( o = i->first_option; o; o=o->next )
			{
				/*
				  If this entry is for the base command,
				  check if any of the arguments match
				*/

				if( !condition_test( o->condition ))
					continue;


				if( (o->short_opt == L'\0' ) && (o->long_opt[0]==L'\0'))
				{
					use_files &= ((o->result_mode & NO_FILES )==0);
					complete_from_args( str, o->comp, C_(o->desc), comp_out, o->flags );
				}
				
				if( wcslen(str) > 0 && use_switches )
				{
					/*
					  Check if the short style option matches
					*/
					if( o->short_opt != L'\0' &&
						short_ok( str, o->short_opt, i->short_opt_str ) )
					{
						const wchar_t *desc = C_(o->desc );
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
						
						string_buffer_t *whole_opt = sb_halloc( context );
						sb_append( whole_opt, o->old_mode?L"-":L"--", o->long_opt, (void *)0 );

						match = wcsncmp( str, (wchar_t *)whole_opt->buff, wcslen(str) )==0;

						if( !match )
						{
							match_no_case = wcsncasecmp( str, (wchar_t *)whole_opt->buff, wcslen(str) )==0;
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

							has_arg = !!wcslen( o->comp );
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
								string_buffer_t completion;
								sb_init( &completion );

								sb_printf( &completion,
										   L"%ls=", 
										   ((wchar_t *)whole_opt->buff)+offset );
						
								completion_allocate( comp_out,
													 (wchar_t *)completion.buff,
													 C_(o->desc),
													 flags );									
								
								sb_destroy( &completion );
								
							}
							
							completion_allocate( comp_out,
												 ((wchar_t *)whole_opt->buff) + offset,
												 C_(o->desc),
												 flags );
						}					
					}
				}
			}
		}
	}
	
	halloc_free( context );
	
	return use_files;
}

/**
   Perform file completion on the specified string
*/
static void complete_param_expand( wchar_t *str,
								   array_list_t *comp_out,
								   int do_file )
{
	wchar_t *comp_str;
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
	
	if( expand_string( 0, 
					   wcsdup(comp_str),
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
							  array_list_t *comp_list )
{
	int i;
	const wchar_t *var = &whole_var[start_offset];
	int varlen = wcslen( var );
	int res = 0;
	array_list_t names;
	al_init( &names );
	env_get_names( &names, 0 );

	for( i=0; i<al_get_count( &names ); i++ )
	{
		wchar_t *name = (wchar_t *)al_get( &names, i );
		int namelen = wcslen( name );
		int match=0, match_no_case=0;	

		if( varlen > namelen )
			continue;

		match = ( wcsncmp( var, name, varlen) == 0 );
		
		if( !match )
		{
			match_no_case = ( wcsncasecmp( var, name, varlen) == 0 );
		}

		if( match || match_no_case )
		{
			wchar_t *value_unescaped, *value;

			value_unescaped = env_get( name );
			if( value_unescaped )
			{
				string_buffer_t desc;
				string_buffer_t comp;
				int flags = 0;
				int offset = 0;
				
				sb_init( &comp );
				if( match )
				{
					sb_append( &comp, &name[varlen] );					
					offset = varlen;
				}
				else
				{
					sb_append_substring( &comp, whole_var, start_offset );
					sb_append( &comp, name );
					flags = COMPLETE_NO_CASE | COMPLETE_DONT_ESCAPE;
				}
				
				value = expand_escape_variable( value_unescaped );

				sb_init( &desc );
				sb_printf( &desc, COMPLETE_VAR_DESC_VAL, value );
				
				completion_allocate( comp_list, 
									 (wchar_t *)comp.buff,
									 (wchar_t *)desc.buff,
									 flags );
				res =1;
				
				free( value );
				sb_destroy( &desc );
				sb_destroy( &comp );
			}
			
		}
	}

	al_destroy( &names );
	return res;
}

/**
   Search the specified string for the \$ sign. If found, try to
   complete as an environment variable. 

   \return 0 if unable to complete, 1 otherwise
*/
static int try_complete_variable( const wchar_t *cmd,
								  array_list_t *comp )
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
							  array_list_t *comp )
{
	const wchar_t *first_char=cmd;
	int res=0;
	double start_time = timef();
	
	if( *first_char ==L'~' && !wcschr(first_char, L'/'))
	{
		const wchar_t *user_name = first_char+1;
		wchar_t *name_end = wcschr( user_name, L'~' );
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
						string_buffer_t desc;
						
						sb_init( &desc );
						sb_printf( &desc,
								   COMPLETE_USER_DESC,
								   pw_name );
						
						completion_allocate( comp, 
											 &pw_name[name_len],
											 (wchar_t *)desc.buff,
											 COMPLETE_NO_SPACE );
						
						res=1;
						
						sb_destroy( &desc );
					}
					else if( wcsncasecmp( user_name, pw_name, name_len )==0 )
					{
						string_buffer_t name;							
						string_buffer_t desc;							
						
						sb_init( &name );
						sb_init( &desc );
						sb_printf( &name,
								   L"~%ls",
								   pw_name );
						sb_printf( &desc,
								   COMPLETE_USER_DESC,
								   pw_name );
							
						completion_allocate( comp, 
											 (wchar_t *)name.buff,
											 (wchar_t *)desc.buff,
											 COMPLETE_NO_CASE | COMPLETE_DONT_ESCAPE | COMPLETE_NO_SPACE );
						res=1;
							
						sb_destroy( &desc );
						sb_destroy( &name );
							
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
			   array_list_t *comp )
{
	wchar_t *tok_begin, *tok_end, *cmdsubst_begin, *cmdsubst_end, *prev_begin, *prev_end;
	wchar_t *buff;
	tokenizer tok;
	wchar_t *current_token=0, *current_command=0, *prev_token=0;
	int on_command=0;
	int pos;
	int done=0;
	int cursor_pos;
	int use_command = 1;
	int use_function = 1;
	int use_builtin = 1;
	int had_ddash = 0;

	CHECK( cmd, );
	CHECK( comp, );

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
		
		buff = wcsndup( cmdsubst_begin, cmdsubst_end-cmdsubst_begin );
		
		if( !buff )
			done=1;
	}

	if( !done )
	{
		int had_cmd=0;
		int end_loop=0;

		tok_init( &tok, buff, TOK_ACCEPT_UNFINISHED );

		while( tok_has_next( &tok) && !end_loop )
		{

			switch( tok_last_type( &tok ) )
			{

				case TOK_STRING:
				{

					wchar_t *ncmd = tok_last( &tok );
					int is_ddash = (wcscmp( ncmd, L"--" ) == 0) && ( (tok_get_pos( &tok )+2) < pos );
					
					if( !had_cmd )
					{

						if( parser_keywords_is_subcommand( ncmd ) )
						{
							if( wcscmp( ncmd, L"builtin" )==0)
							{
								use_function = 0;
								use_command  = 0;
								use_builtin  = 1;
							}
							else if( wcscmp( ncmd, L"command" )==0)
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
							
							free( current_command );
							current_command = wcsdup( ncmd );
							
							token_end = tok_get_pos( &tok ) + wcslen( ncmd );
							
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
		free( buff );

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
			free( current_command );
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
				if( !al_get_count( comp ) )
					do_file = 1;
				
				/*
				  This function wants the unescaped string
				*/
				complete_param_expand( current_token, comp, do_file );
			}
		}
	}
	
	free( current_token );
	free( current_command );
	free( prev_token );

	condition_cache_clear();



}

/**
   Print the GNU longopt style switch \c opt, and the argument \c
   argument to the specified stringbuffer, but only if arguemnt is
   non-null and longer than 0 characters.
*/
static void append_switch( string_buffer_t *out,
						   const wchar_t *opt,
						   const wchar_t *argument )
{
	wchar_t *esc;

	if( !argument || wcscmp( argument, L"") == 0 )
		return;

	esc = escape( argument, 1 );
	sb_printf( out, L" --%ls %ls", opt, esc );
	free(esc);
}

void complete_print( string_buffer_t *out )
{
	complete_entry_t *e;

	CHECK( out, );

	for( e = first_entry; e; e=e->next )
	{
		complete_entry_opt_t *o;
		for( o= e->first_option; o; o=o->next )
		{
			wchar_t *modestr[] =
				{
					L"",
					L" --no-files",
					L" --require-parameter",
					L" --exclusive"
				}
			;

			sb_printf( out,
					   L"complete%ls",
					   modestr[o->result_mode] );

			append_switch( out,
						   e->cmd_type?L"path":L"command",
						   e->cmd );


			if( o->short_opt != 0 )
			{
				sb_printf( out,
						   L" --short-option '%lc'",
						   o->short_opt );
			}


			append_switch( out,
						   o->old_mode?L"old-option":L"long-option",
						   o->long_opt );

			append_switch( out,
						   L"description",
						   C_(o->desc) );

			append_switch( out,
						   L"arguments",
						   o->comp );

			append_switch( out,
						   L"condition",
						   o->condition );

			sb_printf( out, L"\n" );
		}
	}
}

