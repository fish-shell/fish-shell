/** \file builtin.c
	Functions for executing builtin functions.

	How to add a new builtin function:

	1). Create a function in builtin.c with the following signature:

	<tt>static int builtin_NAME( parser_t &parser, wchar_t ** args )</tt>

	where NAME is the name of the builtin, and args is a zero-terminated list of arguments.

	2). Add a line like { L"NAME", &builtin_NAME, N_(L"Bla bla bla") }, to the builtin_data_t variable. The description is used by the completion system. Note that this array is sorted!

	3). Create a file doc_src/NAME.txt, containing the manual for the builtin in Doxygen-format. Check the other builtin manuals for proper syntax.

	4). Use 'darcs add doc_src/NAME.txt' to start tracking changes to the documentation file.

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
#include <stack>

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
#include "exec.h"
#include "highlight.h"
#include "parse_util.h"
#include "parser_keywords.h"
#include "expand.h"
#include "path.h"
#include "history.h"

/**
   The default prompt for the read command
*/
#define DEFAULT_READ_PROMPT L"set_color green; echo -n read; set_color normal; echo -n \"> \""

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
struct builtin_data_t
{
	/**
	   Name of the builtin
	*/
	const wchar_t *name;
	/**
	   Function pointer tothe builtin implementation
	*/
	int (*func)(parser_t &parser, wchar_t **argv);
	/**
	   Description of what the builtin does
	*/
	const wchar_t *desc;
    
    bool operator<(const wcstring &) const;
    bool operator<(const builtin_data_t *) const;
};

bool builtin_data_t::operator<(const wcstring &other) const
{
    return wcscmp(this->name, other.c_str()) < 0;
}

bool builtin_data_t::operator<(const builtin_data_t *other) const
{
    return wcscmp(this->name, other->name) < 0;
}

int builtin_out_redirect;
int builtin_err_redirect;

/* Buffers for storing the output of builtin functions */
wcstring stdout_buffer, stderr_buffer;

const wcstring &get_stdout_buffer() {
    ASSERT_IS_MAIN_THREAD();
    return stdout_buffer;
}

const wcstring &get_stderr_buffer() {
    ASSERT_IS_MAIN_THREAD();
    return stderr_buffer;
}

void builtin_show_error(const wcstring &err) {
    ASSERT_IS_MAIN_THREAD();
    stderr_buffer.append(err);
}

/**
   Stack containing builtin I/O for recursive builtin calls.
*/
struct io_stack_elem_t {
    int in;
    wcstring out;
    wcstring err;
};
static std::stack<io_stack_elem_t, std::vector<io_stack_elem_t> > io_stack;

/**
   The file from which builtin functions should attempt to read, use
   instead of stdin.
*/
static int builtin_stdin;

/**
   The underlying IO redirections behind the current builtin. This
   should normally not be used - sb_out and friends are already
   configured to handle everything.
*/
static const io_chain_t *real_io;

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
	the sb_err string instead of to stderr. Used by the builtin
	commands.
*/

static void builtin_wperror( const wchar_t *s)
{
	if( s != 0 )
	{
        stderr_buffer.append(s);
        stderr_buffer.append(L": ");
	}
	char *err = strerror( errno );
	wchar_t *werr = str2wcs( err );
	if( werr )
	{
        stderr_buffer.append(werr);
        stderr_buffer.push_back(L'\n');
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

wcstring builtin_help_get( parser_t &parser, const wchar_t *name )
{
    wcstring_list_t lst;
    wcstring out;
    const wcstring name_esc = escape_string(name, 1);
    const wcstring cmd = format_string(L"__fish_print_help %ls", name_esc.c_str());
	if( exec_subshell( cmd, lst ) >= 0 )
	{	
		for( size_t i=0; i<lst.size(); i++ )
		{
            out.append(lst.at(i));
            out.push_back(L'\n');
		}
	}
	return out;
}

/**
   Print help for the specified builtin. If \c b is sb_err, also print
   the line information

   If \c b is the buffer representing standard error, and the help
   message is about to be printed to an interactive screen, it may be
   shortened to fit the screen.

   
*/

static void builtin_print_help( parser_t &parser, const wchar_t *cmd, wcstring &b )
{
	
	int is_short = 0;
	
	if( &b == &stderr_buffer )
	{
        stderr_buffer.append(parser.current_line());
	}

	const wcstring h = builtin_help_get( parser, cmd );

	if( !h.size())
		return;

	wchar_t *str = wcsdup( h.c_str() );
	if( str )
	{

		if( &b == &stderr_buffer )
		{
			
			/*
			  Interactive mode help to screen - only print synopsis if
			  the rest won't fit  
			*/
			
			int screen_height, lines;

			screen_height = common_get_height();
			lines = count_char( str, L'\n' );
			if( !get_is_interactive() || (lines > 2*screen_height/3) )
			{
				wchar_t *pos;
				int cut=0;
				int i;
				
				is_short = 1;
								
				/*
				  First move down 4 lines
				*/
				
				pos = str;
				for( i=0; (i<4) && pos && *pos; i++ )
				{
					pos = wcschr( pos+1, L'\n' );
				}
				
				if( pos && *pos )
				{
						
					/* 
					   Then find the next empty line 
					*/
					for( ; *pos; pos++ )
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
								/*
								  And cut it
								*/
								*(pos2+1)=L'\0';
								cut = 1;
								break;
							}
						}
					}
				}
				
				/*
				  We did not find a good place to cut message to
				  shorten it - so we make sure we don't print
				  anything.
				*/
				if( !cut )
				{
					*str = 0;
				}

			}
		}
		
        b.append(str);
		if( is_short )
		{
            append_format(b, _(L"%ls: Type 'help %ls' for related documentation\n\n"), cmd, cmd);
		}
				
		free( str );
	}
}

/**
   Perform error reporting for encounter with unknown option
*/
static void builtin_unknown_option( parser_t &parser, const wchar_t *cmd, const wchar_t *opt )
{
    append_format(stderr_buffer, BUILTIN_ERR_UNKNOWN, cmd, opt);
	builtin_print_help( parser, cmd, stderr_buffer );
}

/**
   Perform error reporting for encounter with missing argument
*/
static void builtin_missing_argument( parser_t &parser, const wchar_t *cmd, const wchar_t *opt )
{
    append_format(stderr_buffer, BUILTIN_ERR_MISSING, cmd, opt);
	builtin_print_help( parser, cmd, stderr_buffer );
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
  'builtin_NAME.c', where NAME is the name of the builtin. These files
  are included directly below.

*/


#include "builtin_set.cpp"
#include "builtin_commandline.cpp"
#include "builtin_complete.cpp"
#include "builtin_ulimit.cpp"
#include "builtin_jobs.cpp"

/* builtin_test lives in builtin_test.cpp */
int builtin_test( parser_t &parser, wchar_t **argv );

/**
   List all current key bindings
 */
static void builtin_bind_list()
{
	size_t i;
    wcstring_list_t lst;
	input_mapping_get_names( lst );
	
	for( i=0; i<lst.size(); i++ )
	{
		wcstring seq = lst.at(i);
		
        wcstring ecmd;
        input_mapping_get(seq, ecmd);
        ecmd = escape_string(ecmd, 1);		
        
        wcstring tname; 
		if( input_terminfo_get_name(seq, tname) )
		{
			append_format(stdout_buffer, L"bind -k %ls %ls\n", tname.c_str(), ecmd.c_str() );
		}
		else
		{
			const wcstring eseq = escape_string( seq, 1 );	
			append_format(stdout_buffer, L"bind %ls %ls\n", eseq.c_str(), ecmd.c_str() );
		}		
	}	
}

/**
   Print terminfo key binding names to string buffer used for standard output.

   \param all if set, all terminfo key binding names will be
   printed. If not set, only ones that are defined for this terminal
   are printed.
 */
static void builtin_bind_key_names( int all )
{
    const wcstring_list_t names = input_terminfo_get_names(!all);	
	for( size_t i=0; i<names.size(); i++ )
	{
        const wcstring &name = names.at(i);
		
		append_format(stdout_buffer, L"%ls\n", name.c_str() );
	}
}

/**
   Print all the special key binding functions to string buffer used for standard output.

 */
static void builtin_bind_function_names()
{
    wcstring_list_t names = input_function_get_names();
	
	for( size_t i=0; i<names.size(); i++ )
	{
		const wchar_t *seq = names.at(i).c_str();		
		append_format(stdout_buffer, L"%ls\n", seq );
	}
}

/**
   Add specified key binding.
 */
static int builtin_bind_add( wchar_t *seq, wchar_t *cmd, int terminfo )
{

	if( terminfo )
	{
		const wchar_t *seq2 = input_terminfo_get_sequence( seq );
		if( seq2 )
		{
			input_mapping_add( seq2, cmd );
		}
		else
		{
			switch( errno )
			{

				case ENOENT:
				{
					append_format(stderr_buffer, _(L"%ls: No key with name '%ls' found\n"), L"bind", seq );
					break;
				}

				case EILSEQ:
				{
					append_format(stderr_buffer, _(L"%ls: Key with name '%ls' does not have any mapping\n"), L"bind", seq );
					break;
				}

				default:
				{
					append_format(stderr_buffer, _(L"%ls: Unknown error trying to bind to key named '%ls'\n"), L"bind", seq );
					break;
				}

			}
			
			return 1;
		}
		
	}
	else
	{
		input_mapping_add( seq, cmd );
	}
	
	return 0;
	
}

/**
   Erase specified key bindings

   \param seq an array of all key bindings to erase
   \param all if specified, _all_ key bindings will be erased
 */
static void builtin_bind_erase( wchar_t **seq, int all )
{
	if( all )
	{
		size_t i;
		wcstring_list_t lst;
		input_mapping_get_names( lst );
		
		for( i=0; i<lst.size(); i++ )
		{
			input_mapping_erase( lst.at(i).c_str() );			
		}		
		
	}
	else
	{
		while( *seq )
		{
			input_mapping_erase( *seq++ );
		}
		
	}
	
}


/**
   The bind builtin, used for setting character sequences
*/
static int builtin_bind( parser_t &parser, wchar_t **argv )
{

	enum
	{
		BIND_INSERT,
		BIND_ERASE,
		BIND_KEY_NAMES,
		BIND_FUNCTION_NAMES
	}
	;
	
	int argc=builtin_count_args( argv );
	int mode = BIND_INSERT;
	int res = STATUS_BUILTIN_OK;
	int all = 0;
	
	int use_terminfo = 0;
	
	woptind=0;

	static const struct woption
		long_options[] =
		{
			{
				L"all", no_argument, 0, 'a'
			}
			,
			{
				L"erase", no_argument, 0, 'e'
			}
			,
			{
				L"function-names", no_argument, 0, 'f'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				L"key", no_argument, 0, 'k'
			}
			,
			{
				L"key-names", no_argument, 0, 'K'
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
					L"aehkKf",
					long_options,
					&opt_index );
		
		if( opt == -1 )
			break;
		
		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
				append_format(stderr_buffer,
						   BUILTIN_ERR_UNKNOWN,
						   argv[0],
						   long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );

				return STATUS_BUILTIN_ERROR;

			case 'a':
				all = 1;
				break;
				
			case 'e':
				mode = BIND_ERASE;
				break;
				

			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;
				
			case 'k':
				use_terminfo = 1;
				break;
				
			case 'K':
				mode = BIND_KEY_NAMES;
				break;
				
			case 'f':
				mode = BIND_FUNCTION_NAMES;
				break;
				
			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}
		
	}

	switch( mode )
	{
		
		case BIND_ERASE:
		{
			builtin_bind_erase( &argv[woptind], all);
			break;
		}
		
		case BIND_INSERT:
		{
			switch( argc-woptind )
			{
				case 0:
				{
					builtin_bind_list();
					break;
				}

				case 2:
				{
					builtin_bind_add(argv[woptind], argv[woptind+1], use_terminfo );
					break;
				}

				default:
				{
					res = STATUS_BUILTIN_ERROR;
					append_format(stderr_buffer, _(L"%ls: Expected zero or two parameters, got %d"), argv[0], argc-woptind );
					break;
				}
			}
			break;
		}

		case BIND_KEY_NAMES:
		{
			builtin_bind_key_names( all );
			break;
		}

		
		case BIND_FUNCTION_NAMES:
		{
			builtin_bind_function_names();
			break;
		}

		
		default:
		{
			res = STATUS_BUILTIN_ERROR;
			append_format(stderr_buffer, _(L"%ls: Invalid state\n"), argv[0] );
			break;
		}
	}
	
	return res;
}

