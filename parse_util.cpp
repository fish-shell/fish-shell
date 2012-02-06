/** \file parse_util.c

    Various mostly unrelated utility functions related to parsing,
    loading and evaluating fish code.

	This library can be seen as a 'toolbox' for functions that are
	used in many places in fish and that are somehow related to
	parsing the code. 
*/

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#include <wchar.h>
#include <map>
#include <set>
#include <algorithm>

#include <time.h>
#include <assert.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "common.h"
#include "tokenizer.h"
#include "parse_util.h"
#include "expand.h"
#include "intern.h"
#include "exec.h"
#include "env.h"
#include "signal.h"
#include "wildcard.h"
#include "halloc_util.h"

/**
   Maximum number of autoloaded items opf a specific type to keep in
   memory at a time.
*/
#define AUTOLOAD_MAX 10

/**
   Minimum time, in seconds, before an autoloaded item will be
   unloaded
*/
#define AUTOLOAD_MIN_AGE 60


int parse_util_lineno( const wchar_t *str, int len )
{
	/**
	   First cached state
	*/
	static wchar_t *prev_str = 0;
	static int i=0;
	static int res = 1;

	/**
	   Second cached state
	*/
	static wchar_t *prev_str2 = 0;
	static int i2 = 0;
	static int res2 = 1;

	CHECK( str, 0 );

	if( str != prev_str || i>len )
	{
		if( prev_str2 == str && i2 <= len )
		{
			wchar_t *tmp_str = prev_str;
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
				
			prev_str = (wchar_t *)str;
			i=0;
			res=1;
		}
	}
	
	for( ; str[i] && i<len; i++ )
	{
		if( str[i] == L'\n' )
		{
			res++;
		}
	}
	return res;
}


int parse_util_get_line_from_offset( const wcstring &str, int pos )
{
	//	return parse_util_lineno( buff, pos );
    const wchar_t *buff = str.c_str();
	int i;
	int count = 0;
	if( pos < 0 )
	{
		return -1;
	}
	
	for( i=0; i<pos; i++ )
	{
		if( !buff[i] )
		{
			return -1;
		}
		
		if( buff[i] == L'\n' )
		{
			count++;
		}
	}
	return count;
}


int parse_util_get_offset_from_line( const wcstring &str, int line )
{
    const wchar_t *buff = str.c_str();
	int i;
	int count = 0;
	
	if( line < 0 )
	{
		return -1;
	}

	if( line == 0 )
		return 0;
		
	for( i=0;; i++ )
	{
		if( !buff[i] )
		{
			return -1;
		}
		
		if( buff[i] == L'\n' )
		{
			count++;
			if( count == line )
			{
				return i+1;
			}
			
		}
	}
}

int parse_util_get_offset( const wcstring &str, int line, int line_offset )
{
    const wchar_t *buff = str.c_str();
	int off = parse_util_get_offset_from_line( buff, line );
	int off2 = parse_util_get_offset_from_line( buff, line+1 );
	int line_offset2 = line_offset;
	
	if( off < 0 )
	{
		return -1;
	}
	
	if( off2 < 0 )
	{
		off2 = wcslen( buff )+1;
	}
	
	if( line_offset2 < 0 )
	{
		line_offset2 = 0;
	}
	
	if( line_offset2 >= off2-off-1 )
	{
		line_offset2 = off2-off-1;
	}
	
	return off + line_offset2;
	
}


