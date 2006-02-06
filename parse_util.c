/** \file parse_util.c

    Various utility functions for parsing a command
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#include <wchar.h>

#include <assert.h>

#include "util.h"
#include "wutil.h"
#include "common.h"
#include "tokenizer.h"
#include "parse_util.h"

int parse_util_lineno( const wchar_t *str, int len )
{
	static int res = 1;
	static int i=0;
	static const wchar_t *prev_str = 0;

	static const wchar_t *prev_str2 = 0;
	static int i2 = 0;
	static int res2 = 1;
	
	if( str != prev_str || i>len )
	{
		if( prev_str2 == str && i2 <= len )
		{
			const wchar_t *tmp_str = prev_str;
			int tmp_i = i;
			int tmp_res = res;
			prev_str = prev_str2;
			i=i2;
			res=res2;
			
			prev_str2 = tmp_str;
			i2 = tmp_i;
			res2 = tmp_res;
		}
		else
		{
			prev_str2 = prev_str;
			i2 = i;
			res2=res;
				
			prev_str = str;
			i=0;
			res=1;
		}
	}
	
	for( ; str[i] && i<len; i++ )
	{
		if( str[i] == L'\n' )
			res++;
	}
	return res;
}

int parse_util_locate_cmdsubst( const wchar_t *in, 
								const wchar_t **begin, 
								const wchar_t **end,
								int allow_incomplete )
{
	const wchar_t *pos;
	wchar_t prev=0;
	int syntax_error=0;
	int paran_count=0;	

	const wchar_t *paran_begin=0, *paran_end=0;

	for( pos=in; *pos; pos++ )
	{
		if( prev != '\\' )
		{
			if( wcschr( L"\'\"", *pos ) )
			{
				wchar_t *end = quote_end( pos );
				if( end && *end)
				{
					pos=end;
				}
				else
					break;
			}
			else
			{
				if( *pos == '(' )
				{
					if(( paran_count == 0)&&(paran_begin==0))
						paran_begin = pos;
				
					paran_count++;
				}
				else if( *pos == ')' )
				{
					paran_count--;
					if( (paran_count == 0) && (paran_end == 0) )
					{
						paran_end = pos;
						break;
					}
				
					if( paran_count < 0 )
					{
						syntax_error = 1;
						break;
					}
				}
			}
			
		}
		
		prev = *pos;
	}
	
	syntax_error |= (paran_count < 0 );
	syntax_error |= ((paran_count>0)&&(!allow_incomplete));
	
	if( syntax_error )
	{
		return -1;
	}
	
	if( paran_begin == 0 )
	{
		return 0;
	}

	*begin = paran_begin;
	*end = paran_count?in+wcslen(in):paran_end;
	
/*	assert( *begin >= in );
	assert( *begin < (in+wcslen(in) ) );
	assert( *end >= *begin );
	assert( *end < (in+wcslen(in) ) );
*/
	return 1;

}


void parse_util_cmdsubst_extent( const wchar_t *buff,
								 int cursor_pos,
								 const wchar_t **a, 
								 const wchar_t **b )
{
	const wchar_t *begin, *end;
	const wchar_t *pos;
	
	if( a )
		*a=0;
	if( b )
		*b = 0;

	if( !buff )
		return;

	pos = buff;
	
	while( 1 )
	{
		int bc, ec;
		
		if( parse_util_locate_cmdsubst( pos,
										&begin,
										&end,
										1 ) <= 0)
		{
			begin=buff;
			end = buff + wcslen(buff);
			break;
		}

		if( !end )
		{
			end = buff + wcslen(buff);
		}

		bc = begin-buff;
		ec = end-buff;
		
		if(( bc < cursor_pos ) && (ec >= cursor_pos) )
		{
			begin++;
			break;
		}
		pos = end+1;
	}
	if( a )
		*a = begin;
	if( b )
		*b = end;
}

