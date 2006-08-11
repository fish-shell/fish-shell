/** \file builtin_commandline.c Functions defining the commandline builtin

Functions used for implementing the commandline builtin. 

*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "common.h"
#include "wgetopt.h"
#include "reader.h"
#include "proc.h"
#include "parser.h"
#include "tokenizer.h"
#include "input_common.h"
#include "input.h"

#include "parse_util.h"

/**
   Which part of the comandbuffer are we operating on
*/
enum
{
	STRING_MODE=1, /**< Operate on entire buffer */
	JOB_MODE, /**< Operate on job under cursor */
	PROCESS_MODE, /**< Operate on process under cursor */
	TOKEN_MODE /**< Operate on token under cursor */
}
	;

/**
   For text insertion, how should it be done
*/
enum
{
	REPLACE_MODE=1, /**< Replace current text */
	INSERT_MODE, /**< Insert at cursor position */
	APPEND_MODE /**< Insert at end of current token/command/buffer */
}
	;

/**
   Returns the current commandline buffer.
*/
static const wchar_t *get_buffer()
{
	const wchar_t *buff = builtin_complete_get_temporary_buffer();
	if( !buff )
		buff = reader_get_buffer();
	return buff;
}

/**
   Returns the position of the cursor
*/
static int get_cursor_pos()
{
	const wchar_t *buff = builtin_complete_get_temporary_buffer();
	if( buff )
		return wcslen( buff );
	else
		return reader_get_cursor_pos();
}


/**
   Replace/append/insert the selection with/at/after the specified string.

   \param begin beginning of selection
   \param end end of selection
   \param insert the string to insert
   \param append_mode can be one of REPLACE_MODE, INSERT_MODE or APPEND_MODE, affects the way the test update is performed
*/
static void replace_part( const wchar_t *begin,
						  const wchar_t *end,
						  wchar_t *insert,
						  int append_mode )
{
	const wchar_t *buff = get_buffer();
	string_buffer_t out;
	int out_pos=get_cursor_pos();
					
	sb_init( &out );

	sb_append_substring( &out, buff, begin-buff );
					
	switch( append_mode)
	{
		case REPLACE_MODE:
		{
							
			sb_append( &out, insert );
			out_pos = wcslen( insert ) + (begin-buff);							
			break;
							
		}
		case APPEND_MODE:
		{
			sb_append_substring( &out, begin, end-begin );
			sb_append( &out, insert );
			break;
		}
		case INSERT_MODE:
		{
			int cursor = get_cursor_pos() -(begin-buff);
			sb_append_substring( &out, begin, cursor );
			sb_append( &out, insert );
			sb_append_substring( &out, begin+cursor, end-begin-cursor );
			out_pos +=  wcslen( insert );							
			break;
		}
	}
	sb_append( &out, end );
	reader_set_buffer( (wchar_t *)out.buff, out_pos );
	sb_destroy( &out );
}
	
/**
   Output the specified selection.

   \param begin start of selection
   \param end  end of selection
   \param cut_at_cursor whether printing should stop at the surrent cursor position
   \param tokenize whether the string should be tokenized, printing one string token on every line and skipping non-string tokens
*/
static void write_part( const wchar_t *begin, 
						const wchar_t *end, 
						int cut_at_cursor, 
						int tokenize )
{	
	tokenizer tok;
	string_buffer_t out;
	wchar_t *buff;
	int pos;

	pos = get_cursor_pos()-(begin-get_buffer());

	if( tokenize )
	{
					
		buff = wcsndup( begin, end-begin );
//		fwprintf( stderr, L"Subshell: %ls, end char %lc\n", buff, *end );
		sb_init( &out );
					
		for( tok_init( &tok, buff, TOK_ACCEPT_UNFINISHED );
			 tok_has_next( &tok );
			 tok_next( &tok ) )
		{
			if( (cut_at_cursor) &&
				(tok_get_pos( &tok)+wcslen(tok_last( &tok)) >= pos) )
				break;
		
//			fwprintf( stderr, L"Next token %ls\n", tok_last( &tok ) );

			switch( tok_last_type( &tok ) )
			{
				case TOK_STRING:
					sb_append2( &out, tok_last( &tok), L"\n", (void *)0 );
					break;
								
			}						
		}
					
		if( out.buff )
			sb_append( sb_out,
					   (wchar_t *)out.buff );
					
		free( buff );
		tok_destroy( &tok );
		sb_destroy( &out );
	}
	else
	{
		if( cut_at_cursor )
		{
			end = begin+pos;			
		}
		sb_append_substring( sb_out, begin, end-begin );		
		sb_append( sb_out, L"\n" );		
	}
}


