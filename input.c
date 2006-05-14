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
#include <sys/ioctl.h>
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
#include "translate.h"
#include "output.h"

static void input_read_inputrc( wchar_t *fn );

/**
   Array containing characters which have been peeked by the escape
   sequence matching functions and returned
*/

typedef struct
{
	wchar_t *seq; /**< Character sequence which generates this event */
	wchar_t *seq_desc; /**< Description of the character sequence suitable for printing on-screen */
	wchar_t *command; /**< command that should be evaluated by this mapping */
		
}
	mapping;

/**
   Symbolic names for some acces-modifiers used when parsing symbolic sequences
*/
#define CTRL_SYMBOL L"Control-"
/**
   Symbolic names for some acces-modifiers used when parsing symbolic sequences
*/
#define META_SYMBOL L"Meta-"

/**
   Names of all the readline functions supported
*/
const wchar_t *name_arr[] = 
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
	L"clear-screen",
	L"exit",
	L"history-token-search-backward",
	L"history-token-search-forward",
	L"self-insert",
	L"null",
	L"eof"
}
	;

/**
   Description of each supported readline function
*/
const wchar_t *desc_arr[] =
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
	L"End of file"
}
	;

/**
   Internal code for each supported readline function
*/
const wchar_t code_arr[] = 
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
	R_CLEAR_SCREEN,
	R_EXIT,
	R_HISTORY_TOKEN_SEARCH_BACKWARD,
	R_HISTORY_TOKEN_SEARCH_FORWARD,
	R_SELF_INSERT,
	R_NULL,
	R_EOF
}
	;


/**
   List of all key bindings, as mappings from one sequence to either a character or a command
*/
static hash_table_t all_mappings;

/**
   Mappings for the current input mode
*/
static array_list_t *current_mode_mappings; 
/**
   Mappings for the current application
*/
static array_list_t *current_application_mappings;
/**
   Global mappings
*/
static array_list_t *global_mappings;

/**
   Number of nested conditional statement levels that are not evaluated
*/
static int inputrc_skip_block_count=0;
/**
   Number of nested conditional statements that have evaluated to true
*/
static int inputrc_block_count=0;

/**
   True if syntax errors were found in the inputrc file
*/
static int inputrc_error = 0;

static int is_init = 0;

wchar_t input_get_code( wchar_t *name )
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
void input_set_mode( wchar_t *name )
{
	current_mode_mappings = (array_list_t *)hash_get( &all_mappings, name );	
}

void input_set_application( wchar_t *name )
{
	current_application_mappings = (array_list_t *)hash_get( &all_mappings, name );	
}

/**
   Get the mapping with the specified name
*/
static array_list_t *get_mapping( const wchar_t *mode )
{

	array_list_t * mappings = (array_list_t *)hash_get( &all_mappings, mode );
	
	if( !mappings )
	{
		mappings = malloc( sizeof( array_list_t ));
		al_init( mappings );
		
		hash_put( &all_mappings, wcsdup(mode), mappings );
		
	}
	return mappings;
	
}


void add_mapping( const wchar_t *mode,
				  const wchar_t *s,
				  const wchar_t *d, 
				  const wchar_t *c )
{
	int i;

	array_list_t *mappings;
		
	if( s == 0 )
		return;
	
	if( mode == 0 )
		return;
	
	mappings = get_mapping( mode );
		
	for( i=0; i<al_get_count( mappings); i++ )
	{
		mapping *m = (mapping *)al_get( mappings, i );
		if( wcscmp( m->seq, s ) == 0 )
		{
			free( m->command );
			free( m->seq_desc );
			m->seq_desc = wcsdup(d );
			m->command=wcsdup(c);
			return;
		}
	}
	
	mapping *m = malloc( sizeof( mapping ) );
	m->seq = wcsdup( s );
	m->seq_desc = wcsdup(d );
	m->command=wcsdup(c);
	al_push( mappings, m );	
}

/**
   Compare sort order for two keyboard mappings. This function is made
   to be suitable for use with the qsort method. 
*/
/*
static int mapping_compare( const void *a, const void *b )
{
	mapping *c = *(mapping **)a;
	mapping *d = *(mapping **)b;

//	fwprintf( stderr, L"%ls %ls\n", c->seq_desc, d->seq_desc );

	return wcscmp( c->seq_desc, d->seq_desc );
	
}
*/


