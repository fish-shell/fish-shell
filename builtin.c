/** \file builtin.c
	Functions for executing builtin functions.

	How to add a new builtin function:

	1). Create a function in builtin.c with the following signature:

	<tt>static int builtin_NAME( wchar_t ** args )</tt>

	where NAME is the name of the builtin, and args is a zero-terminated list of arguments.

	2). Add a line like { L"NAME", &builtin_NAME, N_(L"Bla bla bla") }, to the builtin_data variable. The description is used by the completion system.

	3). Create a file doc_src/NAME.txt, containing the manual for the builtin in Doxygen-format. Check the other builtin manuals for proper syntax.

	4). Add an entry to the BUILTIN_DOC_SRC variable of Makefile.in. Note that the entries should be sorted alphabetically!

	5). Add an entry to the manual at the builtin-overview subsection of doc_src/doc.hdr. Note that the entries should be sorted alphabetically!

	6). Use 'darcs add doc_src/NAME.txt' to start tracking changes to the documentation file.

*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <wctype.h>
#include <sys/time.h>
#include <time.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "function.h"
#include "complete.h"
#include "proc.h"
#include "parser.h"
#include "reader.h"
#include "env.h"
#include "common.h"
#include "wgetopt.h"
#include "sanity.h"
#include "tokenizer.h"
#include "wildcard.h"
#include "input_common.h"
#include "input.h"
#include "intern.h"
#include "event.h"
#include "signal.h"

#include "halloc.h"
#include "halloc_util.h"
#include "parse_util.h"
#include "expand.h"



/**
   The default prompt for the read command
*/
#define DEFAULT_READ_PROMPT L"set_color green; echo read; set_color normal; echo \"> \""

/**
   The mode name to pass to history and input
*/

#define READ_MODE_NAME L"fish_read"

/**
   The send stuff to foreground message
*/
#define FG_MSG _( L"Send job %d, '%ls' to foreground\n" )

/**
   Datastructure to describe a builtin. 
*/
typedef struct builtin_data
{
	/**
	   Name of the builtin
	*/
	const wchar_t *name;
	/**
	   Function pointer tothe builtin implementation
	*/
	int (*func)(wchar_t **argv);
	/**
	   Description of what the builtin does
	*/
	const wchar_t *desc;
}
	builtin_data_t;


/**
   Table of all builtins
*/
static hash_table_t builtin;

int builtin_out_redirect;
int builtin_err_redirect;

/*
  Buffers for storing the output of builtin functions
*/
string_buffer_t *sb_out=0, *sb_err=0;

/**
   Stack containing builtin I/O for recursive builtin calls.
*/
static array_list_t io_stack;

/**
   The file from which builtin functions should attempt to read, use
   instead of stdin.
*/
static int builtin_stdin;

/**
   Table containing descriptions for all builtins
*/
static hash_table_t *desc=0;

/**
   Counts the number of non null pointers in the specified array
*/

static int builtin_count_args( wchar_t **argv )
{
	int argc = 1;
	while( argv[argc] != 0 )
	{
		argc++;
	}
	return argc;
}

/** 
	This function works like wperror, but it prints its result into
	the sb_err string_buffer_t instead of to stderr. Used by the builtin
	commands.
*/

static void builtin_wperror( const wchar_t *s)
{
	if( s != 0 )
	{
		sb_append2( sb_err, s, L": ", (void *)0 );
	}
	char *err = strerror( errno );
	wchar_t *werr = str2wcs( err );
	if( werr )
	{
		sb_append2( sb_err,  werr, L"\n", (void *)0 );
		free( werr );
	}
}

/**
   Count the number of times the specified character occurs in the specified string
*/
static int count_char( const wchar_t *str, wchar_t c )
{
	int res = 0;
	for( ; *str; str++ )
	{
		res += (*str==c);
	}
	return res;
}

/**
   Print help for the specified builtin. If \c b is sb_err, also print
   the line information

   If \c b is the buffer representing standard error, and the help
   message is about to be printed to an interactive screen, it may be
   shortened to fit the screen.

*/


static void builtin_print_help( wchar_t *cmd, string_buffer_t *b )
{
	const char *h;

	if( b == sb_err )
	{
		sb_append( sb_err,
				   parser_current_line() );
	}

	h = builtin_help_get( cmd );

	if( !h )
		return;

	wchar_t *str = str2wcs( h );
	if( str )
	{

		if( is_interactive && !builtin_out_redirect && b==sb_err)
		{
			/* Interactive mode help to screen - only print synopsis if the rest won't fit  */
			
			int screen_height, lines;
			
			screen_height = common_get_height();
			lines = count_char( str, L'\n' );
			if( lines > 2*screen_height/3 )
			{
				wchar_t *pos;
				
				/* Find first empty line */
				for( pos=str; *pos; pos++ )
				{
					if( *pos == L'\n' )
					{
						wchar_t *pos2;
						int is_empty = 1;
						
						for( pos2 = pos+1; *pos2; pos2++ )
						{
							if( *pos2 == L'\n' )
								break;
							
							if( *pos2 != L'\t' && *pos2 !=L' ' )
							{
								is_empty = 0;
								break;
							}
						}
						if( is_empty )
						{
							*(pos+1)=L'\0';
						}
					}
				}
			}
		}
		
		sb_append( b, str );
		free( str );
	}
}

/*
  Here follows the definition of all builtin commands. The function
  names are all on the form builtin_NAME where NAME is the name of the
  builtin. so the function name for the builtin 'fg' is
  'builtin_fg'.

  A few builtins, including 'while', 'command' and 'builtin' are not
  defined here as they are handled directly by the parser. (They are
  not parsed as commands, instead they only alter the parser state)

  The builtins 'break' and 'continue' are so closely related that they
  share the same implementation, namely 'builtin_break_continue.

  Several other builtins, including jobs, ulimit and set are so big
  that they have been given their own file. These files are all named
  'builtin_NAME.c', where NAME is the name of the builtin.

*/


#include "builtin_help.c"
#include "builtin_set.c"
#include "builtin_commandline.c"
#include "builtin_complete.c"
#include "builtin_ulimit.c"
#include "builtin_jobs.c"