/**
   The commandline builtin. It is used for specifying a new value for
   the commandline.
*/
static int builtin_commandline( wchar_t **argv )
{

	int buffer_part=0;
	int cut_at_cursor=0;
	
	int argc = builtin_count_args( argv );
	int append_mode=0;

	int function_mode = 0;
	
	int tokenize = 0;	

	if( !get_buffer() )
	{
		sb_append2( sb_err,
					argv[0],
					L": Can not set commandline in non-interactive mode\n",
					(void *)0 );
		builtin_print_help( argv[0], sb_err );
		return 1;		
	}	

	woptind=0;

	while( 1 )
	{
		const static struct woption
			long_options[] =
			{
				{
					L"append", no_argument, 0, 'a'
				}
				,
				{
					L"insert", no_argument, 0, 'i'
				}
				,
				{
					L"replace", no_argument, 0, 'r'
				}
				,
				{
					L"current-job", no_argument, 0, 'j'
				}
				,
				{
					L"current-process", no_argument, 0, 'p'
				}
				,
				{
					L"current-token", no_argument, 0, 't'
				}
				,		
				{
					L"current-buffer", no_argument, 0, 'b'
				}
				,		
				{
					L"cut-at-cursor", no_argument, 0, 'c'
				}
				,
				{
					L"function", no_argument, 0, 'f'
				}
				,
				{
					L"tokenize", no_argument, 0, 'o'
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
								L"aijpctwforh", 
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
				
			case L'a':
				append_mode = APPEND_MODE;
				break;
				
			case L'i':
				append_mode = INSERT_MODE;
				break;

			case L'r':
				append_mode = REPLACE_MODE;
				break;
								
			case 'c':
				cut_at_cursor=1;
				break;
				
			case 't':
				buffer_part = TOKEN_MODE;
				break;

			case 'j':
				buffer_part = JOB_MODE;
				break;

			case 'p':
				buffer_part = PROCESS_MODE;
				break;

			case 'f':
				function_mode=1;
				break;

			case 'o':
				tokenize=1;
				break;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case L'?':
				builtin_print_help( argv[0], sb_err );				
				return 1;	
		}
	}		

	if( function_mode )
	{
		int i;
		
		/*
		  Check for invalid switch combinations
		*/
		if( buffer_part || cut_at_cursor || append_mode || tokenize )
		{
			sb_printf(sb_err,
					  BUILTIN_ERR_COMBO,
					  argv[0] );
			
			sb_append2(sb_err,
					   parser_current_line(),
					   L"\n",
					   (void *)0);
			return 1;
		}
		

		if( argc == woptind )
		{
			sb_printf( sb_err,
					   BUILTIN_ERR_MISSING,
					   argv[0] );
			
			sb_append2( sb_err,
						parser_current_line(),
						L"\n",
						(void *)0 );
			builtin_print_help( argv[0], sb_err );
			return 1;
 		}
		for( i=woptind; i<argc; i++ )
		{
			wint_t c = input_get_code( argv[i] );
			if( c != -1 )
			{
//				fwprintf( stderr, L"Add function %ls (%d)\n", argv[i], c );
				/*
				  input_unreadch inserts the specified keypress or
				  readline function at the top of the stack of unused
				  keypresses
				*/
				input_unreadch(c);
			}
			else
			{
//				fwprintf( stderr, L"BLUR %ls %d\n", argv[i], c );
				sb_append2( sb_err,
							argv[0],
							L": Unknown readline function '",
							argv[i],
							L"'\n",
							parser_current_line(),
							L"\n",
							(void *)0 );
				builtin_print_help( argv[0], sb_err );
				return 1;
			}
		}
		
		return 0;		
	}
	
	/*
	  Check for invalid switch combinations
	*/
	if( argc-woptind > 1 )
	{
		
		sb_append2( sb_err,
					argv[0],
					L": Too many arguments\n",
					(void *)0 );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( (tokenize || cut_at_cursor) && (argc-woptind) )
	{
		
		sb_printf( sb_err,
				   BUILTIN_ERR_COMBO2,
				   argv[0],
				   L"--cut-at-cursor and --tokenize can not be used when setting the commandline" );
		

		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( append_mode && !(argc-woptind) )
	{
		sb_printf( sb_err,
                    BUILTIN_ERR_COMBO2,
                    argv[0],
				   L"insertion mode switches can not be used when not in insertion mode" );

        builtin_print_help( argv[0], sb_err );		
        return 1;
	}

	/*
	  Set default modes
	*/
	if( !append_mode )
	{
		append_mode = REPLACE_MODE;
	}
	
	if( !buffer_part )
	{
		buffer_part = STRING_MODE;
	}
		
	wchar_t *begin, *end;
	
	switch( buffer_part )
	{
		case STRING_MODE:
		{			
			begin = (wchar_t *)get_buffer();
			end = begin+wcslen(begin);
			break;			
		}

		case PROCESS_MODE:
		{
			parse_util_process_extent( get_buffer(),
									   get_cursor_pos(),
									   &begin, 
									   &end );
			break;
		}
		
		case JOB_MODE:
		{
			parse_util_job_extent( get_buffer(),
								   get_cursor_pos(),
								   &begin,
								   &end );					
			break;			
		}
		
		case TOKEN_MODE:
		{
			parse_util_token_extent( get_buffer(),
									 get_cursor_pos(),
									 &begin, 
									 &end, 
									 0, 0 );
			break;			
		}
				
	}

	switch(argc-woptind)
	{
		case 0:
		{					
			write_part( begin, end, cut_at_cursor, tokenize );
			break;
		}
		
		case 1:
		{
			replace_part( begin, end, argv[woptind], append_mode );
			break;					
		}		
	}
	
	return 0;
}
