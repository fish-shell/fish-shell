/** \file complete.c
	Functions related to tab-completion.

	These functions are used for storing and retrieving tab-completion data, as well as for performing tab-completion.
*/
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

#include "config.h"

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
#include "translate.h"
#include "parse_util.h"
#include "halloc_util.h"
#include "wutil.h"


/*
  Completion description strings, mostly for different types of files, such as sockets, block devices, etc.

  There are a few more completion description strings defined in
  expand.c. Maybe all completion description strings should be defined
  in the same file?
*/

/**
   Description for ~USER completion
*/
#define COMPLETE_USER_DESC _( L"User home" )

/**
   Description for short variables. The value is concatenated to this description
*/
#define COMPLETE_VAR_DESC_VAL _( L"Variable: " )

/**
   Description for generic executable
*/
#define COMPLETE_EXEC_DESC _( L"Executable" )
/**
   Description for link to executable
*/
#define COMPLETE_EXEC_LINK_DESC _( L"Executable link" )

/**
   Description for regular file
*/
#define COMPLETE_FILE_DESC _( L"File" )
/**
   Description for character device
*/
#define COMPLETE_CHAR_DESC _( L"Character device" )
/**
   Description for block device
*/
#define COMPLETE_BLOCK_DESC _( L"Block device" )
/**
   Description for fifo buffer
*/
#define COMPLETE_FIFO_DESC _( L"Fifo" )
/**
   Description for symlink
*/
#define COMPLETE_SYMLINK_DESC _( L"Symbolic link" )
/**
   Description for Rotten symlink
*/
#define COMPLETE_ROTTEN_SYMLINK_DESC _( L"Rotten symbolic link" )
/**
   Description for symlink loop
*/
#define COMPLETE_LOOP_SYMLINK_DESC _( L"Symbolic link loop" )
/**
   Description for socket files
*/
#define COMPLETE_SOCKET_DESC _( L"Socket" )
/**
   Description for directories
*/
#define COMPLETE_DIRECTORY_DESC _( L"Directory" )

/**
   Generic description for functions
*/
#define COMPLETE_FUNCTION_DESC _( L"Function" )
/**
   Generic description for builtin commands
*/
#define COMPLETE_BUILTIN_DESC _( L"Builtin" )

/**
   The command to run to get a description from a file suffix
*/
#define SUFFIX_CMD_STR L"mimedb 2>/dev/null -fd "

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
}
	complete_entry_opt;

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
	complete_entry_opt *first_option;
	/** Next command completion in the linked list */
	struct complete_entry *next;
	/** True if no other options than the ones supplied are possible */
	int authorative;
}
	complete_entry;

/** First node in the linked list of all completion entries */
static complete_entry *first_entry=0;

/** Hashtable containing all descriptions that describe an executable */
static hash_table_t *suffix_hash=0;

/**
   Table of completions conditions that have already been tested and
   the corresponding test results
*/
static hash_table_t *condition_cache=0;

/**
   String buffer used by complete_get_desc
*/
static string_buffer_t *get_desc_buff=0;


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
			die_mem();

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

	if( test_res == CC_TRUE )
	{
		return 1;
	}

	return 0;
}


/**
   Recursively free all complete_entry_opt structs and their contents
*/
static void complete_free_opt_recursive( complete_entry_opt *o )
{
	if( o->next != 0 )
		complete_free_opt_recursive( o->next );
	free(o);
}

/**
   Free a complete_entry and its contents
*/
static void complete_free_entry( complete_entry *c )
{
//	free( c->cmd );
	free( c->short_opt_str );
	complete_free_opt_recursive( c->first_option );
	free( c );
}

/**
   Free hash key and hash value
*/
static void clear_hash_entry( const void *key, const void *data )
{
	free( (void *)key );
	free( (void *)data );
}

void complete_init()
{
}