/**
   The block builtin, used for temporarily blocking events
*/
static int builtin_block( parser_t &parser, wchar_t **argv )
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

	static const struct woption
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
				append_format(stderr_buffer,
						   BUILTIN_ERR_UNKNOWN,
						   argv[0],
						   long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );

				return STATUS_BUILTIN_ERROR;
			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

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
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}

	}

	if( erase )
	{
		if( scope != UNSET )
		{
			append_format(stderr_buffer, _( L"%ls: Can not specify scope when removing block\n" ), argv[0] );
			return STATUS_BUILTIN_ERROR;
		}
        
        if (parser.global_event_blocks.empty())
		{
			append_format(stderr_buffer, _( L"%ls: No blocks defined\n" ), argv[0] );
			return STATUS_BUILTIN_ERROR;
		}
		parser.global_event_blocks.pop_front();
	}
	else
	{
		block_t *block=parser.current_block;

        event_blockage_t eb = {};
		eb.typemask = type;

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
				while( block && 
				       block->type() != FUNCTION_CALL &&
				       block->type() != FUNCTION_CALL_NO_SHADOW )
					block = block->outer;
			}
		}
		if( block )
		{
            block->event_blocks.push_front(eb);
		}
		else
		{
            parser.global_event_blocks.push_front(eb);
		}
	}

	return STATUS_BUILTIN_OK;

}

/**
   The builtin builtin, used for giving builtins precedence over
   functions. Mostly handled by the parser. All this code does is some
   additional operational modes, such as printing a list of all
   builtins, printing help, etc.
*/
static int builtin_builtin( parser_t &parser, wchar_t **argv )
{
	int argc=builtin_count_args( argv );
	int list=0;

	woptind=0;

	static const struct woption
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
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );


				return STATUS_BUILTIN_ERROR;
			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

			case 'n':
				list=1;
				break;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}

	}

	if( list )
	{
        wcstring_list_t names = builtin_get_names();
        sort(names.begin(), names.end());
		
		for( size_t i=0; i<names.size(); i++ )
		{
			const wchar_t *el = names.at(i).c_str();
			
			stdout_buffer.append(el);
            stdout_buffer.append(L"\n");
		}
	}
	return STATUS_BUILTIN_OK;
}

/**
   Implementation of the builtin emit command, used to create events.
 */
static int builtin_emit( parser_t &parser, wchar_t **argv )
{
	int argc=builtin_count_args( argv );
	
	woptind=0;

	static const struct woption
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
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;

			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}

	}

	for (; woptind < argc; woptind++ )
	{
		event_fire_generic( argv[woptind] );
	}

	return STATUS_BUILTIN_OK;
	
	
	
}


/**
   A generic bultin that only supports showing a help message. This is
   only a placeholder that prints the help message. Useful for
   commands that live in the parser.
*/
static int builtin_generic( parser_t &parser, wchar_t **argv )
{
	int argc=builtin_count_args( argv );
	woptind=0;

	static const struct woption
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
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;

			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}

	}
	return STATUS_BUILTIN_ERROR;
}

/**
   Output a definition of the specified function to the specified
   string. Used by the functions builtin.
*/
static void functions_def( const wcstring &name, wcstring &out )
{
    CHECK( ! name.empty(), );
    
    wcstring desc, def;
    function_get_desc(name, &desc);
    function_get_definition(name, &def);

	event_t search(EVENT_ANY);

	search.function_name = name;

	std::vector<event_t *> ev;
	event_get( &search, &ev );

    out.append(L"function ");
    
    /* Typically we prefer to specify the function name first, e.g. "function foo --description bar"
       But If the function name starts with a -, we'll need to output it after all the options. */
    bool defer_function_name = (name.at(0) == L'-');
    if ( ! defer_function_name ){
        out.append(name);
    }

	if (! desc.empty())
	{
        wcstring esc_desc = escape_string(desc, true);
		out.append(L" --description ");
        out.append(esc_desc);
	}

	if( !function_get_shadows( name ) )
	{
		out.append(L" --no-scope-shadowing" );	
	}

	for( size_t i=0; i<ev.size(); i++ )
	{
		event_t *next = ev.at(i);
		switch( next->type )
		{
			case EVENT_SIGNAL:
			{
				append_format( out, L" --on-signal %ls", sig2wcs( next->param1.signal ) );
				break;
			}

			case EVENT_VARIABLE:
			{
				append_format( out, L" --on-variable %ls", next->str_param1.c_str() );
				break;
			}

			case EVENT_EXIT:
			{
				if( next->param1.pid > 0 )
					append_format( out, L" --on-process-exit %d", next->param1.pid );
				else
					append_format( out, L" --on-job-exit %d", -next->param1.pid );
				break;
			}

			case EVENT_JOB_ID:
			{
				const job_t *j = job_get( next->param1.job_id );
				if( j )
					append_format( out, L" --on-job-exit %d", j->pgid );
				break;
			}

			case EVENT_GENERIC:
			{
				append_format( out, L" --on-event %ls", next->str_param1.c_str() );
				break;
			}
						
		}

	}

	
    wcstring_list_t named = function_get_named_arguments( name );
	if( named.size() > 0 )
	{
		append_format( out, L" --argument" );
		for( size_t i=0; i < named.size(); i++ )
		{
			append_format( out, L" %ls", named.at(i).c_str() );
		}
	}

    /* Output the function name if we deferred it */
    if ( defer_function_name ){
        out.append(L" -- ");
        out.append(name);
    }
    
    /* This forced tab is sort of crummy - not all functions start with a tab */
    append_format( out, L"\n\t%ls", def.c_str());
    
    /* Append a newline before the 'end', unless there already is one there */
    if (! string_suffixes_string(L"\n", def)) {
        out.push_back(L'\n');
    }
    out.append(L"end\n");
}


