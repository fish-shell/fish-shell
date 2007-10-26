/** \file tokenizer.c

A specialized tokenizer for tokenizing the fish language. In the
future, the tokenizer should be extended to support marks,
tokenizing multiple strings and disposing of unused string
segments.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <unistd.h>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "tokenizer.h"
#include "common.h"


/**
   Error string for unexpected end of string
*/
#define QUOTE_ERROR _( L"Unexpected end of string, quotes are not balanced" )

/**
   Error string for mismatched parenthesis
*/
#define PARAN_ERROR _( L"Unexpected end of string, parenthesis do not match" )

/**
   Error string for invalid redirections
*/
#define REDIRECT_ERROR _( L"Invalid input/output redirection" )

/**
   Error string for when trying to pipe from fd 0
*/
#define PIPE_ERROR _( L"Can not use fd 0 as pipe output" )

/**
   Characters that separate tokens. They are ordered by frequency of occurrence to increase parsing speed.
*/
#define SEP L" \n|\t;#\r<>^&"

/**
   Maximum length of a string containing a file descriptor number
*/
#define FD_STR_MAX_LEN 16

/**
   Descriptions of all tokenizer errors
*/
static const wchar_t *tok_desc[] =
{
	N_(L"Tokenizer not yet initialized"),
	N_( L"Tokenizer error" ),
	N_( L"Invalid token" ),
	N_( L"String" ),
	N_( L"Pipe" ),
	N_( L"End of command" ),
	N_( L"Redirect output to file" ),
	N_( L"Append output to file" ),
	N_( L"Redirect input to file" ),
	N_( L"Redirect to file descriptor" ),
	N_( L"Redirect output to file if file does not exist" ),
	N_( L"Run job in background" ),
	N_( L"Comment" )
}
	;

/**
   Tests if the tokenizer buffer is large enough to hold contents of
   the specified length, and if not, reallocates the tokenizer buffer.

   \return 0 if the system could not provide the memory needed, and 1 otherwise.
*/
static int check_size( tokenizer *tok, size_t len )
{
	if( tok->last_len <= len )
	{
		wchar_t *tmp;
		tok->last_len = len +1;
		tmp = realloc( tok->last, sizeof(wchar_t)*tok->last_len );
		if( tmp == 0 )
		{
			wperror( L"realloc" );
			return 0;
		}
		tok->last = tmp;
	}
	return 1;
}

/**
   Set the latest tokens string to be the specified error message
*/
static void tok_error( tokenizer *tok, int error_type, const wchar_t *error_message )
{
	tok->last_type = TOK_ERROR;
	tok->error = error_type;
	if( !check_size( tok, wcslen( error_message)+1 ))
	{
		if( tok->last != 0 )
			*tok->last=0;
		return;
	}

	wcscpy( tok->last, error_message );
}

int tok_get_error( tokenizer *tok )
{
	return tok->error;
}


void tok_init( tokenizer *tok, const wchar_t *b, int flags )
{

	CHECK( tok, );

	memset( tok, 0, sizeof( tokenizer) );

	CHECK( b, );


	tok->accept_unfinished = flags & TOK_ACCEPT_UNFINISHED;
	tok->show_comments = flags & TOK_SHOW_COMMENTS;
	tok->has_next=1;

	tok->has_next = (*b != L'\0');
	tok->orig_buff = tok->buff = (wchar_t *)(b);
	tok_next( tok );
}

void tok_destroy( tokenizer *tok )
{
	CHECK( tok, );
	
	free( tok->last );
	if( tok->free_orig )
		free( tok->orig_buff );
}

int tok_last_type( tokenizer *tok )
{
	CHECK( tok, TOK_ERROR );
	CHECK( tok->buff, TOK_ERROR );
	
	return tok->last_type;
}

wchar_t *tok_last( tokenizer *tok )
{
	CHECK( tok, 0 );
	
	return tok->last;
}