void complete_destroy()
{
	complete_entry *i=first_entry, *prev;
	
	while( i )
	{
		prev = i;
		i=i->next;
		complete_free_entry( prev );
	}
	first_entry = 0;
	
	if( suffix_hash )
	{
		hash_foreach( suffix_hash, &clear_hash_entry );
		hash_destroy( suffix_hash );
		free( suffix_hash );
		suffix_hash=0;
	}
	
	parse_util_load_reset(L"fish_complete_path");
	
}

/**
   Search for an exactly matching completion entry
*/
static complete_entry *complete_find_exact_entry( const wchar_t *cmd,
												  const int cmd_type )
{
	complete_entry *i;
	for( i=first_entry; i; i=i->next )
	{
		if( ( wcscmp(cmd, i->cmd)==0) && ( cmd_type == i->cmd_type ))
		{
			return i;
		}
	}
	return 0;
}

void complete_add( const wchar_t *cmd,
				   int cmd_type,
				   wchar_t short_opt,
				   const wchar_t *long_opt,
				   int old_mode,
				   int result_mode,
				   int authorative,
				   const wchar_t *condition,
				   const wchar_t *comp,
				   const wchar_t *desc )
{
	complete_entry *c =
		complete_find_exact_entry( cmd, cmd_type );
	complete_entry_opt *opt;

	if( c == 0 )
	{
		if( !(c = malloc( sizeof(complete_entry) )))
		{
			die_mem();
		}
		
		c->next = first_entry;
		first_entry = c;

		c->first_option = 0;

		c->cmd = intern( cmd );
		c->cmd_type = cmd_type;
		c->short_opt_str = wcsdup(L"");
	}

	if( !(opt = malloc( sizeof( complete_entry_opt ) )))
	{
		die_mem();
	}
	
	opt->next = c->first_option;
	c->first_option = opt;
	c->authorative = authorative;
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

	opt->comp = intern(comp?comp:L"");
	opt->condition = intern(condition?condition:L"");
	opt->long_opt = intern( long_opt?long_opt:L"" );

	if( desc && wcslen( desc ) )
	{
		opt->desc = intern( desc );
	}
	else
	{
		opt->desc = L"";
	}
}

void complete_remove( const wchar_t *cmd,
					  int cmd_type,
					  wchar_t short_opt,
					  const wchar_t *long_opt )
{
	complete_entry *e, *eprev=0, *enext=0;

	for( e = first_entry; e; e=enext )
	{
		enext=e->next;

		if( (cmd_type == e->cmd_type ) &&
			( wcscmp( cmd, e->cmd) == 0 ) )
		{
			complete_entry_opt *o, *oprev=0, *onext=0;

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
				if( eprev == 0 )
				{
					first_entry = e->next;
				}
				else
				{
					eprev->next = e->next;
				}

				free( e->short_opt_str );
				free( e );
				e=0;
			}

		}

		if( e )
		{
			eprev = e;
		}
	}
}

/**
   Find the full path and commandname from a command string.  the
   result of \c pathp must be freed by the caller, the result of \c
   cmdp must not be freed by the caller.
*/
static void parse_cmd_string( const wchar_t *str,
							  wchar_t **pathp,
							  wchar_t **cmdp )
{
    wchar_t *cmd, *path;

	/* Get the path of the command */
	path = get_filename( str );
	if( path == 0 )
	{
		/**
		   Use the empty string as the 'path' for commands that can
		   not be found.
		*/
		path = wcsdup(L"");
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
	complete_entry *i;
	complete_entry_opt *o;
	wchar_t *cmd, *path;
	int found_match = 0;
	int authorative = 1;
	int opt_found=0;
	hash_table_t gnu_match_hash;
	int is_gnu_opt=0;
	int is_old_opt=0;
	int is_short_opt=0;
	int is_gnu_exact=0;
	int gnu_opt_len=0;
	char *short_validated;

	/*
	  Check some generic things like -- and - options.
	*/
	switch( wcslen(opt ) )
	{

		case 0:
		{
			return 1;
		}
		
		case 1:
		{
			return  opt[0] == L'-';
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

	if( !(short_validated = malloc( wcslen( opt ) )))
	{
		die_mem();
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
	
	parse_cmd_string( str, &path, &cmd );

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

		if( !i->authorative )
		{
			authorative = 0;
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
	free( path );

	if( authorative )
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
								 wcsdupcat2(_( L"Unknown option: " ), L"'", str, L"'", 0) );
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
							 wcsdupcat2( _(L"Unknown option: "), L"'", opt, L"\'", 0) );
				}
				else
				{
					al_push( errors,
							 wcsdupcat2( _(L"Multiple matches for option: "), L"'", opt, L"\'", 0) );
				}
			}
		}
	}

	hash_destroy( &gnu_match_hash );
	free( short_validated );

	return (authorative && found_match)?opt_found:1;
}