/**
   The functions builtin, used for listing and erasing functions.
*/
static int builtin_functions( parser_t &parser, wchar_t **argv )
{
	int i;
	int erase=0;
	wchar_t *desc=0;

	int argc=builtin_count_args( argv );
	int list=0;
	int show_hidden=0;
	int res = STATUS_BUILTIN_OK;
	int query = 0;
	int copy = 0;

	woptind=0;

	static const struct woption
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
				L"copy", no_argument, 0, 'c'
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
								L"ed:nahqc",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );


				return STATUS_BUILTIN_ERROR;

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
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

			case 'q':
				query = 1;
				break;

			case 'c':
				copy = 1;
				break;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}

	}

	/*
	  Erase, desc, query, copy and list are mutually exclusive
	*/
	if( (erase + (!!desc) + list + query + copy) > 1 )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Invalid combination of options\n" ),
				   argv[0] );

		builtin_print_help( parser, argv[0], stderr_buffer );

		return STATUS_BUILTIN_ERROR;
	}

	if( erase )
	{
		int i;
		for( i=woptind; i<argc; i++ )
			function_remove( argv[i] );
		return STATUS_BUILTIN_OK;
	}
	else if( desc )
	{
		wchar_t *func;

		if( argc-woptind != 1 )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Expected exactly one function name\n" ),
					   argv[0] );
			builtin_print_help( parser, argv[0], stderr_buffer );

			return STATUS_BUILTIN_ERROR;
		}
		func = argv[woptind];
		if( !function_exists( func ) )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Function '%ls' does not exist\n" ),
					   argv[0],
					   func );

			builtin_print_help( parser, argv[0], stderr_buffer );

			return STATUS_BUILTIN_ERROR;
		}

		function_set_desc( func, desc );

		return STATUS_BUILTIN_OK;
	}
	else if( list || (argc==woptind))
	{
		int is_screen = !builtin_out_redirect && isatty(1);
        size_t i;
		wcstring_list_t names = function_get_names( show_hidden );
        std::sort(names.begin(), names.end());
		if( is_screen )
		{
            wcstring buff;

			for( i=0; i<names.size(); i++ )
			{
                buff.append(names.at(i));
                buff.append(L", ");
			}

			write_screen( buff, stdout_buffer );
		}
		else
		{
			for( i=0; i<names.size(); i++ )
			{
				stdout_buffer.append(names.at(i).c_str());
                stdout_buffer.append(L"\n");
			}
		}

		return STATUS_BUILTIN_OK;
	}
	else if( copy )
	{
		wcstring current_func;
		wcstring new_func;
		
		if( argc-woptind != 2 )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Expected exactly two names (current function name, and new function name)\n" ),
					   argv[0] );
			builtin_print_help ( parser, argv[0], stderr_buffer );
			
			return STATUS_BUILTIN_ERROR;
		}
		current_func = argv[woptind];
		new_func = argv[woptind+1];
		
		if( !function_exists( current_func ) )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Function '%ls' does not exist\n" ),
					   argv[0],
					   current_func.c_str() );
			builtin_print_help( parser, argv[0], stderr_buffer );

			return STATUS_BUILTIN_ERROR;
		}

		if( (wcsfuncname( new_func.c_str() ) != 0) || parser_keywords_is_reserved( new_func ) )
		{
			append_format(stderr_buffer,
			           _( L"%ls: Illegal function name '%ls'\n"),
			           argv[0],
			           new_func.c_str());
			builtin_print_help( parser, argv[0], stderr_buffer );
			return STATUS_BUILTIN_ERROR;
		}

		// keep things simple: don't allow existing names to be copy targets.
		if( function_exists( new_func ) )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Function '%ls' already exists. Cannot create copy '%ls'\n" ),
					   argv[0],
					   new_func.c_str(),
					   current_func.c_str() );            
			builtin_print_help( parser, argv[0], stderr_buffer );

			return STATUS_BUILTIN_ERROR;
		}

		if( function_copy( current_func, new_func ) )
			return STATUS_BUILTIN_OK;
		return STATUS_BUILTIN_ERROR;
	}

	for( i=woptind; i<argc; i++ )
	{
		if( !function_exists( argv[i] ) )
			res++;
		else
		{
			if( !query )
			{
				if( i != woptind)
					stdout_buffer.append( L"\n" );
				
				functions_def( argv[i], stdout_buffer );
			}
		}				
	}

	return res;
}

static unsigned int builtin_echo_digit(wchar_t wc, unsigned int base)
{
    // base must be hex or octal
    assert(base == 8 || base == 16);
    switch (wc)
    {
        case L'0': return 0;
        case L'1': return 1;
        case L'2': return 2;
        case L'3': return 3;
        case L'4': return 4;
        case L'5': return 5;
        case L'6': return 6;
        case L'7': return 7;
    }

    if (base == 16) switch (wc)
    {
        case L'8': return 8;
        case L'9': return 9;
        case L'a': case L'A': return 10;
        case L'b': case L'B': return 11;
        case L'c': case L'C': return 12;
        case L'd': case L'D': return 13;
        case L'e': case L'E': return 14;
        case L'f': case L'F': return 15;
    }
    return UINT_MAX;
}

/* Parse a numeric escape sequence in str, returning whether we succeeded.
   Also return the number of characters consumed and the resulting value.
   Supported escape sequences:
   
   \0nnn: octal value, zero to three digits
   \nnn: octal value, one to three digits
   \xhh: hex value, one to two digits
*/
static bool builtin_echo_parse_numeric_sequence(const wchar_t *str, size_t *consumed, unsigned char *out_val)
{
    bool success = false;
    unsigned char val = 0; //resulting character
    unsigned int start = 0; //the first character of the numeric part of the sequence
    
    unsigned int base = 0, max_digits = 0;
    if (builtin_echo_digit(str[0], 8) != UINT_MAX)
    {
        // Octal escape
        base = 8;

        // If the first digit is a 0, we allow four digits (including that zero)
        // Otherwise we allow 3.
        max_digits = (str[0] == L'0' ? 4 : 3);
    }
    else if (str[0] == L'x')
    {
        // Hex escape
        base = 16;
        max_digits = 2;
        
        // Skip the x
        start = 1;
    }
    
    if (base != 0)
    {
        unsigned int idx;
        for (idx = start; idx < start + max_digits; idx++)
        {
            unsigned int digit = builtin_echo_digit(str[idx], base);
            if (digit == UINT_MAX) break;
            val = val * base + digit;
        }
        
        // We succeeded if we consumed at least one digit
        if (idx > start)
        {
            *consumed = idx;
            *out_val = val;
            success = true;
        }
    }
    return success;
}

/** The echo builtin.
    bash only respects -n if it's the first argument. We'll do the same.
    We also support a new option -s to mean "no spaces"
*/

static int builtin_echo( parser_t &parser, wchar_t **argv )
{
    /* Skip first arg */
    if (! *argv++)
        return STATUS_BUILTIN_ERROR;
    
    /* Process options */
    bool print_newline = true, print_spaces = true, interpret_special_chars = false;
    while (*argv) {
        if (! wcscmp(*argv, L"-n")) {
            print_newline = false;
        } else if (! wcscmp(*argv, L"-s")) {
            print_spaces = false;
        } else if (! wcscmp(*argv, L"-e")) {
            interpret_special_chars = true;
        } else if (! wcscmp(*argv, L"-E")) {
            interpret_special_chars = false;
        } else {
            break;
        }
        argv++;
    }
    
    /* The special character \c can be used to indicate no more output */
    bool continue_output = true;
    
    for (size_t idx = 0; continue_output && argv[idx] != NULL; idx++) {

        if (print_spaces && idx > 0)
            stdout_buffer.push_back(' ');

        const wchar_t *str = argv[idx];
        for (size_t j=0; continue_output && str[j]; j++)
        {
            if (! interpret_special_chars || str[j] != L'\\')
            {
                /* Not an escape */
                stdout_buffer.push_back(str[j]);
            }
            else
            {
                /* Most escapes consume one character in addition to the backslash; the numeric sequences may consume more, while an unrecognized escape sequence consumes none. */
                wchar_t wc;
                size_t consumed = 1;
                switch (str[j+1])
                {
                    case L'a': wc = L'\a'; break;
                    case L'b': wc = L'\b'; break;
                    case L'e': wc = L'\e'; break;
                    case L'f': wc = L'\f'; break;
                    case L'n': wc = L'\n'; break;
                    case L'r': wc = L'\r'; break;
                    case L't': wc = L'\t'; break;
                    case L'v': wc = L'\v'; break;
                    case L'\\': wc = L'\\'; break;
                    
                    case L'c': wc = 0; continue_output = false; break;
                    
                    default:
                    {
                        /* Octal and hex escape sequences */
                        unsigned char narrow_val = 0;
                        if (builtin_echo_parse_numeric_sequence(str + j + 1, &consumed, &narrow_val))
                        {
                            /* Here consumed must have been set to something */
                            wc = narrow_val; //is this OK for conversion?
                        }
                        else
                        {
                            /* Not a recognized escape. We consume only the backslash. */
                            wc = L'\\';
                            consumed = 0;
                        }
                        break;
                    }
                }
            
                /* Skip over characters that were part of this escape sequence (but not the backslash, which will be handled by the loop increment */
                j += consumed;
                
                if (continue_output)
                    stdout_buffer.push_back(wc);
            }
        }
    }
    if (print_newline && continue_output)
        stdout_buffer.push_back('\n');
    return STATUS_BUILTIN_OK;
}

/** The pwd builtin. We don't respect -P to resolve symbolic links because we try to always resolve them. */
static int builtin_pwd( parser_t &parser, wchar_t **argv )
{
	wchar_t dir_path[4096];
	wchar_t *res = wgetcwd( dir_path, 4096 );
    if (res == NULL) {
        return STATUS_BUILTIN_ERROR;
    } else {
        stdout_buffer.append(dir_path);
        stdout_buffer.push_back(L'\n');
        return STATUS_BUILTIN_OK;
    }
}

