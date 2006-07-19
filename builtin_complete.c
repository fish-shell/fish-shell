/** \file builtin_complete.c Functions defining the complete builtin

Functions used for implementing the complete builtin. 

*/
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>

#include "config.h"

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "wgetopt.h"
#include "parser.h"
#include "reader.h"


/**
   Internal storage for the builtin_get_temporary_buffer() function.
*/
const static wchar_t *temporary_buffer;

/*
  builtin_complete_* are a set of rather silly looping functions that
  make sure that all the proper combinations of complete_add or
  complete_remove get called. This is needed since complete allows you
  to specify multiple switches on a single commandline, like 'complete
  -s a -s b -s c', but the complete_add function only accepts one
  short switch and one long switch.
*/

/**
   Silly function
*/
static void	builtin_complete_add2( const wchar_t *cmd,
								   int cmd_type,
								   const wchar_t *short_opt,
								   array_list_t *gnu_opt,
								   array_list_t *old_opt, 
								   int result_mode, 
								   int authorative,
								   const wchar_t *condition,
								   const wchar_t *comp,
								   const wchar_t *desc )
{
	int i;
	const wchar_t *s;
	
	for( s=short_opt; *s; s++ )
	{
		complete_add( cmd, 
					  cmd_type,
					  *s,
					  0,
					  0,
					  result_mode,
					  authorative,
					  condition,
					  comp,
					  desc );
	}
	
	for( i=0; i<al_get_count( gnu_opt ); i++ )
	{
		complete_add( cmd, 
					  cmd_type,
					  0,
					  (wchar_t *)al_get(gnu_opt, i ),
					  0,
					  result_mode,
					  authorative,
					  condition,
					  comp,
					  desc );
	}
	
	for( i=0; i<al_get_count( old_opt ); i++ )
	{
		complete_add( cmd, 
					  cmd_type,
					  0,
					  (wchar_t *)al_get(old_opt, i ),
					  1,
					  result_mode,
					  authorative,
					  condition,
					  comp,
					  desc );
	}	

	if( al_get_count( old_opt )+al_get_count( gnu_opt )+wcslen(short_opt) == 0 )
	{
		complete_add( cmd, 
					  cmd_type,
					  0,
					  0,
					  0,
					  result_mode,
					  authorative,
					  condition,
					  comp,
					  desc );
	}	
}

/**
   Silly function
*/
static void	builtin_complete_add( array_list_t *cmd, 
								  array_list_t *path,
								  const wchar_t *short_opt,
								  array_list_t *gnu_opt,
								  array_list_t *old_opt, 
								  int result_mode, 
								  int authorative,
								  const wchar_t *condition,
								  const wchar_t *comp,
								  const wchar_t *desc )
{
	int i;
	
	for( i=0; i<al_get_count( cmd ); i++ )
	{
		builtin_complete_add2( al_get( cmd, i ),
							   COMMAND,
							   short_opt, 
							   gnu_opt,
							   old_opt, 
							   result_mode, 
							   authorative,
							   condition, 
							   comp, 
							   desc );
	}
	
	for( i=0; i<al_get_count( path ); i++ )
	{
		builtin_complete_add2( al_get( path, i ),
							   PATH,
							   short_opt, 
							   gnu_opt,
							   old_opt, 
							   result_mode, 
							   authorative,
							   condition, 
							   comp, 
							   desc );
	}
	
}

/**
   Silly function
*/
static void builtin_complete_remove3( wchar_t *cmd,
									  int cmd_type,
									  wchar_t short_opt,
									  array_list_t *long_opt )		
{
	int i;
	
	for( i=0; i<al_get_count( long_opt ); i++ )
	{
		complete_remove( cmd,
						 cmd_type,
						 short_opt,
						 (wchar_t *)al_get( long_opt, i ) );
	}	
}