int tok_has_next( tokenizer *tok )
{
	/*
	  Return 1 on broken tokenizer
	*/
	CHECK( tok, 1 );
	CHECK( tok->buff, 1 );

/*	fwprintf( stderr, L"has_next is %ls \n", tok->has_next?L"true":L"false" );*/
	return 	tok->has_next;
}

/**
   Tests if this character can be a part of a string
*/

static int is_string_char( wchar_t c )
{
	return !( !c ||  wcschr( SEP, c ) );
}

/**
   Quick test to catch the most common 'non-magical' characters, makes
   read_string slightly faster by adding a fast path for the most
   common characters. This is obviously not a suitable replacement for
   iswalpha.
*/
static int myal( wchar_t c )
{
	return (c>=L'a' && c<=L'z') || (c>=L'A'&&c<=L'Z');
}

/**
   Read the next token as a string
*/
static void read_string( tokenizer *tok )
{
	const wchar_t *start;
	int len;
	int mode=0;
	wchar_t prev;
	int do_loop=1;
	int paran_count=0;

	start = tok->buff;

	while( 1 )
	{

		if( !myal( *tok->buff ) )
		{
//			debug(1, L"%lc", *tok->buff );

			if( *tok->buff == L'\\' )
			{
				tok->buff++;
				if( *tok->buff == L'\0' )
				{
					if( (!tok->accept_unfinished) )
					{
						tok_error( tok, TOK_UNTERMINATED_ESCAPE, QUOTE_ERROR );
						return;
					}
					else
					{
						do_loop = 0;
					}
					

				}
				else if( *tok->buff == L'\n' && mode == 0)
				{
					tok->buff--;
					do_loop = 0;
					break;
				}
				
				tok->buff++;
				continue;
			}
			
			
			/*
			  The modes are as follows:
			  
			  0: regular text
			  1: inside of subshell
			  2: inside of array brackets
			  3: inside of array brackets and subshell, like in '$foo[(ech'
			*/
			switch( mode )
			{
				case 0:
				{
					switch( *tok->buff )
					{
						case L'(':
						{
							paran_count=1;
							mode = 1;
							break;
						}
						
						case L'[':
						{
							if( tok->buff != start )
								mode=2;
							break;
						}
						
						case L'\'':
						case L'"':
						{
							
							const wchar_t *end = quote_end( tok->buff );
							tok->last_quote = *tok->buff;
							if( end )
							{
								tok->buff=(wchar_t *)end;
							}
							else
							{
								tok->buff += wcslen( tok->buff );
								
								if( (!tok->accept_unfinished) )
								{
									tok_error( tok, TOK_UNTERMINATED_QUOTE, QUOTE_ERROR );
									return;
								}
								do_loop = 0;

							}
							break;
						}

						default:
						{
							if( !is_string_char(*(tok->buff)) )
							{
								do_loop=0;
							}
						}
					}
					break;
				}

				case 3:
				case 1:
					switch( *tok->buff )
					{
						case L'\'':
						case L'\"':
						{
							const wchar_t *end = quote_end( tok->buff );
							if( end )
							{
								tok->buff=(wchar_t *)end;
							}
							else
							{
								tok->buff += wcslen( tok->buff );
								if( (!tok->accept_unfinished) )
								{
									tok_error( tok, TOK_UNTERMINATED_QUOTE, QUOTE_ERROR );
									return;
								}
								do_loop = 0;
							}
							
							break;
						}

						case L'(':
							paran_count++;
							break;
						case L')':
							paran_count--;
							if( paran_count == 0 )
							{
								mode--;
							}
							break;
						case L'\0':
							do_loop = 0;
							break;
					}
					break;
				case 2:
					switch( *tok->buff )
					{
						case L'(':
							paran_count=1;
							mode = 3;
							break;

						case L']':
							mode=0;
							break;

						case L'\0':
							do_loop = 0;
							break;
					}
					break;
			}
		}


		if( !do_loop )
			break;

		prev = *tok->buff;
		tok->buff++;
	}

	if( (!tok->accept_unfinished) && (mode!=0) )
	{
		tok_error( tok, TOK_UNTERMINATED_SUBSHELL, PARAN_ERROR );
		return;
	}


	len = tok->buff - start;

	if( !check_size( tok, len ))
		return;

	memcpy( tok->last, start, sizeof(wchar_t)*len );
	tok->last[len] = L'\0';
	tok->last_type = TOK_STRING;
}