/**
   Print a listing of all keybindings and a description of each
   function. This is used by the dump-functions readline function.
*/
static void dump_functions()
{
/*	int i;
  fwprintf( stdout, L"\n" );

  qsort(current_mappings.arr, 
  al_get_count( &mappings), 
  sizeof( void*),
  &mapping_compare );
	
  for( i=0; i<al_get_count( &mappings ); i++ )
  {
  mapping *m = (mapping *)al_get( &mappings, i );

//			write_sequence( m->seq );
fwprintf( stdout, 
L"%ls: %ls\n", 
m->seq_desc, 
m->command );
}
repaint();*/
}


/**
   Parse special character from the specified inputrc-style key binding. 

   Control-a is expanded to 1, etc.
*/

static wchar_t *input_symbolic_sequence( const wchar_t *in )
{
	wchar_t *res=0;

	if( !*in || *in == L'\n' )
		return 0;
	
	debug( 4, L"Try to parse symbolic sequence %ls", in );

	if( wcsncmp( in, CTRL_SYMBOL, wcslen(CTRL_SYMBOL) ) == 0 )
	{
		int has_meta=0;
		
		in += wcslen(CTRL_SYMBOL);

		/*
		  Control-Meta- Should be rearranged to Meta-Control, this
		  special-case must be handled manually.
		*/
		if( wcsncmp( in, META_SYMBOL, wcslen(META_SYMBOL) ) == 0 )
		{
			in += wcslen(META_SYMBOL);
			has_meta=1;
		}
		
		wchar_t c = towlower( *in );
		in++;		
		if( c < L'a' || c > L'z' )
		{
			debug( 1, _( L"Invalid Control sequence" ) );
			return 0;			
		}
		if( has_meta )
		{
			res = wcsdup( L"\ea" );
			res[1]=1+c-L'a';
		}
		else
		{
			res = wcsdup( L"a" );
			res[0]=1+c-L'a';
		}		
		debug( 4, L"Got control sequence %d", res[0] );

	}
	else if( wcsncmp( in, META_SYMBOL, wcslen(META_SYMBOL) ) == 0 )
	{
		in += wcslen(META_SYMBOL);
		res = wcsdup( L"\e" );		
		debug( 4, L"Got meta" );
	}
	else 
	{
		int i;
		struct 
		{
			wchar_t *in;
			char *out;
		}
		map[]=
		{
			{
				L"rubout",
				key_backspace
			}
			,
			{
				L"del",
				key_dc
			}
			,
			{
				L"esc",
				"\e"
			}
			,
			{
				L"lfd",
				"\r"
			}
			,
			{
				L"newline",
				"\n"
			}
			,
			{
				L"ret",
				"\n"
			}
			,
			{
				L"return",
				"\n"
			}
			,
			{
				L"spc",
				" "
			}
			,
			{
				L"space",
				" "
			}
			,
			{
				L"tab",
				"\t"
			}
			,
			{
				0,
				0
			}
		}
		;
		
		for( i=0; map[i].in; i++ )
		{
			if( wcsncasecmp( in, map[i].in, wcslen(map[i].in) )==0 )
			{
				in+= wcslen( map[i].in );
				res = str2wcs( map[i].out );
				
				break;
			}
		}

		if( !res )
		{
			if( iswalnum( *in ) || iswpunct( *in ) )
			{
				res = wcsdup( L"a" );
				*res = *in++;
				debug( 4, L"Got character %lc", *res );
			}	
		}
	}
	if( !res )
	{
		debug( 1, _( L"Could not parse sequence '%ls'" ), in );
		return 0;
	}
	if( !*in || *in == L'\n')
	{
		debug( 4, L"Finished parsing sequence" );
		return res;
	}
	
	wchar_t *res2 = input_symbolic_sequence( in );
	if( !res2 )
	{
		free( res );
		return 0;
	}
	wchar_t *res3 = wcsdupcat( res, res2 );
	free( res);
	free(res2);
	
	return res3;
}

