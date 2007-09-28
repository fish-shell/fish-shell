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

/**
   A structure representing the autoload state for a specific variable, e.g. fish_complete_path
*/
typedef struct
{
	/**
	   A table containing the modification times of all loaded
	   files. Failed loads (non-existing files) have modification time
	   0.
	*/
	hash_table_t load_time;
	/**
	   A string containg the path used to find any files to load. If
	   this differs from the current environment variable, the
	   autoloader needs to drop all loaded files and reload them.
	*/
	wchar_t *old_path;
	/**
	   A table containing all the files that are currently being
	   loaded. This is here to help prevent recursion.
	*/
	hash_table_t is_loading;
}
	autoload_t;


/**
   Set of files which have been autoloaded
*/
static hash_table_t *all_loaded=0;

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


int parse_util_get_line_from_offset( wchar_t *buff, int pos )
{
	//	return parse_util_lineno( buff, pos );

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


int parse_util_get_offset_from_line( wchar_t *buff, int line )
{
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

int parse_util_get_offset( wchar_t *buff, int line, int line_offset )
{
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
								 wchar_t **a, 
								 wchar_t **b )
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
								   wchar_t **a, 
								   wchar_t **b, 
								   int process )
{
	wchar_t *begin, *end;
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
								wchar_t **a, 
								wchar_t **b )
{
	job_or_process_extent( buff, pos, a, b, 1 );	
}

void parse_util_job_extent( const wchar_t *buff,
							int pos,
							wchar_t **a, 
							wchar_t **b )
{
	job_or_process_extent( buff,pos,a, b, 0 );	
}


void parse_util_token_extent( const wchar_t *buff,
							  int cursor_pos,
							  wchar_t **tok_begin,
							  wchar_t **tok_end,
							  wchar_t **prev_begin, 
							  wchar_t **prev_end )
{
	wchar_t *begin, *end;
	int pos;
	wchar_t *buffcpy;

	tokenizer tok;

	wchar_t *a, *b, *pa, *pb;
	
	CHECK( buff, );
		
	assert( cursor_pos >= 0 );

	a = b = pa = pb = 0;
	
	parse_util_cmdsubst_extent( buff, cursor_pos, &begin, &end );

	if( !end || !begin )
	{
		return;
	}
	
	pos = cursor_pos - (begin - buff);
	
	a = (wchar_t *)buff + pos;
	b = a;
	pa = (wchar_t *)buff + pos;
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

/**
   Free hash value, but not hash key
*/
static void clear_hash_value( void *key, void *data, void *aux )
{
	if( aux )
	{
		wchar_t *name = (wchar_t *)key;
		time_t *time = (time_t *)data;
		void (*handler)(const wchar_t *)= (void (*)(const wchar_t *))aux;

		/*
		  If time[0] is 0, that means the file really doesn't exist,
		  it's simply been checked before. We should not unload it.
		*/
		if( time[0] && handler )
		{
//			debug( 0, L"Unloading function %ls", name );
			handler( name );
		}
	}
	
	free( (void *)data );
}

/**
   Part of the autoloader cleanup 
*/
static void clear_loaded_entry( void *key, 
								void *data,
								void *handler )
{
	autoload_t *loaded = (autoload_t *)data;
	hash_foreach2( &loaded->load_time,
				   &clear_hash_value,
				   handler );
	hash_destroy( &loaded->load_time );
	hash_destroy( &loaded->is_loading );

	free( loaded->old_path );
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
		hash_foreach2( all_loaded,
					   &clear_loaded_entry,
					   0 );
		
		hash_destroy( all_loaded );
		free( all_loaded );	
		all_loaded = 0;
	}
}

void parse_util_load_reset( const wchar_t *path_var_name,
							void (*on_load)(const wchar_t *cmd) )
{
	CHECK( path_var_name, );

	if( all_loaded )
	{
		void *key, *data;
		hash_remove( all_loaded, path_var_name, &key, &data );
		if( key )
		{
			clear_loaded_entry( key, data, (void *)on_load );
		}
	}
	
}

int parse_util_unload( const wchar_t *cmd,
					   const wchar_t *path_var_name,
					   void (*on_load)(const wchar_t *cmd) )
{
	autoload_t *loaded;
	void *val;

	CHECK( path_var_name, 0 );
	CHECK( cmd, 0 );

	if( !all_loaded )
	{
		return 0;
	}
	
	loaded = (autoload_t *)hash_get( all_loaded, path_var_name );
	
	if( !loaded )
	{
		return 0;
	}
	
	hash_remove( &loaded->load_time, cmd, 0, &val );
	if( val )
	{
		if( on_load )
		{
			on_load( cmd );
		}
		free( val );
	}
	
	return !!val;
}

/**

   Unload all autoloaded items that have expired, that where loaded in
   the specified path.

   \param path_var_name The variable containing the path to autoload in
   \param skip unloading the the specified file
   \param on_load the callback function to call for every unloaded file

*/
static void parse_util_autounload( const wchar_t *path_var_name,
								   const wchar_t *skip,
								   void (*on_load)(const wchar_t *cmd) )
{
	autoload_t *loaded;
	int loaded_count=0;

	if( !all_loaded )
	{
		return;
	}
	
	loaded = (autoload_t *)hash_get( all_loaded, path_var_name );
	if( !loaded )
	{
		return;
	}
	
	if( hash_get_count( &loaded->load_time ) >= AUTOLOAD_MAX )
	{
		time_t oldest_access = time(0) - AUTOLOAD_MIN_AGE;
		wchar_t *oldest_item=0;
		int i;
		array_list_t key;
		al_init( &key );
		hash_get_keys( &loaded->load_time, &key );
		for( i=0; i<al_get_count( &key ); i++ )
		{
			wchar_t *item = (wchar_t *)al_get( &key, i );
			time_t *tm = hash_get( &loaded->load_time, item );

			if( wcscmp( item, skip ) == 0 )
			{
				continue;
			}
			
			if( !tm[0] )
			{
				continue;
			}
			
			if( hash_get( &loaded->is_loading, item ) )
			{
				continue;
			}
			
			loaded_count++;
			
			if( tm[1] < oldest_access )
			{
				oldest_access = tm[1];
				oldest_item = item;
			}
		}
		al_destroy( &key );
		
		if( oldest_item && loaded_count > AUTOLOAD_MAX)
		{
			parse_util_unload( oldest_item, path_var_name, on_load );
		}
	}
}

static int parse_util_load_internal( const wchar_t *cmd,
									 void (*on_load)(const wchar_t *cmd),
									 int reload,
									 autoload_t *loaded,
									 array_list_t *path_list );


int parse_util_load( const wchar_t *cmd,
					 const wchar_t *path_var_name,
					 void (*on_load)(const wchar_t *cmd),
					 int reload )
{
	array_list_t *path_list=0;

	autoload_t *loaded;

	wchar_t *path_var;

	int res;
	int c, c2;
	
	CHECK( path_var_name, 0 );
	CHECK( cmd, 0 );

	CHECK_BLOCK( 0 );
	
//	debug( 0, L"Autoload %ls in %ls", cmd, path_var_name );

	parse_util_autounload( path_var_name, cmd, on_load );
	path_var = env_get( path_var_name );	
	
	/*
	  Do we know where to look?
	*/
	if( !path_var )
	{
		return 0;
	}

	/**
	   Init if this is the first time we try to autoload anything
	*/
	if( !all_loaded )
	{
		all_loaded = malloc( sizeof( hash_table_t ) );		
		halloc_register_function_void( global_context, &parse_util_destroy );
		if( !all_loaded )
		{
			DIE_MEM();
		}
		hash_init( all_loaded, &hash_wcs_func, &hash_wcs_cmp );
 	}
	
	loaded = (autoload_t *)hash_get( all_loaded, path_var_name );

	if( loaded )
	{
		/*
		  Check if the lookup path has changed. If so, drop all loaded
		  files and start from scratch.
		*/
		if( wcscmp( path_var, loaded->old_path ) != 0 )
		{
			parse_util_load_reset( path_var_name, on_load);
			reload = parse_util_load( cmd, path_var_name, on_load, reload );
			return reload;
		}

		/**
		   Warn and fail on infinite recursion
		*/
		if( hash_get( &loaded->is_loading, cmd ) )
		{
			debug( 0, 
				   _( L"Could not autoload item '%ls', it is already being autoloaded. " 
					  L"This is a circular dependency in the autoloading scripts, please remove it."), 
				   cmd );
			return 1;
		}
		

	}
	else
	{
		/*
		  We have never tried to autoload using this path name before,
		  set up initial data
		*/
//		debug( 0, L"Create brand new autoload_t for %ls->%ls", path_var_name, path_var );
		loaded = malloc( sizeof( autoload_t ) );
		if( !loaded )
		{
			DIE_MEM();
		}
		hash_init( &loaded->load_time, &hash_wcs_func, &hash_wcs_cmp );
		hash_put( all_loaded, wcsdup(path_var_name), loaded );
		
		hash_init( &loaded->is_loading, &hash_wcs_func, &hash_wcs_cmp );

		loaded->old_path = wcsdup( path_var );
	}


	path_list = al_new( global_context);
	
	tokenize_variable_array( path_var, path_list );
	
	c = al_get_count( path_list );
	
	hash_put( &loaded->is_loading, cmd, cmd );

	/*
	  Do the actual work in the internal helper function
	*/

	res = parse_util_load_internal( cmd, on_load, reload, loaded, path_list );

	autoload_t *loaded2 = (autoload_t *)hash_get( all_loaded, path_var_name );
	if( loaded2 == loaded )
	{
		/**
		   Cleanup
		*/
		hash_remove( &loaded->is_loading, cmd, 0, 0 );
	}
	
	c2 = al_get_count( path_list );

	al_foreach( path_list, &free );
	al_destroy( path_list );
	free( path_list );

	/**
	   Make sure we didn't 'drop' something
	*/
	
	assert( c == c2 );
	
	return res;
}

/**
   This internal helper function does all the real work. By using two
   functions, the internal function can return on various places in
   the code, and the caller can take care of various cleanup work.
*/

static int parse_util_load_internal( const wchar_t *cmd,
									 void (*on_load)(const wchar_t *cmd),
									 int reload,
									 autoload_t *loaded,
									 array_list_t *path_list )
{
	static string_buffer_t *path=0;
	time_t *tm;
	int i;
	int reloaded = 0;

	/*
	  Get modification time of file
	*/
	tm = (time_t *)hash_get( &loaded->load_time, cmd );
	
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
	{
		return 0;
	}
	
	if( !path )
	{
		path = sb_halloc( global_context );
	}
	else
	{
		sb_clear( path );
	}
	
	/*
	  Iterate over path searching for suitable completion files
	*/
	for( i=0; i<al_get_count( path_list ); i++ )
	{
		struct stat buf;
		wchar_t *next = (wchar_t *)al_get( path_list, i );
		sb_clear( path );
		sb_append( path, next, L"/", cmd, L".fish", (void *)0 );

		if( (wstat( (wchar_t *)path->buff, &buf )== 0) &&
			(waccess( (wchar_t *)path->buff, R_OK ) == 0) )
		{
			if( !tm || (tm[0] != buf.st_mtime ) )
			{
				wchar_t *esc = escape( (wchar_t *)path->buff, 1 );
				wchar_t *src_cmd = wcsdupcat( L". ", esc );
				free( esc );
				
				if( !tm )
				{
					tm = malloc(sizeof(time_t)*2);
					if( !tm )
					{
						DIE_MEM();
					}
				}

				tm[0] = buf.st_mtime;
				tm[1] = time(0);
				hash_put( &loaded->load_time,
						  intern( cmd ),
						  tm );

				if( on_load )
				{
					on_load(cmd );
				}
				
				/*
				  Source the completion file for the specified completion
				*/
				if( exec_subshell( src_cmd, 0 ) == -1 )
				{
					/*
					  Do nothing on failiure
					*/
				}
				
				free(src_cmd);
				reloaded = 1;
			}
			else if( tm )
			{
				/*
				  If we are rechecking an autoload file, and it hasn't
				  changed, update the 'last check' timestamp.
				*/
				tm[1] = time(0);				
			}
			
			break;
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
		{
			DIE_MEM();
		}
		
		tm[0] = 0;
		tm[1] = time(0);
		hash_put( &loaded->load_time, intern( cmd ), tm );
	}

	return reloaded;	
}

void parse_util_set_argv( wchar_t **argv, array_list_t *named_arguments )
{
	if( *argv )
	{
		wchar_t **arg;
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

	if( named_arguments )
	{
		wchar_t **arg;
		int i;
		
		for( i=0, arg=argv; i < al_get_count( named_arguments ); i++ )
		{
			env_set( al_get( named_arguments, i ), *arg, ENV_LOCAL );

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

