/** \file input.c

Functions for reading a character of input from stdin, using the
inputrc information for key bindings.

The inputrc file format was invented for the readline library. The
implementation in fish is as of yet incomplete.

*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <unistd.h>
#include <wchar.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif


#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <signal.h>
#include <dirent.h>
#include <wctype.h>



#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "reader.h"
#include "proc.h"
#include "common.h"
#include "sanity.h"
#include "input_common.h"
#include "input.h"
#include "parser.h"
#include "env.h"
#include "expand.h"
#include "event.h"
#include "signal.h"

#include "output.h"
#include "intern.h"
#include "halloc.h"
#include "halloc_util.h"

/**
   Add a new terminfo mapping
*/
#define TERMINFO_ADD(key)						\
	{								\
		terminfo_mapping_t *m = halloc( terminfo_mappings, sizeof( terminfo_mapping_t ) ); \
		m->name = halloc_wcsdup( terminfo_mappings, (L ## #key)+4 ); \
		m->seq = key;						\
		al_push( terminfo_mappings, m );			\
	}


/**
   Struct representing a keybinding. Returned by input_get_mappings.
 */
typedef struct
{
	const wchar_t *seq; /**< Character sequence which generates this event */
	const wchar_t *command; /**< command that should be evaluated by this mapping */
		
}
	input_mapping_t;

/**
   A struct representing the mapping from a terminfo key name to a terminfo character sequence
 */
typedef struct
{
	const wchar_t *name; /**< Name of key */
	const char *seq; /**< Character sequence generated on keypress */
		
}
	terminfo_mapping_t;


/**
   Names of all the readline functions supported
*/
static const wchar_t *name_arr[] = 
{
	L"beginning-of-line",
	L"end-of-line",
	L"forward-char",
	L"backward-char",
	L"forward-word",
	L"backward-word",
	L"history-search-backward",
	L"history-search-forward",
	L"delete-char",
	L"backward-delete-char",
	L"kill-line",
	L"yank",
	L"yank-pop",
	L"complete",
	L"beginning-of-history",
	L"end-of-history",
	L"delete-line",
	L"backward-kill-line",
	L"kill-whole-line",
	L"kill-word",
	L"backward-kill-word",
	L"dump-functions",
	L"winch",
	L"exit",
	L"history-token-search-backward",
	L"history-token-search-forward",
	L"self-insert",
	L"null",
	L"eof",
	L"vi-arg-digit",
	L"execute",
	L"beginning-of-buffer",
	L"end-of-buffer",
	L"repaint",
	L"up-line",
	L"down-line"
}
	;

/**
   Description of each supported readline function
*/
/*
static const wchar_t *desc_arr[] =
{
	L"Move to beginning of line",
	L"Move to end of line",
	L"Move forward one character",
	L"Move backward one character",
	L"Move forward one word",
	L"Move backward one word",
	L"Search backward through list of previous commands",
	L"Search forward through list of previous commands",
	L"Delete one character forward",
	L"Delete one character backward",
	L"Move contents from cursor to end of line to killring",
	L"Paste contents of killring",
	L"Rotate to previous killring entry",
	L"Guess the rest of the next input token",
	L"Move to first item of history",
	L"Move to last item of history",
	L"Clear current line",
	L"Move contents from beginning of line to cursor to killring",
	L"Move entire line to killring",
	L"Move next word to killring",
	L"Move previous word to killring",
	L"Write out keybindings",
	L"Clear entire screen",
	L"Quit the running program",
	L"Search backward through list of previous commands for matching token",
	L"Search forward through list of previous commands for matching token",
	L"Insert the pressed key",
	L"Do nothing",
	L"End of file",
	L"Repeat command"
}
	;
*/

/**
   Internal code for each supported readline function
*/
static const wchar_t code_arr[] = 
{
	R_BEGINNING_OF_LINE,
	R_END_OF_LINE,
	R_FORWARD_CHAR,
	R_BACKWARD_CHAR,
	R_FORWARD_WORD,
	R_BACKWARD_WORD,
	R_HISTORY_SEARCH_BACKWARD,
	R_HISTORY_SEARCH_FORWARD,
	R_DELETE_CHAR,
	R_BACKWARD_DELETE_CHAR,
	R_KILL_LINE,
	R_YANK,
	R_YANK_POP,
	R_COMPLETE,
	R_BEGINNING_OF_HISTORY,
	R_END_OF_HISTORY,
	R_DELETE_LINE,
	R_BACKWARD_KILL_LINE,
	R_KILL_WHOLE_LINE,
	R_KILL_WORD,
	R_BACKWARD_KILL_WORD,
	R_DUMP_FUNCTIONS,
	R_WINCH,
	R_EXIT,
	R_HISTORY_TOKEN_SEARCH_BACKWARD,
	R_HISTORY_TOKEN_SEARCH_FORWARD,
	R_SELF_INSERT,
	R_NULL,
	R_EOF,
	R_VI_ARG_DIGIT,
	R_EXECUTE,
	R_BEGINNING_OF_BUFFER,
	R_END_OF_BUFFER,
	R_REPAINT,
	R_UP_LINE,
	R_DOWN_LINE
}
	;


/**
   Mappings for the current input mode
*/
static array_list_t mappings = {0,0,0}; 

static array_list_t *terminfo_mappings = 0;


/**
   Set to one when the input subsytem has been initialized. 
*/
static int is_init = 0;

static void input_terminfo_init();
static void input_terminfo_destroy();




/**
   Returns the function name for the given function code.
*/
/*
static const wchar_t *input_get_name( wchar_t c )
{

	int i;
	for( i = 0; i<(sizeof( code_arr )/sizeof(wchar_t)) ; i++ )
	{
		if( c == code_arr[i] )
		{
			return name_arr[i];
		}
	}
	return 0;		
}
*/
/**
   Returns the function description for the given function code.
*/
/*
static const wchar_t *input_get_desc( wchar_t c )
{

	int i;
	for( i = 0; i<(sizeof( code_arr )/sizeof(wchar_t)) ; i++ )
	{
		if( c == code_arr[i] )
		{
			return desc_arr[i];
		}
	}
	return 0;		
}
*/


void input_mapping_add( const wchar_t *sequence,
			const wchar_t *command )
{
	int i;

	CHECK( sequence, );
	CHECK( command, );
	
	//	debug( 0, L"Add mapping from %ls to %ls", escape(sequence, 1), escape(command, 1 ) );
	

	for( i=0; i<al_get_count( &mappings); i++ )
	{
		input_mapping_t *m = (input_mapping_t *)al_get( &mappings, i );
		if( wcscmp( m->seq, sequence ) == 0 )
		{
			m->command = intern(command);
			return;
		}
	}
	
	input_mapping_t *m = malloc( sizeof( input_mapping_t ) );
	m->seq = intern( sequence );
	m->command = intern(command);
	al_push( &mappings, m );	
	
}

/**
   Handle interruptions to key reading by reaping finshed jobs and
   propagating the interrupt to the reader.
*/
static int interrupt_handler()
{
	/*
	  Fire any pending events
	*/
	event_fire( 0 );	
	
	/*
	  Reap stray processes, including printing exit status messages
	*/
	if( job_reap( 1 ) )
		repaint();
	
	/*
	  Check if we should exit
	*/
	if( exit_status() )
	{
		return R_EXIT;
	}
	
	/*
	  Tell the reader an event occured
	*/
	if( reader_interrupted() )
	{
		/*
		  Return 3, i.e. the character read by a Control-C.
		*/
		return 3;
	}

	return R_WINCH;	
}

int input_init()
{
	if( is_init )
		return 1;
	
	is_init = 1;

	input_common_init( &interrupt_handler );

	if( setupterm( 0, STDOUT_FILENO, 0) == ERR )
	{
		debug( 0, _( L"Could not set up terminal" ) );
		exit(1);
	}
	output_set_term( env_get( L"TERM" ) );
	
	input_terminfo_init();
	

	return 1;
}

void input_destroy()
{
	if( !is_init )
		return;
	

	is_init=0;
	
	al_foreach( &mappings, &free );		
	al_destroy( &mappings );

	input_common_destroy();
	
	if( del_curterm( cur_term ) == ERR )
	{
		debug( 0, _(L"Error while closing terminfo") );
	}

	input_terminfo_destroy();
	
}

/**
   Perform the action of the specified binding
*/
static wint_t input_exec_binding( input_mapping_t *m, const wchar_t *seq )
{
	wchar_t code = input_function_get_code( m->command );
	if( code != -1 )
	{				
		switch( code )
		{

			case R_SELF_INSERT:
			{
				return seq[0];
			}
			
			default:
			{
				return code;
			}
			
		}
	}
	else
	{
		
		/*
		  This key sequence is bound to a command, which
		  is sent to the parser for evaluation.
		*/
		
		eval( m->command, 0, TOP );
		
		/*
		  We still need to return something to the caller, R_NULL
		  tells the reader that no key press needs to be handled, 
		  and no repaint is needed.

		  Bindings that produce output should emit a R_REPAINT
		  function by calling 'commandline -f repaint' to tell
		  fish that a repaint is in order.
		*/
		
		return R_NULL;

	}
}



/**
   Try reading the specified function mapping
*/
static wint_t input_try_mapping( input_mapping_t *m)
{
	int j, k;
	wint_t c=0;

	/*
	  Check if the actual function code of this mapping is on the stack
	 */
	c = input_common_readch( 0 );
	if( c == input_function_get_code( m->command ) )
	{
		return input_exec_binding( m, m->seq );
	}
	input_unreadch( c );
	
	if( m->seq != 0 )
	{

		for( j=0; m->seq[j] != L'\0' && 
			     m->seq[j] == (c=input_common_readch( j>0 )); j++ )
			;
		
		if( m->seq[j] == L'\0' )
		{
			return input_exec_binding( m, m->seq );
		}
		else
		{
			/*
			  Return the read characters
			*/
			input_unreadch(c);
			for( k=j-1; k>=0; k-- )
			{
				input_unreadch( m->seq[k] );
			}
		}
	}
	return 0;
		
}

void input_unreadch( wint_t ch )
{
	input_common_unreadch( ch );
}

wint_t input_readch()
{
	
	int i;

	CHECK_BLOCK( R_NULL );
	
	/*
	  Clear the interrupted flag
	*/
	reader_interrupted();
	
	/*
	  Search for sequence in various mapping tables
	*/
	
	while( 1 )
	{
		for( i=0; i<al_get_count( &mappings); i++ )
		{
			wint_t res = input_try_mapping( (input_mapping_t *)al_get( &mappings, i ));		
			if( res )
				return res;		
		}
		
		/*
		  No matching exact mapping, try to find generic mapping.
		*/
		
		for( i=0; i<al_get_count( &mappings); i++ )
		{
			input_mapping_t *m = (input_mapping_t *)al_get( &mappings, i );
			if( wcslen( m->seq) == 0 )
			{	
				wchar_t arr[2]=
					{
						0, 
						0
					}
				;
				arr[0] = input_common_readch(0);
				
				return input_exec_binding( m, arr );				
			}
		}
		
		/*
		  No action to take on specified character, ignoer it
		  and move to next one.
		 */
		input_common_readch( 0 );

	}	
}

void input_mapping_get_names( array_list_t *list )
{
	int i;
	
	for( i=0; i<al_get_count( &mappings ); i++ )
	{
		input_mapping_t *m = (input_mapping_t *)al_get( &mappings, i );
		al_push( list, m->seq );
	}
	
}


int input_mapping_erase( const wchar_t *sequence )
{
	int ok = 0;
	int i;
	size_t sz = al_get_count( &mappings );
	
	for( i=0; i<sz; i++ )
	{
		input_mapping_t *m = (input_mapping_t *)al_get( &mappings, i );
		if( !wcscmp( sequence, m->seq ) )
		{
			if( i != (sz-1 ) )
			{
				al_set( &mappings, i, al_get( &mappings, sz -1 ) );
			}
			al_truncate( &mappings, sz-1 );
			ok = 1;
			
			free( m );
			
			break;
			
		}
		
	}

	return ok;
	
}

const wchar_t *input_mapping_get( const wchar_t *sequence )
{
	int i;
	size_t sz = al_get_count( &mappings );
	
	for( i=0; i<sz; i++ )
	{
		input_mapping_t *m = (input_mapping_t *)al_get( &mappings, i );
		if( !wcscmp( sequence, m->seq ) )
		{
			return m->command;
		}
	}
	return 0;
}

/**
   Add all terminfo mappings
 */
static void input_terminfo_init()
{
	terminfo_mappings = al_halloc( 0 );
	
	TERMINFO_ADD( key_down );
	TERMINFO_ADD( key_up );
	TERMINFO_ADD( key_left );
	TERMINFO_ADD( key_right );
	TERMINFO_ADD( key_dc );
	TERMINFO_ADD( key_backspace );
	TERMINFO_ADD( key_home );
	TERMINFO_ADD( key_end );
	TERMINFO_ADD( key_ppage );
	TERMINFO_ADD( key_npage );
	TERMINFO_ADD( key_clear );
	TERMINFO_ADD( key_close );
	TERMINFO_ADD( key_command );
	TERMINFO_ADD( key_copy );
	TERMINFO_ADD( key_create );
	TERMINFO_ADD( key_dl );
	TERMINFO_ADD( key_enter );
	TERMINFO_ADD( key_undo );
	TERMINFO_ADD( key_suspend );
	TERMINFO_ADD( key_cancel );
}

static void input_terminfo_destroy()
{
	
	if( terminfo_mappings )
	{
		halloc_free( terminfo_mappings );
	}
}

const wchar_t *input_terminfo_get_sequence( const wchar_t *name )
{
	const char *res = 0;
	int i;	
	static string_buffer_t *buff = 0;
	int err = ENOENT;
	
	CHECK( name, 0 );
	input_init();
	
	for( i=0; i<al_get_count( terminfo_mappings ); i++ )
	{
		terminfo_mapping_t *m = (terminfo_mapping_t *)al_get( terminfo_mappings, i );
		
		if( !wcscmp( name, m->name ) )
		{
			res = m->seq;
			err = EILSEQ;
			break;
		}
	}
	
	if( !res )
	{
		errno = err;
		return 0;
	}
	
	if( !buff )
	{
		buff = sb_halloc( global_context );
	}
	
	sb_clear( buff );
	sb_printf( buff, L"%s", res );

	return (wchar_t *)buff->buff;
		
}

const wchar_t *input_terminfo_get_name( const wchar_t *seq )
{
	int i;	
	static string_buffer_t *buff = 0;

	CHECK( seq, 0 );
	input_init();
		
	if( !buff )
	{
		buff = sb_halloc( global_context );
	}
	
	for( i=0; i<al_get_count( terminfo_mappings ); i++ )
	{
		terminfo_mapping_t *m = (terminfo_mapping_t *)al_get( terminfo_mappings, i );
		
		if( !m->seq )
		{
			continue;
		}
		
		sb_clear( buff );
		sb_printf( buff, L"%s", m->seq );
		
		if( !wcscmp( seq, (wchar_t *)buff->buff ) )
		{
			return m->name;
		}
	}
	
	return 0;
	
}

void input_terminfo_get_names( array_list_t *lst, int skip_null )
{
	int i;	

	CHECK( lst, );
	input_init();
		
	for( i=0; i<al_get_count( terminfo_mappings ); i++ )
	{
		terminfo_mapping_t *m = (terminfo_mapping_t *)al_get( terminfo_mappings, i );
		
		if( skip_null && !m->seq )
		{
			continue;
		}
		al_push( lst, m->name );
	}
}

void input_function_get_names( array_list_t *lst )
{
	int i;	

	CHECK( lst, );
		
	for( i=0; i<(sizeof(name_arr)/sizeof(wchar_t *)); i++ )
	{
		al_push( lst, name_arr[i] );
	}
}

wchar_t input_function_get_code( const wchar_t *name )
{

	int i;
	for( i = 0; i<(sizeof( code_arr )/sizeof(wchar_t)) ; i++ )
	{
		if( wcscmp( name, name_arr[i] ) == 0 )
		{
			return code_arr[i];
		}
	}
	return -1;		
}