/**
   Unescape special character from the specified inputrc-style key sequence. 

   \\C-a is expanded to 1, etc.
*/
static wchar_t *input_expand_sequence( const wchar_t *in )
{
	const wchar_t *in_orig=in;
	wchar_t *res = malloc( sizeof( wchar_t)*(4*wcslen(in)+1));
	wchar_t *out=res;
	int error = 0;
	
	while( *in && !error)
	{
		switch( *in )
		{
			case L'\\':
			{
				in++;
				switch( *in )
				{
					case L'\0':
						error = 1;
						break;
												
					case L'e':
						*(out++)=L'\e';
						break;
						
					case L'\\':
					case L'\"':
					case L'\'':
						*(out++)=*in;
						break;

					case L'b':
						*(out++)=L'\b';
						break;
						
					case L'd':
					{
						wchar_t *str = str2wcs( key_dc );
						wchar_t *p=str;
						if( p )
						{
							while( *p )
							{
								*(out++)=*(p++);
							}
							free( str );
						}
						break;
					}
					
					case L'f':
						*(out++)=L'\f';
						break;
						
					case L'n':
						*(out++)=L'\n';
						break;
						
					case L'r':
						*(out++)=L'\r';
						break;
						
					case L't':
						*(out++)=L'\t';
						break;
						
					case L'v':
						*(out++)=L'\v';
						break;

						/*
						  Parse numeric backslash escape
						*/
					case L'u':
					case L'U':
					case L'x':
					case L'o':
					{
						int i;
						wchar_t res=0;
						int chars=2;
						int base=16;
					
						switch( *in++ )
						{
							case L'u':
								base=16;
								chars=4;
								break;
							
							case L'U':
								base=16;
								chars=8;
								break;
							
							case L'x':
								base=16;
								chars=2;
								break;
							
							case L'o':
								base=8;
								chars=3;
								break;
								
						}
						
						for( i=0; i<chars; i++ )
						{
							int d = convert_digit( *in++, base);
							if( d < 0 )
							{
								break;
							}
							
							res=(res*base)|d;
							
						}
						in--;
						
						debug( 4, 
							   L"Got numeric key sequence %d", 
							   res );
						
						*(out++) = res;
						break;
					}

					/*
					  Parse control sequence
					*/
					case L'C':
					{
						int has_escape = 0;
						
						in++;
						/* Make sure next key is a dash*/
						if( *in != L'-' )
						{
							error=1;
							debug( 1, _( L"Invalid sequence - no dash after control\n" ) );
							break;
						}
						in++;

						if( (*in == L'\\') && (*(in+1)==L'e') )
						{
							has_escape = 1;
							in += 2;
							
						}
						
						
						if( (*in >= L'a') && 
							(*in < L'a'+32) )
						{
							if( has_escape )
								*(out++)=L'\e';							
							*(out++)=*in-L'a'+1;
							break;
						}
						
						if( (*in >= L'A') && 
							(*in < L'A'+32) )
						{
							if( has_escape )
								*(out++)=L'\e';							
							*(out++)=*in-L'A'+1;
							break;
						}
						debug( 1, _( L"Invalid sequence - Control-nothing?\n" ) );
						error = 1;
												
						break;
					}
					
					/*
					  Parse meta sequence
					*/
					case L'M':
					{
						in++;
						if( *in != L'-' )
						{
							error=1;
							debug( 1, _( L"Invalid sequence - no dash after meta\n" ) );
							break;
						}
						if( !*(in+1) )
						{
							debug( 1, _( L"Invalid sequence - Meta-nothing?" ) );
							error=1;
							break;
						}
						*(out++)=L'\e';
						
						break;
					}
					
					default:
					{
						*(out++)=*in;
						break;
					}
					
				}
				
				break;
			}
			default:
			{
				*(out++)=*in;
				break;
			}
		}
		in++;
	}


	
	if( error )
	{
		free( res);
		res=0;
	}
	else
	{
//		fwprintf( stderr, L"%ls translated ok\n", in_orig );
		*out = L'\0';
	}
	
	if( !error )
	{
		if( wcslen( res ) == 0 )
		{
			debug( 1, _( L"Invalid sequence - '%ls' expanded to zero characters" ), in_orig );
			error =1;
			res = 0;
		}
	}	

	return res;
}


