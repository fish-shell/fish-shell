#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

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

#include <errno.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

/*
  Small utility for setting the color.
  Usage: set_color COLOR
  where COLOR is either an integer from 0 to seven or one of the strings in the col array.
*/

#define COLORS (sizeof(col)/sizeof(char *))

/**
   Program name
*/
#define SET_COLOR "set_color"

#if HAVE_GETTEXT
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

char *col[]=
{
	"black",
	"red",
	"green",
	"brown",
	"yellow",
	"blue",
	"magenta",
	"purple",
	"cyan",
	"white",
	"normal"
}
;

int col_idx[]=
{
	0, 
	1,
	2,
	3,
	3,
	4,
	5,
	5,
	6,
	7,
	8
}
;

void print_help();

int translate_color( char *str )
{
	char *endptr;
	int color;

	if( !str )
		return -1;
	

	color = strtol( str, &endptr, 10 );
	if(endptr<=str)
	{
		int i;
		color = -1;
		for( i=0; i<COLORS; i++ )
		{
			
			if( strcasecmp( col[i], str ) == 0 )
			{
				color = col_idx[i];				
				break;
			}
		}
	}	
	return color;
	
}

void print_colors()
{
	int i;
	for( i=0; i<COLORS; i++ )
	{
		printf( "%s\n", col[i] );
	}	
}

static void locale_init()
{
	setlocale( LC_ALL, "" );
#if HAVE_GETTEXT
	bindtextdomain( PACKAGE_NAME, LOCALEDIR );
	textdomain( PACKAGE_NAME );
#endif
}


int main( int argc, char **argv )
{
	char *bgcolor=0;
	char *fgcolor=0;
	int fg, bg;
	int bold=0;

	locale_init();
	
		
	while( 1 )
	{
#ifdef HAVE_GETOPT_LONG
		static struct option
			long_options[] =
			{
				{
					"background", required_argument, 0, 'b' 
				}
				,
				{
					"help", no_argument, 0, 'h' 
				}
				,
				{
					"bold", no_argument, 0, 'o' 
				}
				,
				{
					"version", no_argument, 0, 'v' 
				}
				,
				{
					"print-colors", no_argument, 0, 'c' 
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
							   "b:hvoc", 
							   long_options, 
							   &opt_index );
#else
		int opt = getopt( argc,
						  argv, 
						  "b:hvoc" );
#endif
		if( opt == -1 )
			break;
			
		switch( opt )
		{
			case 0:
				break;
				
			case 'b':				
				bgcolor = optarg;
				break;
			case 'h':
				print_help();
				exit(0);				
								
			case 'o':
				bold=1;
				break;
				
			case 'v':
				fprintf( stderr, _("%s, version %s\n"), SET_COLOR, PACKAGE_VERSION );
				exit( 0 );								

			case 'c':
				print_colors();
				exit(0);
				
			case '?':
				return 1;
				
		}
		
	}		
	
	switch( argc-optind)
	{
		case 0:
//			printf( "no fg\n" );
			break;
			
		case 1:
			fgcolor=argv[optind];
//			printf( "fg %s\n", fgcolor );
			break;
			
		default:
			printf( _("%s: Too many arguments\n"), SET_COLOR );
			return 1;
	}

	if( !fgcolor && !bgcolor && !bold )
	{
		fprintf( stderr, _("%s: Expected an argument\n"), SET_COLOR );
		print_help();		
		return 1;
	}
	
	fg = translate_color(fgcolor);
	if( fgcolor && (fg==-1))
	{
		fprintf( stderr, _("%s: Unknown color '%s'\n"), SET_COLOR, fgcolor );
		return 1;		
	}

	bg = translate_color(bgcolor);
	if( bgcolor && (bg==-1))
	{
		fprintf( stderr, _("%s: Unknown color '%s'\n"), SET_COLOR, bgcolor );
		return 1;		
	}

	setupterm( 0, STDOUT_FILENO, 0);

	if( bold )
	{
		putp( enter_bold_mode );
	}

	if( bgcolor )
	{
		if( bg == 8 )
		{
			putp( tparm( set_a_background, 0) );
			putp( tparm(exit_attribute_mode) );		
		}	
	}

	if( fgcolor )
	{
		if( fg == 8 )
		{
			putp( tparm( set_a_foreground, 0) );
			putp( tparm(exit_attribute_mode) );		
		}	
		else
		{
			putp( tparm( set_a_foreground, fg) );
		}
	}
	
	if( bgcolor )
	{
		if( bg != 8 )
		{
			putp( tparm( set_a_background, bg) );
		}
	}
	del_curterm( cur_term );
}