/**
   The bind builtin, used for setting character sequences
*/
static int builtin_bind( wchar_t **argv )
{
	int i;
	int argc=builtin_count_args( argv );

	woptind=0;

	const static struct woption
		long_options[] =
		{
			{
				L"set-mode", required_argument, 0, 'M'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"M:h",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
				sb_printf( sb_err,
						   BUILTIN_ERR_UNKNOWN,
						   argv[0],
						   long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );

				return 1;

			case 'M':
				input_set_mode( woptarg );
				break;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;
				
			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}

	for( i=woptind; i<argc; i++ )
	{
		input_parse_inputrc_line( argv[i] );
	}

	return 0;
}

/**
   The block builtin, used for temporarily blocking events
*/
static int builtin_block( wchar_t **argv )
{
	enum
	{
		UNSET,
		GLOBAL,
		LOCAL,
	}
	;

	int scope=UNSET;
	int erase = 0;
	int argc=builtin_count_args( argv );
	int type = (1<<EVENT_ANY);

	woptind=0;

	const static struct woption
		long_options[] =
		{
			{
				L"erase", no_argument, 0, 'e'
			}
			,
			{
				L"local", no_argument, 0, 'l'
			}
			,
			{
				L"global", no_argument, 0, 'g'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"elgh",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
				sb_printf( sb_err,
						   BUILTIN_ERR_UNKNOWN,
						   argv[0],
						   long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );

				return 1;
			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case 'g':
				scope = GLOBAL;
				break;

			case 'l':
				scope = LOCAL;
				break;

			case 'e':
				erase = 1;
				break;

			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}

	if( erase )
	{
		if( scope != UNSET )
		{
			sb_printf( sb_err, _( L"%ls: Can not specify scope when removing block\n" ), argv[0] );
			return 1;
		}

		if( !global_event_block )
		{
			sb_printf( sb_err, _( L"%ls: No blocks defined\n" ), argv[0] );
			return 1;
		}

		event_block_t *eb = global_event_block;
		global_event_block = eb->next;
		free( eb );
	}
	else
	{
		block_t *block=current_block;

		event_block_t *eb = malloc( sizeof( event_block_t ) );

		if( !eb )
			DIE_MEM();

		eb->type = type;

		switch( scope )
		{
			case LOCAL:
			{
				if( !block->outer )
					block=0;
				break;
			}
			case GLOBAL:
			{
				block=0;
			}
			case UNSET:
			{
				while( block && block->type != FUNCTION_CALL )
					block = block->outer;
			}
		}
		if( block )
		{
			eb->next = block->first_event_block;
			block->first_event_block = eb;
			halloc_register( block, eb );
		}
		else
		{
			eb->next = global_event_block;
			global_event_block=eb;
		}
	}

	return 0;

}

/**
   The builtin builtin, used for given builtins precedence over functions. Mostly handled by the parser. All this code does is some additional operational modes, such as printing a list of all builtins.
*/
static int builtin_builtin(  wchar_t **argv )
{
	int argc=builtin_count_args( argv );
	int list=0;

	woptind=0;

	const static struct woption
		long_options[] =
		{
			{
				L"names", no_argument, 0, 'n'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"nh",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );


				return 1;
			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case 'n':
				list=1;
				break;

			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}

	if( list )
	{
		array_list_t names;
		int i;

		al_init( &names );
		builtin_get_names( &names );
		sort_list( &names );
		
		for( i=0; i<al_get_count( &names ); i++ )
		{
			wchar_t *el = (wchar_t *)al_get( &names, i );
			
			if( wcscmp( el, L"count" ) == 0 )
				continue;

			sb_append2( sb_out,
						el,
						L"\n",
						(void *)0 );
		}
		al_destroy( &names );
	}
	return 0;
}

/**
   A generic bultin that only supports showing a help message. This is
   only a placeholder that prints the help message. Useful for
   commands that live in hte parser.
*/
static int builtin_generic( wchar_t **argv )
{
	int argc=builtin_count_args( argv );
	woptind=0;

	const static struct woption
		long_options[] =
		{
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"h",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );
				return 1;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}
	return 1;
}

/**
   Output a definition of the specified function to the sb_out
   stringbuffer. Used by the functions builtin.
*/
static void functions_def( wchar_t *name )
{
	const wchar_t *desc = function_get_desc( name );
	const wchar_t *def = function_get_definition(name);

	array_list_t ev;
	event_t search;

	int i;

	search.function_name = name;
	search.type = EVENT_ANY;

	al_init( &ev );
	event_get( &search, &ev );

	sb_append2( sb_out,
				L"function ",
				name,
				(void *)0);

	if( desc && wcslen(desc) )
	{
		wchar_t *esc_desc = escape( desc, 1 );

		sb_append2( sb_out, L" --description ", esc_desc, (void *)0 );
		free( esc_desc );
	}

	for( i=0; i<al_get_count( &ev); i++ )
	{
		event_t *next = (event_t *)al_get( &ev, i );
		switch( next->type )
		{
			case EVENT_SIGNAL:
			{
				sb_printf( sb_out, L" --on-signal %ls", sig2wcs( next->param1.signal ) );
				break;
			}

			case EVENT_VARIABLE:
			{
				sb_printf( sb_out, L" --on-variable %ls", next->param1.variable );
				break;
			}

			case EVENT_EXIT:
			{
				if( next->param1.pid > 0 )
					sb_printf( sb_out, L" --on-process-exit %d", next->param1.pid );
				else
					sb_printf( sb_out, L" --on-job-exit %d", -next->param1.pid );
				break;
			}

			case EVENT_JOB_ID:
			{
				job_t *j = job_get( next->param1.job_id );
				if( j )
					sb_printf( sb_out, L" --on-job-exit %d", j->pgid );
				break;
			}

		}

	}

	al_destroy( &ev );

	sb_append2( sb_out,
				L"\n\t",
				def,
				L"\nend\n\n",
				(void *)0);

}


/**
   The functions builtin, used for listing and erasing functions.
*/
static int builtin_functions( wchar_t **argv )
{
	int i;
	int erase=0;
	wchar_t *desc=0;

	array_list_t names;

	int argc=builtin_count_args( argv );
	int list=0;
	int show_hidden=0;
	int res = 0;
	int query = 0;

	woptind=0;

	const static struct woption
		long_options[] =
		{
			{
				L"erase", no_argument, 0, 'e'
			}
			,
			{
				L"description", required_argument, 0, 'd'
			}
			,
			{
				L"names", no_argument, 0, 'n'
			}
			,
			{
				L"all", no_argument, 0, 'a'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				L"query", no_argument, 0, 'q'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"ed:nahq",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );


				return 1;

			case 'e':
				erase=1;
				break;

			case 'd':
				desc=woptarg;
				break;

			case 'n':
				list=1;
				break;

			case 'a':
				show_hidden=1;
				break;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case 'q':
				query = 1;
				break;

			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}

	/*
	  Erase, desc, query and list are mutually exclusive
	*/
	if( (erase + (desc!=0) + list + query) > 1 )
	{
		sb_printf( sb_err,
				   _( L"%ls: Invalid combination of options\n" ),
				   argv[0] );


		builtin_print_help( argv[0], sb_err );

		return 1;
	}

	if( erase )
	{
		int i;
		for( i=woptind; i<argc; i++ )
			function_remove( argv[i] );
		return 0;
	}
	else if( desc )
	{
		wchar_t *func;

		if( argc-woptind != 1 )
		{
			sb_printf( sb_err,
					   _( L"%ls: Expected exactly one function name\n" ),
					   argv[0] );
			builtin_print_help( argv[0], sb_err );

			return 1;
		}
		func = argv[woptind];
		if( !function_exists( func ) )
		{
			sb_printf( sb_err,
					   _( L"%ls: Function '%ls' does not exist\n" ),
					   argv[0],
					   func );

			builtin_print_help( argv[0], sb_err );

			return 1;
		}

		function_set_desc( func, desc );

		return 0;
	}
	else if( list )
	{
		int is_screen = !builtin_out_redirect && isatty(1);

		al_init( &names );
		function_get_names( &names, show_hidden );
		sort_list( &names );
		if( is_screen )
		{
			string_buffer_t buff;
			sb_init( &buff );

			for( i=0; i<al_get_count( &names ); i++ )
			{
				sb_append2( &buff,
							al_get(&names, i),
							L", ",
							(void *)0 );
			}

			write_screen( (wchar_t *)buff.buff, sb_out );
			sb_destroy( &buff );
		}
		else
		{
			for( i=0; i<al_get_count( &names ); i++ )
			{
				sb_append2( sb_out,
							al_get(&names, i),
							L"\n",
							(void *)0 );
			}
		}

		al_destroy( &names );
		return 0;
	}

	switch( argc - woptind )
	{
		case 0:
		{
			if( !query )
			{
				sb_append( sb_out, _( L"Current function definitions are:\n\n" ) );
				al_init( &names );
				function_get_names( &names, show_hidden );
				sort_list( &names );
				
				for( i=0; i<al_get_count( &names ); i++ )
				{
					functions_def( (wchar_t *)al_get( &names, i ) );
				}
				
				al_destroy( &names );
			}
			
			break;
		}

		default:
		{
			for( i=woptind; i<argc; i++ )
			{
				if( !function_exists( argv[i] ) )
					res++;
				else
				{
					if( !query )
					{
						functions_def( argv[i] );
					}
				}				
			}

			break;
		}
	}
	return res;

}

/**
   Test whether the specified string is a valid name for a keybinding
*/
static int wcsbindingname( wchar_t *str )
{
	while( *str )
	{
		if( (!iswalnum(*str)) && (*str != L'-' ) )
		{
			return 0;
		}
		str++;
	}
	return 1;
}


/**
   The function builtin, used for providing subroutines.
   It calls various functions from function.c to perform any heavy lifting.
*/
static int builtin_function( wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int res=0;
	wchar_t *desc=0;
	int is_binding=0;
	array_list_t *events;
	int i;

	woptind=0;

	parser_push_block( FUNCTION_DEF );
	events=al_halloc( current_block );

	const static struct woption
		long_options[] =
		{
			{
				L"description", required_argument, 0, 'd'
			}
			,
			{
				L"key-binding", no_argument, 0, 'b'
			}
			,
			{
				L"on-signal", required_argument, 0, 's'
			}
			,
			{
				L"on-job-exit", required_argument, 0, 'j'
			}
			,
			{
				L"on-process-exit", required_argument, 0, 'p'
			}
			,
			{
				L"on-variable", required_argument, 0, 'v'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 && (!res ) )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"bd:s:j:p:v:h",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );

				res = 1;
				break;

			case 'd':
				desc=woptarg;
				break;

			case 'b':
				is_binding=1;
				break;

			case 's':
			{
				int sig = wcs2sig( woptarg );
				event_t *e;

				if( sig < 0 )
				{
					sb_printf( sb_err,
							   _( L"%ls: Unknown signal '%ls'\n" ),
							   argv[0],
							   woptarg );
					res=1;
					break;
				}

				e = halloc( current_block, sizeof(event_t));
				if( !e )
					DIE_MEM();
				e->type = EVENT_SIGNAL;
				e->param1.signal = sig;
				e->function_name=0;
				al_push( events, e );
				break;
			}

			case 'v':
			{
				event_t *e;

				if( wcsvarname( woptarg ) )
				{
					sb_printf( sb_err,
							   _( L"%ls: Invalid variable name '%ls'\n" ),
							   argv[0],
							   woptarg );
					res=1;
					break;
				}

				e = halloc( current_block, sizeof(event_t));
				if( !e )
					DIE_MEM();
				e->type = EVENT_VARIABLE;
				e->param1.variable = halloc_wcsdup( current_block, woptarg );
				e->function_name=0;
				al_push( events, e );
				break;
			}

			case 'j':
			case 'p':
			{
				pid_t pid;
				wchar_t *end;
				event_t *e;

				e = halloc( current_block, sizeof(event_t));
				if( !e )
					DIE_MEM();
				
				if( ( opt == 'j' ) &&
					( wcscasecmp( woptarg, L"caller" ) == 0 ) )
				{
					int job_id = -1;

					if( is_subshell )
					{
						block_t *b = current_block;

						while( b && (b->type != SUBST) )
							b = b->outer;

						if( b )
						{
							b=b->outer;
						}
						if( b->job )
						{
							job_id = b->job->job_id;
						}
					}

					if( job_id == -1 )
					{
						sb_printf( sb_err,
								   _( L"%ls: Cannot find calling job for event handler\n" ),
								   argv[0] );
						res=1;
					}
					else
					{
						e->type = EVENT_JOB_ID;
						e->param1.job_id = job_id;
					}

				}
				else
				{
					errno = 0;
					pid = wcstol( woptarg, &end, 10 );
					if( errno || !end || *end )
					{
						sb_printf( sb_err,
								   _( L"%ls: Invalid process id %ls\n" ),
								   argv[0],
								   woptarg );
						res=1;
						break;
					}


					e->type = EVENT_EXIT;
					e->param1.pid = (opt=='j'?-1:1)*abs(pid);
				}
				if( res )
				{
					free( e );
				}
				else
				{
					e->function_name=0;
					al_push( events, e );
				}
				break;
			}

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;
				
			case '?':
				builtin_print_help( argv[0], sb_err );
				res = 1;
				break;

		}

	}

	if( !res )
	{
		if( argc-woptind != 1 )
		{
			sb_printf( sb_err,
					   _( L"%ls: Expected one argument, got %d\n" ),
					   argv[0],
					   argc-woptind );
			res=1;
		}
		else if( !(is_binding?wcsbindingname( argv[woptind] ) : !wcsvarname( argv[woptind] ) ))
		{
			sb_printf( sb_err,
					   _( L"%ls: Illegal function name '%ls'\n" ),
					   argv[0],
					   argv[woptind] );

			res=1;
		}
		else if( parser_is_reserved(argv[woptind] ) )
		{

			sb_printf( sb_err,
					   _( L"%ls: The name '%ls' is reserved,\nand can not be used as a function name\n" ),
					   argv[0],
					   argv[woptind] );

			res=1;
		}
	}

	if( res )
	{
		int i;
		array_list_t names;
		int chars=0;

		builtin_print_help( argv[0], sb_err );
		const wchar_t *cfa =  _( L"Current functions are: " );
		sb_append( sb_err, cfa );
		chars += wcslen( cfa );

		al_init( &names );
		function_get_names( &names, 0 );
		sort_list( &names );

		for( i=0; i<al_get_count( &names ); i++ )
		{
			wchar_t *nxt = (wchar_t *)al_get( &names, i );
			int l = wcslen( nxt + 2 );
			if( chars+l > common_get_width() )
			{
				chars = 0;
				sb_append(sb_err, L"\n" );
			}

			sb_append2( sb_err,
						nxt, L"  ", (void *)0 );
		}
		al_destroy( &names );
		sb_append( sb_err, L"\n" );

		parser_pop_block();
		parser_push_block( FAKE );
	}
	else
	{
		current_block->param1.function_name=halloc_wcsdup( current_block, argv[woptind]);
		current_block->param2.function_description=desc?halloc_wcsdup( current_block, desc):0;
		current_block->param3.function_is_binding = is_binding;
		current_block->param4.function_events = events;
		
		for( i=0; i<al_get_count( events ); i++ )
		{
			event_t *e = (event_t *)al_get( events, i );
			e->function_name = current_block->param1.function_name;
		}
	}
	
	current_block->tok_pos = parser_get_pos();
	current_block->skip = 1;

	return 0;

}

