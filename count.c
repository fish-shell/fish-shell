/** \file count.c
   The length command, used for determining the number of items in an
   environment variable array.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>

/**
   The main function. Does nothing but return the number of arguments.

   This command, unlike all other fish commands, does not feature a -h
   or --help option. This is because we want to avoid errors on arrays
   that have -h or --help as entries, which is very common when
   parsing options, etc. For this reason, the main fish binary does a
   check and prints help usage if -h or --help is explicitly given to
   the command, but not if it is the contents of a variable.
*/
int main( int argc, char **argv )
{
	printf( "%d\n", argc-1 );
	return argc==1;
}