int complete_is_valid_argument( const wchar_t *str,
								const wchar_t *opt,
								const wchar_t *arg )
{
	return 1;
}

/**
   Use the mimedb command to look up a description for a given suffix
*/
static const wchar_t *complete_get_desc_suffix( const wchar_t *suff_orig )
{

	int len;
	wchar_t *suff;
	wchar_t *pos;
	wchar_t *tmp;
	wchar_t *desc;

	len = wcslen(suff_orig );
	
	if( len == 0 )
		return COMPLETE_FILE_DESC;

	if( !suffix_hash )
	{
		suffix_hash = malloc( sizeof( hash_table_t) );
		if( !suffix_hash )
			die_mem();
		hash_init( suffix_hash, &hash_wcs_func, &hash_wcs_cmp );
	}

	suff = wcsdup(suff_orig);

	for( pos=suff; *pos; pos++ )
	{
		if( wcschr( L"?;#~@&", *pos ) )
		{
			*pos=0;
			break;
		}
	}

	tmp = escape( suff, 1 );
	free(suff);
	suff = tmp;
	desc = (wchar_t *)hash_get( suffix_hash, suff );

	if( !desc )
	{
		wchar_t *cmd = wcsdupcat( SUFFIX_CMD_STR, suff );

		if( cmd )
		{
			array_list_t l;
			al_init( &l );

			exec_subshell( cmd, &l );
			free(cmd);

			if( al_get_count( &l )>0 )
			{
				wchar_t *ln = (wchar_t *)al_get(&l, 0 );
				if( wcscmp( ln, L"unknown" ) != 0 )
				{
					desc = wcsdup( ln);
					/*
					  I have decided I prefer to have the description
					  begin in uppercase and the whole universe will just
					  have to accept it. Hah!
					*/
					desc[0]=towupper(desc[0]);
				}
			}

			al_foreach( &l, (void (*)(const void *))&free );
			al_destroy( &l );
		}

		if( !desc )
		{
			desc = wcsdup(COMPLETE_FILE_DESC);
		}

		hash_put( suffix_hash, suff!=suff_orig?suff:wcsdup(suff), desc );
	}
	else
	{
		free( suff );
	}

	return desc;
}


