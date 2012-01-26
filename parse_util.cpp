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
#include "builtin_scripts.h"

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

autoload_t::autoload_t(const wcstring &env_var_name_var, const builtin_script_t * const scripts, size_t script_count) :
                       env_var_name(env_var_name_var),
                       builtin_scripts(scripts),
                       builtin_script_count(script_count)
{
}

void autoload_t::node_was_evicted(autoload_function_t *node) {
    // Tell ourselves that the command was removed, unless it was a placeholder
    if (! node->is_placeholder)
        this->command_removed(node->key);
    delete node;
}

void autoload_t::reset( )
{
    this->evict_all_nodes();
}

int autoload_t::unload( const wcstring &cmd )
{
    return this->evict_node(cmd);
}

int autoload_t::load( const wcstring &cmd, bool reload )
{
	int res;
	int c, c2;
	
	CHECK_BLOCK( 0 );

	const env_var_t path_var = env_get_string( env_var_name.c_str() );	
	
	/*
	  Do we know where to look?
	*/
	if( path_var.empty() )
	{
		return 0;
	}
    
    /*
      Check if the lookup path has changed. If so, drop all loaded
      files.
    */
    if( path_var != this->path )
    {
        this->path = path_var;
        this->reset();
    }

    /**
       Warn and fail on infinite recursion
    */
    if (this->is_loading(cmd))
    {
        debug( 0, 
               _( L"Could not autoload item '%ls', it is already being autoloaded. " 
                  L"This is a circular dependency in the autoloading scripts, please remove it."), 
               cmd.c_str() );
        return 1;
    }



    std::vector<wcstring> path_list;
	tokenize_variable_array2( path_var, path_list );
	
	c = path_list.size();
	
    is_loading_set.insert(cmd);

	/*
	  Do the actual work in the internal helper function
	*/
	res = this->load_internal( cmd, reload, path_list );
    
    int erased = is_loading_set.erase(cmd);
    assert(erased);
	
	c2 = path_list.size();

	/**
	   Make sure we didn't 'drop' something
	*/
	
	assert( c == c2 );
	
	return res;
}

static bool script_name_precedes_script_name(const builtin_script_t &script1, const builtin_script_t &script2)
{
    return wcscmp(script1.name, script2.name) < 0;
}

/**
   This internal helper function does all the real work. By using two
   functions, the internal function can return on various places in
   the code, and the caller can take care of various cleanup work.
*/

int autoload_t::load_internal( const wcstring &cmd,
                               int reload,
                               const wcstring_list_t &path_list )
{

	size_t i;
	int reloaded = 0;

    /* Get the function */
    autoload_function_t * func = this->get_function_with_name(cmd);

    /* Return if already loaded and we are skipping reloading */
	if( !reload && func )
		return 0;

    /* Nothing to do if we just checked it */
    if (func && time(NULL) - func->access.last_checked <= 1)
        return 0;
    
    /* The source of the script will end up here */
    wcstring script_source;
    bool has_script_source = false;
    
    /*
     Look for built-in scripts via a binary search
    */
    const builtin_script_t *matching_builtin_script = NULL;
    if (builtin_script_count > 0)
    {
        const builtin_script_t test_script = {cmd.c_str(), NULL};
        const builtin_script_t *array_end = builtin_scripts + builtin_script_count;
        const builtin_script_t *found = std::lower_bound(builtin_scripts, array_end, test_script, script_name_precedes_script_name);
        if (found != array_end && ! wcscmp(found->name, test_script.name))
        {
            /* We found it */
            matching_builtin_script = found;
        }
    }
    if (matching_builtin_script) {
        has_script_source = true;
        script_source = str2wcstring(matching_builtin_script->def);
    }
    
    if (! has_script_source)
    {
        /*
          Iterate over path searching for suitable completion files
        */
        for( i=0; i<path_list.size(); i++ )
        {
            wcstring next = path_list.at(i);
            wcstring path = next + L"/" + cmd + L".fish";

            const file_access_attempt_t access = access_file(path, R_OK);
            if (access.accessible) {
                if (! func || access.mod_time != func->access.mod_time) {
                    wcstring esc = escape_string(path, 1);
                    script_source = L". " + esc;
                    has_script_source = true;
                    
                    if( !func )
                        func = new autoload_function_t(cmd);
                    func->access = access;
                    
                    // Remove this command because we are going to reload it
                    command_removed(cmd);
                                        
                    reloaded = 1;
                }
                else if( func )
                {
                    /*
                      If we are rechecking an autoload file, and it hasn't
                      changed, update the 'last check' timestamp.
                    */
                    func->access = access;
                }
                
                break;
            }
        }

        /*
          If no file was found we insert a placeholder function. Later we only
          research if the current time is at least five seconds later.
          This way, the files won't be searched over and over again.
        */
        if( !func )
        {
            func = new autoload_function_t(cmd);
            func->access.last_checked = time(NULL);
            func->is_placeholder = true;
        }
    }
    
    /* If we have a script, either built-in or a file source, then run it */
    if (has_script_source)
    {
        if( exec_subshell( script_source.c_str(), 0 ) == -1 )
        {
            /*
              Do nothing on failiure
            */
        }

    }

	return reloaded;	
}

void parse_util_set_argv( wchar_t **argv, const wcstring_list_t &named_arguments )
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

	if( named_arguments.size() )
	{
		wchar_t **arg;
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

