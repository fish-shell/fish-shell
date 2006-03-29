/** \file parser.c

The fish parser. Contains functions for parsing code.

*/

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <pwd.h>
#include <dirent.h>
#include <signal.h>

#include "config.h"

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "proc.h"
#include "parser.h"
#include "tokenizer.h"
#include "exec.h"
#include "wildcard.h"
#include "function.h"
#include "builtin.h"
#include "builtin_help.h"
#include "env.h"
#include "expand.h"
#include "reader.h"
#include "sanity.h"
#include "env_universal.h"
#include "event.h"
#include "translate.h"
#include "intern.h"
#include "parse_util.h"
#include "halloc.h"
#include "halloc_util.h"

/**
   Maximum number of block levels in code. This is not the same as
   maximum recursion depth, this only has to do with how many block
   levels are legal in the source code, not at evaluation.
*/
#define BLOCK_MAX_COUNT 64

/**
   Maximum number of function calls, i.e. recursion depth.
*/
#define MAX_RECURSION_DEPTH 128

/**
   Message about reporting bugs, used on weird internal error to
   hopefully get them to report stuff.
*/
#define BUGREPORT_MSG _( L"If this error can be reproduced, please send a bug report to %s.")

/**
   Error message for improper use of the exec builtin
*/
#define EXEC_ERR_MSG _(L"This command can not be used in a pipeline")

/**
   Error message for tokenizer error. The tokenizer message is
   appended to this message.
*/
#define TOK_ERR_MSG _( L"Tokenizer error: '%ls'")

/**
   Error message for short circuit command error.
*/
#define COND_ERR_MSG _( L"Short circuit command requires additional command")

/**
   Error message on reaching maximum recusrion depth
*/
#define RECURSION_ERR_MSG _( L"Maximum recursion depth reached. Accidental infinite loop?")

/**
   Error message used when the end of a block can't be located
*/
#define BLOCK_END_ERR_MSG _( L"Could not locate end of block. The 'end' command is missing, misspelled or a ';' is missing.")

/**
   Error message on reaching maximum number of block calls
*/
#define BLOCK_ERR_MSG _( L"Maximum number of nested blocks reached.")

/**
   Error message when a non-string token is found when expecting a command name
*/
#define CMD_ERR_MSG _( L"Expected a command name, got token of type '%ls'")

/**
   Error message when a non-string token is found when expecting a command name
*/
#define CMD_OR_ERR_MSG _( L"Expected a command name, got token of type '%ls'. Did you mean 'COMMAND; or COMMAND'? For more information on the 'or' builtin command, see the help section for 'or' by typing 'help or'.")

/**
   Error message when a non-string token is found when expecting a command name
*/
#define CMD_AND_ERR_MSG _( L"Expected a command name, got token of type '%ls'. Did you mean 'COMMAND; and COMMAND'? For more information on the 'and' builtin command, see the help section for 'and' by typing 'help and'.")

/**
   Error message when encountering an illegal command name
*/
#define ILLEGAL_CMD_ERR_MSG _( L"Illegal command name '%ls'")

/**
   Error message for wildcards with no matches
*/
#define WILDCARD_ERR_MSG _( L"Warning: No match for wildcard '%ls'. The command will not be executed.")

/**
   Error when using case builtin outside of switch block
*/
#define INVALID_CASE_ERR_MSG _( L"'case' builtin not inside of switch block")

/**
   Error when using loop control builtins (break or continue) outside of loop
*/
#define INVALID_LOOP_ERR_MSG _( L"Loop control command while not inside of loop" )

/**
   Error when using else builtin outside of if block
*/
#define INVALID_ELSE_ERR_MSG _( L"'else' builtin not inside of if block" )

/**
   Error when using end builtin outside of block
*/
#define INVALID_END_ERR_MSG _( L"'end' command outside of block")

/**
   Error message for Posix-style assignment
*/
#define COMMAND_ASSIGN_ERR_MSG _( L"Unknown command '%ls'. Did you mean 'set VARIABLE VALUE'? For information on setting variable values, see the help section on the set command by typing 'help set'.")

/**
   Error for invalid redirection token
*/
#define REDIRECT_TOKEN_ERR_MSG _( L"Expected redirection specification, got token of type '%ls'")

/**
   Error when encountering redirection without a command
*/
#define INVALID_REDIRECTION_ERR_MSG _( L"Encountered redirection when expecting a command name. Fish does not allow a redirection operation before a command.")

/**
   Error for evaluating null pointer
*/
#define EVAL_NULL_ERR_MSG _( L"Tried to evaluate null pointer." )

/**
   Error for evaluating in illegal scope
*/
#define INVALID_SCOPE_ERR_MSG _( L"Tried to evaluate commands using invalid block type '%ls'" )


/**
	Error for wrong token type
*/
#define UNEXPECTED_TOKEN_ERR_MSG _( L"Unexpected token of type '%ls'")

/**
   Unexpected error in parser_get_filename()
*/
#define MISSING_COMMAND_ERR_MSG _( L"Error while searching for command '%ls'" )


/**
	While block description
*/
#define WHILE_BLOCK _( L"'while' block" )


/**
	For block description
*/
#define FOR_BLOCK _( L"'for' block" )


/**
	If block description
*/
#define IF_BLOCK _( L"'if' conditional block" )


/**
	function definition block description
*/
#define FUNCTION_DEF_BLOCK _( L"function definition block" )


/**
	Function invocation block description
*/
#define FUNCTION_CALL_BLOCK _( L"function invocation block" )


/**
	Switch block description
*/
#define SWITCH_BLOCK _( L"'switch' block" )


/**
	Fake block description
*/
#define FAKE_BLOCK _( L"unexecutable block" )


/**
	Top block description
*/
#define TOP_BLOCK _( L"global root block" )


/**
	Command substitution block description
*/
#define SUBST_BLOCK _( L"command substitution block" )


/**
	Begin block description
*/
#define BEGIN_BLOCK _( L"'begin' unconditional block" )


/**
	Source block description
*/
#define SOURCE_BLOCK _( L"Block created by the . builtin" )

/**
	Source block description
*/
#define EVENT_BLOCK _( L"event handler block" )


/**
	Unknown block description
*/
#define UNKNOWN_BLOCK _( L"unknown/invalid block" )


/** Last error code */
int error_code;

event_block_t *global_event_block=0;

/** Position of last error */

static int err_pos;

/** Description of last error */
static wchar_t err_str[256];

/** Pointer to the current tokenizer */
static tokenizer *current_tokenizer;

/** String for representing the current line */
static string_buffer_t  *lineinfo=0;

/** This is the position of the beginning of the currently parsed command */
static int current_tokenizer_pos;

/** The current innermost block */
block_t *current_block=0;

/** List of called functions, used to help prevent infinite recursion */
static array_list_t *forbidden_function;

/**
   String index where the current job started.
*/
static int job_start_pos;

io_data_t *block_io;

/**
   List of all profiling data
*/
static array_list_t profile_data;

/**
   Keeps track of how many recursive eval calls have been made. Eval
   doesn't call itself directly, recursion happens on blocks and on
   command substitutions.
*/
static int eval_level=-1;

static int parse_job( process_t *p,
					  job_t *j,
					  tokenizer *tok );

/**
   Struct used to keep track of profiling data for a command
*/
typedef struct
{
	/**
	   Time spent executing the specified command, including parse time for nested blocks.
	*/
	int exec;
	/**
	   Time spent parsing the specified command, including execution time for command substitutions.
	*/
	int parse;
	/**
	   The block level of the specified command. nested blocks and command substitutions both increase the block level.
	*/
	int level;
	/**
	   If the execution of this command was skipped.
	*/
	int skipped;
	/**
	   The command string.
	*/
	wchar_t *cmd;
} profile_element_t;

/**
   Return the current number of block nestings
*/
static int block_count( block_t *b )
{

	if( b==0)
		return 0;
	return( block_count(b->outer)+1);
}