/**
   The function builtin, used for providing subroutines.
   It calls various functions from function.c to perform any heavy lifting.
*/
static int builtin_function( parser_t &parser, wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int res=STATUS_BUILTIN_OK;
	wchar_t *desc=0;
	std::vector<event_t> events;
    
    std::auto_ptr<wcstring_list_t> named_arguments(NULL);

	wchar_t *name = 0;
	bool shadows = true;
		
	woptind=0;

    function_def_block_t * const fdb = new function_def_block_t();
	parser.push_block( fdb );

	static const struct woption
		long_options[] =
		{
			{
				L"description", required_argument, 0, 'd'
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
				L"on-event", required_argument, 0, 'e'
			}
			,
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				L"argument-names", no_argument, 0, 'a'
			}
			,
			{
				L"no-scope-shadowing", no_argument, 0, 'S'
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
					L"d:s:j:p:v:e:haS",
					long_options,
					&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );

				res = 1;
				break;

			case 'd':
				desc=woptarg;
				break;

			case 's':
			{
				int sig = wcs2sig( woptarg );

				if( sig < 0 )
				{
					append_format(stderr_buffer,
							   _( L"%ls: Unknown signal '%ls'\n" ),
							   argv[0],
							   woptarg );
					res=1;
					break;
				}
                events.push_back(event_t::signal_event(sig));
				break;
			}

			case 'v':
			{
				if( wcsvarname( woptarg ) )
				{
					append_format(stderr_buffer,
							   _( L"%ls: Invalid variable name '%ls'\n" ),
							   argv[0],
							   woptarg );
					res=STATUS_BUILTIN_ERROR;
					break;
				}

                events.push_back(event_t::variable_event(woptarg));
				break;
			}


			case 'e':
			{
                events.push_back(event_t::generic_event(woptarg));
				break;
			}

			case 'j':
			case 'p':
			{
				pid_t pid;
				wchar_t *end;
                event_t e(EVENT_ANY);
				
				if( ( opt == 'j' ) &&
					( wcscasecmp( woptarg, L"caller" ) == 0 ) )
				{
					int job_id = -1;

					if( is_subshell )
					{
						block_t *b = parser.current_block;

						while( b && (b->type() != SUBST) )
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
						append_format(stderr_buffer,
								   _( L"%ls: Cannot find calling job for event handler\n" ),
								   argv[0] );
						res=1;
					}
					else
					{
						e.type = EVENT_JOB_ID;
						e.param1.job_id = job_id;
					}

				}
				else
				{
					errno = 0;
					pid = fish_wcstoi( woptarg, &end, 10 );
					if( errno || !end || *end )
					{
						append_format(stderr_buffer,
								   _( L"%ls: Invalid process id %ls\n" ),
								   argv[0],
								   woptarg );
						res=1;
						break;
					}


					e.type = EVENT_EXIT;
					e.param1.pid = (opt=='j'?-1:1)*abs(pid);
				}
				if( res )
				{
                    /* nothing */
				}
				else
				{
                    events.push_back(e);
				}
				break;
			}

			case 'a':
				if( named_arguments.get() == NULL )
					named_arguments.reset(new wcstring_list_t);
				break;
				
			case 'S':
				shadows = 0;
				break;
				
			case 'h':
				parser.pop_block();
				parser.push_block( new fake_block_t() );
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;
				
			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				res = 1;
				break;

		}

	}

	if( !res )
	{
		
		if( argc == woptind )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Expected function name\n" ),
					   argv[0] );
			res=1;
		}
		else if( wcsfuncname( argv[woptind] ) )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Illegal function name '%ls'\n" ),
					   argv[0],
					   argv[woptind] );

			res=1;
		}
		else if( parser_keywords_is_reserved(argv[woptind] ) )
		{

			append_format(stderr_buffer,
					   _( L"%ls: The name '%ls' is reserved,\nand can not be used as a function name\n" ),
					   argv[0],
					   argv[woptind] );

			res=1;
		}
		else
		{

			name = argv[woptind++];
			
			if( named_arguments.get() )
			{
				while( woptind < argc )
				{
					if( wcsvarname( argv[woptind] ) )
					{
						append_format(stderr_buffer,
								   _( L"%ls: Invalid variable name '%ls'\n" ),
								   argv[0],
								   argv[woptind] );
						res = STATUS_BUILTIN_ERROR;
						break;
					}
					
                    named_arguments->push_back(argv[woptind++]);
				}
			}
			else if( woptind != argc )
			{
				append_format(stderr_buffer,
						   _( L"%ls: Expected one argument, got %d\n" ),
						   argv[0],
						   argc );
				res=1;
				
			}
		}
	}

	if( res )
	{
		size_t i;
		size_t chars=0;

		builtin_print_help( parser, argv[0], stderr_buffer );
		const wchar_t *cfa =  _( L"Current functions are: " );
		stderr_buffer.append( cfa );
		chars += wcslen( cfa );

        wcstring_list_t names = function_get_names(0);
        sort(names.begin(), names.end());

		for( i=0; i<names.size(); i++ )
		{
			const wchar_t *nxt = names.at(i).c_str();
			size_t l = wcslen( nxt + 2 );
			if( chars+l > common_get_width() )
			{
				chars = 0;
                stderr_buffer.push_back(L'\n');
			}

            stderr_buffer.append(nxt);
            stderr_buffer.append(L"  ");
		}
        stderr_buffer.push_back(L'\n');

		parser.pop_block();
		parser.push_block( new fake_block_t() );
	}
	else
	{
		function_data_t &d = fdb->function_data;
		
        d.name = name;
        if (desc)
            d.description = desc;
		d.events.swap(events);
		d.shadows = shadows;
        if (named_arguments.get())
            d.named_arguments.swap(*named_arguments);
		
		for( size_t i=0; i<d.events.size(); i++ )
		{
			event_t &e = d.events.at(i);
			e.function_name = d.name;
		}
	}
	
	parser.current_block->tok_pos = parser.get_pos();
	parser.current_block->skip = 1;

	return STATUS_BUILTIN_OK;

}

/**
   The random builtin. For generating random numbers.
*/
static int builtin_random( parser_t &parser, wchar_t **argv )
{
	static int seeded=0;
	static struct drand48_data seed_buffer;
	
	int argc = builtin_count_args( argv );

	woptind=0;

	static const struct woption
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
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );

				return STATUS_BUILTIN_ERROR;

			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				break;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

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
			
			append_format(stdout_buffer, L"%ld\n", labs(res%32767) );
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
				append_format(stderr_buffer,
						   _( L"%ls: Seed value '%ls' is not a valid number\n" ),
						   argv[0],
						   argv[woptind] );

				return STATUS_BUILTIN_ERROR;
			}
			seeded=1;
			srand48_r( foo, &seed_buffer);
			break;
		}

		default:
		{
			append_format(stderr_buffer,
					   _( L"%ls: Expected zero or one argument, got %d\n" ),
					   argv[0],
					   argc-woptind );
			builtin_print_help( parser, argv[0], stderr_buffer );
			return STATUS_BUILTIN_ERROR;
		}
	}
	return STATUS_BUILTIN_OK;
}