/**
   Silly function
*/
static void	builtin_complete_remove2( wchar_t *cmd,
									  int cmd_type,
									  const wchar_t *short_opt,
									  array_list_t *gnu_opt,
									  array_list_t *old_opt )
{
	const wchar_t *s = (wchar_t *)short_opt;
	if( *s )
	{
		for( ; *s; s++ )
		{
			if( al_get_count( old_opt) + al_get_count( gnu_opt ) == 0 )
			{
				complete_remove(cmd,
								cmd_type,
								*s,
								0 );
				
			}
			else
			{
				builtin_complete_remove3( cmd,
										  cmd_type,
										  *s,
										  gnu_opt );
				builtin_complete_remove3( cmd,
										  cmd_type,
										  *s,
										  old_opt );
			}
		}
	}
	else
	{
		builtin_complete_remove3( cmd,
								  cmd_type,
								  0,
								  gnu_opt );
		builtin_complete_remove3( cmd,
								  cmd_type,
								  0,
								  old_opt );
										  
	}				

	
}

/**
   Silly function
*/
static void	builtin_complete_remove( array_list_t *cmd, 
									 array_list_t *path,
									 const wchar_t *short_opt,
									 array_list_t *gnu_opt,
									 array_list_t *old_opt )
{
	
	int i;
	
	for( i=0; i<al_get_count( cmd ); i++ )
	{
		builtin_complete_remove2( (wchar_t *)al_get( cmd, i ),
								  COMMAND,
								  short_opt, 
								  gnu_opt,
								  old_opt );
	}
	
	for( i=0; i<al_get_count( path ); i++ )
	{
		builtin_complete_remove2( (wchar_t *)al_get( path, i ),
								  PATH,
								  short_opt, 
								  gnu_opt,
								  old_opt );
	}
	
}


const wchar_t *builtin_complete_get_temporary_buffer()
{
	return temporary_buffer;
}