void input_parse_inputrc_line( wchar_t *cmd )
{
	wchar_t *p=cmd;
	
	/* Make all whitespace into space characters */
	while( *p )
	{
		if( *p == L'\t' || *p == L'\r' )
			*p=L' ';
		p++;
	}

	/* Remove spaces at beginning/end */
	while( *cmd == L' ' )
		cmd++;
	
	p = cmd + wcslen(cmd)-1;
	while( (p >= cmd) && (*p == L' ') )
	{
		*p=L'\0';
		p--;
	}
		
	/* Skip comments */
	if( *cmd == L'#' )
		return;

	/* Skip empty lines */
	if( *cmd == L'\0' )
		return;
	
	if( wcscmp( L"$endif", cmd) == 0 )
	{
		if( inputrc_skip_block_count )
		{
			inputrc_skip_block_count--;
/*
  if( !inputrc_skip_block_count )
  fwprintf( stderr, L"Stop skipping\n" );
  else
  fwprintf( stderr, L"Decrease skipping\n" );
*/
		}
		else
		{
			if( inputrc_block_count )
			{
				inputrc_block_count--;
//				fwprintf( stderr, L"End of active block\n" );
			}
			else
			{
				inputrc_error = 1;
				debug( 1, 
					   _( L"Mismatched $endif in inputrc file" ) );
			}
		}					
		return;
	}

	if( wcscmp( L"$else", cmd) == 0 )
	{
		if( inputrc_skip_block_count )
		{
			if( inputrc_skip_block_count == 1 )
			{
				inputrc_skip_block_count--;
				inputrc_block_count++;
			}
			
		}
		else
		{
			inputrc_skip_block_count++;
			inputrc_block_count--;
		}
		
		return;
	}
	
	if( inputrc_skip_block_count )
	{
		if( wcsncmp( L"$if ", cmd, wcslen( L"$if " )) == 0 )
			inputrc_skip_block_count++;
//		fwprintf( stderr, L"Skip %ls\n", cmd );
		
		return;
	}
	
	if( *cmd == L'\"' )
	{

		wchar_t *key;
		wchar_t *val;
		wchar_t *sequence;
		wchar_t prev=0;
		

		cmd++;
		key=cmd;
		
		for( prev=0; ;prev=*cmd,cmd++ )
		{
			if( !*cmd )
			{
				debug( 1, 
					   _( L"Mismatched quote" ) );
				inputrc_error = 1;
				return;
			}
			
			if(( *cmd == L'\"' ) && prev != L'\\' )
				break;
			
		}
		*cmd=0;
		cmd++;
		if( *cmd != L':' )
		{
			debug( 1, 
				   _( L"Expected a \':\'" ) );
			inputrc_error = 1;
			return;
		}
		cmd++;
		while( *cmd == L' ' )
			cmd++;
		
		val = cmd;
		
		sequence = input_expand_sequence( key );
		if( sequence )
		{
			add_mapping( L"global", sequence, key, val );
			free( sequence );
		}
		
		return;	
	}
	else if( wcsncmp( L"$include ", cmd, wcslen(L"$include ") ) == 0 )
	{
		wchar_t *tmp;
		
		cmd += wcslen( L"$include ");
		while( *cmd == L' ' )
			cmd++;		
		tmp=wcsdup(cmd);
		tmp = expand_tilde(tmp);
		if( tmp )
			input_read_inputrc( tmp );			
		free(tmp);		
		return;
	}
	else if( wcsncmp( L"set", cmd, wcslen( L"set" ) ) == 0 )
    {
		wchar_t *set, *key, *value, *end;
		wchar_t *state;
		
		set = wcstok( cmd, L" \t", &state ); 
		key =   wcstok( 0, L" \t", &state );
		value = wcstok( 0, L" \t", &state );
		end =   wcstok( 0, L" \t", &state );

		if( wcscmp( set, L"set" ) != 0 )
		{
			debug( 1, _( L"I don\'t know what '%ls' means" ), set );	
		}
		else if( end )
		{
			debug( 1, _( L"Expected end of line, got '%ls'" ), end );
			
		}
		else if( (!key) || (!value) )
		{
			debug( 1, _( L"Syntax: set KEY VALUE" ) );
		}
		else 
		{
			if( wcscmp( key, L"editing-mode" ) == 0 )
			{
//				current_mode_mappings = get_mapping( value );
			}
		}
		
		return;
    }
	else if( wcsncmp( L"$if ", cmd, wcslen( L"$if " )) == 0 )
	{
		wchar_t *term_line = wcsdupcat( L"term=", env_get( L"TERM" ) );
		wchar_t *term_line2 = wcsdup( term_line );
		wchar_t *mode_line = L"mode=emacs";
		wchar_t *app_line = L"fish";
		
		wchar_t *term_line2_end = wcschr( term_line2, L'-' );
		if( term_line2_end )
			*term_line2_end=0;
		
		
		cmd += wcslen( L"$if ");
		while( *cmd == L' ' )
			cmd++;		
		
		if( (wcscmp( cmd, app_line )==0) || 
			(wcscmp( cmd, term_line )==0) || 
			(wcscmp( cmd, mode_line )==0) )
		{
//			fwprintf( stderr, L"Conditional %ls is true\n", cmd );
			inputrc_block_count++;
		}
		else
		{
//			fwprintf( stderr, L"Conditional %ls is false\n", cmd );
			inputrc_skip_block_count++;
		}
		free( term_line );
		free( term_line2 );
		
		return;		
	}
	else 
	{
		/*
		  This is a redular key binding, like 

		  Control-o: kill-word

		  Or at least we hope it is, since if it isn't, we have no idea what it is.
		*/
		
		wchar_t *key;
		wchar_t *val;
		wchar_t *sequence;
		
		key=cmd;
		
		cmd = wcschr( cmd, ':' );
		
		if( !cmd )
		{
				debug( 1, 
					   _( L"Unable to parse key binding" ) );
				inputrc_error = 1;
				return;
		}
		*cmd = 0;

		cmd++;
		
		while( *cmd == L' ' )
			cmd++;
		
		val = cmd;
		
		debug( 3, L"Map %ls to %ls\n", key, val );

		sequence = input_symbolic_sequence( key );
		if( sequence )
		{
			add_mapping( L"global", sequence, key, val );
			free( sequence );
		}
				
		return;	
		
	}
	
	debug( 1, _( L"I don\'t know what %ls means" ), cmd );	
}

