/** file tokenize.c
  Small utility command for tokenizing an argument.
  \c tokenize is used for splitting a text string into separate parts (i.e. tokenizing) with a user supplied delimiter character. 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/**
   Print help message
*/
void print_help();

/**
   Main program
*/
int main( int argc, char **argv )
{
	char *delim = " \t";
	int empty_ok = 0;
	int i;
	extern int optind;	
	
	while( 1 )
	{
#ifdef HAVE_GETOPT_LONG
		static struct option
			long_options[] =
			{
				{
					"with-empty", no_argument, 0, 'e' 
				}
				,
				{
					"no-empty", no_argument, 0, 'n' 
				}
				,
				{
					"delimiter", required_argument, 0, 'd' 
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
							   "end:hv", 
							   long_options, 
							   &opt_index );
#else
		int opt = getopt( argc,
						  argv, 
						  "end:hv" );
#endif
		if( opt == -1 )
			break;
			
		switch( opt )
		{
			case 0:
				break;
				
			case 'e':				
				empty_ok = 1;
				break;

			case 'n':				
				empty_ok = 0;
				break;

			case 'd':				
				delim = optarg;
				break;
			case 'h':
				print_help();
				exit(0);				
								
			case 'v':
				printf( "tokenize, version %s\n", PACKAGE_VERSION );
				exit( 0 );								

			case '?':
				return 1;
				
		}
		
	}		
	
	for( i=optind; i<argc; i++ )
	{
		char *curr;
		int printed=0;
		for( curr = argv[i]; *curr; curr++ )
		{
			if( strchr( delim, *curr )==0 )
			{
				printed = 1;
				putchar( *curr );
			}
			else
			{
				if( empty_ok || printed )
					putchar( '\n' );
				printed=0;
			}
		}
		if( printed )
			putchar( '\n' );
	}
	
}