/**
   The read builtin. Reads from stdin and stores the values in environment variables.
*/
static int builtin_read( parser_t &parser, wchar_t **argv )
{
	wchar_t *buff=0;
	int i, argc = builtin_count_args( argv );
	int place = ENV_USER;
	wchar_t *nxt;
	const wchar_t *prompt = DEFAULT_READ_PROMPT;
	const wchar_t *commandline = L"";
	int exit_res=STATUS_BUILTIN_OK;
	const wchar_t *mode_name = READ_MODE_NAME;
	int shell = 0;
	
	woptind=0;
	
	while( 1 )
	{
		static const struct woption
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
					L"mode-name", required_argument, 0, 'm'
				}
				,
				{
					L"shell", no_argument, 0, 's'
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
								L"xglUup:c:hm:s",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );

				return STATUS_BUILTIN_ERROR;

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

			case L'm':
				mode_name = woptarg;
				break;

			case 's':
				shell = 1;
				break;
				
			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

			case L'?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;
		}

	}

	if( ( place & ENV_UNEXPORT ) && ( place & ENV_EXPORT ) )
	{
		append_format(stderr_buffer,
				   BUILTIN_ERR_EXPUNEXP,
				   argv[0] );


		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}

	if( (place&ENV_LOCAL?1:0) + (place & ENV_GLOBAL?1:0) + (place & ENV_UNIVERSAL?1:0) > 1)
	{
		append_format(stderr_buffer,
				   BUILTIN_ERR_GLOCAL,
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );

		return STATUS_BUILTIN_ERROR;
	}

	/*
	  Verify all variable names
	*/
	for( i=woptind; i<argc; i++ )
	{
		wchar_t *src;

		if( !wcslen( argv[i] ) )
		{
			append_format(stderr_buffer, BUILTIN_ERR_VARNAME_ZERO, argv[0] );
			return STATUS_BUILTIN_ERROR;
		}

		for( src=argv[i]; *src; src++ )
		{
			if( (!iswalnum(*src)) && (*src != L'_' ) )
			{
				append_format(stderr_buffer, BUILTIN_ERR_VARCHAR, argv[0], *src );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;
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
		const wchar_t *line;
		
		reader_push( mode_name );
		reader_set_left_prompt( prompt );
		if( shell )
		{
			reader_set_complete_function( &complete );
			reader_set_highlight_function( &highlight_shell );
			reader_set_test_function( &reader_shell_test );
		}
		
		reader_set_buffer( commandline, wcslen( commandline ) );
		proc_push_interactive( 1 );
		
		event_fire_generic(L"fish_prompt");
		line = reader_readline( );
		proc_pop_interactive();
		if( line )
		{
			buff = wcsdup( line );
		}
		else
		{
			exit_res = STATUS_BUILTIN_ERROR;
		}
		reader_pop();
	}
	else
	{
		int eof=0;
		
		wcstring sb;
		
		while( 1 )
		{
			int finished=0;

			wchar_t res=0;
			static mbstate_t state;
			memset (&state, '\0', sizeof (state));

			while( !finished )
			{
				char b;
				if( read_blocked( builtin_stdin, &b, 1 ) <= 0 )
				{
					eof=1;
					break;
				}

				size_t sz = mbrtowc( &res, &b, 1, &state );

				switch( sz )
				{
					case (size_t)(-1):
						memset (&state, '\0', sizeof (state));
						break;

					case (size_t)(-2):
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
    
            sb.push_back(res);
		}

		if( sb.size() < 2 && eof )
		{
			exit_res = 1;
		}
		
		buff = wcsdup( sb.c_str() );
	}

	if( i != argc && !exit_res )
	{
		
		wchar_t *state;

		env_var_t ifs = env_get_string( L"IFS" );
		if( ifs.missing() )
			ifs = L"";
		
		nxt = wcstok( buff, (i<argc-1)?ifs.c_str():L"", &state );
		
		while( i<argc )
		{
			env_set( argv[i], nxt != 0 ? nxt: L"", place );
			
			i++;
			if( nxt != 0 )
				nxt = wcstok( 0, (i<argc-1)?ifs.c_str():L"", &state);
		}
	}
	
	free( buff );

	return exit_res;
}

/**
   The status builtin. Gives various status information on fish.
*/
static int builtin_status( parser_t &parser, wchar_t **argv )
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
	int res=STATUS_BUILTIN_OK;

	woptind=0;


	const struct woption
		long_options[] =
		{
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				L"is-command-substitution", no_argument, 0, 'c'
			}
			,
			{
				L"is-block", no_argument, 0, 'b'
			}
			,
			{
				L"is-interactive", no_argument, 0, 'i'
			}
			,
			{
				L"is-login", no_argument, 0, 'l'
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
				L"current-filename", no_argument, 0, 'f'
			}
			,
			{
				L"current-line-number", no_argument, 0, 'n'
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
								L":cbilfnhj:t",
								long_options,
								&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
				if(long_options[opt_index].flag != 0)
					break;
                append_format(stderr_buffer,
                           BUILTIN_ERR_UNKNOWN,
                           argv[0],
                           long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;

			case 'c':
				mode = IS_SUBST;
				break;

			case 'b':
				mode = IS_BLOCK;
				break;

			case 'i':
				mode = IS_INTERACTIVE;
				break;

			case 'l':
				mode = IS_LOGIN;
				break;

			case 'f':
				mode = CURRENT_FILENAME;
				break;

			case 'n':
				mode = CURRENT_LINE_NUMBER;
				break;

			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;

			case 'j':
				if( wcscmp( woptarg, L"full" ) == 0 )
					job_control_mode = JOB_CONTROL_ALL;
				else if( wcscmp( woptarg, L"interactive" ) == 0 )
					job_control_mode = JOB_CONTROL_INTERACTIVE;
				else if( wcscmp( woptarg, L"none" ) == 0 )
					job_control_mode = JOB_CONTROL_NONE;
				else
				{
					append_format(stderr_buffer,
							   L"%ls: Invalid job control mode '%ls'\n",
							   woptarg );
					res = 1;
				}
				mode = DONE;				
				break;

			case 't':
				mode = STACK_TRACE;
				break;
				

			case ':':
				builtin_missing_argument( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

		}

	}

	if( !res )
	{

		switch( mode )
		{
			case CURRENT_FILENAME:
			{
				const wchar_t *fn = parser.current_filename();
				
				if( !fn )
					fn = _(L"Standard input");
				
				append_format(stdout_buffer, L"%ls\n", fn );
				
				break;
			}
			
			case CURRENT_LINE_NUMBER:
			{
				append_format(stdout_buffer, L"%d\n", parser.get_lineno() );
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
				parser.stack_trace( parser.current_block, stdout_buffer );
				break;
			}

			case NORMAL:
			{
				if( is_login )
					append_format(stdout_buffer, _( L"This is a login shell\n" ) );
				else
					append_format(stdout_buffer, _( L"This is not a login shell\n" ) );

				append_format(stdout_buffer, _( L"Job control: %ls\n" ),
						   job_control_mode==JOB_CONTROL_INTERACTIVE?_( L"Only on interactive jobs" ):
						   (job_control_mode==JOB_CONTROL_NONE ? _( L"Never" ) : _( L"Always" ) ) );

				parser.stack_trace( parser.current_block, stdout_buffer );
				break;
			}
		}
	}

	return res;
}


/**
   The exit builtin. Calls reader_exit to exit and returns the value specified.
*/
static int builtin_exit( parser_t &parser, wchar_t **argv )
{
	int argc = builtin_count_args( argv );

	long ec=0;
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
				append_format(stderr_buffer,
					   _( L"%ls: Argument '%ls' must be an integer\n" ),
					   argv[0],
					   argv[1] );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;
			}
			break;
		}

		default:
		{
			append_format(stderr_buffer,
				   BUILTIN_ERR_TOO_MANY_ARGUMENTS,
				   argv[0] );

			builtin_print_help( parser, argv[0], stderr_buffer );
			return STATUS_BUILTIN_ERROR;
		}
		
	}
	reader_exit( 1, 0 );
	return (int)ec;
}

/**
   The cd builtin. Changes the current directory to the one specified
   or to $HOME if none is specified. The directory can be relative to
   any directory in the CDPATH variable.
*/
static int builtin_cd( parser_t &parser, wchar_t **argv )
{
	env_var_t dir_in;
	wcstring dir;
	int res=STATUS_BUILTIN_OK;

	
	if (argv[1] == NULL)
	{
		dir_in = env_get_string( L"HOME" );
		if( dir_in.missing_or_empty() )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Could not find home directory\n" ),
					   argv[0] );
		}
	}
	else {
		dir_in = argv[1];
    }

    bool got_cd_path = false;
    if (! dir_in.missing())
    {
        got_cd_path = path_get_cdpath(dir_in, &dir);
    }

	if( !got_cd_path )
	{
		if( errno == ENOTDIR )
		{
			append_format(stderr_buffer,
				   _( L"%ls: '%ls' is not a directory\n" ),
				   argv[0],
				   dir_in.c_str() );
		}
		else if( errno == ENOENT )
		{
			append_format(stderr_buffer,
				   _( L"%ls: The directory '%ls' does not exist\n" ),
				   argv[0],
				   dir_in.c_str() );			
		}
		else if( errno == EROTTEN )
		{
			append_format(stderr_buffer,
				   _( L"%ls: '%ls' is a rotten symlink\n" ),
				   argv[0],
				   dir_in.c_str() );			
			
		} 
		else 
		{
			append_format(stderr_buffer,
				   _( L"%ls: Unknown error trying to locate directory '%ls'\n" ),
				   argv[0],
				   dir_in.c_str() );			
			
		}
		
		
		if( !get_is_interactive() )
		{
            stderr_buffer.append(parser.current_line());
		}
		
		res = 1;
	}
	else if( wchdir( dir ) != 0 )
	{
		struct stat buffer;
		int status;
		
		status = wstat( dir, &buffer );
		if( !status && S_ISDIR(buffer.st_mode))
		{
			append_format(stderr_buffer,
				   _( L"%ls: Permission denied: '%ls'\n" ),
				   argv[0],
				   dir.c_str() );
			
		}
		else
		{
			
			append_format(stderr_buffer,
				   _( L"%ls: '%ls' is not a directory\n" ),
				   argv[0],
				   dir.c_str() );
		}
		
		if( !get_is_interactive() )
		{
            stderr_buffer.append(parser.current_line());
		}
		
		res = 1;
	}
	else if( !env_set_pwd() )
	{
		res=1;
		append_format(stderr_buffer, _( L"%ls: Could not set PWD variable\n" ), argv[0] );
	}

	return res;
}

/**
   Implementation of the builtin count command, used to count the
   number of arguments sent to it.
 */
static int builtin_count( parser_t &parser, wchar_t ** argv )
{
	int argc;
	argc = builtin_count_args( argv );
	append_format(stdout_buffer, L"%d\n", argc-1 );
	return !(argc-1);
}

/**
   Implementation of the builtin contains command, used to check if a
   specified string is part of a list.
 */