const wchar_t *complete_get_desc( const wchar_t *filename )
{
	struct stat buf;

	if( !get_desc_buff )
	{
		get_desc_buff = sb_halloc( global_context);
	}
	else
	{
		sb_clear( get_desc_buff );
	}
	
	if( lwstat( filename, &buf )==0)
	{
		if( S_ISCHR(buf.st_mode) )
		{
			sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_CHAR_DESC );
		}
		else if( S_ISBLK(buf.st_mode) )
			sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_BLOCK_DESC );
		else if( S_ISFIFO(buf.st_mode) )
			sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_FIFO_DESC );
		else if( S_ISLNK(buf.st_mode))
		{
			struct stat buf2;

			if( wstat( filename, &buf2 ) == 0 )
			{
				if( S_ISDIR(buf2.st_mode) )
				{
					sb_printf( get_desc_buff, L"/%lc%ls", COMPLETE_SEP, COMPLETE_SYMLINK_DESC );
				}
				else if( waccess( filename, X_OK ) == 0 )
					sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_EXEC_LINK_DESC );
				else
					sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_SYMLINK_DESC );
			}
			else
			{
				switch( errno )
				{
					case ENOENT:
					{
						sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_ROTTEN_SYMLINK_DESC );
						break;
					}
					
					case EACCES:
					{
						break;
					}
					
					default:
					{
						if( errno == ELOOP )
						{
							sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_LOOP_SYMLINK_DESC );
						}

						/*
						  Some kind of unknown broken symlink. We
						  ignore it here, and it will get a 'file'
						  description, or one based on suffix.
						*/
						break;
					}
				}
			}

		}
		else if( S_ISSOCK(buf.st_mode))
			sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_SOCKET_DESC );
		else if( S_ISDIR(buf.st_mode) )
			sb_printf( get_desc_buff, L"/%lc%ls", COMPLETE_SEP, COMPLETE_DIRECTORY_DESC );
		else if( waccess( filename, X_OK ) == 0 )
		{
			sb_printf( get_desc_buff, L"%lc%ls", COMPLETE_SEP, COMPLETE_EXEC_DESC );
		}

	}

	if( wcslen((wchar_t *)get_desc_buff->buff) == 0 )
	{
		wchar_t *suffix = wcsrchr( filename, L'.' );
		if( suffix != 0 && !wcsrchr( suffix, L'/' ) )
		{
			sb_printf( get_desc_buff,
					   L"%lc%ls",
					   COMPLETE_SEP,
					   complete_get_desc_suffix( suffix ) );
		}
		else
		{
			sb_printf( get_desc_buff,
					   L"%lc%ls",
					   COMPLETE_SEP,
					   COMPLETE_FILE_DESC );
		}

	}

	return (wchar_t *)get_desc_buff->buff;
}