void parser_push_block( int type )
{
	block_t *new = halloc( 0, sizeof( block_t ));
	
	new->src_lineno = parser_get_lineno();
	new->src_filename = parser_current_filename()?intern(parser_current_filename()):0;
	
	new->outer = current_block;
	new->type = (current_block && current_block->skip)?FAKE:type;
	
	/*
	  New blocks should be skipped if the outer block is skipped,
	  except TOP ans SUBST block, which open up new environments. Fake
	  blocks should always be skipped. Rather complicated... :-(
	*/
	new->skip=current_block?current_block->skip:0;
	if( type == TOP || type == SUBST )
	{
		new->skip = 0;
	}
	if( type == FAKE )
	{
		new->skip = 1;
	}
	
	new->job = 0;
	new->loop_status=LOOP_NORMAL;

	current_block = new;

	if( (new->type != FUNCTION_DEF) &&
		(new->type != FAKE) &&
		(new->type != TOP) )
	{
		env_push( type == FUNCTION_CALL );
		halloc_register_function_void( current_block, &env_pop );
	}
}

void parser_pop_block()
{
	block_t *old = current_block;
	current_block = current_block->outer;
	halloc_free( old );
}

const wchar_t *parser_get_block_desc( int block )
{
	switch( block )
	{
		case WHILE:
			return WHILE_BLOCK;

		case FOR:
			return FOR_BLOCK;

		case IF:
			return IF_BLOCK;

		case FUNCTION_DEF:
			return FUNCTION_DEF_BLOCK;

		case FUNCTION_CALL:
			return FUNCTION_CALL_BLOCK;

		case SWITCH:
			return SWITCH_BLOCK;

		case FAKE:
			return FAKE_BLOCK;

		case TOP:
			return TOP_BLOCK;

		case SUBST:
			return SUBST_BLOCK;

		case BEGIN:
			return BEGIN_BLOCK;

		case SOURCE:
			return SOURCE_BLOCK;

		case EVENT:
			return EVENT_BLOCK;

		default:
			return UNKNOWN_BLOCK;
	}

}

/**
   Check if the specified bcommand is one of the builtins that cannot
   have arguments, any followin argument is interpreted as a new
   command
*/
static int parser_skip_arguments( const wchar_t *cmd )
{

	return contains_str( cmd,
						 L"else",
						 L"begin",
						 (void *)0 );
}

int parser_is_subcommand( const wchar_t *cmd )
{

	return parser_skip_arguments( cmd ) ||
		contains_str( cmd,
					  L"command",
					  L"builtin",
					  L"while",
					  L"exec",
					  L"if",
					  L"and",
					  L"or",
					  L"not",
					  (void *)0 );
}

int parser_is_block( const wchar_t *word)
{
	return contains_str( word,
						 L"for",
						 L"while",
						 L"if",
						 L"function",
						 L"switch",
						 L"begin",
						 (void *)0 );
}

int parser_is_reserved( const wchar_t *word)
{
	return parser_is_block(word) ||
		parser_is_subcommand( word ) ||
		contains_str( word,
					  L"end",
					  L"case",
					  L"else",
					  L"return",
					  L"continue",
					  L"break",
					  (void *)0 );
}

/**
   Returns 1 if the specified command is a builtin that may not be used in a pipeline
*/
static int parser_is_pipe_forbidden( wchar_t *word )
{
	return contains_str( word,
						 L"exec",
						 L"case",
						 L"break",
						 L"return",
						 L"continue",
						 (void *)0 );
}

/**
   Search the text for the end of the current block
*/
static const wchar_t *parser_find_end( const wchar_t * buff )
{
	tokenizer tok;
	int had_cmd=0;
	int count = 0;
	int error=0;
	int mark=0;

	for( tok_init( &tok, buff, 0 );
		 tok_has_next( &tok ) && !error;
		 tok_next( &tok ) )
	{
		int last_type = tok_last_type( &tok );
		switch( last_type )
		{
			case TOK_STRING:
			{
				if( !had_cmd )
				{
					if( wcscmp(tok_last(&tok), L"end")==0)
					{
						count--;
					}
					else if( parser_is_block( tok_last(&tok) ) )
					{
						count++;
					}

					if( count < 0 )
					{
						error = 1;
					}
					had_cmd = 1;
				}
				break;
			}

			case TOK_END:
			{
				had_cmd = 0;
				break;
			}

			case TOK_PIPE:
			case TOK_BACKGROUND:
			{
				if( had_cmd )
				{
					had_cmd = 0;
				}
				else
				{
					error = 1;
				}
				break;

			}

			case TOK_ERROR:
				error = 1;
				break;

			default:
				break;

		}
		if(!count)
		{
			tok_next( &tok );
			mark = tok_get_pos( &tok );
			break;
		}

	}

	tok_destroy( &tok );
	if(!count && !error){

		return buff+mark;
	}
	return 0;

}

wchar_t *parser_cdpath_get( wchar_t *dir )
{
	wchar_t *res = 0;

	if( !dir )
		return 0;


	if( dir[0] == L'/'|| (wcsncmp( dir, L"./", 2 )==0) )
	{
		struct stat buf;
		if( wstat( dir, &buf ) == 0 )
		{
			if( S_ISDIR(buf.st_mode) )
			{
				res = wcsdup( dir );
			}
		}
	}
	else
	{
		wchar_t *path;
		wchar_t *path_cpy;
		wchar_t *nxt_path;
		wchar_t *state;
		wchar_t *whole_path;

		path = env_get(L"CDPATH");

		if( !path || !wcslen(path) )
		{
			path = L".";
		}

		nxt_path = path;
		path_cpy = wcsdup( path );

		if( !path_cpy )
		{
			die_mem();
		}

		for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
			 nxt_path != 0;
			 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
		{
			wchar_t *expanded_path = expand_tilde( wcsdup(nxt_path) );

//			debug( 2, L"woot %ls\n", expanded_path );

			int path_len = wcslen( expanded_path );
			if( path_len == 0 )
			{
				free(expanded_path );
				continue;
			}

			whole_path =
				wcsdupcat2( expanded_path,
							( expanded_path[path_len-1] != L'/' )?L"/":L"",
							dir, 0 );

			free(expanded_path );

			struct stat buf;
			if( wstat( whole_path, &buf ) == 0 )
			{
				if( S_ISDIR(buf.st_mode) )
				{
					res = whole_path;
					break;
				}
			}
			free( whole_path );

		}
		free( path_cpy );
	}
	return res;
}


void parser_forbid_function( wchar_t *function )
{
/*
  if( function )
  debug( 2, L"Forbid %ls\n", function );
*/
	al_push( forbidden_function, function?wcsdup(function):0 );
}

void parser_allow_function()
{
/*
  if( al_peek( &forbidden_function) )
  debug( 2, L"Allow %ls\n", al_peek( &forbidden_function)  );
*/
	free( (void *) al_pop( forbidden_function ) );
}

void error( int ec, int p, const wchar_t *str, ... )
{
	va_list va;

	error_code = ec;
	err_pos = p;

	va_start( va, str );
	vswprintf( err_str, 256, str, va );
	va_end( va );

}

