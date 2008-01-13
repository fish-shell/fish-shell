/*
Copyright (C) 2005-2008 Axel Liljencrantz

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


/** \file fish_indent.c
	The fish_indent proegram.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <locale.h>

#include "fallback.h"
#include "util.h"
#include "common.h"
#include "wutil.h"
#include "halloc.h"
#include "halloc_util.h"
#include "tokenizer.h"
#include "print_help.h"
#include "parser_keywords.h"

/**
   The string describing the single-character options accepted by the main fish binary
*/
#define GETOPT_STRING "hvi"

/**
   Read the entire contents of a file into the specified string_Buffer_t
 */
static void read_file( FILE *f, string_buffer_t *b )
{
	while( 1 )
	{
		errno=0;
		wint_t c = fgetwc( f );
		if( c == WEOF )
		{
			if( errno )
			{
				wperror(L"fgetwc");
				exit(1);
			}
			
			break;
		}
		
		sb_append_char( b, c );
	}
}

/**
   Insert the specified number of tabe into the output buffer
 */
static void insert_tabs( string_buffer_t *out, int indent )
{
	int i;
	
	for( i=0; i<indent; i++ )
	{
		sb_append( out, L"\t" );
	}
	
}

/**
   Indent the specified input
 */
static int indent( string_buffer_t *out, wchar_t *in, int flags )
{
	tokenizer tok;
	int res=0;
	int is_command = 1;
	int indent = 0;
	int do_indent = 1;
	int prev_type = 0;
	int prev_prev_type = 0;

	tok_init( &tok, in, TOK_SHOW_COMMENTS );
	
	for( ; tok_has_next( &tok ); tok_next( &tok ) )
	{
		int type = tok_last_type( &tok );
		wchar_t *last = tok_last( &tok );
		
		switch( type )
		{
			case TOK_STRING:
			{
				if( is_command )
				{
					int next_indent = indent;
					is_command = 0;

					wchar_t *unesc = unescape( last, UNESCAPE_SPECIAL );
					
					if( parser_keywords_is_block( unesc ) )
					{
						next_indent++;
					}
					else if( wcscmp( unesc, L"else" ) == 0 )
					{
						indent--;
					}
					else if( wcscmp( unesc, L"end" ) == 0 )
					{
						indent--;
						next_indent--;
					}
					
	
					if( do_indent && flags)
					{
						insert_tabs( out, indent );
					}
					
					sb_printf( out, L"%ls", last );
					
					indent = next_indent;
					
				}
				else
				{
					sb_printf( out, L" %ls", last );
				}
				
				break;
			}
			
			case TOK_END:
			{
				if( prev_type != TOK_END || prev_prev_type != TOK_END ) 
					sb_append( out, L"\n" );
				do_indent = 1;
				is_command = 1;
				break;
			}

			case TOK_PIPE:
			{
				sb_append( out, L" | " );
				is_command = 1;
				break;
			}
			
			case TOK_REDIRECT_OUT:
			case TOK_REDIRECT_APPEND:
			case TOK_REDIRECT_IN:
			case TOK_REDIRECT_FD:
			{
				sb_append( out, last );
				switch( type )
				{
					case TOK_REDIRECT_OUT:
						sb_append( out, L"> " );
						break;
						
					case TOK_REDIRECT_APPEND:
						sb_append( out, L">> " );
						break;
						
					case TOK_REDIRECT_IN:
						sb_append( out, L"< " );
						break;
						
					case TOK_REDIRECT_FD:
						sb_append( out, L">& " );
						break;
															
				}
				break;
			}
			

			case TOK_BACKGROUND:
			{
				sb_append( out, L"&\n" );
				do_indent = 1;
				is_command = 1;
				break;
			}
			

			case TOK_COMMENT:
			{
				if( do_indent && flags)
				{
					insert_tabs( out, indent );
				}
				
				sb_printf( out, L"%ls", last );
				do_indent = 1;
				break;				
			}
			
			default:
			{
				debug( 0, L"Unknown token '%ls'", last );
				exit(1);
			}			
		}
		
		prev_prev_type = prev_type;
		prev_type = type;
		
	}
	
	tok_destroy( &tok );

	return res;
}

/**
   Remove any prefix and suffix newlines from the specified
   string. Does not allocete a new string, edits the string in place
   and returns a pointer somewhere into the string.
 */
static wchar_t *trim( wchar_t *in )
{
	wchar_t *end;
	
	while( *in == L'\n' )
	{
		in++;
	}
	
	end = in + wcslen(in);
		
	while( 1 )
	{
		if( end < in+2 )
			break;

		end--;
		
		if( (*end == L'\n' ) && ( *(end-1) == L'\n' ) )
			*end=0;
		else
			break;
	}
	
	return in;
}


/**
   The main mathod. Run the program.
 */
int main( int argc, char **argv )
{
	string_buffer_t sb_in;
	string_buffer_t sb_out;
	
	int do_indent=1;
	
	wsetlocale( LC_ALL, L"" );
	program_name=L"fish_indent";

	while( 1 )
	{
		static struct option
			long_options[] =
			{
				{
					"no-indent", no_argument, 0, 'i' 
				}
				,
				{
					"help", no_argument, 0, 'h' 
				}
				,
				{
					"version", no_argument, 0, 'v' 
				}
				,
				{ 
					0, 0, 0, 0 
				}
			}
		;
		
		int opt_index = 0;
		
		int opt = getopt_long( argc,
				       argv, 
				       GETOPT_STRING,
				       long_options, 
				       &opt_index );
		
		if( opt == -1 )
			break;
		
		switch( opt )
		{
			case 0:
			{
				break;
			}
			
			case 'h':
			{
				print_help( "fish_indent", 1 );
				exit( 0 );				
				break;
			}
			
			case 'v':
			{
				fwprintf( stderr, 
						  _(L"%ls, version %s\n"), 
						  program_name,
						  PACKAGE_VERSION );
				exit( 0 );				
			}

			case 'i':
			{
				do_indent = 0;
				break;
			}
			
			
			case '?':
			{
				exit( 1 );
			}
			
		}		
	}

	halloc_util_init();	

	sb_init( &sb_in );
	sb_init( &sb_out );

	read_file( stdin, &sb_in );
	
	wutil_init();

	if( !indent( &sb_out, (wchar_t *)sb_in.buff, do_indent ) )
	{
		fwprintf( stdout, L"%ls", trim( (wchar_t *)sb_out.buff) );
	}
	else
	{
		/*
		  Indenting failed - print original input
		*/
		fwprintf( stdout, L"%ls", (wchar_t *)sb_in.buff );
	}
	

	wutil_destroy();

	halloc_util_destroy();

	return 0;
}