static int builtin_contains( parser_t &parser, wchar_t ** argv )
{
	int argc;
	argc = builtin_count_args( argv );
	int i;
	wchar_t *needle;
	int index=0;
	
	woptind=0;

	const struct woption
		long_options[] =
		{
			{
				L"help", no_argument, 0, 'h'
			}
			,
			{
				L"index", no_argument, 0, 'i'
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
					L"+hi",
					long_options,
					&opt_index );
		if( opt == -1 )
			break;

		switch( opt )
		{
			case 0:
                assert(opt_index >= 0 && (size_t)opt_index < sizeof long_options / sizeof *long_options);
				if(long_options[opt_index].flag != 0)
					break;
				append_format(stderr_buffer,
					   BUILTIN_ERR_UNKNOWN,
					   argv[0],
					   long_options[opt_index].name );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;


			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return STATUS_BUILTIN_OK;


			case ':':
				builtin_missing_argument( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return STATUS_BUILTIN_ERROR;

			case 'i':
				index=1;
				break;
		}
		
	}



	needle = argv[woptind];
	if (!needle)
	{
		append_format(stderr_buffer, _( L"%ls: Key not specified\n" ), argv[0] );
	}
	

	for( i=woptind+1; i<argc; i++ )
	{
		
		if( !wcscmp( needle, argv[i]) )
		{
			if ( index ) append_format(stdout_buffer, L"%d\n", i-woptind );
			return 0;
		}
	}
	return 1;

}


/**
   The  . (dot) builtin, sometimes called source. Evaluates the contents of a file.
*/
static int builtin_source( parser_t &parser, wchar_t ** argv )
{
    ASSERT_IS_MAIN_THREAD();
	int fd;
	int res = STATUS_BUILTIN_OK;
	struct stat buf;
	int argc;

	argc = builtin_count_args( argv );

	const wchar_t *fn;
	const wchar_t *fn_intern;
		


	if( argc < 2 || (wcscmp( argv[1], L"-" ) == 0) )
	{
		fn = L"-";
		fn_intern = fn;
		fd = dup(builtin_stdin);
	}
	else
	{
		
		if( ( fd = wopen_cloexec( argv[1], O_RDONLY ) ) == -1 )
		{
			append_format(stderr_buffer, _(L"%ls: Error encountered while sourcing file '%ls':\n"), argv[0], argv[1] );
			builtin_wperror( L"." );
			return STATUS_BUILTIN_ERROR;
		}
        
		if( fstat(fd, &buf) == -1 )
		{
			close(fd);
			append_format(stderr_buffer, _(L"%ls: Error encountered while sourcing file '%ls':\n"), argv[0], argv[1] );
			builtin_wperror( L"." );
			return STATUS_BUILTIN_ERROR;
		}

		if( !S_ISREG(buf.st_mode) )
		{
			close(fd);
			append_format(stderr_buffer, _( L"%ls: '%ls' is not a file\n" ), argv[0], argv[1] );
			return STATUS_BUILTIN_ERROR;
		}

		fn = wrealpath( argv[1], 0 );

		if( !fn )
		{
			fn_intern = intern( argv[1] );
		}
		else
		{
			fn_intern = intern(fn);
			free( (void *)fn );
		}

	}
	
	parser.push_block( new source_block_t(fn_intern) );
	reader_push_current_filename( fn_intern );
		
	parse_util_set_argv( (argc>2)?(argv+2):(argv+1), wcstring_list_t());
	
	res = reader_read( fd, real_io ? *real_io : io_chain_t() );
		
	parser.pop_block();
		
	if( res )
	{
		append_format(stderr_buffer,
			   _( L"%ls: Error while reading file '%ls'\n" ),
			   argv[0],
				   fn_intern == intern_static(L"-") ? L"<stdin>" : fn_intern );
	}
	else
	{
		res = proc_get_last_status();
	}
	
	/*
	  Do not close fd after calling reader_read. reader_read
	  automatically closes it before calling eval.
	*/
	
	reader_pop_current_filename();

	return res;
}

/**
   Make the specified job the first job of the job list. Moving jobs
   around in the list makes the list reflect the order in which the
   jobs were used.
*/
static void make_first( job_t *j )
{
    job_promote(j);
}


/**
   Builtin for putting a job in the foreground
*/
static int builtin_fg( parser_t &parser, wchar_t **argv )
{
	job_t *j=NULL;

	if( argv[1] == 0 )
	{
		/*
		  Select last constructed job (I.e. first job in the job que)
		  that is possible to put in the foreground
		*/
        
        job_iterator_t jobs;
        while ((j = jobs.next()))
		{
			if( job_get_flag( j, JOB_CONSTRUCTED ) && (!job_is_completed(j)) && 
				( (job_is_stopped(j) || (!job_get_flag(j, JOB_FOREGROUND)) ) && job_get_flag( j, JOB_CONTROL) ) )
			{
				break;
			}
		}
		if( !j )
		{
			append_format(stderr_buffer,
					   _( L"%ls: There are no suitable jobs\n" ),
					   argv[0] );
			builtin_print_help( parser, argv[0], stderr_buffer );
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
		wchar_t *endptr;
		int pid;
		int found_job = 0;
		
		errno = 0;
		pid = fish_wcstoi( argv[1], &endptr, 10 );
		if( !( *endptr || errno )  )
		{			
			j = job_get_from_pid( pid );
			if( j )
				found_job = 1;
		}
		
		if( found_job )
		{
			append_format(stderr_buffer,
					   _( L"%ls: Ambiguous job\n" ),
					   argv[0] );
		}
		else
		{
			append_format(stderr_buffer,
					   _( L"%ls: '%ls' is not a job\n" ),
					   argv[0],
					   argv[1] );
		}

		builtin_print_help( parser, argv[0], stderr_buffer );

		j=0;

	}
	else
	{
		wchar_t *end;		
		int pid;
		errno = 0;
		pid = abs(fish_wcstoi( argv[1], &end, 10 ));
		
		if( *end || errno )
		{
				append_format(stderr_buffer,
						   BUILTIN_ERR_NOT_NUMBER,
						   argv[0],
						   argv[1] );
				builtin_print_help( parser, argv[0], stderr_buffer );
		}
		else
		{
			j = job_get_from_pid( pid );
			if( !j || !job_get_flag( j, JOB_CONSTRUCTED ) || job_is_completed( j ))
			{
				append_format(stderr_buffer,
						   _( L"%ls: No suitable job: %d\n" ),
						   argv[0],
						   pid );
				builtin_print_help( parser, argv[0], stderr_buffer );
				j=0;
			}
			else if( !job_get_flag( j, JOB_CONTROL) )
			{
				append_format(stderr_buffer,
						   _( L"%ls: Can't put job %d, '%ls' to foreground because it is not under job control\n" ),
						   argv[0],
						   pid,
						   j->command_wcstr() );
				builtin_print_help( parser, argv[0], stderr_buffer );
				j=0;
			}
		}
	}

	if( j )
	{
		if( builtin_err_redirect )
		{
			append_format(stderr_buffer,
					   FG_MSG,
					   j->job_id,
					   j->command_wcstr() );
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
					  j->command_wcstr() );
		}

		wchar_t *ft = tok_first( j->command_wcstr() );
		if( ft != 0 )
			env_set( L"_", ft, ENV_EXPORT );
		free(ft);
		reader_write_title();

		make_first( j );
		job_set_flag( j, JOB_FOREGROUND, 1 );

		job_continue( j, job_is_stopped(j) );
	}
	return j != 0;
}

/**
   Helper function for builtin_bg()
*/
static int send_to_bg( parser_t &parser, job_t *j, const wchar_t *name )
{
	if( j == 0 )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Unknown job '%ls'\n" ),
				   L"bg",
				   name );
		builtin_print_help( parser, L"bg", stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}
	else if( !job_get_flag( j, JOB_CONTROL ) )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Can't put job %d, '%ls' to background because it is not under job control\n" ),
				   L"bg",
				   j->job_id,
				   j->command_wcstr() );
		builtin_print_help( parser, L"bg", stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}
	else
	{
		append_format(stderr_buffer,
				   _(L"Send job %d '%ls' to background\n"),
				   j->job_id,
				   j->command_wcstr() );
	}
	make_first( j );
	job_set_flag( j, JOB_FOREGROUND, 0 );
	job_continue( j, job_is_stopped(j) );
	return STATUS_BUILTIN_OK;
}


/**
   Builtin for putting a job in the background
*/
static int builtin_bg( parser_t &parser, wchar_t **argv )
{
	int res = STATUS_BUILTIN_OK;

	if( argv[1] == 0 )
	{
  		job_t *j;
        job_iterator_t jobs;
        while ((j = jobs.next()))
        {
			if( job_is_stopped(j) && job_get_flag( j, JOB_CONTROL ) && (!job_is_completed(j)) )
			{
				break;
			}
		}
		
		if( !j )
		{
			append_format(stderr_buffer,
					   _( L"%ls: There are no suitable jobs\n" ),
					   argv[0] );
			res = 1;
		}
		else
		{
			res = send_to_bg( parser, j, _(L"(default)" ) );
		}
	}
	else
	{
		wchar_t *end;
		int i;
		int pid;
		int err = 0;
		
		for( i=1; argv[i]; i++ )
		{
			errno=0;
			pid = fish_wcstoi( argv[i], &end, 10 );
			if( errno || pid < 0 || *end || !job_get_from_pid( pid ) )
			{
				append_format(stderr_buffer,
						   _( L"%ls: '%ls' is not a job\n" ),
						   argv[0],
						   argv[i] );
				err = 1;
				break;
			}		
		}

		if( !err )
		{
			for( i=1; !res && argv[i]; i++ )
			{
				pid = fish_wcstoi( argv[i], 0, 10 );
				res |= send_to_bg( parser, job_get_from_pid( pid ), *argv);
			}
		}
	}
	
	return res;
}