/**
   Read the specified inputrc file
*/
static void input_read_inputrc( wchar_t *fn )
{
	FILE *rc;
	wchar_t *buff=0;
	int buff_len=0;
	int error=0;
//	fwprintf( stderr, L"read %ls\n", fn );
		
	signal_block();
	rc = wfopen( fn, "r" );
	
	if( rc )
	{	
		while( !feof( rc ) && (!error))
		{
			switch( fgetws2( &buff, &buff_len, rc ) )
			{
				case -1:
				{
					debug( 1, 
						   _( L"Error while reading input information from file '%ls'" ),
						   fn );
						
					wperror( L"fgetws2 (read_ni)" );
					error=1;
					break;
				}
					
				default:
				{
					input_parse_inputrc_line( buff );
					
					if( inputrc_error )
					{
						fwprintf( stderr, L"%ls\n", buff );
						error=1;
					}
				}
			}
		}
		free( buff );
		fclose( rc );
	}
	signal_unblock();
	
	inputrc_skip_block_count=0;
	inputrc_block_count=0;	
}

/**
   Add a char * based character string mapping.
*/
static void add_terminfo_mapping( const wchar_t *mode, 
								  const char *seq, 
								  const wchar_t *desc, 
								  const wchar_t *func )
{
	if( seq )
	{
		wchar_t *tmp;
		tmp=str2wcs(seq);
		if( tmp )
		{
			add_mapping( mode, tmp, desc, func );
			free(tmp);
		}
	}
	
}

/**
   Call input_expand_sequence on seq, and add the result as a mapping
*/
static void add_escaped_mapping( const wchar_t *mode, 
								 const wchar_t *seq,
								 const wchar_t *desc, 
								 const wchar_t *func )
{
	wchar_t *esc = input_expand_sequence( seq );
	if( esc )
	{
		add_mapping( mode, esc, desc, func );
		free(esc);
	}
}