/**
   Copy any strings in possible_comp which have the specified prefix
   to the list comp_out. The prefix may contain wildcards.

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
static void copy_strings_with_prefix( array_list_t *comp_out,
									  const wchar_t *wc_escaped,
									  const wchar_t *desc,
									  const wchar_t *(*desc_func)(const wchar_t *),
									  array_list_t *possible_comp )
{
	int i;
	wchar_t *wc, *tmp;

	tmp = expand_one( 0,
					  wcsdup(wc_escaped), EXPAND_SKIP_SUBSHELL | EXPAND_SKIP_WILDCARDS);
	if(!tmp)
		return;

	wc = parse_util_unescape_wildcards( tmp );
	free(tmp);
	
	for( i=0; i<al_get_count( possible_comp ); i++ )
	{
		wchar_t *next_str = (wchar_t *)al_get( possible_comp, i );
		if( next_str )
			wildcard_complete( next_str, wc, desc, desc_func, comp_out );
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
	exec_subshell( lookup_cmd, &list );
	
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
		key = el+wcslen(cmd);
		
		if( !el )
			continue;

		key_end = wcschr( el, L'\t' );
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
		wchar_t *el = (wchar_t *)al_get( comp, i );
		wchar_t *cmd_end = wcschr( el,
								   COMPLETE_SEP );
		wchar_t *new_desc;

		if( cmd_end )
			*cmd_end = 0;

		new_desc = (wchar_t *)hash_get( &lookup,
										el );

		if( new_desc )
		{
			wchar_t *new_el = wcsdupcat2( el,
										  COMPLETE_SEP_STR,
										  new_desc,
										  0 );

			al_set( comp, i, new_el );
			free( el );
		}
		else
		{
			if( cmd_end )
				*cmd_end = COMPLETE_SEP;
		}
	}

	hash_destroy( &lookup );
	al_foreach( &list,
				(void(*)(const void *))&free );
	al_destroy( &list );
	free( lookup_cmd );
}

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
						  array_list_t *comp )
{
	wchar_t *path;
	wchar_t *path_cpy;
	wchar_t *nxt_path;
	wchar_t *state;
	array_list_t possible_comp;
	int i;
	array_list_t tmp;
	wchar_t *nxt_completion;

	wchar_t *cdpath = env_get(L"CDPATH");
	wchar_t *cdpath_cpy = wcsdup( cdpath?cdpath:L"." );

	if( (wcschr( cmd, L'/') != 0) || (cmd[0] == L'~' ) )
	{
		array_list_t tmp;
		al_init( &tmp );

		if( expand_string( 0,
						   wcsdup(cmd),
						   comp,
						   ACCEPT_INCOMPLETE | EXECUTABLES_ONLY ) != EXPAND_ERROR )
		{
			complete_cmd_desc( cmd, comp );
		}
		al_destroy( &tmp );
	}
	else
	{
		path = env_get(L"PATH");
		path_cpy = wcsdup( path );

		for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
			 nxt_path != 0;
			 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
		{
			nxt_completion = wcsdupcat2( nxt_path,
										 (nxt_path[wcslen(nxt_path)-1]==L'/'?L"":L"/"),
										 cmd,
										 0 );
			if( ! nxt_completion )
				continue;

			al_init( &tmp );

			if( expand_string( 0,
							   nxt_completion,
							   &tmp,
							   ACCEPT_INCOMPLETE |
							   EXECUTABLES_ONLY ) != EXPAND_ERROR )
			{
				for( i=0; i<al_get_count(&tmp); i++ )
				{
					al_push( comp, al_get( &tmp, i ) );
				}
			}

			al_destroy( &tmp );

		}
		free( path_cpy );

		complete_cmd_desc( cmd, comp );

		/*
		  These return the original strings - don't free them
		*/

		al_init( &possible_comp );
		function_get_names( &possible_comp, cmd[0] == L'_' );
		copy_strings_with_prefix( comp, cmd, COMPLETE_FUNCTION_DESC, &complete_function_desc, &possible_comp );
		al_truncate( &possible_comp, 0 );

		builtin_get_names( &possible_comp );
		copy_strings_with_prefix( comp, cmd, COMPLETE_BUILTIN_DESC, &builtin_get_desc, &possible_comp );
		al_destroy( &possible_comp );


	}

	/*
	  Tab complete implicit cd for directories in CDPATH
	*/
	if( cmd[0] != L'/' && ( wcsncmp( cmd, L"./", 2 )!=0) )
	{
		
		for( nxt_path = wcstok( cdpath_cpy, ARRAY_SEP_STR, &state );
			 nxt_path != 0;
			 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
		{
			int i;
			array_list_t tmp;
			wchar_t *nxt_completion=
				wcsdupcat2( nxt_path,
							(nxt_path[wcslen(nxt_path)-1]==L'/'?L"":L"/"),
							cmd,
							0 );
			if( ! nxt_completion )
			{
				continue;
			}
			
			al_init( &tmp );

			if( expand_string( 0,
							   nxt_completion,
							   &tmp,
							   ACCEPT_INCOMPLETE | DIRECTORIES_ONLY ) != EXPAND_ERROR )
			{

				for( i=0; i<al_get_count(&tmp); i++ )
				{
					wchar_t *nxt = (wchar_t *)al_get( &tmp, i );

					wchar_t *desc = wcsrchr( nxt, COMPLETE_SEP );
					if( desc )
					{
						int is_valid = desc && (wcscmp(desc+1,
													   COMPLETE_DIRECTORY_DESC)==0);
						if( is_valid )
						{
							al_push( comp, nxt );
						}
						else
						{
							free(nxt);
						}
					}
				}
			}

			al_destroy( &tmp );
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
								array_list_t *comp_out )
{

	array_list_t possible_comp;
/*	int i; */

	al_init( &possible_comp );

	proc_push_interactive(0);
	eval_args( args, &possible_comp );
	proc_pop_interactive();
	
	/*
	  Completion results where previously unescaped by the
	  code below. I have no idea why that was done, but I have
	  not removed this since I'm not sure if this might be
	  correct, though I can't think of any reason why it
	  should be.

	  If this code is readded - add an explanation of _why_ completion
	  strings should be escaped!
	*/
	/*
	for( i=0; i< al_get_count( &possible_comp ); i++ )
	{
		wchar_t *next = (wchar_t *)al_get( &possible_comp, i );
		wchar_t *next_unescaped;
		if( next )
		{
			next_unescaped = unescape( next, 0 );
			if( next_unescaped )
			{
				al_set( &possible_comp , i, next_unescaped );
			}
			else
			{
				al_set( &possible_comp , i, 0 );
				debug( 2, L"Could not expand string %ls on line %d of file %s", next, __LINE__, __FILE__ );
			}
			free( next );
		}
		else
		{
			debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
		}
	}
	*/

	copy_strings_with_prefix( comp_out, str, desc, 0, &possible_comp );

	al_foreach( &possible_comp, (void (*)(const void *))&free );
	al_destroy( &possible_comp );
}

/**
   Match against an old style long option
*/
static int param_match_old( complete_entry_opt *e,
							wchar_t *optstr )
{
	return  (optstr[0] == L'-') && (wcscmp( e->long_opt, &optstr[1] ) == 0);
}

/**
   Match a parameter
*/
static int param_match( complete_entry_opt *e,
						wchar_t *optstr )
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
static wchar_t *param_match2( complete_entry_opt *e,
							  wchar_t *optstr )
{
	if( e->short_opt != L'\0' && e->short_opt == optstr[1] )
		return &optstr[2];
	if( !e->old_mode && (wcsncmp( L"--", optstr, 2 ) == 0) )
	{
		int len = wcslen( e->long_opt );

		if( wcsncmp( e->long_opt, &optstr[2],len ) == 0 )
		{
			if( optstr[len+2] == L'=' )
				return &optstr[len+3];
		}
	}
	return 0;
}

/**
   Tests whether a short option is a viable completion
*/
static int short_ok( wchar_t *arg,
					 wchar_t nextopt,
					 wchar_t *allopt )
{
	wchar_t *ptr;

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

static void complete_load_handler( const wchar_t *cmd )
{
	complete_remove( cmd, COMMAND, 0, 0 );
}

void complete_load( const wchar_t *name, int reload )
{
	parse_util_load( name, L"fish_complete_path", &complete_load_handler, reload );	
}

/**
   Find completion for the argument str of command cmd_orig with
   previous option popt. Insert results into comp_out. Return 0 if file
   completion should be disabled, 1 otherwise.
*/
static int complete_param( wchar_t *cmd_orig,
						   wchar_t *popt,
						   wchar_t *str,
						   array_list_t *comp_out )
{
	complete_entry *i;
	complete_entry_opt *o;

	array_list_t matches;
	wchar_t *cmd, *path;
	int use_common=1, use_files=1;

	parse_cmd_string( cmd_orig, &path, &cmd );

	complete_load( cmd, 1 );

	al_init( &matches );


	for( i=first_entry; i; i=i->next )
	{
		wchar_t *match = i->cmd_type?path:cmd;

		if( ( (!wildcard_match( match, i->cmd ) ) ) )
			continue;

		use_common=1;
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
					complete_from_args( arg, o->comp, _(o->desc), comp_out );
				}

			}
		}
		else if( popt[0] == L'-' )
		{
			/* Check if the previous option has any specified
			   arguments to match against */
			int found_old = 0;

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
						found_old = 1;
						use_common &= ((o->result_mode & NO_COMMON )==0);
						use_files &= ((o->result_mode & NO_FILES )==0);
						complete_from_args( str, o->comp, _(o->desc), comp_out );
					}
				}
			}

			/*
			  No old style option matched, or we are not using old
			  style options. We check if any short (or gnu style
			  options do.
			*/
			if( !found_old )
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
						complete_from_args( str, o->comp, _(o->desc), comp_out );

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
					complete_from_args( str, o->comp, _(o->desc), comp_out );
				}

				if( wcslen(str) > 0 )
				{
					/*
					  Check if the short style option matches
					*/
					if( o->short_opt != L'\0' &&
						short_ok( str, o->short_opt, i->short_opt_str ) )
					{
						const wchar_t *desc = _(o->desc );
						wchar_t *next_opt =
							malloc( sizeof(wchar_t)*(3 + wcslen(desc)));
						if( !next_opt )
							die_mem();

						next_opt[0]=o->short_opt;
						next_opt[1]=COMPLETE_SEP;
						next_opt[2]=L'\0';
						wcscat( next_opt, desc );
						al_push( comp_out, next_opt );
					}

					/*
					  Check if the long style option matches
					*/
					if( o->long_opt[0] != L'\0' )
					{
						string_buffer_t whole_opt;
						sb_init( &whole_opt );
						sb_append2( &whole_opt, o->old_mode?L"-":L"--", o->long_opt, (void *)0 );

						if( wcsncmp( str, (wchar_t *)whole_opt.buff, wcslen(str) )==0)
						{
							/*
							  If the option requires arguments, add
							  option with an appended '=' . If the
							  option does not accept arguments, add
							  option. If option accepts but does not
							  require arguments, add both.
							*/

							if( o->old_mode || !(o->result_mode & NO_COMMON ) )
							{
								al_push( comp_out,
										 wcsdupcat2( &((wchar_t *)whole_opt.buff)[wcslen(str)], 
													 COMPLETE_SEP_STR, 
													 _(o->desc), 
													 (void *)0) );
							}

							if( !o->old_mode && ( wcslen(o->comp) || (o->result_mode & NO_COMMON ) ) )
							{
								al_push( comp_out,
										 wcsdupcat2( &((wchar_t *)whole_opt.buff)[wcslen(str)], 
													 L"=", 
													 COMPLETE_SEP_STR, 
													 _(o->desc), 
													 (void *)0) );
							}
						}
						sb_destroy( &whole_opt );
					}
				}
			}
		}
	}
	free( path );
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

	if( (wcsncmp( str, L"--", 2 )) == 0 && (comp_str = wcschr(str, L'=' ) ) )
	{
		comp_str++;
	}
	else
	{
		comp_str = str;
	}
	
	debug( 3,
		   L"expand_string( \"%ls\", comp_out, EXPAND_SKIP_SUBSHELL | ACCEPT_INCOMPLETE | %ls );",
		   comp_str,
		   do_file?L"0":L"EXPAND_SKIP_WILDCARDS" );
	
	expand_string( 0, 
				   wcsdup(comp_str),
				   comp_out,
				   EXPAND_SKIP_SUBSHELL | ACCEPT_INCOMPLETE | (do_file?0:EXPAND_SKIP_WILDCARDS) );
}

