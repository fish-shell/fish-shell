/** \file gen_hdr2.c
  A program that reads data from stdin and outputs it as a C string.

   It is used as a part of the build process to generate help texts
   for the built in commands.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/**
   The main function, does all the work
*/
int main()
{
	int line = 0;
	printf( "\t\t\"" );
	int c;
	int count=0;
	while( (c=getchar()) != EOF )
	{
		if( c == '\n' )
			line++;
		
		if( line > 4 )
			break;
	} 
	
	while( (c=getchar()) != EOF )
	{
		if( (c >= 'a' && c <= 'z' ) || 
			(c >= 'A' && c <= 'Z' ) || 
			(c >= '0' && c <= '9' ) ||
			( strchr(" ,.!;:-_#$%&(){}[]<>=?+-*/'",c) != 0) )
		{
			count++;
			putchar(c);
		}
		else
		{
			switch(c)
			{
				case '\n':
					printf( "\\n" );
					printf( "\"\n\t\t\"" );
					count =0;
					break;
				case '\t':
					printf( "\\t" );
					count +=2;
					break;
				case '\r':
					printf( "\\r" );
					count +=2;
					break;
					
				case '\"':
				case '\\':
					printf( "\\%c",c );
					count +=2;
					break;
					
				default:
					count +=7;
					printf( "\\x%02x\" \"", c );
					break;
			}
		}
		if( count > 60 )
		{
			count=0;
			printf( "\"\n\t\t\"" );
		}
	}
	printf( "\"" );
	return 0;	
}