/**
   Add bindings common to emacs and vi
*/
static void add_common_bindings()
{
	static const wchar_t *name[] =
		{
			L"emacs",
			L"vi",
			L"vi-command"
		}
	;
	int i;
	
	/*
	  Universal bindings
	*/
	for( i=0; i<3; i++ )
	{
		/*
		  We need alternative keybidnings for arrowkeys, since
		  terminfo sometimes specifies a different sequence than what
		  keypresses actually generate
		*/
		add_mapping( name[i], L"\e[A", L"Up", L"history-search-backward" );
		add_mapping( name[i], L"\e[B", L"Down", L"history-search-forward" );
		add_terminfo_mapping( name[i], (key_up), L"Up", L"history-search-backward" );
		add_terminfo_mapping( name[i], (key_down), L"Down", L"history-search-forward" );

		add_mapping( name[i], L"\e[C", L"Right", L"forward-char" );
		add_mapping( name[i], L"\e[D", L"Left", L"backward-char" );
		add_terminfo_mapping( name[i], (key_right), L"Right", L"forward-char" );
		add_terminfo_mapping( name[i], (key_left), L"Left", L"backward-char" );
		
		add_terminfo_mapping( name[i], (key_dc), L"Delete", L"delete-char" );
		
		add_terminfo_mapping( name[i], (key_backspace), L"Backspace", L"backward-delete-char" );
		add_mapping( name[i], L"\x7f", L"Backspace", L"backward-delete-char" );
		
		add_mapping( name[i], L"\e[H", L"Home", L"beginning-of-line" );
		add_mapping( name[i], L"\e[F", L"End", L"end-of-line" );
		add_terminfo_mapping( name[i], (key_home), L"Home", L"beginning-of-line" );
		add_terminfo_mapping( name[i], (key_end), L"End", L"end-of-line" );

		/*
		  We need lots of alternative keybidnings, since terminal
		  emulators can't seem to agree on what sequence to generate,
		  and terminfo doesn't specify what sequence should be
		  generated
		*/

		add_mapping( name[i], L"\e\eOC", L"Alt-Right", L"nextd-or-forward-word" );
		add_mapping( name[i], L"\e\eOD", L"Alt-Left", L"prevd-or-backward-word" );
		add_mapping( name[i], L"\e\e[C", L"Alt-Right", L"nextd-or-forward-word" );
		add_mapping( name[i], L"\e\e[D", L"Alt-Left", L"prevd-or-backward-word" );
		add_mapping( name[i], L"\eO3C", L"Alt-Right", L"nextd-or-forward-word" );
		add_mapping( name[i], L"\eO3D", L"Alt-Left", L"prevd-or-backward-word" );
		add_mapping( name[i], L"\e[3C", L"Alt-Right", L"nextd-or-forward-word" );
		add_mapping( name[i], L"\e[3D", L"Alt-Left", L"prevd-or-backward-word" );
		add_mapping( name[i], L"\e[1;3C", L"Alt-Right", L"nextd-or-forward-word" );
		add_mapping( name[i], L"\e[1;3D", L"Alt-Left", L"prevd-or-backward-word" );		
		
		add_mapping( name[i], L"\e\eOA", L"Alt-Up", L"history-token-search-backward" );
		add_mapping( name[i], L"\e\eOB", L"Alt-Down", L"history-token-search-forward" );
		add_mapping( name[i], L"\e\e[A", L"Alt-Up", L"history-token-search-backward" );
		add_mapping( name[i], L"\e\e[B", L"Alt-Down", L"history-token-search-forward" );
		add_mapping( name[i], L"\eO3A", L"Alt-Up", L"history-token-search-backward" );
		add_mapping( name[i], L"\eO3B", L"Alt-Down", L"history-token-search-forward" );
		add_mapping( name[i], L"\e[3A", L"Alt-Up", L"history-token-search-backward" );
		add_mapping( name[i], L"\e[3B", L"Alt-Down", L"history-token-search-forward" );
		add_mapping( name[i], L"\e[1;3A", L"Alt-Up", L"history-token-search-backward" );
		add_mapping( name[i], L"\e[1;3B", L"Alt-Down", L"history-token-search-forward" );
	}

	/*
	  Bindings used in emacs and vi mode, but not in vi-command mode
	*/
	for( i=0; i<2; i++ )
	{		
		add_mapping( name[i], L"\t", L"Tab", L"complete" );
		add_escaped_mapping( name[i], (L"\\C-k"), L"Control-k", L"kill-line" );
		add_escaped_mapping( name[i], (L"\\C-y"), L"Control-y", L"yank" );
		add_mapping( name[i], L"", L"Any key", L"self-insert" );
	}		
}