wchar_t *get_filename( const wchar_t *cmd )
{
	wchar_t *path;

	debug( 3, L"get_filename( '%ls' )", cmd );

	if(wcschr( cmd, L'/' ) != 0 )
	{
		if( waccess( cmd, X_OK )==0 )
		{
			struct stat buff;
			wstat( cmd, &buff );
			if( S_ISREG(buff.st_mode) )
				return wcsdup( cmd );
			else
				return 0;
		}
	}
	else
	{
		path = env_get(L"PATH");
		if( path != 0 )
		{
			/*
			  Allocate string long enough to hold the whole command
			*/
			wchar_t *new_cmd = malloc( sizeof(wchar_t)*(wcslen(cmd)+wcslen(path)+2) );
			/*
			   We tokenize a copy of the path, since strtok modifies
			   its arguments
			*/
			wchar_t *path_cpy = wcsdup( path );
			wchar_t *nxt_path = path;
			wchar_t *state;
			
			if( (new_cmd==0) || (path_cpy==0) )
			{
				die_mem();
			}

			for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
				 nxt_path != 0;
				 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
			{
				int path_len = wcslen( nxt_path );
				wcscpy( new_cmd, nxt_path );
				if( new_cmd[path_len-1] != L'/' )
				{
					new_cmd[path_len++]=L'/';
				}
				wcscpy( &new_cmd[path_len], cmd );
				if( waccess( new_cmd, X_OK )==0 )
				{
					struct stat buff;
					if( wstat( new_cmd, &buff )==-1 )
					{
						if( errno != EACCES )
						{
							wperror( L"stat" );
						}
						continue;
					}
					if( S_ISREG(buff.st_mode) )
					{
						free( path_cpy );
						return new_cmd;
					}
				}
				else
				{
					switch( errno )
					{
						case ENOENT:
						case ENAMETOOLONG:
						case EACCES:
						case ENOTDIR:
							break;
						default:
						{
							debug( 1,
								   MISSING_COMMAND_ERR_MSG,
								   new_cmd );
							wperror( L"access" );
						}
					}
				}
			}
			free( path_cpy );
			free( new_cmd );
		}
	}
	return 0;
}

void parser_init()
{
	if( profile )
	{
		al_init( &profile_data);
	}
	forbidden_function = al_new();

}

/**
   Print profiling information to the specified stream
*/
static void print_profile( array_list_t *p,
						   int pos,
						   FILE *out )
{
	profile_element_t *me, *prev;
	int i;
	int my_time;

	if( pos >= al_get_count( p ) )
	{
		return;
	}
	
	me= (profile_element_t *)al_get( p, pos );
	if( !me->skipped )
	{
		my_time=me->parse+me->exec;

		for( i=pos+1; i<al_get_count(p); i++ )
		{
			prev = (profile_element_t *)al_get( p, i );
			if( prev->skipped )
			{
				continue;
			}
			
			if( prev->level <= me->level )
			{
				break;
			}

			if( prev->level > me->level+1 )
			{
				continue;
			}

			my_time -= prev->parse;
			my_time -= prev->exec;
		}

		if( me->cmd )
		{
			fwprintf( out, L"%d\t%d\t", my_time, me->parse+me->exec );
			for( i=0; i<me->level; i++ )
			{
				fwprintf( out, L"-" );
			}
			fwprintf( out, L"> %ls\n", me->cmd );
		}
	}
	print_profile( p, pos+1, out );
	free( me->cmd );
	free( me );
}

void parser_destroy()
{
	if( profile )
	{
		/*
		  Save profiling information
		*/
		FILE *f = fopen( profile, "w" );
		if( !f )
		{
			debug( 1,
				   _(L"Could not write profiling information to file '%s'"),
				   profile );
		}
		else
		{
			fwprintf( f,
					  _(L"Time\tSum\tCommand\n"),
					  al_get_count( &profile_data ) );
			print_profile( &profile_data, 0, f );
			fclose( f );
		}
		al_destroy( &profile_data );
	}

	if( lineinfo )
	{
		sb_destroy( lineinfo );
		free(lineinfo );
		lineinfo = 0;
	}

	al_destroy( forbidden_function );
	free( forbidden_function );

}

/**
   Print error message if an error has occured while parsing
*/
static void print_errors()
{
	if( error_code )
	{
		int tmp;

		debug( 0, L"%ls", err_str );

		tmp = current_tokenizer_pos;
		current_tokenizer_pos = err_pos;

		fwprintf( stderr, L"%ls", parser_current_line() );

		current_tokenizer_pos=tmp;
	}
}

int eval_args( const wchar_t *line, array_list_t *args )
{
	tokenizer tok;
	/*
	  eval_args may be called while evaulating another command, so we
	  save the previous tokenizer and restore it on exit
	*/
	tokenizer *previous_tokenizer=current_tokenizer;
	int previous_pos=current_tokenizer_pos;
	int do_loop=1;

	proc_push_interactive(0);	

	current_tokenizer_pos = 0;
	
	tok_init( &tok, line, 0 );
	error_code=0;

	for(;do_loop && tok_has_next( &tok) ; tok_next( &tok ) )
	{
		current_tokenizer_pos = tok_get_pos( &tok );
		switch(tok_last_type( &tok ) )
		{
			case TOK_STRING:
			{
				wchar_t *tmp = wcsdup(tok_last( &tok ));
				
				if( !tmp )
				{
					die_mem();
				}
				
				if( expand_string( 0, tmp, args, 0 ) == EXPAND_ERROR )
				{
					err_pos=tok_get_pos( &tok );
					do_loop=0;
				}
				break;
			}
			
			case TOK_END:
			{
				break;
			}
			
			case TOK_ERROR:
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( &tok ),
					   TOK_ERR_MSG,
					   tok_last(&tok) );

				do_loop=0;
				break;
			}

			default:
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( &tok ),
					   UNEXPECTED_TOKEN_ERR_MSG,
					   tok_get_desc( tok_last_type(&tok)) );

				do_loop=0;
				break;
			}
		}
	}
	
	print_errors();
	tok_destroy( &tok );
	
	current_tokenizer=previous_tokenizer;
	current_tokenizer_pos = previous_pos;
	proc_pop_interactive();
	
	return 1;
}

void parser_stack_trace( block_t *b, string_buffer_t *buff)
{
	if( !b )
		return;
	
	if( b->type==EVENT )
	{
		sb_printf( buff, _(L"in event handler: %ls\n"), event_get_desc( b->param1.event ));
		sb_printf( buff,
				   L"\n" );
		return;
	}
	
	if( b->type == FUNCTION_CALL || b->type==SOURCE || b->type==SUBST)
	{
		int i;

		switch( b->type)
		{
			case SOURCE:
			{
				sb_printf( buff, _(L"in . (source) call of file '%ls',\n"), b->param1.source_dest );
				break;
			}
			case FUNCTION_CALL:
			{
				sb_printf( buff, _(L"in function '%ls',\n"), b->param1.function_name );
				break;
			}
			case SUBST:
			{
				sb_printf( buff, _(L"in command substitution\n") );
				break;
			}
		}

		const wchar_t *file = b->src_filename;

		if( file )
		{
			sb_printf( buff,
					   _(L"\tcalled on line %d of file '%ls',\n"),
					   b->src_lineno,
					   file );
		}
		else
		{
			sb_printf( buff,
					   _(L"\tcalled on standard input,\n") );
		}
		
		if( b->type == FUNCTION_CALL )
		{			
			if( b->param2.function_call_process->argv[1] )
			{
				string_buffer_t tmp;
				sb_init( &tmp );
				
				for( i=1; b->param2.function_call_process->argv[i]; i++ )
				{
					sb_append2( &tmp, i>1?L" ":L"", b->param2.function_call_process->argv[i], (void *)0 );
				}
				sb_printf( buff, _(L"\twith parameter list '%ls'\n"), (wchar_t *)tmp.buff );
				
				sb_destroy( &tmp );
			}
		}
		
		sb_printf( buff,
				   L"\n" );
	}
	parser_stack_trace( b->outer, buff );
}

static const wchar_t *is_function()
{
	block_t *b = current_block;
	while( 1 )
	{
		if( !b )
		{
			return 0;
		}
		if( b->type == FUNCTION_CALL )
		{
			return b->param1.function_name;
		}
		b=b->outer;
	}
}


int parser_get_lineno()
{
	const wchar_t *whole_str;
	const wchar_t *function_name;

	int lineno;
	
/*	static const wchar_t *prev_str = 0;
	static int i=0;
	static int lineno=1;
*/
	if( !current_tokenizer )
		return -1;
	
	whole_str = tok_string( current_tokenizer );

	if( !whole_str )
		return -1;
	
	lineno = parse_util_lineno( whole_str, current_tokenizer_pos );

	if( (function_name = is_function()) )
	{
		lineno += function_get_definition_offset( function_name );
	}

	return lineno;
}