/**
   Read the next token as a comment.
*/
static void read_comment( tokenizer *tok )
{
	const wchar_t *start;
	int len;

	start = tok->buff;
	while( *(tok->buff)!= L'\n' && *(tok->buff)!= L'\0' )
		tok->buff++;

	len = tok->buff - start;
	if( !check_size( tok, len ))
		return;

	memcpy( tok->last, start, sizeof(wchar_t)*len );
	tok->last[len] = L'\0';
	tok->last_type = TOK_COMMENT;
}

/**
   Read a FD redirection.
*/
static void read_redirect( tokenizer *tok, int fd )
{
	int mode = -1;

	if( (*tok->buff == L'>') ||
		(*tok->buff == L'^') )
	{
		tok->buff++;
		if( *tok->buff == *(tok->buff-1) )
		{
			tok->buff++;
			mode = 1;
		}
		else
		{
			mode = 0;
		}

		if( *tok->buff == L'|' )
		{
			if( fd == 0 )
			{
				tok_error( tok, TOK_OTHER, PIPE_ERROR );
				return;
			}
			check_size( tok, FD_STR_MAX_LEN );
			tok->buff++;
			swprintf( tok->last, FD_STR_MAX_LEN, L"%d", fd );
			tok->last_type = TOK_PIPE;
			return;
		}
	}
	else if( *tok->buff == L'<' )
	{
		tok->buff++;
		mode = 2;
	}
	else
	{
		tok_error( tok, TOK_OTHER, REDIRECT_ERROR);
	}

	if( !check_size( tok, 2 ))
	{
		return;
	}

	swprintf( tok->last, tok->last_len, L"%d", fd );

	if( *tok->buff == L'&' )
	{
		tok->buff++;
		tok->last_type = TOK_REDIRECT_FD;
	}
	else if( *tok->buff == L'?' )
	{
		tok->buff++;
		tok->last_type = TOK_REDIRECT_NOCLOB;
	}
	else
	{
		tok->last_type = TOK_REDIRECT_OUT + mode;
	}
}

wchar_t tok_last_quote( tokenizer *tok )
{
	CHECK( tok, 0 );
	
	return tok->last_quote;
}

/**
   Test if a character is whitespace. Differs from iswspace in that it
   does not consider a newline to be whitespace.
*/
static int my_iswspace( wchar_t c )
{
	if( c == L'\n' )
		return 0;
	else
		return iswspace( c );
}


const wchar_t *tok_get_desc( int type )
{
	if( type < 0 || type >= sizeof( tok_desc ) )
	{
		return _(L"Invalid token type");
	}
	return _(tok_desc[type]);
}