/**
   Add emacs-specific bindings
*/
static void add_emacs_bindings()
{	
	add_escaped_mapping( L"emacs", (L"\\C-a"), L"Control-a", L"beginning-of-line" );
	add_escaped_mapping( L"emacs", (L"\\C-e"), L"Control-e", L"end-of-line" );
	add_escaped_mapping( L"emacs", (L"\\M-y"), L"Alt-y", L"yank-pop" );
	add_escaped_mapping( L"emacs", (L"\\C-h"), L"Control-h", L"backward-delete-char" );
	add_escaped_mapping( L"emacs", (L"\\C-e"), L"Control-e", L"end-of-line" );
	add_escaped_mapping( L"emacs", (L"\\C-w"), L"Control-w", L"backward-kill-word" );
	add_escaped_mapping( L"emacs", (L"\e\x7f"), L"Alt-backspace", L"backward-kill-word" );
	add_terminfo_mapping( L"emacs", (key_ppage), L"Page Up", L"beginning-of-history" );
	add_terminfo_mapping( L"emacs", (key_npage), L"Page Down", L"end-of-history" );
}

/**
   Add vi-specific bindings
*/
static void add_vi_bindings()
{
	add_mapping( L"vi", L"\e", L"Escape", L"bind -M vi-command" );
	
	add_mapping( L"vi-command", L"i", L"i", L"bind -M vi" );
	add_mapping( L"vi-command", L"I", L"I", L"bind -M vi" );
	add_mapping( L"vi-command", L"k", L"k", L"history-search-backward" );
	add_mapping( L"vi-command", L"j", L"j", L"history-search-forward" );
	add_mapping( L"vi-command", L" ", L"Space", L"forward-char" );
	add_mapping( L"vi-command", L"l", L"l", L"forward-char" );
	add_mapping( L"vi-command", L"h", L"h", L"backward-char" );
	add_mapping( L"vi-command", L"$", L"$", L"end-of-line" );
	add_mapping( L"vi-command", L"^", L"^", L"beginning-of-line" );
	add_mapping( L"vi-command", L"0", L"0", L"beginning-of-line" );
	add_mapping( L"vi-command", L"b", L"b", L"backward-word" );
	add_mapping( L"vi-command", L"B", L"B", L"backward-word" );
	add_mapping( L"vi-command", L"w", L"w", L"forward-word" );
	add_mapping( L"vi-command", L"W", L"W", L"forward-word" );
	add_mapping( L"vi-command", L"x", L"x", L"delete-char" );
	
/*
  movement ("h", "l"), word movement
  ("b", "B", "w", "W", "e", "E"), moving to beginning and end of line
  ("0", "^", "$"), and inserting and appending ("i", "I", "a", "A"),
  changing and deleting ("c", "C", "d", "D"), character replacement and
  deletion ("r", "x"), and finally yanking and pasting ("y", "p")
*/
	
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
	if( reader_interupted() )
	{
		/*
		  Return 3, i.e. the character read by a Control-C.
		*/
		return 3;
	}

	return 0;	
}

