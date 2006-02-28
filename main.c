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

#include "fallback.h"
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
#include "translate.h"
#include "halloc_util.h"

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
	
	eval( L"builtin cd " DATADIR L"/fish 2>/dev/null; . fish 2>/dev/null", 0, TOP );
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

	wsetlocale( LC_ALL, L"" );
	is_interactive_session=1;
	program_name=L"fish";
	
	while( 1 )
	{
#ifdef HAVE_GETOPT_LONG
		static struct option
			long_options[] =
			{
				{
					"command", required_argument, 0, 'c' 
				}
				,
				{
					"debug-level", required_argument, 0, 'd' 
				}
				,
				{
					"interactive", no_argument, 0, 'i' 
				}
				,
				{
					"login", no_argument, 0, 'l' 
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
							   "hilvc:p:d:", 
							   long_options, 
							   &opt_index );
		
#else	
		int opt = getopt( argc,
						  argv, 
						  "hilvc:p:d:" );
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

			case 'd':		
			{
				char *end;
				int tmp = strtol(optarg, &end, 10);
				if( tmp >= 0 && tmp <=10 && !*end )
				{
					debug_level=tmp;
				}
				else
				{
					debug( 0, _(L"Invalid value '%s' for debug level switch"), optarg );
					exit(1);
				}
				break;
			}
			
			case 'h':
				cmd = "help";
				break;

			case 'i':
				force_interactive = 1;
				break;				
				
			case 'l':
				is_login=1;
				break;				
				
			case 'p':
				profile = optarg;
				break;				
				
			case 'v':
				fwprintf( stderr, 
						  _(L"%s, version %s\n"), 
						  PACKAGE_NAME,
						  PACKAGE_VERSION );
				exit( 0 );				

			case '?':
				return 1;
				
		}		
	}

	my_optind = optind;
	
	is_login |= (strcmp( argv[0], "-fish") == 0);
		
	is_interactive_session &= (cmd == 0);
	is_interactive_session &= (my_optind == argc);
	is_interactive_session &= isatty(STDIN_FILENO);	
	is_interactive_session |= force_interactive;

	common_init();
	halloc_util_init();	

	proc_init();	
	event_init();	
	exec_init();	
	wutil_init();
	parser_init();
	builtin_init();
	function_init();
	env_init();
	complete_init();
	reader_init();
	
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
				res = reader_read( 0 );				
			}
			else
			{
				char **ptr; 
				char *file = *(argv+1);
				int i; 
				string_buffer_t sb;
				int fd;
				wchar_t *rel_filename, *abs_filename;
								
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

				rel_filename = str2wcs( file );
				abs_filename = wrealpath( rel_filename, 0 );
				if( !abs_filename )
					abs_filename = wcsdup(rel_filename);
				reader_push_current_filename( intern( abs_filename ) );
				free( rel_filename );
				free( abs_filename );

				res = reader_read( fd );

				if( res )
				{
					debug( 1, 
						   _(L"Error while reading file %ls\n"), 
						   reader_current_filename()?reader_current_filename(): _(L"Standard input") );
				}				
				reader_pop_current_filename();
			}
		}
	}

	proc_fire_event( L"PROCESS_EXIT", EVENT_EXIT, getpid(), res );

	complete_destroy();
	proc_destroy();
	env_destroy();
	builtin_destroy();
	function_destroy();
	reader_destroy();
	parser_destroy();
	wutil_destroy();
	exec_destroy();	
	event_destroy();

	common_destroy();
	halloc_util_destroy();
	intern_free_all();

	return res;	
}