const wchar_t *parser_current_filename()
{
	block_t *b = current_block;

	while( 1 )
	{
		if( !b )
		{
			return reader_current_filename();
		}
		if( b->type == FUNCTION_CALL )
		{
			return function_get_definition_file(b->param1.function_name );
		}
		b=b->outer;
	}
}

static int printed_width( const wchar_t *str, int len )
{
	int res=0;
	int i;
	for( i=0; i<len; i++ )
	{
		if( str[i] == L'\t' )
		{
			res=(res+8)&~7;
		}
		else
		{
			res += wcwidth( str[i] );
		}
	}
	return res;
}


wchar_t *parser_current_line()
{
	int lineno=1;

	const wchar_t *file = parser_current_filename();
	wchar_t *whole_str = tok_string( current_tokenizer );
	wchar_t *line = whole_str;
	wchar_t *line_end;
	int i;
	int offset;
	int current_line_width;
	const wchar_t *function_name=0;
	int current_line_start=0;


	if( !line )
		return L"";

	if( !lineinfo )
	{
		lineinfo = malloc( sizeof(string_buffer_t) );
		sb_init( lineinfo );
	}
	sb_clear( lineinfo );

	/*
	  Calculate line number, line offset, etc.
	*/
	for( i=0; i<current_tokenizer_pos && whole_str[i]; i++ )
	{
		if( whole_str[i] == L'\n' )
		{
			lineno++;
			current_line_start=i+1;
			line = &whole_str[i+1];
		}
	}

//	lineno = current_tokenizer_pos;
	

	current_line_width=printed_width(whole_str+current_line_start, current_tokenizer_pos-current_line_start );

	if( (function_name = is_function()) )
	{
		lineno += function_get_definition_offset( function_name );
	}

	/*
	  Copy current line from whole string
	*/
	line_end = wcschr( line, L'\n' );
	if( !line_end )
		line_end = line+wcslen(line);

	line = wcsndup( line, line_end-line );

	/**
	   If we are not going to print a stack trace, at least print the line number and filename
	*/
	if( !is_interactive || is_function() )
	{
		int prev_width = my_wcswidth( (wchar_t *)lineinfo->buff );
		if( file )
			sb_printf( lineinfo,
					   _(L"%ls (line %d): "),
					   file,
					   lineno );
		else
			sb_printf( lineinfo,
					   L"%ls: ",
					   _(L"Standard input"),
					   lineno );
		offset = my_wcswidth( (wchar_t *)lineinfo->buff ) - prev_width;
	}
	else
	{
		offset=0;
	}

//	debug( 1, L"Current pos %d, line pos %d, file_length %d, is_interactive %d, offset %d\n", current_tokenizer_pos,  current_line_pos, wcslen(whole_str), is_interactive, offset);
	/*
	   Skip printing character position if we are in interactive mode
	   and the error was on the first character of the line.
	*/
	if( !is_interactive || is_function() || (current_line_width!=0) )
	{
		// Workaround since it seems impossible to print 0 copies of a character using %*lc
		if( offset+current_line_width )
		{
			sb_printf( lineinfo,
					   L"%ls\n%*lc^\n",
					   line,
					   offset+current_line_width,
					   L' ' );
		}
		else
		{
			sb_printf( lineinfo,
					   L"%ls\n^\n",
					   line );
		}
	}

	free( line );
	parser_stack_trace( current_block, lineinfo );

	return (wchar_t *)lineinfo->buff;
}

int parser_get_pos()
{
	return tok_get_pos( current_tokenizer );
}

int parser_get_job_pos()
{
	return job_start_pos;
}


void parser_set_pos( int p)
{
	tok_set_pos( current_tokenizer, p );
}

const wchar_t *parser_get_buffer()
{
	return tok_string( current_tokenizer );
}


int parser_is_help( wchar_t *s, int min_match )
{
	int len = wcslen(s);

	return ( wcscmp( L"-h", s ) == 0 ) ||
		( len >= 3 && (wcsncmp( L"--help", s, len ) == 0) );
}

/**
   Parse options for the specified job

   \param p the process to parse options for
   \param j the job to which the process belongs to
   \param tok the tokenizer to read options from
   \param args the argument list to insert options into
*/
static void parse_job_main_loop( process_t *p,
								 job_t *j,
								 tokenizer *tok,
								 array_list_t *args )
{
	int is_finished=0;

	int proc_is_count=0;

	int matched_wildcard = 0, unmatched_wildcard = 0;

	wchar_t *unmatched = 0;
	int unmatched_pos=0;

	/*
	  Test if this is the 'count' command. We need to special case
	  count, since it should display a help message on 'count .h',
	  but not on 'set foo -h; count $foo'.
	*/
	if( p->actual_cmd )
	{
		wchar_t *woot = wcsrchr( p->actual_cmd, L'/' );
		if( !woot )
			woot = p->actual_cmd;
		else
			woot++;
		proc_is_count = wcscmp( woot, L"count" )==0;
	}

	while( 1 )
	{

		switch( tok_last_type( tok ) )
		{
			case TOK_PIPE:
			{
				if( (p->type == INTERNAL_EXEC) )
				{
					error( SYNTAX_ERROR,
						   tok_get_pos( tok ),
						   EXEC_ERR_MSG );
					return;
				}
				p->pipe_fd = wcstol( tok_last( tok ), 0, 10 );
				if( !p->argv )
					halloc_register( j, p->argv = list_to_char_arr( args ) );
				p->next = halloc( j, sizeof( process_t ) );
				if( p->next == 0 )
				{
					die_mem();
				}
				tok_next( tok );
				
				/*
				  Don't do anything on failiure. parse_job will notice the error flag beeing set
				*/
				parse_job( p->next, j, tok );

				is_finished = 1;
				break;
			}
			
			case TOK_BACKGROUND:
			{
				j->fg = 0;
			}
			
			case TOK_END:
			{
				if( !p->argv )
					halloc_register( j, p->argv = list_to_char_arr( args ) );
				if( tok_has_next(tok))
					tok_next(tok);

				is_finished = 1;

				break;
			}

			case TOK_STRING:
			{
				int skip=0;

				if( current_block->skip )
				{
					/*
					  If this command should be skipped, we do not expand the arguments
					*/
					skip=1;

					/*
					  But if this is in fact a case statement, then it should be evaluated
					*/

					if( (current_block->type == SWITCH) &&
						(wcscmp( al_get( args, 0), L"case" )==0) &&
						p->type == INTERNAL_BUILTIN )
					{
						skip=0;
					}
				}

				if( !skip )
				{
					if( proc_is_count &&
						(al_get_count( args) == 1) &&
						( parser_is_help( tok_last(tok), 0) ) )
					{
						/*
						  Display help for count
						*/
						p->type = INTERNAL_BUILTIN;
						p->actual_cmd = L"count";
					}


					switch( expand_string( j, wcsdup(tok_last( tok )), args, 0 ) )
					{
						case EXPAND_ERROR:
						{
							err_pos=tok_get_pos( tok );
							if( error_code == 0 )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( tok ),
									   _(L"Could not expand string '%ls'"),
									   tok_last(tok) );

							}
							break;
						}

						case EXPAND_WILDCARD_NO_MATCH:
						{
							unmatched_wildcard = 1;
							if( !unmatched )
							{
								unmatched = halloc_wcsdup( j, tok_last( tok ));
								unmatched_pos = tok_get_pos( tok );
							}

							break;
						}

						case EXPAND_WILDCARD_MATCH:
						{
							matched_wildcard = 1;
							break;
						}

						case EXPAND_OK:
						{
							break;
						}

					}

				}

				break;
			}

			case TOK_REDIRECT_OUT:
			case TOK_REDIRECT_IN:
			case TOK_REDIRECT_APPEND:
			case TOK_REDIRECT_FD:
			{
				int type = tok_last_type( tok );
				io_data_t *new_io;
				wchar_t *target = 0;

				/*
				  Don't check redirections in skipped part

				  Otherwise, bogus errors may be the result. (Do check
				  that token is string, though)
				*/
				if( current_block->skip )
				{
					tok_next( tok );
					if( tok_last_type( tok ) != TOK_STRING )
					{
						error( SYNTAX_ERROR,
							   tok_get_pos( tok ),
							   REDIRECT_TOKEN_ERR_MSG,
							   tok_get_desc( tok_last_type(tok)) );
					}

					break;
				}

				new_io = halloc( j, sizeof(io_data_t) );
				if( !new_io )
					die_mem();

				new_io->fd = wcstol( tok_last( tok ),
									 0,
									 10 );
				tok_next( tok );

				switch( tok_last_type( tok ) )
				{
					case TOK_STRING:
					{
						target = (wchar_t *)expand_one( j, wcsdup( tok_last( tok ) ), 0);

						if( target == 0 && error_code == 0 )
						{
							error( SYNTAX_ERROR,
								   tok_get_pos( tok ),
								   REDIRECT_TOKEN_ERR_MSG,
								   tok_last( tok ) );

						}
					}
					break;
					default:
						error( SYNTAX_ERROR,
							   tok_get_pos( tok ),
							   REDIRECT_TOKEN_ERR_MSG,
							   tok_get_desc( tok_last_type(tok)) );
				}

				if( target == 0 || wcslen( target )==0 )
				{
					if( error_code == 0 )
						error( SYNTAX_ERROR,
							   tok_get_pos( tok ),
							   _(L"Invalid IO redirection") );
					tok_next(tok);
				}
				else
				{


					switch( type )
					{
						case TOK_REDIRECT_APPEND:
							new_io->io_mode = IO_FILE;
							new_io->param2.flags = O_CREAT | O_APPEND | O_WRONLY;
							new_io->param1.filename = target;
							break;

						case TOK_REDIRECT_OUT:
							new_io->io_mode = IO_FILE;
							new_io->param2.flags = O_CREAT | O_WRONLY | O_TRUNC;
							new_io->param1.filename = target;
							break;

						case TOK_REDIRECT_IN:
							new_io->io_mode = IO_FILE;
							new_io->param2.flags = O_RDONLY;
							new_io->param1.filename = target;
							break;

						case TOK_REDIRECT_FD:
							if( wcscmp( target, L"-" ) == 0 )
							{
								new_io->io_mode = IO_CLOSE;
							}
							else
							{
								new_io->io_mode = IO_FD;
								new_io->param1.old_fd = wcstol( target,
																0,
																10 );
								if( ( new_io->param1.old_fd < 0 ) ||
									( new_io->param1.old_fd > 10 ) )
								{
									error( SYNTAX_ERROR,
										   tok_get_pos( tok ),
										   _(L"Requested redirection to something that is not a file descriptor %ls"),
										   target );

									tok_next(tok);
								}
							}
							break;
					}
				}

				j->io = io_add( j->io, new_io );
				
			}
			break;

			case TOK_ERROR:
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   TOK_ERR_MSG,
					   tok_last(tok) );

				return;
			}

			default:
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   UNEXPECTED_TOKEN_ERR_MSG,
					   tok_get_desc( tok_last_type(tok)) );

				tok_next(tok);
				break;
		}

		if( (is_finished) || (error_code != 0) )
			break;

		tok_next( tok );
	}

	if( !error_code )
	{
		if( unmatched_wildcard && !matched_wildcard )
		{
			j->wildcard_error = 1;
			proc_set_last_status( 1 );
			if( is_interactive && !is_block )
			{
				int tmp;

				debug( 1, WILDCARD_ERR_MSG, unmatched );
				tmp = current_tokenizer_pos;
				current_tokenizer_pos = unmatched_pos;

				fwprintf( stderr, L"%ls", parser_current_line() );

				current_tokenizer_pos=tmp;
			}

		}
	}

	return;
}