int input_init()
{
	wchar_t *fn;
	
	if( is_init )
		return 1;
	
	is_init = 1;

	input_common_init( &interrupt_handler );
	
	if( setupterm( 0, STDOUT_FILENO, 0) == ERR )
	{
		debug( 0, _( L"Could not set up terminal" ) );
		exit(1);
	}
	hash_init( &all_mappings, &hash_wcs_func, &hash_wcs_cmp );
	
	/* 
	   Add the default key bindings.

	   Maybe some/most of these should be moved to the keybindings file?
	*/

	/*
	  Many terminals (xterm, screen, etc.) have two different valid escape
	  sequences for arrow keys. One which is defined in terminfo/termcap
	  and one which is actually emitted by the arrow keys. The logic
	  escapes me, but I put in these hardcodes here for that reason.
	*/	

	add_common_bindings();
	add_emacs_bindings();
	add_vi_bindings();
	
	current_mode_mappings = (array_list_t *)hash_get( &all_mappings, 
													  L"emacs" );


	fn = env_get( L"INPUTRC" );

	if( !fn )
		fn = L"~/.inputrc";
	
	fn = expand_tilde( wcsdup( fn ));

	if( fn )
	{	
		input_read_inputrc( fn );
		free(fn);
	}
	
	current_application_mappings = (array_list_t *)hash_get( &all_mappings,
															 L"fish" );
	global_mappings = (array_list_t *)hash_get( &all_mappings, 
												L"global" );
	
	return 1;
	
}

/**
   Free memory used by the specified mapping
*/
static void destroy_mapping( const void *key, const void *val )
{
	int i;
	array_list_t *mappings = (array_list_t *)val;
	
	for( i=0; i<al_get_count( mappings ); i++ )
	{
		mapping *m = (mapping *)al_get( mappings, i );
		free( m->seq );
		free( m->seq_desc );
		
		free( m->command );
		free(m );
	}
		
	al_destroy( mappings );
	free((void *)key);
	free((void *)val);	
}


void input_destroy()
{
	if( !is_init )
		return;
	
	is_init=0;
	
	input_common_destroy();
	
	hash_foreach( &all_mappings, &destroy_mapping );	
	hash_destroy( &all_mappings );
	
	del_curterm( cur_term );
}

/**
   Perform the action of the specified binding
*/
static wint_t input_exec_binding( mapping *m, const wchar_t *seq )
{
//	fwprintf( stderr, L"Binding %ls\n", m->command );
	wchar_t code = input_get_code( m->command );
	if( code != -1 )
	{				
		switch( code )
		{
			case R_DUMP_FUNCTIONS:
			{
				dump_functions();
				return R_NULL;							
			}
			case R_SELF_INSERT:
			{
				return seq[0];							
			}
			default:
				return code;
		}	
	}
	else
	{

		/*
		  This key sequence is bound to a command, which
		  is sent to the parser for evaluation.
		*/
		
		/*
		  First clear the commandline. Do not issue a linebreak, since
		  many shortcut commands do not procuce output.
		*/
		write( 1, "\r", 1 );
		tputs(clr_eol,1,&writeb);
		
		reader_run_command( m->command );
		
		/*
		  We still need to return something to the caller, R_NULL
		  tells the reader that nothing happened, but it might be a
		  godd idea to redraw and reexecute the prompt.
		*/
		return R_NULL;
	}
	
}



/**
   Try reading the specified function mapping
*/
static wint_t input_try_mapping( mapping *m)
{
	int j, k;
	wint_t c=0;
	
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
			input_unreadch(c);
			for(k=j-1; k>=0; k--)
				input_unreadch(m->seq[k]);
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
	
	/*
	  Clear the interrupted flag
	*/
	reader_interupted();

	/*
	  Search for sequence in various mapping tables
	*/
	
	while( 1 )
	{
		
		if( current_application_mappings )
		{		
			for( i=0; i<al_get_count( current_application_mappings); i++ )
			{
				wint_t res = input_try_mapping( (mapping *)al_get( current_application_mappings, i ));		
				if( res )
					return res;		
			}
		}
		
		if( global_mappings )
		{
			for( i=0; i<al_get_count( global_mappings); i++ )
			{
				wint_t res = input_try_mapping( (mapping *)al_get( global_mappings, i ));		
				if( res )
					return res;		
			}
		}
		
		if( current_mode_mappings )
		{
			for( i=0; i<al_get_count( current_mode_mappings); i++ )
			{
				wint_t res = input_try_mapping( (mapping *)al_get( current_mode_mappings, i ));		
				if( res )
					return res;		
			}
		}
		
		/*
		  No matching exact mapping, try to find the generic mapping.
		*/
		
		for( i=0; i<al_get_count( current_mode_mappings); i++ )
		{
			mapping *m = (mapping *)al_get( current_mode_mappings, i );
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
		
		input_common_readch( 0 );

	}	
}
