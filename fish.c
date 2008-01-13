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


/** \file fish.c
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
#include "halloc.h"
#include "halloc_util.h"
#include "history.h"
#include "path.h"

/**
   The string describing the single-character options accepted by the main fish binary
*/
#define GETOPT_STRING "hilnvc:p:d:"

/**
   Parse init files
*/
static int read_init()
{
	wchar_t *config_dir;
	wchar_t *config_dir_escaped;
	void *context;
	string_buffer_t *eval_buff;

	eval( L"builtin . " DATADIR "/fish/config.fish 2>/dev/null", 0, TOP );
	eval( L"builtin . " SYSCONFDIR L"/fish/config.fish 2>/dev/null", 0, TOP );
	
	/*
	  We need to get the configuration directory before we can source the user configuration file
	*/
	context = halloc( 0, 0 );
	eval_buff = sb_halloc( context );
	config_dir = path_get_config( context );

	/*
	  If config_dir is null then we have no configuration directory
	  and no custom config to load.
	*/
	if( config_dir )
	{
		config_dir_escaped = escape( config_dir, 1 );
		sb_printf( eval_buff, L"builtin . %ls/config.fish 2>/dev/null", config_dir_escaped );
		eval( (wchar_t *)eval_buff->buff, 0, TOP );
		free( config_dir_escaped );
	}
	
	halloc_free( context );
	
	return 1;
}


/**
  Parse the argument list, return the index of the first non-switch
  arguments.
 */
static int fish_parse_opt( int argc, char **argv, char **cmd_ptr )
{
	int my_optind;
	int force_interactive=0;
		
	while( 1 )
	{
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
					"no-execute", no_argument, 0, 'n' 
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
			
			case 'c':		
			{
				*cmd_ptr = optarg;				
				is_interactive_session = 0;
				break;
			}
			
			case 'd':		
			{
				char *end;
				int tmp;

				errno = 0;
				tmp = strtol(optarg, &end, 10);
				
				if( tmp >= 0 && tmp <=10 && !*end && !errno )
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
			{
				*cmd_ptr = "__fish_print_help fish";
				break;
			}
			
			case 'i':
			{
				force_interactive = 1;
				break;				
			}
			
			case 'l':
			{
				is_login=1;
				break;				
			}
			
			case 'n':
			{
				no_exec=1;
				break;				
			}
			
			case 'p':
			{
				profile = optarg;
				break;				
			}
			
			case 'v':
			{
				fwprintf( stderr, 
						  _(L"%s, version %s\n"), 
						  PACKAGE_NAME,
						  PACKAGE_VERSION );
				exit( 0 );				
			}
			
			case '?':
			{
				exit( 1 );
			}
			
		}		
	}

	my_optind = optind;
	
	is_login |= (strcmp( argv[0], "-fish") == 0);
		
	/*
	  We are an interactive session if we have not been given an
	  explicit command to execute, _and_ stdin is a tty.
	 */
	is_interactive_session &= (*cmd_ptr == 0);
	is_interactive_session &= (my_optind == argc);
	is_interactive_session &= isatty(STDIN_FILENO);	

	/*
	  We are also an interactive session if we have are forced-
	 */
	is_interactive_session |= force_interactive;

	return my_optind;
}


/**
   Calls a bunch of init functions, parses the init files and then
   parses commands from stdin or files, depending on arguments
*/

int main( int argc, char **argv )
{
	int res=1;
	char *cmd=0;
	int my_optind=0;

	halloc_util_init();	

	wsetlocale( LC_ALL, L"" );
	is_interactive_session=1;
	program_name=L"fish";


	my_optind = fish_parse_opt( argc, argv, &cmd );

	/*
	  No-exec is prohibited when in interactive mode
	*/
	if( is_interactive_session && no_exec)
	{
		debug( 1, _(L"Can not use the no-execute mode when running an interactive session") );
		no_exec = 0;
	}
	
	proc_init();	
	event_init();	
	wutil_init();
	parser_init();
	builtin_init();
	function_init();
	env_init();
	reader_init();
	history_init();

	if( read_init() )
	{
		if( cmd != 0 )
		{
			wchar_t *cmd_wcs = str2wcs( cmd );
			res = eval( cmd_wcs, 0, TOP );
			free(cmd_wcs);
			reader_exit(0, 0);
		}
		else
		{
			if( my_optind == argc )
			{
				res = reader_read( 0, 0 );				
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

				if( *(argv+2))
				{
					sb_init( &sb );
				
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
				{
					abs_filename = wcsdup(rel_filename);
				}

				reader_push_current_filename( intern( abs_filename ) );
				free( rel_filename );
				free( abs_filename );

				res = reader_read( fd, 0 );

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
	
	history_destroy();
	proc_destroy();
	builtin_destroy();
	function_destroy();
	reader_destroy();
	parser_destroy();
	wutil_destroy();
	event_destroy();
	
	halloc_util_destroy();
	
	env_destroy();
	
	intern_free_all();
	
	return res?STATUS_UNKNOWN_COMMAND:proc_get_last_status();	
}