/**
   Fully parse a single job. Does not call exec on it.

   \param p The process structure that should be used to represent the first process in the job.
   \param j The job structure to contain the parsed job
   \param tok tokenizer to read from

   \return 1 on success, 0 on error
*/
static int parse_job( process_t *p,
					  job_t *j,
					  tokenizer *tok )
{
	array_list_t *args = al_halloc( j );      // The list that will become the argc array for the program
	int use_function = 1;   // May functions be considered when checking what action this command represents
	int use_builtin = 1;    // May builtins be considered when checking what action this command represents
	int is_new_block=0;     // Does this command create a new block?

	block_t *prev_block = current_block;
	int prev_tokenizer_pos = current_tokenizer_pos;	

	current_tokenizer_pos = tok_get_pos( tok );

	while( al_get_count( args ) == 0 )
	{
		wchar_t *nxt=0;
		switch( tok_last_type( tok ))
		{
			case TOK_STRING:
			{
				nxt = expand_one( j,
								  wcsdup(tok_last( tok )),
								  EXPAND_SKIP_SUBSHELL | EXPAND_SKIP_VARIABLES);
				
				if( nxt == 0 )
				{
					error( SYNTAX_ERROR,
						   tok_get_pos( tok ),
						   ILLEGAL_CMD_ERR_MSG,
						   tok_last( tok ) );
					
					current_tokenizer_pos = prev_tokenizer_pos;	
					return 0;
				}
				break;
			}

			case TOK_ERROR:
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   TOK_ERR_MSG,
					   tok_last(tok) );

				current_tokenizer_pos = prev_tokenizer_pos;	
				return 0;
			}

			case TOK_PIPE:
			{
				wchar_t *str = tok_string( tok );
				if( tok_get_pos(tok)>0 && str[tok_get_pos(tok)-1] == L'|' )
				{
					error( SYNTAX_ERROR,
						   tok_get_pos( tok ),
						   CMD_OR_ERR_MSG,
						   tok_get_desc( tok_last_type(tok) ) );
				}
				else
				{
					error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   CMD_ERR_MSG,
						   tok_get_desc( tok_last_type(tok) ) );
				}

				current_tokenizer_pos = prev_tokenizer_pos;	
				return 0;
			}

			default:
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   CMD_ERR_MSG,
					   tok_get_desc( tok_last_type(tok) ) );

				current_tokenizer_pos = prev_tokenizer_pos;	
				return 0;
			}
		}
		
		int mark = tok_get_pos( tok );

		int consumed = 0;
		

		if( wcscmp( L"command", nxt )==0 )
		{
			tok_next( tok );
			if( parser_is_help( tok_last( tok ), 0 ) )
			{
				tok_set_pos( tok, mark);
			}
			else
			{
				use_function = 0;
				use_builtin=0;
				consumed=1;
			}
		}
		else if(  wcscmp( L"builtin", nxt )==0 )
		{
			tok_next( tok );
			if( tok_last(tok)[0] == L'-' )
			{
				tok_set_pos( tok, mark);
			}
			else
			{
				use_function = 0;
				consumed=1;
			}
		}
		else if(  wcscmp( L"not", nxt )==0 )
		{
			tok_next( tok );
			if( tok_last(tok)[0] == L'-' )
			{
				tok_set_pos( tok, mark);
			}
			else
			{
				j->negate=1-j->negate;
				consumed=1;
			}
		}
		else if(  wcscmp( L"and", nxt )==0 )
		{
			tok_next( tok );
			if( tok_last(tok)[0] == L'-' )
			{
				tok_set_pos( tok, mark);
			}
			else
			{
				j->skip = proc_get_last_status();
				consumed=1;
			}
		}
		else if(  wcscmp( L"or", nxt )==0 )
		{
			tok_next( tok );
			if( tok_last(tok)[0] == L'-' )
			{
				tok_set_pos( tok, mark);
			}
			else
			{
				j->skip = !proc_get_last_status();
				consumed=1;
			}
		}
		else if(  wcscmp( L"exec", nxt )==0 )
		{
			if( p != j->first_process )
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   EXEC_ERR_MSG );
				current_tokenizer_pos = prev_tokenizer_pos;	
				return 0;
			}

			tok_next( tok );
			if( tok_last(tok)[0] == L'-' )
			{
				tok_set_pos( tok, mark);
			}
			else
			{
				use_function = 0;
				use_builtin=0;
				p->type=INTERNAL_EXEC;
				consumed=1;
				current_tokenizer_pos = prev_tokenizer_pos;	
			}
		}
		else if( wcscmp( L"while", nxt ) ==0 )
		{
			int new_block = 0;
			tok_next( tok );

			if( ( current_block->type != WHILE ) )
			{
				new_block = 1;
			}
			else if( current_block->param1.while_state == WHILE_TEST_AGAIN )
			{
				current_block->param1.while_state = WHILE_TEST_FIRST;
			}
			else
			{
				new_block = 1;
			}

			if( new_block )
			{
				parser_push_block( WHILE );
				current_block->param1.while_state=WHILE_TEST_FIRST;
				current_block->tok_pos = mark;
			}

			consumed=1;
			is_new_block=1;
		}
		else if( wcscmp( L"if", nxt ) ==0 )
		{
			tok_next( tok );

			parser_push_block( IF );

			current_block->param1.if_state=0;
			current_block->tok_pos = mark;

			is_new_block=1;
			consumed=1;			
		}

		if( consumed )
		{
			continue;
		}

		if( use_function && !current_block->skip )
		{
			int nxt_forbidden;
			wchar_t *forbid;

			forbid = (wchar_t *)(al_get_count( forbidden_function)?al_peek( forbidden_function ):0);
			nxt_forbidden = forbid && (wcscmp( forbid, nxt) == 0 );

			/*
			  Make feeble attempt to avoid infinite recursion. Will at
			  least catch some accidental infinite recursion calls.
			*/
			if( function_exists( nxt ) && !nxt_forbidden)
			{
				/*
				  Check if we have reached the maximum recursion depth
				*/
				if( al_get_count( forbidden_function ) > MAX_RECURSION_DEPTH )
				{
					error( SYNTAX_ERROR,
						   tok_get_pos( tok ),
						   RECURSION_ERR_MSG );
				}
				else
				{
					p->type = INTERNAL_FUNCTION;
				}
			}
		}
		al_push( args, nxt );
	}

	if( error_code == 0 )
	{
		if( !p->type )
		{
			if( use_builtin &&
				builtin_exists( (wchar_t *)al_get( args, 0 ) ) )
			{
				p->type = INTERNAL_BUILTIN;
				is_new_block = parser_is_block( (wchar_t *)al_get( args, 0 ) );
			}
		}

		if( !p->type || (p->type == INTERNAL_EXEC) )
		{
			/*
			   If we are not executing the current block, allow
			   non-existent commands.
			*/
			if( current_block->skip )
			{
				p->actual_cmd = L"";
			}
			else
			{
				p->actual_cmd = halloc_register(j, get_filename( (wchar_t *)al_get( args, 0 ) ));
				
				/*
				  Check if the specified command exists
				*/
				if( p->actual_cmd == 0 )
				{

					/*
					  That is not a command! Test if it is a
					  directory, in which case, we use 'cd' as the
					  implicit command.
					*/
					wchar_t *pp =
						parser_cdpath_get( (wchar_t *)al_get( args, 0 ) );
					if( pp )
					{
						wchar_t *tmp;
						free( pp );

						tmp = (wchar_t *)al_get( args, 0 );
						al_truncate( args, 0 );
						al_push( args, wcsdup( L"cd" ) );
						al_push( args, tmp );
						/*
						  If we have defined a wrapper around cd, use it,
						  otherwise use the cd builtin
						*/
						if( function_exists( L"cd" ) )
							p->type = INTERNAL_FUNCTION;
						else
							p->type = INTERNAL_BUILTIN;
					}
					else
					{
						int tmp;

						/* 
						   We couln't find the specified command.

						   What we want to happen now is that the
						   specified job won't get executed, and an
						   error message is printed on-screen, but
						   otherwise, the parsing/execution of the
						   file continues. Because of this, we don't
						   want to call error(), since that would stop
						   execution of the file. Instead we let
						   p->actual_command be 0 (null), which will
						   cause the job to silently not execute. We
						   also print an error message.
						*/
						if( wcschr( (wchar_t *)al_get( args, 0 ), L'=' ) )
						{
							debug( 0,
								   COMMAND_ASSIGN_ERR_MSG,
								   (wchar_t *)al_get( args, 0 ) );
							

						}
						else
						{
							debug( 0,
								   _(L"Unknown command '%ls'"),
								   (wchar_t *)al_get( args, 0 ) );
						}
						
						tmp = current_tokenizer_pos;
						current_tokenizer_pos = tok_get_pos(tok);
						
						fwprintf( stderr, L"%ls", parser_current_line() );
						
						current_tokenizer_pos=tmp;
						j->skip=1;
						
					}
				}
			}
		}
	}
	
	if( is_new_block )
	{
		
		const wchar_t *end=parser_find_end( tok_string( tok ) +
											current_tokenizer_pos );
		tokenizer subtok;
		int make_sub_block = j->first_process != p;

		if( !end )
		{
			error( SYNTAX_ERROR,
				   tok_get_pos( tok ),
				   BLOCK_END_ERR_MSG );
			
		}

		if( !make_sub_block )
		{
			tok_init( &subtok, end, 0 );
			switch( tok_last_type( &subtok ) )
			{
				case TOK_END:
					break;

				case TOK_REDIRECT_OUT:
				case TOK_REDIRECT_APPEND:
				case TOK_REDIRECT_IN:
				case TOK_REDIRECT_FD:
				case TOK_PIPE:
				{
					make_sub_block = 1;
					break;
				}

				default:
				{
					error( SYNTAX_ERROR,
						   current_tokenizer_pos,
						   BLOCK_END_ERR_MSG );
				}
			}
			tok_destroy( &subtok );
		}

		if( make_sub_block )
		{

			int end_pos = end-tok_string( tok );
			wchar_t *sub_block= wcsndup( tok_string( tok ) + current_tokenizer_pos,
										 end_pos - current_tokenizer_pos);

			p->type = INTERNAL_BLOCK;
			al_set( args, 0, sub_block );

			tok_set_pos( tok,
						 end_pos );

			while( prev_block != current_block )
			{
				parser_pop_block();
			}

		}
		else tok_next( tok );

	}
	else tok_next( tok );

	if( !error_code )
	{
		if( p->type == INTERNAL_BUILTIN && parser_skip_arguments( (wchar_t *)al_get(args, 0) ) )
		{			
			if( !p->argv )
				halloc_register( j, p->argv = list_to_char_arr( args ) );
		}
		else
		{
			parse_job_main_loop( p, j, tok, args );
		}
	}

	if( error_code )
	{
		/*
		  Make sure the block stack is consistent
		*/
		while( prev_block != current_block )
		{
			parser_pop_block();
		}
	}
	current_tokenizer_pos = prev_tokenizer_pos;	
	return !error_code;
}

