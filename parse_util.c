/** \file parse_util.c

    Various mostly unrelated utility functions related to parsing,
    loading and evaluating fish code.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#include <wchar.h>

#include <time.h>
#include <assert.h>

#include "util.h"
#include "wutil.h"
#include "common.h"
#include "tokenizer.h"
#include "parse_util.h"
#include "expand.h"
#include "intern.h"
#include "exec.h"
#include "env.h"
#include "wildcard.h"
#include "halloc_util.h"

/**
   Set of files which have been autoloaded
*/
static hash_table_t *all_loaded=0;

int parse_util_lineno( const wchar_t *str, int len )
{
	/**
	   First cached state
	*/
	static const wchar_t *prev_str = 0;
	static int i=0;
	static int res = 1;

	/**
	   Second cached state
	*/
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
				const wchar_t *end = quote_end( pos );
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

	if( begin )
		*begin = paran_begin;
	if( end )
		*end = paran_count?in+wcslen(in):paran_end;
	
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

	for( tok_init( &tok, buffcpy, TOK_ACCEPT_UNFINISHED );
		 tok_has_next( &tok ) && !finished;
		 tok_next( &tok ) )
	{
		int tok_begin = tok_get_pos( &tok );

		switch( tok_last_type( &tok ) )
		{
			case TOK_PIPE:
				if( !process )
					break;
				
			case TOK_END:
			case TOK_BACKGROUND:
			{
				
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

/**
   Free hash value, but not hash key
*/
static void clear_hash_value( const void *key, const void *data )
{
	free( (void *)data );
}

/**
   Part of the autoloader cleanup 
*/
static void clear_loaded_entry( const void *key, const void *data )
{
	hash_table_t *loaded = (hash_table_t *)data;
	hash_foreach( loaded,
				  &clear_hash_value );
	hash_destroy( loaded );
	free( loaded );	
	free( (void *)key );
}

/**
   The autoloader cleanup function. It is run on shutdown and frees
   any memory used by the autoloader code to keep track of loaded
   files.
*/
static void parse_util_destroy()
{
	if( all_loaded )
	{
		hash_foreach( all_loaded,
					  &clear_loaded_entry );
		
		hash_destroy( all_loaded );
		free( all_loaded );	
		all_loaded = 0;
	}
}

void parse_util_load_reset( const wchar_t *path_var )
{
	if( all_loaded )
	{
		void *key, *data;
		hash_remove( all_loaded, path_var, (const void **)&key, (const void **)&data );
		if( key )
			clear_loaded_entry( key, data );
	}
	
}


int parse_util_load( const wchar_t *cmd,
					 const wchar_t *path_var_name,
					 void (*on_load)(const wchar_t *cmd),
					 int reload )
{
	static array_list_t *path_list=0;
	static string_buffer_t *path=0;

	int i;
	time_t *tm;
	int reloaded = 0;
	hash_table_t *loaded;

	const wchar_t *path_var = env_get( path_var_name );

	/*
	  Do we know where to look
	*/
	
	if( !path_var )
		return 0;
	
	if( !all_loaded )
	{
		all_loaded = malloc( sizeof( hash_table_t ) );		
		halloc_register_function_void( global_context, &parse_util_destroy );
		if( !all_loaded )
		{
			die_mem();
		}
		hash_init( all_loaded, &hash_wcs_func, &hash_wcs_cmp );
 	}
	
	loaded = (hash_table_t *)hash_get( all_loaded, path_var_name );
	
	if( !loaded )
	{
		loaded = malloc( sizeof( hash_table_t ) );
		if( !loaded )
		{
			die_mem();
		}
		hash_init( loaded, &hash_wcs_func, &hash_wcs_cmp );
		hash_put( all_loaded, wcsdup(path_var_name), loaded );
	}

	/*
	  Get modification time of file
	*/
	tm = (time_t *)hash_get( loaded, cmd );

	/*
	  Did we just check this?
	*/
	if( tm )
	{
		if(time(0)-tm[1]<=1)
		{
			return 0;
		}
	}
	
	/*
	  Return if already loaded and we are skipping reloading
	*/
	if( !reload && tm )
		return 0;
	
	if( !path_list )
		path_list = al_halloc( global_context);
	
	if( !path )
		path = sb_halloc( global_context );
	else
		sb_clear( path );
	
	expand_variable_array( path_var, path_list );
	
	/*
	  Iterate over path searching for suitable completion files
	*/
	for( i=0; i<al_get_count( path_list ); i++ )
	{
		struct stat buf;
		wchar_t *next = (wchar_t *)al_get( path_list, i );
		sb_clear( path );
		sb_append2( path, next, L"/", cmd, L".fish", (void *)0 );
		if( (wstat( (wchar_t *)path->buff, &buf )== 0) &&
			(waccess( (wchar_t *)path->buff, R_OK ) == 0) )
		{
			if( !tm || (tm[0] != buf.st_mtime ) )
			{
				wchar_t *esc = escape( (wchar_t *)path->buff, 1 );
				wchar_t *src_cmd = wcsdupcat( L". ", esc );
				
				if( !tm )
				{
					tm = malloc(sizeof(time_t)*2);
					if( !tm )
						die_mem();
				}

				tm[0] = buf.st_mtime;
				tm[1] = time(0);
				hash_put( loaded,
						  intern( cmd ),
						  tm );

				free( esc );

				if( on_load )
					on_load(cmd );

				/*
				  Source the completion file for the specified completion
				*/
				exec_subshell( src_cmd, 0 );
				free(src_cmd);
				reloaded = 1;
				break;
			}
		}
	}

	/*
	  If no file was found we insert the current time. Later we only
	  research if the current time is at least five seconds later.
	  This way, the files won't be searched over and over again.
	*/
	if( !tm )
	{
		tm = malloc(sizeof(time_t)*2);
		if( !tm )
			die_mem();
		
		tm[0] = 0;
		tm[1] = time(0);
		hash_put( loaded, intern( cmd ), tm );
	}

	al_foreach( path_list, (void (*)(const void *))&free );
	al_truncate( path_list, 0 );

	return reloaded;	
}

void parse_util_set_argv( wchar_t **argv )
{
	if( *argv )
	{
		wchar_t **arg;
		string_buffer_t sb;
		sb_init( &sb );
		
		for( arg=argv; *arg; arg++ )
		{
			if( arg != argv )
				sb_append( &sb, ARRAY_SEP_STR );
			sb_append( &sb, *arg );
		}
			
		env_set( L"argv", (wchar_t *)sb.buff, ENV_LOCAL );
		sb_destroy( &sb );
	}
	else
	{
		env_set( L"argv", 0, ENV_LOCAL );
	}				
}

wchar_t *parse_util_unescape_wildcards( const wchar_t *str )
{
	wchar_t *in, *out;
	wchar_t *unescaped = wcsdup(str);

	if( !unescaped )
		die_mem();
	
	for( in=out=unescaped; *in; in++ )
	{
		switch( *in )
		{
			case L'\\':
				if( *(in+1) )
				{
					in++;
					*(out++)=*in;
				}
				*(out++)=*in;
				break;
				
			case L'*':
				*(out++)=ANY_STRING;					
				break;
				
			case L'?':
				*(out++)=ANY_CHAR;					
				break;
				
			default:
				*(out++)=*in;
				break;
		}
		
	}
	return unescaped;
}