/**
   The complete builtin. Used for specifying programmable
   tab-completions. Calls the functions in complete.c for any heavy
   lifting. Defined in builtin_complete.c
*/
static int builtin_complete( wchar_t **argv )
{
	int res=0;
	int argc=0;
	int result_mode=SHARED;
	int remove = 0;
	int authorative = 1;
	
	string_buffer_t short_opt;
	array_list_t gnu_opt, old_opt;
	wchar_t *comp=L"", *desc=L"", *condition=L"";

	wchar_t *do_complete = 0;
	
	array_list_t cmd;
	array_list_t path;

	static int recursion_level=0;
	
	if( !is_interactive_session )
	{
		debug( 1, _(L"%ls: Command only available in interactive sessions"), argv[0] );
	}
	
	al_init( &cmd );
	al_init( &path );
	sb_init( &short_opt );
	al_init( &gnu_opt );
	al_init( &old_opt );
	
	argc = builtin_count_args( argv );	
	
	woptind=0;
	
	while( res == 0 )
	{
		const static struct woption
			long_options[] =
			{
				{
					L"exclusive", no_argument, 0, 'x' 
				}
				,
				{
					L"no-files", no_argument, 0, 'f' 
				}
				,
				{
					L"require-parameter", no_argument, 0, 'r' 
				}
				,
				{
					L"path", required_argument, 0, 'p'
				}
				,					
				{
					L"command", required_argument, 0, 'c' 
				}
				,					
				{
					L"short-option", required_argument, 0, 's' 
				}
				,
				{
					L"long-option", required_argument, 0, 'l'
				}
				,
				{
					L"old-option", required_argument, 0, 'o' 
				}
				,
				{
					L"description", required_argument, 0, 'd'
				}
				,
				{
					L"arguments", required_argument, 0, 'a'
				}
				,
				{
					L"erase", no_argument, 0, 'e'
				}
				,
				{
					L"unauthorative", no_argument, 0, 'u'
				}
				,
				{
					L"condition", required_argument, 0, 'n'
				}
				,
				{
					L"do-complete", required_argument, 0, 'C'
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
								L"a:c:p:s:l:o:d:frxeun:C::h", 
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
				sb_append( sb_err, 
						   parser_current_line() );
				
				builtin_print_help( argv[0], sb_err );

				
				res = 1;
				break;
				
			case 'x':					
				result_mode |= EXCLUSIVE;
				break;
					
			case 'f':					
				result_mode |= NO_FILES;
				break;
				
			case 'r':					
				result_mode |= NO_COMMON;
				break;
					
			case 'p':	
			case 'c':
			{
				wchar_t *a = unescape( woptarg, 1);
				if( a )
				{
					al_push( (opt=='p'?&path:&cmd), a );
				}
				else
				{
					sb_printf( sb_err, L"%ls: Invalid token '%ls'\n", argv[0], woptarg );
					res = 1;					
				}				
				break;
			}
				
			case 'd':
				desc = woptarg;
				break;
				
			case 'u':
				authorative=0;
				break;
				
			case 's':
				sb_append( &short_opt, woptarg );
				break;
					
			case 'l':
				al_push( &gnu_opt, woptarg );
				break;
				
			case 'o':
				al_push( &old_opt, woptarg );
				break;

			case 'a':
				comp = woptarg;
				break;
				
			case 'e':
				remove = 1;
				break;

			case 'n':
				condition = woptarg;
				break;
				
			case 'C':
				do_complete = woptarg?woptarg:reader_get_buffer();
				break;
				
			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;
				
			case '?':
				sb_append( sb_err, 
						   parser_current_line() );
				builtin_print_help( argv[0], sb_err );
				
				res = 1;
				break;
				
		}
		
	}

	if( !res )
	{
		if( condition && wcslen( condition ) )
		{
			if( parser_test( condition, 0, 0 ) )
			{
				sb_printf( sb_err,
						   L"%ls: Condition '%ls' contained a syntax error\n", 
						   argv[0],
						   condition );
				
				parser_test( condition, sb_err, argv[0] );
				
				res = 1;
			}
		}
	}
	
	if( !res )
	{
		if( comp && wcslen( comp ) )
		{
			if( parser_test_args( comp, 0, 0 ) )
			{
				sb_printf( sb_err,
						   L"%ls: Completion '%ls' contained a syntax error\n", 
						   argv[0],
						   comp );
				
				parser_test_args( comp, sb_err, argv[0] );
				
				res = 1;
			}
		}
	}

	if( !res )
	{
		if( do_complete )
		{
			array_list_t comp;
			int i;

			const wchar_t *prev_temporary_buffer = temporary_buffer;
			temporary_buffer = do_complete;		

			if( recursion_level < 1 )
			{
				recursion_level++;
			
				al_init( &comp );
			
				complete( do_complete, &comp );
			
				for( i=0; i<al_get_count( &comp ); i++ )
				{
					wchar_t *next = (wchar_t *)al_get( &comp, i );
					wchar_t *sep = wcschr( next, COMPLETE_SEP );
					if( sep )
						*sep = L'\t';
					sb_printf( sb_out, L"%ls\n", next );
				}
			
				al_foreach( &comp, &free );
				al_destroy( &comp );
				recursion_level--;
			}
		
			temporary_buffer = prev_temporary_buffer;		
		
		}
		else if( woptind != argc )
		{
			sb_printf( sb_err, 
					   _( L"%ls: Too many arguments\n" ),
					   argv[0] );
			sb_append( sb_err, 
					   parser_current_line() );
			builtin_print_help( argv[0], sb_err );

			res = 1;
		}
		else if( (al_get_count( &cmd) == 0 ) && (al_get_count( &path) == 0 ) )
		{
			/* No arguments specified, meaning we print the definitions of
			 * all specified completions to stdout.*/
			complete_print( sb_out );		
		}
		else
		{
			if( remove )
			{
				builtin_complete_remove( &cmd,
										 &path,
										 (wchar_t *)short_opt.buff,
										 &gnu_opt,
										 &old_opt );									 
			}
			else
			{
				builtin_complete_add( &cmd, 
									  &path,
									  (wchar_t *)short_opt.buff,
									  &gnu_opt,
									  &old_opt, 
									  result_mode, 
									  authorative,
									  condition,
									  comp,
									  desc ); 
			}

		}	
	}
	
	al_foreach( &cmd, &free );
	al_foreach( &path, &free );

	al_destroy( &cmd );
	al_destroy( &path );
	sb_destroy( &short_opt );
	al_destroy( &gnu_opt );
	al_destroy( &old_opt );

	return res;
}