/**
   Do skipped execution of command. This means that only limited
   execution of block level commands such as end and switch should be
   preformed.

   \param j the job to execute

*/
static void skipped_exec( job_t * j )
{
	process_t *p;

	for( p = j->first_process; p; p=p->next )
	{
		if( p->type == INTERNAL_BUILTIN )
		{
			if(( wcscmp( p->argv[0], L"for" )==0) ||
			   ( wcscmp( p->argv[0], L"switch" )==0) ||
			   ( wcscmp( p->argv[0], L"begin" )==0) ||
			   ( wcscmp( p->argv[0], L"function" )==0))
			{
				parser_push_block( FAKE );
			}
			else if( wcscmp( p->argv[0], L"end" )==0)
			{
				if(!current_block->outer->skip )
				{
					exec( j );
					return;
				}
				parser_pop_block();
			}
			else if( wcscmp( p->argv[0], L"else" )==0)
			{
				if( (current_block->type == IF ) &&
					(current_block->param1.if_state != 0))
				{
					exec( j );
					return;
				}
			}
			else if( wcscmp( p->argv[0], L"case" )==0)
			{
				if( (current_block->type == SWITCH ) )
				{
					exec( j );
					return;
				}
			}
		}
	}
	job_free( j );
}

/**
   Evaluates a job from the specified tokenizer. First calls
   parse_job to parse the job and then calls exec to execute it.

   \param tok The tokenizer to read tokens from
*/