void tok_next( tokenizer *tok )
{

	CHECK( tok, );
	CHECK( tok->buff, );
	
	if( tok_last_type( tok ) == TOK_ERROR )
	{
		tok->has_next=0;
		return;
	}

	if( !tok->has_next )
	{
/*		wprintf( L"EOL\n" );*/
		tok->last_type = TOK_END;
		return;
	}

	while( 1 )
	{
		if( my_iswspace(*(tok->buff) ) )
		{
			tok->buff++;
		}
		else
		{
			if(( *(tok->buff) == L'\\') &&( *(tok->buff+1) == L'\n') )
			{
				tok->last_pos = tok->buff - tok->orig_buff;
				tok->buff+=2;
				tok->last_type = TOK_END;
				return;
			}
			break;
		}
	}
	

	if( *tok->buff == L'#')
	{
		if( tok->show_comments )
		{
			tok->last_pos = tok->buff - tok->orig_buff;
			read_comment( tok );
			return;
		}
		else
		{
			while( *(tok->buff)!= L'\n' && *(tok->buff)!= L'\0' )
				tok->buff++;
		}

		while( my_iswspace(*(tok->buff) ) )
			tok->buff++;
	}

	tok->last_pos = tok->buff - tok->orig_buff;

	switch( *tok->buff )
	{

		case L'\0':
			tok->last_type = TOK_END;
			/*fwprintf( stderr, L"End of string\n" );*/
			tok->has_next = 0;
			break;
		case 13:
		case L'\n':
		case L';':
			tok->last_type = TOK_END;
			tok->buff++;
			break;
		case L'&':
			tok->last_type = TOK_BACKGROUND;
			tok->buff++;
			break;

		case L'|':
			check_size( tok, 2 );

			tok->last[0]=L'1';
			tok->last[1]=L'\0';
			tok->last_type = TOK_PIPE;
			tok->buff++;
			break;

		case L'>':
                        read_redirect( tok, 1 );
			return;
		case L'<':
                        read_redirect( tok, 0 );
			return;
		case L'^':
                        read_redirect( tok, 2 );
			return;

		default:
		{
			if( iswdigit( *tok->buff ) )
			{
				wchar_t *orig = tok->buff;
				int fd = 0;
				while( iswdigit( *tok->buff ) )
					fd = (fd*10) + (*(tok->buff++) - L'0');

				switch( *(tok->buff))
				{
					case L'^':
					case L'>':
					case L'<':
						read_redirect( tok, fd );
						return;
				}
				tok->buff = orig;
			}
			read_string( tok );
		}

	}

}

wchar_t *tok_string( tokenizer *tok )
{
	return tok?tok->orig_buff:0;
}

wchar_t *tok_first( const wchar_t *str )
{
	tokenizer t;
	wchar_t *res=0;

	CHECK( str, 0 );
	
	tok_init( &t, str, 0 );

	switch( tok_last_type( &t ) )
	{
		case TOK_STRING:
//				fwprintf( stderr, L"Got token %ls\n", tok_last( &t ));
			res = wcsdup(tok_last( &t ));
			break;
		default:
			break;
	}

	tok_destroy( &t );
	return res;
}


int tok_get_pos( tokenizer *tok )
{
	CHECK( tok, 0 );
	
	return tok->last_pos;
}


void tok_set_pos( tokenizer *tok, int pos )
{
	CHECK( tok, );
	
	tok->buff = tok->orig_buff + pos;
	tok->has_next = 1;
	tok_next( tok );
}


#ifdef TOKENIZER_TEST

/**
   This main function is used for compiling the tokenizer_test command, used for testing the tokenizer.
*/
int main( int argc, char **argv )
{
	tokenizer tok;
	int i;
	for ( i=1; i<argc; i++ )
	{
		wprintf( L"Tokenizing string %s\n", argv[i] );
		for( tok_init( &tok, str2wcs(argv[i]), 0 ); tok_has_next( &tok ); tok_next( &tok ) )
		{
			switch( tok_last_type( &tok ) )
 			{
				case TOK_INVALID:
					wprintf( L"Type: INVALID\n" );
					break;
				case TOK_STRING:
					wprintf( L"Type: STRING\t Value: %ls\n", tok_last( &tok ) );
					break;
				case TOK_PIPE:
					wprintf( L"Type: PIPE\n" );
					break;
				case TOK_END:
					wprintf( L"Type: END\n" );
					break;
				case TOK_ERROR:
					wprintf( L"Type: ERROR\n" );
					break;
				default:
					wprintf( L"Type: Unknown\n"  );
					break;
			}
		}
		tok_destroy( &tok );

	}
}

#endif
