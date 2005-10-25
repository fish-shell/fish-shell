/*
Copyright (C) 2005 Axel Liljencrantz

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


/** \file main.c
	The main loop of <tt>fish</tt>.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <locale.h>
#include <signal.h>

#include "util.h"
#include "common.h"
#include "reader.h"
#include "builtin.h"
#include "function.h"
#include "complete.h"
#include "wutil.h"
#include "env.h"
#include "sanity.h"
#include "proc.h"
#include "parser.h"
#include "expand.h"
#include "intern.h"
#include "exec.h"
#include "event.h"
#include "output.h"

/**
   Parse init files
*/
static int read_init()
{
	char cwd[4096];
	wchar_t *wcwd;
	
	if( !getcwd( cwd, 4096 ) )
	{
		wperror( L"getcwd" );		
		return 0;
	}

	env_set( L"__fish_help_dir", DOCDIR, 0);	
 	
	eval( L"builtin cd " SYSCONFDIR L" 2>/dev/null; . fish 2>/dev/null", 0, TOP );
	eval( L"builtin cd 2>/dev/null;. .fish 2>/dev/null", 0, TOP );
	
	if( chdir( cwd ) == -1 )
	{
//		fwprintf( stderr, L"Invalid directory: %s\n", cwd );
//		wperror( L"chdir" );
//		return 0;
	}	
	wcwd = str2wcs( cwd );
	if( wcwd )
	{
		env_set( L"PWD", wcwd, ENV_EXPORT );
		free( wcwd );
	}

	return 1;
}

/**
   Calls a bunch of init functions, parses the init files and then
   parses commands from stdin or files, depending on arguments
*/

int main( int argc, char **argv )
{
	int res=1;
	int force_interactive=0;
	int my_optind;
	
	char *cmd=0;	

	fish_setlocale( LC_ALL, L"" );
	is_interactive_session=1;
	program_name=L"fish";
	
	while( 1 )
	{
#ifdef __GLIBC__
		static struct option
			long_options[] =
			{
				{
					"command", required_argument, 0, 'c' 
				}
				,
				{
					"interactive", no_argument, 0, 'i' 
				}
				,
				{
					"profile", required_argument, 0, 'p' 
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
							   "hivc:p:", 
							   long_options, 
							   &opt_index );
		
#else	
		int opt = getopt( argc,
						  argv, 
						  "hivc:p:" );
#endif
		if( opt == -1 )
			break;
		
		switch( opt )
		{
			case 0:
				break;
				
			case 'c':		
				cmd = optarg;				
				is_interactive_session = 0;
				break;

			case 'h':
				cmd = "help";
				//interactive=0;
				
				break;

			case 'i':
				force_interactive = 1;
				break;				
				
			case 'p':
				profile = optarg;
				break;				
				
			case 'v':
				fwprintf( stderr, 
						  L"%s, version %s\n", 
						  PACKAGE_NAME,
						  PACKAGE_VERSION );
				exit( 0 );				

			case '?':
				return 1;
				
		}		
	}

	my_optind = optind;
	
	is_login |= strcmp( argv[0], "-fish") == 0;
//	fwprintf( stderr, L"%s\n", argv[0] );
		
	is_interactive_session &= (cmd == 0);
	is_interactive_session &= (my_optind == argc);
	is_interactive_session &= isatty(STDIN_FILENO);	

//	fwprintf( stderr, L"%d %d %d\n", cmd==0, my_optind == argc, isatty(STDIN_FILENO) );

	if( force_interactive )
		is_interactive_session=1;	

	proc_init();	
	output_init();	
	event_init();	
	exec_init();	
	parser_init();
	builtin_init();
	function_init();
	env_init();
	complete_init();
	reader_init();

	reader_push_current_filename( L"(internal)" );

	if( read_init() )
	{
		if( cmd != 0 )
		{
			wchar_t *cmd_wcs = str2wcs( cmd );
			res = eval( cmd_wcs, 0, TOP );
			free(cmd_wcs);
			reader_exit(0);			
		}
		else
		{
			if( my_optind == argc )
			{
				reader_push_current_filename( L"(stdin)" );
				res = reader_read( 0 );				
				reader_pop_current_filename();
			}
			else
			{
				char **ptr; 
				char *file = *(argv+1);
				int i; 
				string_buffer_t sb;
				int fd;
				
				if( ( fd = open(file, O_RDONLY) ) == -1 )
				{
					wperror( L"open" );
					return 1;
				}

				sb_init( &sb );
				
				if( *(argv+2))
				{
					for( i=1,ptr = argv+2; *ptr; i++, ptr++ )
					{
						if( i != 1 )
							sb_append( &sb, ARRAY_SEP_STR );
						wchar_t *val = str2wcs( *ptr );
						sb_append( &sb, val );
						free( val );
					}
				
					env_set( L"argv", (wchar_t *)sb.buff, 0 );
					sb_destroy( &sb );
				}
				
				reader_push_current_filename( str2wcs( file ) );
				res = reader_read( fd );

				if( res )
				{
					debug( 1, 
						   L"Error while reading file %ls\n", 
						   reader_current_filename() );
				}				
				free(reader_pop_current_filename());
			}
		}
	}

	
	if( function_exists(L"fish_on_exit"))
	{
		eval( L"fish_on_exit", 0, TOP );
	}
	
	reader_pop_current_filename();	
	
	proc_destroy();
	env_destroy();
	builtin_destroy();
	function_destroy();
	complete_destroy();
	reader_destroy();
	parser_destroy();
	wutil_destroy();
	common_destroy();
	exec_destroy();	
	event_destroy();
	output_destroy();
	
	intern_free_all();

	return res;	
}