static void eval_job( tokenizer *tok )
{
	job_t *j;

	int start_pos = job_start_pos = tok_get_pos( tok );
	long long t1=0, t2=0, t3=0;
	profile_element_t *p=0;
	int skip = 0;
	int job_begin_pos, prev_tokenizer_pos;
	
	if( profile )
	{
		p=malloc( sizeof(profile_element_t));
		p->cmd=0;
		al_push( &profile_data, p );
		p->skipped=1;
		t1 = get_time();
	}

	switch( tok_last_type( tok ) )
	{
		case TOK_STRING:
		{
			j = job_create();
			j->fg=1;
			j->terminal = j->job_control && (!is_subshell && !is_event);
			j->skip_notification = is_subshell || is_block || is_event || (!is_interactive);
			
			current_block->job = j;
			
			if( is_interactive )
			{
				if( tcgetattr (0, &j->tmodes) )
				{
					tok_next( tok );
					wperror( L"tcgetattr" );
					job_free( j );
					break;
				}
			}

			j->first_process = halloc( j, sizeof( process_t ) );

			job_begin_pos = tok_get_pos( tok );
			
			if( parse_job( j->first_process, j, tok ) &&
				j->first_process->argv )
			{
				if( job_start_pos < tok_get_pos( tok ) )
				{
					int stop_pos = tok_get_pos( tok );
					wchar_t *newline = wcschr(  tok_string(tok)+start_pos,
												L'\n' );
					if( newline )
						stop_pos = mini( stop_pos, newline - tok_string(tok) );

					j->command = halloc_wcsndup( j,
												 tok_string(tok)+start_pos,
												 stop_pos-start_pos );
				}
				else
					j->command = L"";

				if( profile )
				{
					t2 = get_time();
					p->cmd = wcsdup( j->command );
					p->skipped=current_block->skip;
				}

				skip |= current_block->skip;
				skip |= j->wildcard_error;
				skip |= j->skip;

				if(!skip )
				{
					int was_builtin = 0;
//					if( j->first_process->type==INTERNAL_BUILTIN && !j->first_process->next)
//						was_builtin = 1;
					prev_tokenizer_pos = current_tokenizer_pos;
					current_tokenizer_pos = job_begin_pos;		
					exec( j );
					current_tokenizer_pos = prev_tokenizer_pos;
					
					/* Only external commands require a new fishd barrier */
					if( !was_builtin )
						proc_had_barrier=0;
				}
				else
				{
					skipped_exec( j );
				}

				if( profile )
				{
					t3 = get_time();
					p->level=eval_level;
					p->parse = t2-t1;
					p->exec=t3-t2;
				}

				if( current_block->type == WHILE )
				{

					switch( current_block->param1.while_state )
					{
						case WHILE_TEST_FIRST:
						{
							current_block->skip = proc_get_last_status()!= 0;
							current_block->param1.while_state=WHILE_TESTED;
						}
						break;
					}
				}

				if( current_block->type == IF )
				{
					if( (!current_block->param1.if_state) &&
						(!current_block->skip) )
					{
						current_block->skip = proc_get_last_status()!= 0;
						current_block->param1.if_state++;
					}
				}

			}
			else
			{
				/*
				  This job could not be properly parsed. We free it instead, and set the status to 1.
				*/
				job_free( j );

				proc_set_last_status( 1 );
			}
			current_block->job = 0;
			break;
		}

		case TOK_END:
		{
			if( tok_has_next( tok ))
				tok_next( tok );
			break;
		}

		case TOK_BACKGROUND:
		{
			wchar_t *str = tok_string( tok );
			if( tok_get_pos(tok)>0 && str[tok_get_pos(tok)-1] == L'&' )
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   CMD_AND_ERR_MSG,
					   tok_get_desc( tok_last_type(tok) ) );
			}
			else
			{
				error( SYNTAX_ERROR,
					   tok_get_pos( tok ),
					   CMD_ERR_MSG,
					   tok_get_desc( tok_last_type(tok) ) );
			}

			return;
		}

		case TOK_ERROR:
		{
			error( SYNTAX_ERROR,
				   tok_get_pos( tok ),
				   TOK_ERR_MSG,
				   tok_last(tok) );

			return;
		}

		default:
		{
			error( SYNTAX_ERROR,
				   tok_get_pos( tok ),
				   CMD_ERR_MSG,
				   tok_get_desc( tok_last_type(tok)) );

			return;
		}
	}

	job_reap( 0 );

}

int eval( const wchar_t *cmd, io_data_t *io, int block_type )
{
	int forbid_count;
	int code;
	tokenizer *previous_tokenizer=current_tokenizer;
	block_t *start_current_block = current_block;
	io_data_t *prev_io = block_io;
	array_list_t *prev_forbidden = forbidden_function;

	if( block_type == SUBST )
	{
		forbidden_function = al_new();
	}
	
	forbid_count = al_get_count( forbidden_function );

	block_io = io;

	job_reap( 0 );

	debug( 4, L"eval: %ls", cmd );

	if( !cmd )
	{
		debug( 1,
			   EVAL_NULL_ERR_MSG );
		debug( 1,
			   BUGREPORT_MSG,
			   PACKAGE_BUGREPORT );
		return 1;
	}

	if( (block_type != TOP) &&
		(block_type != SUBST))
	{
		debug( 1,
			   INVALID_SCOPE_ERR_MSG,
			   parser_get_block_desc( block_type ) );

		debug( 1,
			   BUGREPORT_MSG,
			   PACKAGE_BUGREPORT );
		return 1;
	}

	eval_level++;

	parser_push_block( block_type );

	current_tokenizer = malloc( sizeof(tokenizer));
	tok_init( current_tokenizer, cmd, 0 );

	error_code = 0;

	event_fire( 0 );

	while( tok_has_next( current_tokenizer ) &&
		   !error_code &&
		   !sanity_check() &&
		   !exit_status() )
	{
		eval_job( current_tokenizer );
		event_fire( 0 );
	}

	int prev_block_type = current_block->type;

	parser_pop_block();

	while( start_current_block != current_block )
	{
		if( current_block == 0 )
		{
			debug( 0,
				   _(L"End of block mismatch. Program terminating.") );
			debug( 0,
				   BUGREPORT_MSG,
				   PACKAGE_BUGREPORT );
			exit(1);
			break;
		}

		if( (!error_code) && (!exit_status()) && (!proc_get_last_status()) )
		{
			char *h;

			//debug( 2, L"Status %d\n", proc_get_last_status() );

			debug( 1,
				   L"%ls", parser_get_block_desc( current_block->type ) );
			debug( 1,
				   BLOCK_END_ERR_MSG );
			fwprintf( stderr, L"%ls", parser_current_line() );

			h = builtin_help_get( L"end" );
			if( h )
				fwprintf( stderr, L"%s", h );
			break;

		}
		prev_block_type = current_block->type;
		parser_pop_block();
	}

	print_errors();

	tok_destroy( current_tokenizer );
	free( current_tokenizer );

	while( al_get_count( forbidden_function ) > forbid_count )
		parser_allow_function();

	if( block_type == SUBST )
	{
		al_destroy( forbidden_function );
		free( forbidden_function );
	}

	/*
	   Restore previous eval state
	*/
	forbidden_function = prev_forbidden;
	current_tokenizer=previous_tokenizer;
	block_io = prev_io;
	eval_level--;

	code=error_code;
	error_code=0;

	job_reap( 0 );

	return code;
}