int parse_util_locate_cmdsubst( const wchar_t *in, 
								wchar_t **begin, 
								wchar_t **end,
								int allow_incomplete )
{
	wchar_t *pos;
	wchar_t prev=0;
	int syntax_error=0;
	int paran_count=0;	

	wchar_t *paran_begin=0, *paran_end=0;

	CHECK( in, 0 );
	
	for( pos = (wchar_t *)in; *pos; pos++ )
	{
		if( prev != '\\' )
		{
			if( wcschr( L"\'\"", *pos ) )
			{
				wchar_t *q_end = quote_end( pos );
				if( q_end && *q_end)
				{
					pos=q_end;
				}
				else
				{
					break;
				}
			}
			else
			{
				if( *pos == '(' )
				{
					if(( paran_count == 0)&&(paran_begin==0))
					{
						paran_begin = pos;
					}
					
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

	if( begin )
	{
		*begin = paran_begin;
	}

	if( end )
	{
		*end = paran_count?(wchar_t *)in+wcslen(in):paran_end;
	}
	
	return 1;
}


void parse_util_cmdsubst_extent( const wchar_t *buff,
								 int cursor_pos,
								 const wchar_t **a, 
								 const wchar_t **b )
{
	wchar_t *begin, *end;
	wchar_t *pos;
	const wchar_t *cursor = buff + cursor_pos;
	
	CHECK( buff, );

	if( a )
	{
		*a = (wchar_t *)buff;
	}

	if( b )
	{
		*b = (wchar_t *)buff+wcslen(buff);
	}
	
	pos = (wchar_t *)buff;
	
	while( 1 )
	{
		if( parse_util_locate_cmdsubst( pos,
										&begin,
										&end,
										1 ) <= 0)
		{
			/*
			  No subshell found
			*/
			break;
		}

		if( !end )
		{
			end = (wchar_t *)buff + wcslen(buff);
		}

		if(( begin < cursor ) && (end >= cursor) )
		{
			begin++;

			if( a )
			{
				*a = begin;
			}

			if( b )
			{
				*b = end;
			}

			break;
		}

		if( !*end )
		{
			break;
		}
		
		pos = end+1;
	}
	
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

	CHECK( buff, );
	
	if( a )
	{
		*a=0;
	}

	if( b )
	{
		*b = 0;
	}
	
	parse_util_cmdsubst_extent( buff, cursor_pos, &begin, &end );
	if( !end || !begin )
	{
		return;
	}
	
	pos = cursor_pos - (begin - buff);

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
		DIE_MEM();
	}

	for( tok_init( &tok, buffcpy, TOK_ACCEPT_UNFINISHED );
		 tok_has_next( &tok ) && !finished;
		 tok_next( &tok ) )
	{
		int tok_begin = tok_get_pos( &tok );

		switch( tok_last_type( &tok ) )
		{
			case TOK_PIPE:
			{
				if( !process )
				{
					break;
				}
			}
			
			case TOK_END:
			case TOK_BACKGROUND:
			{
				
				if( tok_begin >= pos )
				{
					finished=1;					
					if( b )
					{
						*b = (wchar_t *)buff + tok_begin;
					}
				}
				else
				{
					if( a )
					{
						*a = (wchar_t *)buff + tok_begin+1;
					}
				}

				break;				
			}
		}
	}

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
	
	CHECK( buff, );
		
	assert( cursor_pos >= 0 );

	a = b = pa = pb = 0;
	
	parse_util_cmdsubst_extent( buff, cursor_pos, &begin, &end );

	if( !end || !begin )
	{
		return;
	}
	
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
		DIE_MEM();
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
		{
			tok_end +=wcslen(tok_last(&tok));
		}
		
		/*
		  Cursor was before beginning of this token, means that the
		  cursor is between two tokens, so we set it to a zero element
		  string and break
		*/
		if( tok_begin > pos )
		{
			a = b = (wchar_t *)buff + pos;
			break;
		}

		/*
		  If cursor is inside the token, this is the token we are
		  looking for. If so, set a and b and break
		*/
		if( (tok_last_type( &tok ) == TOK_STRING) && (tok_end >= pos ) )
		{			
			a = begin + tok_get_pos( &tok );
			b = a + wcslen(tok_last(&tok));
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
	{
		*tok_begin = a;
	}

	if( tok_end )
	{
		*tok_end = b;
	}

	if( prev_begin )
	{
		*prev_begin = pa;
	}

	if( prev_end )
	{
		*prev_end = pb;
	}
	
	assert( pa >= buff );
	assert( pa <= (buff+wcslen(buff) ) );
	assert( pb >= pa );
	assert( pb <= (buff+wcslen(buff) ) );

}

void parse_util_set_argv( const wchar_t * const *argv, const wcstring_list_t &named_arguments )
{
	if( *argv )
	{
		const wchar_t * const *arg;
		string_buffer_t sb;
		sb_init( &sb );
		
		for( arg=argv; *arg; arg++ )
		{
			if( arg != argv )
			{
				sb_append( &sb, ARRAY_SEP_STR );
			}
			sb_append( &sb, *arg );
		}
			
		env_set( L"argv", (wchar_t *)sb.buff, ENV_LOCAL );
		sb_destroy( &sb );
	}
	else
	{
		env_set( L"argv", 0, ENV_LOCAL );
	}				

	if( named_arguments.size() )
	{
		const wchar_t * const *arg;
		size_t i;
		
		for( i=0, arg=argv; i < named_arguments.size(); i++ )
		{
			env_set( named_arguments.at(i).c_str(), *arg, ENV_LOCAL );

			if( *arg )
				arg++;
		}
			
		
	}
	
}

wchar_t *parse_util_unescape_wildcards( const wchar_t *str )
{
	wchar_t *in, *out;
	wchar_t *unescaped;

	CHECK( str, 0 );
	
	unescaped = wcsdup(str);

	if( !unescaped )
	{
		DIE_MEM();
	}
	
	for( in=out=unescaped; *in; in++ )
	{
		switch( *in )
		{
			case L'\\':
			{
				if( *(in+1) )
				{
					in++;
					*(out++)=*in;
				}
				*(out++)=*in;
				break;
			}
			
			case L'*':
			{
				*(out++)=ANY_STRING;					
				break;
			}
			
			case L'?':
			{
				*(out++)=ANY_CHAR;					
				break;
			}
			
			default:
			{
				*(out++)=*in;
				break;
			}
		}		
	}
	return unescaped;
}