/**
   Builtin for looping over a list
*/
static int builtin_for( parser_t &parser, wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int res=STATUS_BUILTIN_ERROR;


	if( argc < 3)
	{
		append_format(stderr_buffer,
				   BUILTIN_FOR_ERR_COUNT,
				   argv[0] ,
				   argc );
		builtin_print_help( parser, argv[0], stderr_buffer );
	}
	else if ( wcsvarname(argv[1]) )
	{
		append_format(stderr_buffer,
				   BUILTIN_FOR_ERR_NAME,
				   argv[0],
				   argv[1] );
		builtin_print_help( parser, argv[0], stderr_buffer );
	}
	else if (wcscmp( argv[2], L"in") != 0 )
	{
		append_format(stderr_buffer,
				   BUILTIN_FOR_ERR_IN,
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
	}
	else
	{
		res=0;
	}


	if( res )
	{
		parser.push_block( new fake_block_t() );
	}
	else
	{
        const wchar_t *for_variable = argv[1];
        for_block_t *fb = new for_block_t(for_variable);
		parser.push_block( fb );
		fb->tok_pos = parser.get_pos();

        /* Note that we store the sequence of values in opposite order */
        wcstring_list_t &for_vars = fb->sequence;
		for( int i=argc-1; i>3; i-- )
            for_vars.push_back(argv[i]);

		if( argc > 3 )
		{
			env_set( for_variable, argv[3], ENV_LOCAL );
		}
		else
		{
			parser.current_block->skip=1;
		}
	}
	return res;
}

/**
   The begin builtin. Creates a nex block.
*/
static int builtin_begin( parser_t &parser, wchar_t **argv )
{
	parser.push_block( new scope_block_t(BEGIN) );
	parser.current_block->tok_pos = parser.get_pos();
	return proc_get_last_status();
}


/**
   Builtin for ending a block of code, such as a for-loop or an if statement.

   The end command is whare a lot of the block-level magic happens.
*/
static int builtin_end( parser_t &parser, wchar_t **argv )
{
	if( !parser.current_block->outer )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Not inside of block\n" ),
				   argv[0] );

		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}
	else
	{
		/**
		   By default, 'end' kills the current block scope. But if we
		   are rewinding a loop, this should be set to false, so that
		   variables in the current loop scope won't die between laps.
		*/
		int kill_block = 1;

		switch( parser.current_block->type() )
		{
			case WHILE:
			{
				/*
				  If this is a while loop, we rewind the loop unless
				  it's the last lap, in which case we continue.
				*/
				if( !( parser.current_block->skip && (parser.current_block->loop_status != LOOP_CONTINUE )))
				{
					parser.current_block->loop_status = LOOP_NORMAL;
					parser.current_block->skip = 0;
					kill_block = 0;
					parser.set_pos( parser.current_block->tok_pos);
                    while_block_t *blk = static_cast<while_block_t *>(parser.current_block);
                    blk->status = WHILE_TEST_AGAIN;
				}

				break;
			}

			case IF:
			case SUBST:
			case BEGIN:
            case SWITCH:
            case FAKE:
				/*
				  Nothing special happens at the end of these commands. The scope just ends.
				*/

				break;

			case FOR:
			{
				/*
				  set loop variable to next element, and rewind to the beginning of the block.
				*/
                for_block_t *fb = static_cast<for_block_t *>(parser.current_block);
                wcstring_list_t &for_vars = fb->sequence;
				if( parser.current_block->loop_status == LOOP_BREAK )
				{
                    for_vars.clear();
				}

				if( ! for_vars.empty() )
				{
                    const wcstring val = for_vars.back();
                    for_vars.pop_back();
                    const wcstring &for_variable = fb->variable;
					env_set( for_variable.c_str(), val.c_str(),  ENV_LOCAL);
					parser.current_block->loop_status = LOOP_NORMAL;
					parser.current_block->skip = 0;
					
					kill_block = 0;
					parser.set_pos( parser.current_block->tok_pos );
				}
				break;
			}

			case FUNCTION_DEF:
			{
				function_def_block_t *fdb = static_cast<function_def_block_t *>(parser.current_block);
				function_data_t &d = fdb->function_data;
				
                if (d.name.empty())
                {
                    /* Disallow empty function names */
                    append_format(stderr_buffer, _( L"%ls: No function name given\n" ), argv[0] );
                    
                    /* Return an error via a crummy way. Don't just return here, since we need to pop the block. */
                    proc_set_last_status(STATUS_BUILTIN_ERROR);
                }
                else
                {
                    /**
                       Copy the text from the beginning of the function
                       until the end command and use as the new definition
                       for the specified function
                    */

                    wchar_t *def = wcsndup( parser.get_buffer()+parser.current_block->tok_pos,
                                            parser.get_job_pos()-parser.current_block->tok_pos );
                    d.definition = def;
        
                    function_add( d, parser );	
                    free( def );
                }
			}
			break;
            
            default:
                assert(false); //should never get here
                break;

		}
		if( kill_block )
		{
			parser.pop_block();
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
static int builtin_else( parser_t &parser, wchar_t **argv )
{
    bool block_ok = false;
    if_block_t *if_block = NULL;
    if (parser.current_block != NULL && parser.current_block->type() == IF)
    {
        if_block = static_cast<if_block_t *>(parser.current_block);
        /* Ensure that we're past IF but not up to an ELSE */
        if (if_block->if_expr_evaluated && ! if_block->else_evaluated)
        {
            block_ok = true;
        }
    }
    
	if( ! block_ok )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Not inside of 'if' block\n" ),
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}
	else
	{
        /* Run the else block if the IF expression was false and so were all the ELSEIF expressions (if any) */
        bool run_else = ! if_block->any_branch_taken;
		if_block->skip = ! run_else;
        if_block->else_evaluated = true;
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
static int builtin_break_continue( parser_t &parser, wchar_t **argv )
{
	int is_break = (wcscmp(argv[0],L"break")==0);
	int argc = builtin_count_args( argv );

	block_t *b = parser.current_block;

	if( argc != 1 )
	{
		append_format(stderr_buffer,
				   BUILTIN_ERR_UNKNOWN,
				   argv[0],
				   argv[1] );

		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}


	while( (b != 0) &&
		   ( b->type() != WHILE) &&
		   (b->type() != FOR ) )
	{
		b = b->outer;
	}

	if( b == 0 )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Not inside of loop\n" ),
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}

	b = parser.current_block;
	while( ( b->type() != WHILE) &&
		   (b->type() != FOR ) )
	{
		b->skip=1;
		b = b->outer;
	}
	b->skip=1;
	b->loop_status = is_break?LOOP_BREAK:LOOP_CONTINUE;
	return STATUS_BUILTIN_OK;
}

/**
   Implementation of the builtin breakpoint command, used to launch the
   interactive debugger.
 */

static int builtin_breakpoint( parser_t &parser, wchar_t **argv )
{
	parser.push_block( new breakpoint_block_t() );
	
	reader_read( STDIN_FILENO, real_io ? *real_io : io_chain_t() );
	
	parser.pop_block();		
	
	return proc_get_last_status();
}


/**
   Function for handling the \c return builtin
*/
static int builtin_return( parser_t &parser, wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int status = proc_get_last_status();

	block_t *b = parser.current_block;

	switch( argc )
	{
		case 1:
			break;
		case 2:
		{
			wchar_t *end;
			errno = 0;
			status = fish_wcstoi(argv[1],&end,10);
			if( errno || *end != 0)
			{
				append_format(stderr_buffer,
						   _( L"%ls: Argument '%ls' must be an integer\n" ),
						   argv[0],
						   argv[1] );
				builtin_print_help( parser, argv[0], stderr_buffer );
				return STATUS_BUILTIN_ERROR;
			}
			break;
		}
		default:
			append_format(stderr_buffer,
					   _( L"%ls: Too many arguments\n" ),
					   argv[0] );
			builtin_print_help( parser, argv[0], stderr_buffer );
			return STATUS_BUILTIN_ERROR;
	}


	while( (b != 0) &&
		   ( b->type() != FUNCTION_CALL &&
		     b->type() != FUNCTION_CALL_NO_SHADOW) )
	{
		b = b->outer;
	}

	if( b == 0 )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Not inside of function\n" ),
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}

	b = parser.current_block;
	while( ( b->type() != FUNCTION_CALL &&
		 b->type() != FUNCTION_CALL_NO_SHADOW ) )
	{
        b->mark_as_fake();
		b->skip=1;
		b = b->outer;
	}
	b->skip=1;

	return status;
}

/**
   Builtin for executing one of several blocks of commands depending
   on the value of an argument.
*/
static int builtin_switch( parser_t &parser, wchar_t **argv )
{
	int res=STATUS_BUILTIN_OK;
	int argc = builtin_count_args( argv );

	if( argc != 2 )
	{
		append_format(stderr_buffer,
				   _( L"%ls: Expected exactly one argument, got %d\n" ),
				   argv[0],
				   argc-1 );

		builtin_print_help( parser, argv[0], stderr_buffer );
		res=1;
		parser.push_block( new fake_block_t() );
	}
	else
	{
		parser.push_block( new switch_block_t(argv[1]) );
		parser.current_block->skip=1;
        res = proc_get_last_status();
	}
	
	return res;
}

/**
   Builtin used together with the switch builtin for conditional
   execution
*/
static int builtin_case( parser_t &parser, wchar_t **argv )
{
	int argc = builtin_count_args( argv );
	int i;
	wchar_t *unescaped=0;
	
	if( parser.current_block->type() != SWITCH )
	{
		append_format(stderr_buffer,
				   _( L"%ls: 'case' command while not in switch block\n" ),
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return STATUS_BUILTIN_ERROR;
	}
	
	parser.current_block->skip = 1;
	switch_block_t *sb = static_cast<switch_block_t *>(parser.current_block);
	if( sb->switch_taken )
	{
		return proc_get_last_status();
	}
	
    const wcstring &switch_value = sb->switch_value;
	for( i=1; i<argc; i++ )
	{
		int match;
		
		unescaped = parse_util_unescape_wildcards( argv[i] );
		match = wildcard_match( switch_value, unescaped );
		free( unescaped );
		
		if( match )
		{
			parser.current_block->skip = 0;
			sb->switch_taken = true;
			break;
		}
	}
	
	return proc_get_last_status();
}


/**
   History of commands executed by user
*/
static int builtin_history( parser_t &parser, wchar_t **argv )
{
    int argc = builtin_count_args(argv);

    bool search_history = false; 
    bool delete_item = false;
    bool search_prefix = false;
    bool save_history = false;
    bool clear_history = false;

    static const struct woption long_options[] =
        {
            { L"prefix", no_argument, 0, 'p' },
            { L"delete", no_argument, 0, 'd' },
            { L"search", no_argument, 0, 's' },
            { L"contains", no_argument, 0, 'c' },
            { L"save", no_argument, 0, 'v' },
            { L"clear", no_argument, 0, 'l' },
            { L"help", no_argument, 0, 'h' },
            { 0, 0, 0, 0 }
        };

    int opt = 0;
    int opt_index = 0;
    woptind = 0;
    history_t *history = reader_get_history();
    
    /* Use the default history if we have none (which happens if invoked non-interactively, e.g. from webconfig.py */
    if (! history)
        history = &history_t::history_with_name(L"fish");
    
    while((opt = wgetopt_long_only( argc, argv, L"pdscvl", long_options, &opt_index )) != -1)
    {
        switch(opt)
        {
           case 'p':
                search_prefix = true;
                break;
           case 'd':
                delete_item = true;
                break;
           case 's':
                search_history = true;
                break;
           case 'c':
                break;
           case 'v':
                save_history = true;
                break;
           case 'l':
                clear_history = true;
                break; 
           case 'h':
                builtin_print_help( parser, argv[0], stdout_buffer );
                return STATUS_BUILTIN_OK;
                break;
           case EOF:
                /* Remainder are arguments */
                break;
           case '?':
                append_format(stderr_buffer, BUILTIN_ERR_UNKNOWN, argv[0], argv[woptind-1]);
                return STATUS_BUILTIN_ERROR;
                break;
           default:
                append_format(stderr_buffer, BUILTIN_ERR_UNKNOWN, argv[0], argv[woptind-1]);
                return STATUS_BUILTIN_ERROR;
        }
    }	

    /* Everything after is an argument */
    const wcstring_list_t args(argv + woptind, argv + argc);

    if (argc == 1)
    {
        wcstring full_history;
        history->get_string_representation(full_history, wcstring(L"\n"));
        stdout_buffer.append(full_history);
        stdout_buffer.push_back('\n');
        return STATUS_BUILTIN_OK;
    }

    if (search_history)
    {
        int res = STATUS_BUILTIN_ERROR;
        for (wcstring_list_t::const_iterator iter = args.begin(); iter != args.end(); ++iter)
        {
            const wcstring &search_string = *iter;
            if (search_string.empty())
            {
                append_format(stderr_buffer, BUILTIN_ERR_COMBO2, argv[0], L"Use --search with either --contains or --prefix");
                return res;
            }

            history_search_t searcher = history_search_t(*history, search_string, search_prefix?HISTORY_SEARCH_TYPE_PREFIX:HISTORY_SEARCH_TYPE_CONTAINS);
            while (searcher.go_backwards())
            {
                stdout_buffer.append(searcher.current_string());
                stdout_buffer.append(L"\n"); 
                res = STATUS_BUILTIN_OK;
            }
        }
        return res;
    }

    if (delete_item)
    {
        for (wcstring_list_t::const_iterator iter = args.begin(); iter != args.end(); ++iter)
        {
            wcstring delete_string = *iter;
            if (delete_string[0] == '"' && delete_string[delete_string.length() - 1] == '"')
                delete_string = delete_string.substr(1, delete_string.length() - 2);
           
            history->remove(delete_string);
        }
        return STATUS_BUILTIN_OK;
    }

    if (save_history)
    {
        history->save();
        return STATUS_BUILTIN_OK;
    }

    if (clear_history)
    {
        history->clear();
        history->save();
        return STATUS_BUILTIN_OK;
    }

    return STATUS_BUILTIN_ERROR;
}


/*
  END OF BUILTIN COMMANDS
  Below are functions for handling the builtin commands.
  THESE MUST BE SORTED BY NAME! Completion lookup uses binary search.
*/

/**
   Data about all the builtin commands in fish.
   Functions that are bound to builtin_generic are handled directly by the parser.
   NOTE: These must be kept in sorted order!
*/
static const builtin_data_t builtin_datas[]=
{
	{ 		L".",  &builtin_source, N_( L"Evaluate contents of file" )   },
	{ 		L"and",  &builtin_generic, N_( L"Execute command if previous command suceeded" )  },
	{ 		L"begin",  &builtin_begin, N_( L"Create a block of code" )   },
	{ 		L"bg",  &builtin_bg, N_( L"Send job to background" )   },
	{ 		L"bind",  &builtin_bind, N_( L"Handle fish key bindings" )  },
	{ 		L"block",  &builtin_block, N_( L"Temporarily block delivery of events" ) },
	{ 		L"break",  &builtin_break_continue, N_( L"Stop the innermost loop" )   },
	{ 		L"breakpoint",  &builtin_breakpoint, N_( L"Temporarily halt execution of a script and launch an interactive debug prompt" )   },
	{ 		L"builtin",  &builtin_builtin, N_( L"Run a builtin command instead of a function" ) },
	{ 		L"case",  &builtin_case, N_( L"Conditionally execute a block of commands" )   },
	{ 		L"cd",  &builtin_cd, N_( L"Change working directory" )   },
	{ 		L"command",   &builtin_generic, N_( L"Run a program instead of a function or builtin" )   },
	{ 		L"commandline",  &builtin_commandline, N_( L"Set or get the commandline" )   },
	{ 		L"complete",  &builtin_complete, N_( L"Edit command specific completions" )   },
	{ 		L"contains",  &builtin_contains, N_( L"Search for a specified string in a list" )   },
	{ 		L"continue",  &builtin_break_continue, N_( L"Skip the rest of the current lap of the innermost loop" )   },
	{ 		L"count",  &builtin_count, N_( L"Count the number of arguments" )   },
	{       L"echo",  &builtin_echo, N_( L"Print arguments" ) },
	{ 		L"else",  &builtin_else, N_( L"Evaluate block if condition is false" )   },
	{ 		L"emit",  &builtin_emit, N_( L"Emit an event" ) },
	{ 		L"end",  &builtin_end, N_( L"End a block of commands" )   },
	{ 		L"exec",  &builtin_generic, N_( L"Run command in current process" )  },
	{ 		L"exit",  &builtin_exit, N_( L"Exit the shell" ) },
	{ 		L"fg",  &builtin_fg, N_( L"Send job to foreground" )   },
	{ 		L"for",  &builtin_for, N_( L"Perform a set of commands multiple times" )   },
	{ 		L"function",  &builtin_function, N_( L"Define a new function" )   },
	{ 		L"functions",  &builtin_functions, N_( L"List or remove functions" )   },
	{ 		L"history",  &builtin_history, N_( L"History of commands executed by user" )   },
 	{ 		L"if",  &builtin_generic, N_( L"Evaluate block if condition is true" )   },
	{ 		L"jobs",  &builtin_jobs, N_( L"Print currently running jobs" )   },
	{ 		L"not",  &builtin_generic, N_( L"Negate exit status of job" )  },
	{ 		L"or",  &builtin_generic, N_( L"Execute command if previous command failed" )  },
    { 		L"pwd",  &builtin_pwd, N_( L"Print the working directory" )  },
	{ 		L"random",  &builtin_random, N_( L"Generate random number" )  },
	{ 		L"read",  &builtin_read, N_( L"Read a line of input into variables" )   },
	{ 		L"return",  &builtin_return, N_( L"Stop the currently evaluated function" )   },
	{ 		L"set",  &builtin_set, N_( L"Handle environment variables" )   },
	{ 		L"status",  &builtin_status, N_( L"Return status information about fish" )  },
	{ 		L"switch",  &builtin_switch, N_( L"Conditionally execute a block of commands" )   },
    { 		L"test",  &builtin_test, N_( L"Test a condition" )   },
	{ 		L"ulimit",  &builtin_ulimit, N_( L"Set or get the shells resource usage limits" )  },
	{ 		L"while",  &builtin_generic, N_( L"Perform a command multiple times" )   }
};

#define BUILTIN_COUNT (sizeof builtin_datas / sizeof *builtin_datas)

static const builtin_data_t *builtin_lookup(const wcstring &name) {
    const builtin_data_t *array_end = builtin_datas + BUILTIN_COUNT;
    const builtin_data_t *found = std::lower_bound(builtin_datas, array_end, name);
    if (found != array_end && name == found->name) {
        return found;
    } else {
        return NULL;
    }
}

void builtin_init()
{
	
	wopterr = 0;
	for( size_t i=0; i < BUILTIN_COUNT; i++ )
	{
		intern_static( builtin_datas[i].name );
	}
}

void builtin_destroy()
{
}

int builtin_exists( const wcstring &cmd )
{
	return !!builtin_lookup(cmd);
}

/**
   Return true if the specified builtin should handle it's own help,
   false otherwise.
*/
static int internal_help( const wchar_t *cmd )
{
	CHECK( cmd, 0 );
	return contains( cmd, L"for", L"while", L"function",
			 L"if", L"end", L"switch", L"case", L"count" );
}


int builtin_run( parser_t &parser, const wchar_t * const *argv, const io_chain_t &io )
{
	int (*cmd)(parser_t &parser, const wchar_t * const *argv)=0;
	real_io = &io;
		
	CHECK( argv, STATUS_BUILTIN_ERROR );
	CHECK( argv[0], STATUS_BUILTIN_ERROR );
    
    const builtin_data_t *data = builtin_lookup(argv[0]);
	cmd = (int (*)(parser_t &parser, const wchar_t * const*))(data ? data->func : NULL);
	
	if( argv[1] != 0 && !internal_help(argv[0]) )
	{
		if( argv[2] == 0 && (parser.is_help( argv[1], 0 ) ) )
		{
			builtin_print_help( parser, argv[0], stdout_buffer );
			return STATUS_BUILTIN_OK;
		}
	}

	if( data != NULL )
	{
		int status;

		status = cmd(parser, argv);
		return status;

	}
	else
	{
		debug( 0, _( L"Unknown builtin '%ls'" ), argv[0] );
	}
	return STATUS_BUILTIN_ERROR;
}


wcstring_list_t builtin_get_names(void)
{
    wcstring_list_t result;
    result.reserve(BUILTIN_COUNT);
    for (size_t i=0; i < BUILTIN_COUNT; i++) {
        result.push_back(builtin_datas[i].name);
    }
    return result;
}

void builtin_get_names(std::vector<completion_t> &list) {
	for (size_t i=0; i < BUILTIN_COUNT; i++) {
		list.push_back(completion_t(builtin_datas[i].name));
	}
}

wcstring builtin_get_desc( const wcstring &name )
{
    wcstring result;
	const builtin_data_t *builtin = builtin_lookup(name);
    if (builtin) {
        result = _(builtin->desc);
    }
    return result;
}

void builtin_push_io( parser_t &parser, int in )
{
    ASSERT_IS_MAIN_THREAD();
	if( builtin_stdin != -1 )
	{
        struct io_stack_elem_t elem = {builtin_stdin, stdout_buffer, stderr_buffer};
        io_stack.push(elem);
	}
	builtin_stdin = in;
    stdout_buffer.clear();
    stderr_buffer.clear();
}

void builtin_pop_io(parser_t &parser)
{
    ASSERT_IS_MAIN_THREAD();
	builtin_stdin = 0;
	if( ! io_stack.empty() )
	{
        struct io_stack_elem_t &elem = io_stack.top();
		stderr_buffer = elem.err;
		stdout_buffer = elem.out;
		builtin_stdin = elem.in;
        io_stack.pop();
	}
	else
	{
        stdout_buffer.clear();
        stderr_buffer.clear();
		builtin_stdin = 0;
	}
}