int parser_test( wchar_t * buff,
				 int babble )
{
	tokenizer tok;
	int had_cmd=0;
	int count = 0;
	int err=0;
	tokenizer *previous_tokenizer=current_tokenizer;
	int previous_pos=current_tokenizer_pos;
	static int block_pos[BLOCK_MAX_COUNT];
	static int block_type[BLOCK_MAX_COUNT];
	int is_pipeline = 0;
	int forbid_pipeline = 0;
	int needs_cmd=0;
	int require_additional_commands=0;

	current_tokenizer = &tok;

	for( tok_init( &tok, buff, 0 );
		 tok_has_next( &tok );
		 tok_next( &tok ) )
	{
		current_tokenizer_pos = tok_get_pos( &tok );

		int last_type = tok_last_type( &tok );
		switch( last_type )
		{
			case TOK_STRING:
			{
				if( !had_cmd )
				{
					int mark = tok_get_pos( &tok );
					had_cmd = 1;

					if( require_additional_commands )
					{
						if( contains_str( tok_last(&tok),
										  L"end",
										  0 ) )
						{
							err=1;
							if( babble )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( &tok ),
									   COND_ERR_MSG );

								print_errors();
							}
						}

						require_additional_commands--;
					}

					/*
					  Decrement block count on end command
					*/
					if( wcscmp(tok_last(&tok), L"end")==0)
					{
						tok_next( &tok );
						count--;
						tok_set_pos( &tok, mark );
					}

					/*
					  Handle block commands
					*/
					if( parser_is_block( tok_last(&tok) ) )
					{
						if( count >= BLOCK_MAX_COUNT )
						{
							error( SYNTAX_ERROR,
								   tok_get_pos( &tok ),
								   BLOCK_ERR_MSG );

							print_errors();
						}
						else
						{
							if( wcscmp( tok_last(&tok), L"while") == 0 )
								block_type[count] = WHILE;
							else if( wcscmp( tok_last(&tok), L"for") == 0 )
								block_type[count] = FOR;
							else if( wcscmp( tok_last(&tok), L"switch") == 0 )
								block_type[count] = SWITCH;
							else if( wcscmp( tok_last(&tok), L"if") == 0 )
								block_type[count] = IF;
							else if( wcscmp( tok_last(&tok), L"function") == 0 )
								block_type[count] = FUNCTION_DEF;
							else if( wcscmp( tok_last(&tok), L"begin") == 0 )
								block_type[count] = BEGIN;
							else
								block_type[count] = -1;

//							debug( 2, L"add block of type %d after cmd %ls\n", block_type[count], tok_last(&tok) );


							block_pos[count] = current_tokenizer_pos;
							tok_next( &tok );
							count++;
							tok_set_pos( &tok, mark );
						}
					}

					/*
					  If parser_is_subcommand is true, the command
					  accepts a second command as it's first
					  argument. If parser_skip_arguments is true, the
					  second argument is optional.
					*/
					if( parser_is_subcommand( tok_last( &tok ) ) && !parser_skip_arguments(tok_last( &tok ) ) )
					{
						needs_cmd = 1;
						had_cmd = 0;
					}

					/*
					  The short circuit commands requires _two_ additional commands.
					*/
					if( contains_str( tok_last( &tok ),
									  L"or",
									  L"and",
									  0 ) )
					{
						if( is_pipeline )
						{
							err=1;
							if( babble )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( &tok ),
									   EXEC_ERR_MSG );

								print_errors();

							}
						}
						require_additional_commands=1;
					}

					/*
					  There are a lot of situations where pipelines
					  are forbidden, inclusing when using the exec
					  builtin.
					*/
					if( parser_is_pipe_forbidden( tok_last( &tok ) ) )
					{
						if( is_pipeline )
						{
							err=1;
							if( babble )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( &tok ),
									   EXEC_ERR_MSG );

								print_errors();

							}
						}
						forbid_pipeline = 1;
					}

					/*
					  Test that the case builtin is only used in a switch block
					*/
					if( wcscmp( L"case", tok_last( &tok ) )==0 )
					{
						if( !count || block_type[count-1]!=SWITCH )
						{
							err=1;

//							debug( 2, L"Error on block type %d\n", block_type[count-1] );


							if( babble )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( &tok ),
									   INVALID_CASE_ERR_MSG );

								print_errors();
							}
						}
					}

					/*
					  Test that break and continue are only used in loop blocks
					*/
					if( wcscmp( L"break", tok_last( &tok ) )==0 ||
						wcscmp( L"continue", tok_last( &tok ) )==0)
					{
						int found_loop=0;
						int i;
						for( i=count-1; i>=0; i-- )
						{
							if( (block_type[i]==WHILE) ||
								(block_type[i]==FOR) )
							{
								found_loop=1;
								break;
							}
						}

						if( !found_loop )
						{
							err=1;

							if( babble )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( &tok ),
									   INVALID_LOOP_ERR_MSG );
								print_errors();
							}
						}
					}

					/*
					  Test that else is only used in an if-block
					*/
					if( wcscmp( L"else", tok_last( &tok ) )==0 )
					{
						if( !count || block_type[count-1]!=IF )
						{
							err=1;
							if( babble )
							{
								error( SYNTAX_ERROR,
									   tok_get_pos( &tok ),
									   INVALID_ELSE_ERR_MSG );

								print_errors();
							}
						}

					}

					/*
					  Test that end is not used when not inside a blovk
					*/
					if( count < 0 )
					{
						err = 1;
						if( babble )
						{
							error( SYNTAX_ERROR,
								   tok_get_pos( &tok ),
								   INVALID_END_ERR_MSG );
							print_errors();
						}
					}
				}
				break;
			}

			case TOK_REDIRECT_OUT:
			case TOK_REDIRECT_IN:
			case TOK_REDIRECT_APPEND:
			case TOK_REDIRECT_FD:
			{
				if( !had_cmd )
				{
					err = 1;
					if( babble )
					{
						error( SYNTAX_ERROR,
							   tok_get_pos( &tok ),
							   INVALID_REDIRECTION_ERR_MSG );
						print_errors();
					}
				}
				break;
			}

			case TOK_END:
			{
				if( needs_cmd && !had_cmd )
				{
					err = 1;
					if( babble )
					{
						error( SYNTAX_ERROR,
							   tok_get_pos( &tok ),
							   CMD_ERR_MSG,
							   tok_get_desc( tok_last_type(&tok)));
						print_errors();
					}
				}
				needs_cmd=0;
				had_cmd = 0;
				is_pipeline=0;
				forbid_pipeline=0;
				break;
			}

			case TOK_PIPE:
			{
				if( forbid_pipeline )
				{
					err=1;
					if( babble )
					{
						error( SYNTAX_ERROR,
							   tok_get_pos( &tok ),
							   EXEC_ERR_MSG );

						print_errors();
					}
				}
				needs_cmd=0;
				is_pipeline=1;
			}


			case TOK_BACKGROUND:
			{
				if( needs_cmd && !had_cmd )
				{
					err = 1;
					if( babble )
					{
						error( SYNTAX_ERROR,
							   tok_get_pos( &tok ),
							   CMD_ERR_MSG,
							   tok_get_desc( tok_last_type(&tok)));

						print_errors();
					}
				}

				if( had_cmd )
				{
					had_cmd = 0;
				}
				break;
			}

			case TOK_ERROR:
			default:
				err = 1;
				if( babble )
				{
					error( SYNTAX_ERROR,
						   tok_get_pos( &tok ),
						   TOK_ERR_MSG,
						   tok_last(&tok) );


					print_errors();
					//debug( 2, tok_last( &tok) );
				}
				break;
		}
	}

	if( require_additional_commands )
	{
		err=1;
		if( babble )
		{
			error( SYNTAX_ERROR,
				   tok_get_pos( &tok ),
				   COND_ERR_MSG );

			print_errors();
		}
	}


	if( babble && count>0 )
	{
		error( SYNTAX_ERROR,
			   block_pos[count-1],
			   BLOCK_END_ERR_MSG );
		print_errors();
	}

	tok_destroy( &tok );


	current_tokenizer=previous_tokenizer;
	current_tokenizer_pos = previous_pos;

	error_code=0;

	return err | ((count!=0)<<1);
}