/**
   Complete the specified string as an environment variable
*/
static int complete_variable( const wchar_t *var,
							  array_list_t *comp )
{
	int i;
	int varlen = wcslen( var );
	int res = 0;

	array_list_t names;
	al_init( &names );
	env_get_names( &names, 0 );

	for( i=0; i<al_get_count( &names ); i++ )
	{
		wchar_t *name = (wchar_t *)al_get( &names, i );
		int namelen = wcslen( name );

		if( varlen > namelen )
			continue;

		if( wcsncmp( var, name, varlen) == 0 )
		{
			wchar_t *value = expand_escape_variable( env_get( name ));
			wchar_t *blarg;
			/*
			  Variable description is 'Variable: VALUE
			*/
			blarg = wcsdupcat2( &name[varlen], COMPLETE_SEP_STR, COMPLETE_VAR_DESC_VAL, value, 0 );

			if( blarg )
			{
				res =1;
				al_push( comp, blarg );
			}
			free( value );

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
			return complete_variable( &cmd[i+1], comp );
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
	const wchar_t *first_char=0;
	const wchar_t *p;
	int mode = 0;
	int res = 0;

	for( p=cmd; *p; p++ )
	{
		switch( mode )
		{
			/*Between parameters*/
			case 0:
				switch( *p )
				{
					case L'\"':
						mode=2;
						p++;
						first_char = p;
						break;
					case L' ':
					case L'\t':
					case L'\n':
					case L'\r':
						break;
					default:
						mode=1;
						first_char = p;
				}
				break;
				/*Inside non-quoted parameter*/
			case 1:
				switch( *p )
				{
					case L' ':
					case L'\t':
					case L'\n':
					case L'\r':
						mode = 0;
						break;
				}
				break;
			case 2:
				switch( *p )
				{
					case L'\"':
						if( *(p-1) != L'\\' )
							mode =0;
						break;
				}
				break;
		}
	}

	if( mode != 0 )
	{
		if( *first_char ==L'~' )
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
					wchar_t *pw_name = str2wcs( pw->pw_name );
					if( pw_name )
					{
						if( wcsncmp( user_name, pw_name, name_len )==0 )
						{
							wchar_t *blarg = wcsdupcat2( &pw_name[name_len],
														 L"/",
														 COMPLETE_SEP_STR,
														 COMPLETE_USER_DESC,
														 0 );
							if( blarg != 0 )
							{
								al_push( comp, blarg );
								res=1;
							}
						}
						free( pw_name );
					}
				}
				endpwent();
			}
		}
	}

	return res;
}