/**
   Get the beginning and end of the job or process definition under the cursor
*/
static void job_or_process_extent( const wchar_t *buff,
								   int cursor_pos,
								   const wchar_t **a, 
								   const wchar_t **b, 
								   int process )
{
	const wchar_t *begin, *end;
	int pos;
	wchar_t *buffcpy;
	int finished=0;
	
	tokenizer tok;

	if( a )
		*a=0;
	if( b )
		*b = 0;
	
	parse_util_cmdsubst_extent( buff, cursor_pos, &begin, &end );
	if( !end || !begin )
		return;

	pos = cursor_pos - (begin - buff);
//	fwprintf( stderr, L"Subshell extent: %d %d %d\n", begin-buff, end-buff, pos );

	if( a )
	{
		*a = begin;
	}
	
	if( b )
	{
		*b = end;
	}

	buffcpy = wcsndup( begin, end-begin );

	if( !buffcpy )
	{
		die_mem();
	}
//	fwprintf( stderr, L"Strlen: %d\n", wcslen(buffcpy ) );

	for( tok_init( &tok, buffcpy, TOK_ACCEPT_UNFINISHED );
		 tok_has_next( &tok ) && !finished;
		 tok_next( &tok ) )
	{
		int tok_begin = tok_get_pos( &tok );
//		fwprintf( stderr, L".");

		switch( tok_last_type( &tok ) )
		{
			case TOK_PIPE:
				if( !process )
					break;
				
			case TOK_END:
			case TOK_BACKGROUND:
			{

//				fwprintf( stderr, L"New cmd at %d\n", tok_begin );
				
				if( tok_begin >= pos )
				{
					finished=1;					
					if( b )
						*b = buff + tok_begin;
				}
				else
				{
					if( a )
						*a = buff + tok_begin+1;
				}
				break;
				
			}
		}
	}

//	fwprintf( stderr, L"Res: %d %d\n", *a-buff, *b-buff );
	free( buffcpy);
	
	tok_destroy( &tok );

}

void parse_util_process_extent( const wchar_t *buff,
								int pos,
								const wchar_t **a, 
								const wchar_t **b )
{
	job_or_process_extent( buff, pos, a, b, 1 );	
}

void parse_util_job_extent( const wchar_t *buff,
							int pos,
							const wchar_t **a, 
							const wchar_t **b )
{
	job_or_process_extent( buff,pos,a, b, 0 );	
}


void parse_util_token_extent( const wchar_t *buff,
							  int cursor_pos,
							  const wchar_t **tok_begin,
							  const wchar_t **tok_end,
							  const wchar_t **prev_begin, 
							  const wchar_t **prev_end )
{
	const wchar_t *begin, *end;
	int pos;
	wchar_t *buffcpy;

	tokenizer tok;

	const wchar_t *a, *b, *pa, *pb;
	

	a = b = pa = pb = 0;
	
	parse_util_cmdsubst_extent( buff, cursor_pos, &begin, &end );

	if( !end || !begin )
		return;

	pos = cursor_pos - (begin - buff);

	a = buff + pos;
	b = a;
	pa = buff + pos;
	pb = pa;

	assert( begin >= buff );
	assert( begin <= (buff+wcslen(buff) ) );
	assert( end >= begin );
	assert( end <= (buff+wcslen(buff) ) );

	buffcpy = wcsndup( begin, end-begin );

	if( !buffcpy )
	{
		die_mem();
	}

	for( tok_init( &tok, buffcpy, TOK_ACCEPT_UNFINISHED );
		 tok_has_next( &tok );
		 tok_next( &tok ) )
	{
		int tok_begin = tok_get_pos( &tok );
		int tok_end=tok_begin;

		/*
		  Calculate end of token
		*/
		if( tok_last_type( &tok ) == TOK_STRING )
			tok_end +=wcslen(tok_last(&tok));
		
		/*
		  Cursor was before beginning of this token, means that the
		  cursor is between two tokens, so we set it to a zero element
		  string and break
		*/
		if( tok_begin > pos )
		{
			a = b = buff + pos;
			break;
		}

		/*
		  If cursor is inside the token, this is the token we are
		  looking for. If so, set a and b and break
		*/
		if( tok_end >= pos )
		{
			a = begin + tok_get_pos( &tok );
			b = a + wcslen(tok_last(&tok));

//			fwprintf( stderr, L"Whee %ls\n", *a );

			break;
		}
		
		/*
		  Remember previous string token
		*/
		if( tok_last_type( &tok ) == TOK_STRING )
		{
			pa = begin + tok_get_pos( &tok );
			pb = pa + wcslen(tok_last(&tok));
		}
	}

	free( buffcpy);
	
	tok_destroy( &tok );

	if( tok_begin )
		*tok_begin = a;
	if( tok_end )
		*tok_end = b;
	if( prev_begin )
		*prev_begin = pa;
	if( prev_end )
		*prev_end = pb;

	assert( pa >= buff );
	assert( pa <= (buff+wcslen(buff) ) );
	assert( pb >= pa );
	assert( pb <= (buff+wcslen(buff) ) );

}