/**
   The random builtin. For generating random numbers.
*/
static int builtin_random( wchar_t **argv )
{
	static int seeded=0;
	static struct drand48_data seed_buffer;
	
	int argc = builtin_count_args( argv );

	woptind=0;

	const static struct woption
		long_options[] =
		{
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"h",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );

				return 1;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				break;

			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}

	switch( argc-woptind )
	{

		case 0:
		{
			long res;
			
			if( !seeded )
			{
				seeded=1;
				srand48_r(time(0), &seed_buffer);
			}
			lrand48_r( &seed_buffer, &res );
			
			sb_printf( sb_out, L"%d\n", res%32767 );
			break;
		}

		case 1:
		{
			long foo;
			wchar_t *end=0;

			errno=0;
			foo = wcstol( argv[woptind], &end, 10 );
			if( errno || *end )
			{
				sb_printf( sb_err,
						   _( L"%ls: Seed value '%ls' is not a valid number\n" ),
						   argv[0],
						   argv[woptind] );

				return 1;
			}
			seeded=1;
			srand48_r( foo, &seed_buffer);
			break;
		}

		default:
		{
			sb_printf( sb_err,
					   _( L"%ls: Expected zero or one argument, got %d\n" ),
					   argv[0],
					   argc-woptind );
			builtin_print_help( argv[0], sb_err );
			return 1;
		}
	}
	return 0;
}