void complete( const wchar_t *cmd,
			   array_list_t *comp )
{
	const wchar_t *begin, *end, *prev_begin, *prev_end;
	wchar_t *buff;
	tokenizer tok;
	wchar_t *current_token=0, *current_command=0, *prev_token=0;

	int on_command=0;
	int pos;

	int old_error_max = error_max;
	int done=0;

	int cursor_pos = wcslen(cmd );

	error_max=0;

	/**
	   If we are completing a variable name or a tilde expansion user
	   name, we do that and return. No need for any other competions.
	*/

	if( try_complete_variable( cmd, comp ))
	{
		done=1;

	}
	else if( try_complete_user( cmd, comp ))
	{
		done=1;
	}

	/*
	  Set on_command to true if cursor is over a command, and set the
	  name of the current command, and various other parsing to find
	  out what we should complete, and how it should be completed.
	*/

	if( !done )
	{
		parse_util_cmdsubst_extent( cmd, cursor_pos, &begin, &end );

		if( !begin )
			done=1;
	}

	if( !done )
	{
		pos = cursor_pos-(begin-cmd);
		
		buff = wcsndup( begin, end-begin );
		
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
					if( !had_cmd )
					{
						if( parser_is_subcommand( tok_last( &tok ) ) )
							break;

						current_command = wcsdup( tok_last( &tok ) );

						on_command = (pos <= tok_get_pos( &tok) + wcslen( tok_last( &tok ) ) );
						had_cmd=1;
					}
					break;

				case TOK_END:
				case TOK_PIPE:
				case TOK_BACKGROUND:
					had_cmd=0;
					break;


				case TOK_ERROR:
					end_loop=1;
					break;

			}
			if( tok_get_pos( &tok ) >= pos )
				end_loop=1;

			tok_next( &tok );

		}

		tok_destroy( &tok );
		free( buff );

		/*
		  Get the string to complete
		*/

		parse_util_token_extent( cmd, cursor_pos, &begin, &end, &prev_begin, &prev_end );

		current_token = wcsndup( begin, cursor_pos-(begin-cmd) );

		prev_token = prev_begin ? wcsndup( prev_begin, prev_end - prev_begin ): wcsdup(L"");

//		fwprintf( stderr, L"on_command: %d, %ls %ls\n", on_command, current_compmand, current_token );

		if( current_token && current_command && prev_token )
		{
			if( on_command )
			{
				/* Complete command filename */
				complete_cmd( current_token,
							  comp );
			}
			else
			{
				/*
				  Complete parameter. Parameter expansion should be
				  performed against both the globbed and the unglobbed
				  version of the string, so we create a list containing
				  all possible versions of the string that is to be
				  expanded. This is potentially very slow.
				*/

				int do_file;

				do_file = complete_param( current_command, prev_token, current_token, comp );
				complete_param_expand( current_token, comp, do_file );
			}
		}

		free( current_token );
		free( current_command );
		free( prev_token );
	}

	error_max=old_error_max;
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

	if( !argument || argument==L"" )
		return;

	esc = escape( argument, 1 );
	sb_printf( out, L" --%ls %ls", opt, esc );
	free(esc);
}

void complete_print( string_buffer_t *out )
{
	complete_entry *e;

	for( e = first_entry; e; e=e->next )
	{
		complete_entry_opt *o;
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
						   _(o->desc) );

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