/**
   The read builtin. Reads from stdin and stores the values in environment variables.
*/
static int builtin_read( wchar_t **argv )
{
	wchar_t *buff=0;
	int i, argc = builtin_count_args( argv );
	wchar_t *ifs;
	int place = ENV_USER;
	wchar_t *nxt;
	wchar_t *prompt = DEFAULT_READ_PROMPT;
	wchar_t *commandline = L"";
	int exit_res=0;
	
	woptind=0;
	
	while( 1 )
	{
		const static struct woption
			long_options[] =
			{
				{
					L"export", no_argument, 0, 'x'
				}
				,
				{
					L"global", no_argument, 0, 'g'
				}
				,
				{
					L"local", no_argument, 0, 'l'
				}
				,
				{
					L"universal", no_argument, 0, 'U'
				}
				,
				{
					L"unexport", no_argument, 0, 'u'
				}
				,
				{
					L"prompt", required_argument, 0, 'p'
				}
				,
				{
					L"command", required_argument, 0, 'c'
				}
				,
				{
					L"help", no_argument, 0, 'h'
				}
				,
				{
					0, 0, 0, 0
				}
			}
		;

		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"xglUup:c:h",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );

				return 1;

			case L'x':
				place |= ENV_EXPORT;
				break;
			case L'g':
				place |= ENV_GLOBAL;
				break;
			case L'l':
				place |= ENV_LOCAL;
				break;
			case L'U':
				place |= ENV_UNIVERSAL;
				break;
			case L'u':
				place |= ENV_UNEXPORT;
				break;
			case L'p':
				prompt = woptarg;
				break;
			case L'c':
				commandline = woptarg;
				break;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case L'?':
				builtin_print_help( argv[0], sb_err );

				return 1;
		}

	}

	if( ( place & ENV_UNEXPORT ) && ( place & ENV_EXPORT ) )
	{
		sb_printf( sb_err,
				   BUILTIN_ERR_EXPUNEXP,
				   argv[0],
				   parser_current_line() );


		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( (place&ENV_LOCAL?1:0) + (place & ENV_GLOBAL?1:0) + (place & ENV_UNIVERSAL?1:0) > 1)
	{
		sb_printf( sb_err,
				   BUILTIN_ERR_GLOCAL,
				   argv[0],
				   parser_current_line() );
		builtin_print_help( argv[0], sb_err );

		return 1;
	}

	/*
	  Verify all variable names
	*/
	for( i=woptind; i<argc; i++ )
	{
		wchar_t *src;

		if( !wcslen( argv[i] ) )
		{
			sb_printf( sb_err, BUILTIN_ERR_VARNAME_ZERO, argv[0] );
			return 1;
		}

		for( src=argv[i]; *src; src++ )
		{
			if( (!iswalnum(*src)) && (*src != L'_' ) )
			{
				sb_printf( sb_err, BUILTIN_ERR_VARCHAR, argv[0], *src );
				sb_append2(sb_err, parser_current_line(), L"\n", (void *)0 );
				return 1;
			}
		}

	}

	/*
	  The call to reader_readline may change woptind, so we save it away here
	*/
	i=woptind;

	/*
	  Check if we should read interactively using \c reader_readline()
	*/
	if( isatty(0) && builtin_stdin == 0 )
	{
		reader_push( READ_MODE_NAME );
		reader_set_prompt( prompt );

		reader_set_buffer( commandline, wcslen( commandline ) );
		buff = wcsdup(reader_readline( ));
		reader_pop();
	}
	else
	{
		string_buffer_t sb;
		int eof=0;
		
		sb_init( &sb );
		
		while( 1 )
		{
			int finished=0;

			wchar_t res=0;
			static mbstate_t state;
			memset (&state, '\0', sizeof (state));

			while( !finished )
			{
				char b;
				int read_res = read_blocked( builtin_stdin, &b, 1 );
				if( read_res <= 0 )
				{
					eof=1;
					break;
				}

				int sz = mbrtowc( &res, &b, 1, &state );

				switch( sz )
				{
					case -1:
						memset (&state, '\0', sizeof (state));
						break;

					case -2:
						break;
					case 0:
						eof=1;
						finished = 1;
						break;

					default:
						finished=1;
						break;

				}
			}

			if( eof )
				break;

			if( res == L'\n' )
				break;

			sb_append_char( &sb, res );
		}

		if( sb.used < 2 && eof )
		{
			exit_res = 1;
		}
		
		buff = wcsdup( (wchar_t *)sb.buff );
		sb_destroy( &sb );
	}

	if( i != argc )
	{
		
		wchar_t *state;

		ifs = env_get( L"IFS" );
		if( ifs == 0 )
			ifs = L"";
		
		nxt = wcstok( buff, (i<argc-1)?ifs:L"", &state );
		
		while( i<argc )
		{
			env_set( argv[i], nxt != 0 ? nxt: L"", place );
			
			i++;
			if( nxt != 0 )
				nxt = wcstok( 0, (i<argc-1)?ifs:L"", &state);
		}
	}
	
	free( buff );

	return exit_res;
}

/**
   The status builtin. Gives various status information on fish.
*/
static int builtin_status( wchar_t **argv )
{
	enum
	{
		NORMAL,
		IS_SUBST,
		IS_BLOCK,
		IS_INTERACTIVE,
		IS_LOGIN,
		IS_FULL_JOB_CONTROL,
		IS_INTERACTIVE_JOB_CONTROL,
		IS_NO_JOB_CONTROL,
		STACK_TRACE,
		DONE,
		CURRENT_FILENAME,
		CURRENT_LINE_NUMBER
	}
	;

	int mode = NORMAL;

	int argc = builtin_count_args( argv );
	int res=0;

	woptind=0;

	const struct woption
		long_options[] =
		{
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				L"is-command-substitution", no_argument, &mode, IS_SUBST
			}
			,
			{
				L"is-block", no_argument, &mode, IS_BLOCK
			}
			,
			{
				L"is-interactive", no_argument, &mode, IS_INTERACTIVE
			}
			,
			{
				L"is-login", no_argument, &mode, IS_LOGIN
			}
			,
			{
				L"is-full-job-control", no_argument, &mode, IS_FULL_JOB_CONTROL
			}
			,
			{
				L"is-interactive-job-control", no_argument, &mode, IS_INTERACTIVE_JOB_CONTROL
			}
			,
			{
				L"is-no-job-control", no_argument, &mode, IS_NO_JOB_CONTROL
			}
			,
			{
				L"current-filename", no_argument, &mode, CURRENT_FILENAME
			}
			,
			{
				L"current-line-number", no_argument, &mode, CURRENT_LINE_NUMBER
			}
			,
			{
				L"job-control", required_argument, 0, 'j'
			}
			,
			{
				L"print-stack-trace", no_argument, 0, 't'
			}
			,
			{
				0, 0, 0, 0
			}
		}
	;

	while( 1 )
	{
		int opt_index = 0;

		int opt = wgetopt_long( argc,
								argv,
								L"hjt",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                sb_printf( sb_err,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( argv[0], sb_err );

				return 1;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case 'j':
				if( wcscmp( woptarg, L"full" ) == 0 )
					job_control_mode = JOB_CONTROL_ALL;
				else if( wcscmp( woptarg, L"interactive" ) == 0 )
					job_control_mode = JOB_CONTROL_INTERACTIVE;
				else if( wcscmp( woptarg, L"none" ) == 0 )
					job_control_mode = JOB_CONTROL_NONE;
				else
				{
					sb_printf( sb_err,
							   L"%ls: Invalid job control mode '%ls'\n",
							   woptarg );
					res = 1;
				}
				mode = DONE;				
				break;

			case '?':
				builtin_print_help( argv[0], sb_err );

				return 1;

		}

	}

	if( !res )
	{

		switch( mode )
		{
			case CURRENT_FILENAME:
			{
				const wchar_t *fn = parser_current_filename();
				
				if( !fn )
					fn = _(L"Standard input");
				
				sb_printf( sb_out, L"%ls\n", fn );
				
				break;
			}
			
			case CURRENT_LINE_NUMBER:
			{
				sb_printf( sb_out, L"%d\n", parser_get_lineno() );
				break;				
			}
			
			case IS_INTERACTIVE:
				return !is_interactive_session;

			case IS_SUBST:
				return !is_subshell;

			case IS_BLOCK:
				return !is_block;

			case IS_LOGIN:
				return !is_login;
			
			case IS_FULL_JOB_CONTROL:
				return job_control_mode != JOB_CONTROL_ALL;

			case IS_INTERACTIVE_JOB_CONTROL:
				return job_control_mode != JOB_CONTROL_INTERACTIVE;

			case IS_NO_JOB_CONTROL:
				return job_control_mode != JOB_CONTROL_NONE;

			case STACK_TRACE:
			{
				parser_stack_trace( current_block, sb_out );
				break;
			}

			case NORMAL:
			{
				if( is_login )
					sb_printf( sb_out, _( L"This is a login shell\n" ) );
				else
					sb_printf( sb_out, _( L"This is not a login shell\n" ) );

				sb_printf( sb_out, _( L"Job control: %ls\n" ),
						   job_control_mode==JOB_CONTROL_INTERACTIVE?_( L"Only on interactive jobs" ):
						   (job_control_mode==JOB_CONTROL_NONE ? _( L"Never" ) : _( L"Always" ) ) );

				parser_stack_trace( current_block, sb_out );
				break;
			}
		}
	}

	return res;
}


/**
   The eval builtin. Concatenates the arguments and calls eval on the
   result.
*/
static int builtin_eval( wchar_t **argv )
{
	string_buffer_t sb;
	int i;
	int argc = builtin_count_args( argv );
	sb_init( &sb );

	for( i=1; i<argc; i++ )
	{
		sb_append( &sb, argv[i] );
		sb_append( &sb, L" " );
	}

	eval( (wchar_t *)sb.buff, block_io, TOP );
	sb_destroy( &sb );

	return proc_get_last_status();
}

/**
   The exit builtin. Calls reader_exit to exit and returns the value specified.
*/
static int builtin_exit( wchar_t **argv )
{
	int argc = builtin_count_args( argv );

	int ec=0;
	switch( argc )
	{
		case 1:
		{
			ec = proc_get_last_status();
			break;
		}
		
		case 2:
		{
			wchar_t *end;
			errno = 0;
			ec = wcstol(argv[1],&end,10);
			if( errno || *end != 0)
			{
				sb_printf( sb_err,
						   _( L"%ls: Argument '%ls' must be an integer\n" ),
						   argv[0],
						   argv[1] );
				builtin_print_help( argv[0], sb_err );
				return 1;
			}
			break;
		}

		default:
		{
			sb_printf( sb_err,
					   BUILTIN_ERR_TOO_MANY_ARGUMENTS,
					   argv[0] );

			builtin_print_help( argv[0], sb_err );
			return 1;
		}
		
	}
	reader_exit( 1, 0 );
	return ec;
}

/**
   Helper function for builtin_cd, used for seting the current working directory
*/
static int set_pwd( wchar_t *env)
{
	wchar_t dir_path[4096];
	wchar_t *res = wgetcwd( dir_path, 4096 );
	if( !res )
	{
		builtin_wperror( L"wgetcwd" );
		return 0;
	}
	env_set( env, dir_path, ENV_EXPORT | ENV_GLOBAL );
	return 1;
}

/**
   The cd builtin. Changes the current directory to the one specified
   or to $HOME if none is specified. If '-' is the directory specified,
   the directory is changed to the previous working directory. The
   directory can be relative to any directory in the CDPATH variable.
*/
static int builtin_cd( wchar_t **argv )
{
	wchar_t *dir_in;
	wchar_t *dir;
	int res=0;
	void *context = halloc( 0, 0 );

	
	if( argv[1]  == 0 )
	{
		dir_in = env_get( L"HOME" );
		if( !dir_in )
		{
			sb_printf( sb_err,
					   _( L"%ls: Could not find home directory\n" ),
					   argv[0] );
		}
	}
	else
		dir_in = argv[1];

	dir = parser_cdpath_get( context, dir_in );

	if( !dir )
	{
		sb_printf( sb_err,
				   _( L"%ls: '%ls' is not a directory or you do not have permission to enter it\n" ),
				   argv[0],
				   dir_in );
		if( !is_interactive )
		{
			sb_append2( sb_err,
						parser_current_line(),
						(void *)0 );
		}
		
		res = 1;
	}
	else if( wchdir( dir ) != 0 )
	{
		sb_printf( sb_err,
				   _( L"%ls: '%ls' is not a directory\n" ),
				   argv[0],
				   dir );
		if( !is_interactive )
		{
			sb_append2( sb_err,
						parser_current_line(),
						(void *)0 );
		}
		
		res = 1;
	}
	else if( !set_pwd(L"PWD") )
	{
		res=1;
		sb_printf( sb_err, _( L"%ls: Could not set PWD variable\n" ), argv[0] );
	}

	halloc_free( context );

	return res;
}



/**
   The  . (dot) builtin, sometimes called source. Evaluates the contents of a file.
*/
static int builtin_source( wchar_t ** argv )
{
	int fd;
	int res;
	struct stat buf;
	int argc;

	argc = builtin_count_args( argv );

	if( argc < 2 )
	{
		sb_printf( sb_err, _( L"%ls: Expected at least one argument, got %d\n" ), argv[0], argc );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( wstat(argv[1], &buf) == -1 )
	{
		builtin_wperror( L"stat" );
		return 1;
	}

	if( !S_ISREG(buf.st_mode) )
	{
		sb_printf( sb_err, _( L"%ls: '%ls' is not a file\n" ), argv[0], argv[1] );
		builtin_print_help( argv[0], sb_err );

		return 1;
	}
	if( ( fd = wopen( argv[1], O_RDONLY ) ) == -1 )
	{
		builtin_wperror( L"open" );
		res = 1;
	}
	else
	{
		wchar_t *fn = wrealpath( argv[1], 0 );
		const wchar_t *fn_intern;
		
		if( !fn )
		{
			fn_intern = intern( argv[1] );
		}
		else
		{
			fn_intern = intern(fn);
			free( fn );
		}
		
		parser_push_block( SOURCE );		
		reader_push_current_filename( fn_intern );

		current_block->param1.source_dest = fn_intern;

		parse_util_set_argv( argv+2);

		res = reader_read( fd );
		
		parser_pop_block();
		if( res )
		{
			sb_printf( sb_err,
					   _( L"%ls: Error while reading file '%ls'\n" ),
					   argv[0],
					   argv[1] );
		}

		/*
		  Do not close fd after calling reader_read. reader_read
		  automatically closes it before calling eval.
		*/

		reader_pop_current_filename();
	}

	return res;
}

/**
   Make the specified job the first job of the job list. Moving jobs
   around in the list makes the list reflect the order in which the
   jobs were used.
*/
static void make_first( job_t *j )
{
	job_t *prev=0;
	job_t *curr;
	for( curr = first_job; curr != j; curr = curr->next )
	{
		prev=curr;
	}
	if( curr == j )
	{
		if( prev == 0 )
		{
			return;
		}
		else
		{
			prev->next = curr->next;
			curr->next = first_job;
			first_job = curr;
		}
	}
}


/**
   Builtin for putting a job in the foreground
*/
static int builtin_fg( wchar_t **argv )
{
	job_t *j=0;

	if( argv[1] == 0 )
	{
		/*
		  Select last constructed job (I.e. first job in the job que)
		  that is possible to put in the foreground
		*/
		for( j=first_job; j; j=j->next )
		{
			if( j->constructed && (!job_is_completed(j)) && 
				( (job_is_stopped(j) || !j->fg) && (j->job_control) ) )
			{
				break;
			}
		}
		if( !j )
		{
			sb_printf( sb_err,
					   _( L"%ls: There are no suitable jobs\n" ),
					   argv[0] );
			builtin_print_help( argv[0], sb_err );
		}
	}
	else if( argv[2] != 0 )
	{
		/*
		  Specifying what more than one job to put to the foreground
		  is a syntax error, we still try to locate the job argv[1],
		  since we want to know if this is an ambigous job
		  specification or if this is an malformed job id
		*/
		int pid = wcstol( argv[1], 0, 10 );
		j = job_get_from_pid( pid );
		if( j != 0 )
		{
			sb_printf( sb_err,
					   _( L"%ls: Ambiguous job\n" ),
					   argv[0] );
		}
		else
		{
			sb_printf( sb_err,
					   _( L"%ls: '%ls' is not a job\n" ),
					   argv[0],
					   argv[1] );
		}
		builtin_print_help( argv[0], sb_err );

		j=0;

	}
	else
	{
		wchar_t *end;		
		int pid = abs(wcstol( argv[1], &end, 10 ));
		
		if( *end )
		{
				sb_printf( sb_err,
						   _( L"%ls: Argument '%ls' is not a number\n" ),
						   argv[0],
						   argv[1] );
				builtin_print_help( argv[0], sb_err );
		}
		else
		{
			j = job_get_from_pid( pid );
			if( !j || !j->constructed || job_is_completed( j ))
			{
				sb_printf( sb_err,
						   _( L"%ls: No suitable job: %d\n" ),
						   argv[0],
						   pid );
				builtin_print_help( argv[0], sb_err );
				j=0;
			}
			else if( !j->job_control )
			{
				sb_printf( sb_err,
						   _( L"%ls: Can't put job %d, '%ls' to foreground because it is not under job control\n" ),
						   argv[0],
						   pid,
						   j->command );
				builtin_print_help( argv[0], sb_err );
				j=0;
			}
		}
	}

	if( j )
	{
		if( builtin_err_redirect )
		{
			sb_printf( sb_err,
					   FG_MSG,
					   j->job_id,
					   j->command );
		}
		else
		{
			/*
			  If we aren't redirecting, send output to real stderr,
			  since stuff in sb_err won't get printed until the
			  command finishes.
			*/
			fwprintf( stderr,
					  FG_MSG,
					  j->job_id,
					  j->command );
		}

		wchar_t *ft = tok_first( j->command );
		if( ft != 0 )
			env_set( L"_", ft, ENV_EXPORT );
		free(ft);
		reader_write_title();

		make_first( j );
		j->fg=1;

		job_continue( j, job_is_stopped(j) );
	}
	return j != 0;
}

/**
   Helper function for builtin_bg()
*/
static int send_to_bg( job_t *j, const wchar_t *name )
{
	if( j == 0 )
	{
		sb_printf( sb_err,
				   _( L"%ls: Unknown job '%ls'\n" ),
				   L"bg",
				   name );
		builtin_print_help( L"bg", sb_err );
		return 1;
	}
	else if( !j->job_control )
	{
		sb_printf( sb_err,
				   _( L"%ls: Can't put job %d, '%ls' to background because it is not under job control\n" ),
				   L"bg",
				   j->job_id,
				   j->command );
		builtin_print_help( L"bg", sb_err );
		return 1;
	}
	else
	{
		sb_printf( sb_err,
				   _(L"Send job %d '%ls' to background\n"),
				   j->job_id,
				   j->command );
	}
	make_first( j );
	j->fg=0;
	job_continue( j, job_is_stopped(j) );
	return 0;
}


/**
   Builtin for putting a job in the background
*/
static int builtin_bg( wchar_t **argv )
{
	int res = 0;

	if( argv[1] == 0 )
	{
  		job_t *j;
		for( j=first_job; j; j=j->next )
		{
			if( job_is_stopped(j) && j->job_control && (!job_is_completed(j)) )
			{
				break;
			}
		}
		
		if( !j )
		{
			sb_printf( sb_err,
					   _( L"%ls: There are no suitable jobs\n" ),
					   argv[0] );
			res = 1;
		}
		else
		{
			res = send_to_bg( j, _(L"(default)" ) );
		}
	}
	else
	{
		for( argv++; !res && *argv != 0; argv++ )
		{
			int pid = wcstol( *argv, 0, 10 );
			res |= send_to_bg( job_get_from_pid( pid ), *argv);
		}
	}
	return res;
}


/**
   Builtin for looping over a list
*/
static int builtin_for( wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int res=1;


	if( argc < 3)
	{
		sb_printf( sb_err,
				   _( L"%ls: Expected at least two arguments, got %d\n"),
				   argc ,
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
	}
	else if ( wcsvarname(argv[1]) )
	{
		sb_printf( sb_err,
				   _( L"%ls: '%ls' is not a valid variable name\n" ),
				   argv[0],
				   argv[1] );
		builtin_print_help( argv[0], sb_err );
	}
	else if (wcscmp( argv[2], L"in") != 0 )
	{
		sb_printf( sb_err,
				   BUILTIN_FOR_ERR_IN,
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
	}
	else
	{
		res=0;
	}


	if( res )
	{
		parser_push_block( FAKE );
	}
	else
	{
		parser_push_block( FOR );
		al_init( &current_block->param2.for_vars);

		int i;
		current_block->tok_pos = parser_get_pos();
		current_block->param1.for_variable = halloc_wcsdup( current_block, argv[1] );

		for( i=argc-1; i>3; i-- )
		{
			al_push( &current_block->param2.for_vars, halloc_wcsdup( current_block, argv[ i ] ) );
		}
		halloc_register( current_block, current_block->param2.for_vars.arr );

		if( argc > 3 )
		{
			env_set( current_block->param1.for_variable, argv[3], ENV_LOCAL );
		}
		else
		{
			current_block->skip=1;
		}
	}
	return res;
}

/**
   The begin builtin. Creates a nex block.
*/
static int builtin_begin( wchar_t **argv )
{
	parser_push_block( BEGIN );
	current_block->tok_pos = parser_get_pos();
	return proc_get_last_status();
}


/**
   Builtin for ending a block of code, such as a for-loop or an if statement.

   The end command is whare a lot of the block-level magic happens.
*/
static int builtin_end( wchar_t **argv )
{
	if( !current_block->outer )
	{
		sb_printf( sb_err,
				   _( L"%ls: Not inside of block\n" ),
				   argv[0] );

		builtin_print_help( argv[0], sb_err );
		return 1;
	}
	else
	{
		/**
		   By default, 'end' kills the current block scope. But if we
		   are rewinding a loop, this should be set to false, so that
		   variables in the current loop scope won't die between laps.
		*/
		int kill_block = 1;

		switch( current_block->type )
		{
			case WHILE:
			{
				/*
				  If this is a while loop, we rewind the loop unless
				  it's the last lap, in which case we continue.
				*/
				if( !( current_block->skip && (current_block->loop_status != LOOP_CONTINUE )))
				{
					current_block->loop_status = LOOP_NORMAL;
					current_block->skip = 0;
					kill_block = 0;
					parser_set_pos( current_block->tok_pos);
					current_block->param1.while_state = WHILE_TEST_AGAIN;
				}

				break;
			}

			case IF:
			case SUBST:
			case BEGIN:
				/*
				  Nothing special happens at the end of these commands. The scope just ends.
				*/

				break;

			case FOR:
			{
				/*
				  set loop variable to next element, and rewind to the beginning of the block.
				*/
				if( current_block->loop_status == LOOP_BREAK )
				{
					al_truncate( &current_block->param2.for_vars, 0 );
				}

				if( al_get_count( &current_block->param2.for_vars ) )
				{
					wchar_t *val = (wchar_t *)al_pop( &current_block->param2.for_vars );
					env_set( current_block->param1.for_variable, val,  ENV_LOCAL);
					current_block->loop_status = LOOP_NORMAL;
					current_block->skip = 0;
					
					kill_block = 0;
					parser_set_pos( current_block->tok_pos );
				}
				break;
			}

			case FUNCTION_DEF:
			{
				/**
				   Copy the text from the beginning of the function
				   until the end command and use as the new definition
				   for the specified function
				*/
				wchar_t *def = wcsndup( parser_get_buffer()+current_block->tok_pos,
										parser_get_job_pos()-current_block->tok_pos );
				
				function_add( current_block->param1.function_name,
							  def,
							  current_block->param2.function_description,
							  current_block->param4.function_events,
							  current_block->param3.function_is_binding );

				free(def);
			}
			break;

		}
		if( kill_block )
		{
			parser_pop_block();
		}


		/*
		  If everything goes ok, return status of last command to execute.
		*/
		return proc_get_last_status();
	}
}

/**
   Builtin for executing commands if an if statement is false
*/
static int builtin_else( wchar_t **argv )
{
	if( current_block == 0 ||
		current_block->type != IF ||
		current_block->param1.if_state != 1)
	{
		sb_printf( sb_err,
				   _( L"%ls: Not inside of 'if' block\n" ),
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}
	else
	{
		current_block->param1.if_state++;
		current_block->skip = !current_block->skip;
		env_pop();
		env_push(0);
	}

	/*
	  If everything goes ok, return status of last command to execute.
	*/
	return proc_get_last_status();
}

/**
   This function handles both the 'continue' and the 'break' builtins
   that are used for loop control.
*/
static int builtin_break_continue( wchar_t **argv )
{
	int is_break = (wcscmp(argv[0],L"break")==0);
	int argc = builtin_count_args( argv );

	block_t *b = current_block;

	if( argc != 1 )
	{
		sb_printf( sb_err,
				   BUILTIN_ERR_UNKNOWN,
				   argv[0],
				   argv[1] );

		builtin_print_help( argv[0], sb_err );
		return 1;
	}


	while( (b != 0) &&
		   ( b->type != WHILE) &&
		   (b->type != FOR ) )
	{
		b = b->outer;
	}

	if( b == 0 )
	{
		sb_printf( sb_err,
				   _( L"%ls: Not inside of loop\n" ),
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	b = current_block;
	while( ( b->type != WHILE) &&
		   (b->type != FOR ) )
	{
		b->skip=1;
		b = b->outer;
	}
	b->skip=1;
	b->loop_status = is_break?LOOP_BREAK:LOOP_CONTINUE;
	return 0;
}

/**
   Function for handling the \c return builtin
*/
static int builtin_return( wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int status = proc_get_last_status();

	block_t *b = current_block;

	switch( argc )
	{
		case 1:
			break;
		case 2:
		{
			wchar_t *end;
			errno = 0;
			status = wcstol(argv[1],&end,10);
			if( errno || *end != 0)
			{
				sb_printf( sb_err,
						   _( L"%ls: Argument '%ls' must be an integer\n" ),
						   argv[0],
						   argv[1] );
				builtin_print_help( argv[0], sb_err );
				return 1;
			}
			break;
		}
		default:
			sb_printf( sb_err,
					   _( L"%ls: Too many arguments\n" ),
					   argv[0] );
			builtin_print_help( argv[0], sb_err );
			return 1;
	}


	while( (b != 0) &&
		   ( b->type != FUNCTION_CALL)  )
	{
		b = b->outer;
	}

	if( b == 0 )
	{
		sb_printf( sb_err,
				   _( L"%ls: Not inside of function\n" ),
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	b = current_block;
	while( ( b->type != FUNCTION_CALL))
	{
		b->skip=1;
		b = b->outer;
	}
	b->skip=1;
//	proc_set_last_status( status );

	return status;
}

/**
   Builtin for executing one of several blocks of commands depending on the value of an argument.
*/
static int builtin_switch( wchar_t **argv )
{
	int res=0;
	int argc = builtin_count_args( argv );

	if( argc != 2 )
	{
		sb_printf( sb_err,
				   _( L"%ls: Expected exactly one argument, got %d\n" ),
				   argv[0],
				   argc-1 );

		builtin_print_help( argv[0], sb_err );
		res=1;
		parser_push_block( FAKE );
	}
	else
	{
		parser_push_block( SWITCH );
		current_block->param1.switch_value = halloc_wcsdup( current_block, argv[1]);
		current_block->skip=1;
		current_block->param2.switch_taken=0;
	}

	return res;
}

/**
   Builtin used together with the switch builtin for conditional execution
*/
static int builtin_case( wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int i;
	wchar_t *unescaped=0;

	if( current_block->type != SWITCH )
	{
		sb_printf( sb_err,
				   _( L"%ls: 'case' command while not in switch block\n" ),
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	current_block->skip = 1;

	if( current_block->param2.switch_taken )
	{
		return 0;
	}

	for( i=1; i<argc; i++ )
	{
		free( unescaped );

		unescaped = parse_util_unescape_wildcards( argv[i] );
		
		
		if( wildcard_match( current_block->param1.switch_value, unescaped ) )
		{
			current_block->skip = 0;
			current_block->param2.switch_taken = 1;
			break;
		}
	}
	free( unescaped );

	return 0;
}


/*
  END OF BUILTIN COMMANDS
  Below are functions for handling the builtin commands
*/

/**
   Data about all the builtin commands in fish
*/
const static builtin_data_t builtin_data[]=
{
	{
		L"exit",  &builtin_exit, N_( L"Exit the shell" )  
	}
	,
	{
		L"block",  &builtin_block, N_( L"Temporarily block delivery of events" )  
	}
	,
	{
		L"builtin",  &builtin_builtin, N_( L"Run a builtin command instead of a function" )  
	}
	,
	{
		L"cd",  &builtin_cd, N_( L"Change working directory" )  
	}
	,
	{
		L"function",  &builtin_function, N_( L"Define a new function" )  
	}
	,
	{
		L"functions",  &builtin_functions, N_( L"List or remove functions" )  
	}
	,
	{
		L"complete",  &builtin_complete, N_( L"Edit command specific completions" )  
	}
	,
	{
		L"end",  &builtin_end, N_( L"End a block of commands" )  
	}
	,
	{
		L"else",  &builtin_else, N_( L"Evaluate block if condition is false" )  
	}
	,
	{
		L"eval",  &builtin_eval, N_( L"Evaluate parameters as a command" )  
	}
	,
	{
		L"for",  &builtin_for, N_( L"Perform a set of commands multiple times" )  
	}
	,
	{
		L".",  &builtin_source, N_( L"Evaluate contents of file" )  
	}
	,
	{
		L"set",  &builtin_set, N_( L"Handle environment variables" )  
	}
	,
	{
		L"fg",  &builtin_fg, N_( L"Send job to foreground" )  
	}
	,
	{
		L"bg",  &builtin_bg, N_( L"Send job to background" )  
	}
	,
	{
		L"jobs",  &builtin_jobs, N_( L"Print currently running jobs" )  
	}
	,
	{
		L"read",  &builtin_read, N_( L"Read a line of input into variables" )  
	}
	,
	{
		L"break",  &builtin_break_continue, N_( L"Stop the innermost loop" )  
	}
	,
	{
		L"continue",  &builtin_break_continue, N_( L"Skip the rest of the current lap of the innermost loop" )  
	}
	,
	{
		L"return",  &builtin_return, N_( L"Stop the currently evaluated function" )  
	}
	,
	{
		L"commandline",  &builtin_commandline, N_( L"Set or get the commandline" )  
	}
	,
	{
		L"switch",  &builtin_switch, N_( L"Conditionally execute a block of commands" )  
	}
	,
	{
		L"case",  &builtin_case, N_( L"Conditionally execute a block of commands" )  
	}
	,
	{
		L"bind",  &builtin_bind, N_( L"Handle fish key bindings" ) 
	}
	,
	{
		L"random",  &builtin_random, N_( L"Generate random number" ) 
	}
	,
	{
		L"status",  &builtin_status, N_( L"Return status information about fish" ) 
	}
	,
	{
		L"ulimit",  &builtin_ulimit, N_( L"Set or get the shells resource usage limits" ) 
	}
	,
	{
		L"begin",  &builtin_begin, N_( L"Create a block of code" )  
	}
	,

	/*
	  Builtins that are handled directly by the parser. They are
	  bound to a noop function only so that they show up in the
	  listings of builtin commands, etc..
	*/
	{
		L"command",   &builtin_generic, N_( L"Run a program instead of a function or builtin" )  
	}
	,
	{
		L"if",  &builtin_generic, N_( L"Evaluate block if condition is true" )  
	}
	,
	{
		L"while",  &builtin_generic, N_( L"Perform a command multiple times" )  
	}
	,
	{
		L"not",  &builtin_generic, N_( L"Negate exit status of job" ) 
	}
	,
	{
		L"and",  &builtin_generic, N_( L"Execute command if previous command suceeded" ) 
	}
	,
	{
		L"or",  &builtin_generic, N_( L"Execute command if previous command failed" ) 
	}
	,
	{
		L"exec",  &builtin_generic, N_( L"Run command in current process" ) 
	}
	,

	/*
	  This is not a builtin, but fish handles it's help display
	  internally. So some ugly special casing to make sure 'count -h'
	  displays the help for count, but 'count (echo -h)' does not.
	*/
	{
		L"count",  &builtin_generic, 0 
	}
	,
	{
		0, 0, 0
	}
}
	;
	

void builtin_init()
{
	
	int i;
	
	al_init( &io_stack );
	hash_init( &builtin, &hash_wcs_func, &hash_wcs_cmp );

	for( i=0; builtin_data[i].name; i++ )
	{
		hash_put( &builtin, builtin_data[i].name, builtin_data[i].func );
		intern_static( builtin_data[i].name );
	}
}

void builtin_destroy()
{
	if( desc )
	{
		hash_destroy( desc );
		free( desc );
		desc=0;
	}

	al_destroy( &io_stack );
	hash_destroy( &builtin );
}

int builtin_exists( wchar_t *cmd )
{
	CHECK( cmd, 0 );
		
	/*
	  Count is not a builtin, but it's help is handled internally by
	  fish, so it is in the hash_table_t.
	*/
	if( wcscmp( cmd, L"count" )==0)
		return 0;

	return (hash_get(&builtin, cmd) != 0 );
}

/**
   Return true if the specified builtin should handle it's own help,
   false otherwise.
*/
static int internal_help( wchar_t *cmd )
{
	if( wcscmp( cmd, L"for" ) == 0 ||
		wcscmp( cmd, L"while" ) == 0 ||
		wcscmp( cmd, L"function" ) == 0 ||
		wcscmp( cmd, L"if" ) == 0 ||
		wcscmp( cmd, L"end" ) == 0 ||
		wcscmp( cmd, L"switch" ) == 0 )
		return 1;
	return 0;
}


int builtin_run( wchar_t **argv )
{
	int (*cmd)(wchar_t **argv)=0;
	
	CHECK( argv, 1 );
	CHECK( argv[0], 1 );
		
	cmd = (int (*)(wchar_t **))hash_get( &builtin, argv[0] );

	if( argv[1] != 0 && !internal_help(argv[0]) )
	{
		if( argv[2] == 0 && (parser_is_help( argv[1], 0 ) ) )
		{
			builtin_print_help( argv[0], sb_out );
			return 0;
		}
	}

	if( cmd != 0 )
	{
		int status;

		status = cmd(argv);
		return status;

	}
	else
	{
		debug( 0, _( L"Unknown builtin '%ls'" ), argv[0] );
	}
	return 1;
}


void builtin_get_names( array_list_t *list )
{
	CHECK( list, );
		
 	hash_get_keys( &builtin, list );
}

const wchar_t *builtin_get_desc( const wchar_t *b )
{
	CHECK( b, 0 );
	
	if( !desc )
	{
		int i;
		desc = malloc( sizeof( hash_table_t ) );
		if( !desc)
			return 0;

		hash_init( desc, &hash_wcs_func, &hash_wcs_cmp );

		for( i=0; builtin_data[i].name; i++ )
		{
			hash_put( desc, builtin_data[i].name, builtin_data[i].desc );
		}
	}

	return _( hash_get( desc, b ));
}

void builtin_push_io( int in )
{
	if( builtin_stdin != -1 )
	{
		al_push_long( &io_stack, (long)builtin_stdin );
		al_push( &io_stack, sb_out );
		al_push( &io_stack, sb_err );
	}
	builtin_stdin = in;
	sb_out = malloc(sizeof(string_buffer_t));
	sb_err = malloc(sizeof(string_buffer_t));
	sb_init( sb_out );
	sb_init( sb_err );
}

void builtin_pop_io()
{
	builtin_stdin = 0;
	sb_destroy( sb_out );
	sb_destroy( sb_err );
	free( sb_out);
	free(sb_err);

	if( al_get_count( &io_stack ) >0 )
	{
		sb_err = (string_buffer_t *)al_pop( &io_stack );
		sb_out = (string_buffer_t *)al_pop( &io_stack );
		builtin_stdin = (int)al_pop_long( &io_stack );
	}
	else
	{
		sb_out = sb_err = 0;
		builtin_stdin = 0;
	}
}

