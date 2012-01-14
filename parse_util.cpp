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

struct autoload_function_t
{   
    bool is_placeholder; //whether we are a placeholder that stands in for "no such function"
    time_t modification_time; // st_mtime
    time_t load_time; // when function was loaded
    
    
    autoload_function_t() : is_placeholder(false), modification_time(0), load_time(0) { }
};

/**
   A structure representing the autoload state for a specific variable, e.g. fish_complete_path
*/
struct autoload_t
{    
private:
    typedef std::map<wcstring, autoload_function_t> autoload_functions_map_t;
    autoload_functions_map_t autoload_functions;
public:

	/**
	   A table containing all the files that are currently being
	   loaded. This is here to help prevent recursion.
	*/
    std::set<wcstring> is_loading_set;
    
    bool is_loading(const wcstring &name) const {
        return is_loading_set.find(name) != is_loading_set.end();
    }
    
    autoload_function_t *create_function_with_name(const wcstring &name)
    {
        return &autoload_functions[name];
    }
    
    bool remove_function_with_name(const wcstring &name)
    {
        return autoload_functions.erase(name) > 0;
    }
    
    autoload_function_t *get_function_with_name(const wcstring &name)
    {
        autoload_function_t *result = NULL;
        autoload_functions_map_t::iterator iter = autoload_functions.find(name);
        if (iter != autoload_functions.end())
            result = &iter->second;
        return result;
    }
    
    void remove_all_functions(void)
    {
        autoload_functions.clear();
    }
    
    size_t function_count(void) const
    {
        return autoload_functions.size();
    }
    
    /* Get the name of the function that was least recently loaded, if it was loaded before cutoff_access. Return NULL if no function qualifies. */ 
    const wcstring *get_lru_function_name(const wcstring &skip, time_t cutoff_access) const
    {
        const wcstring *resultName = NULL;
        const autoload_function_t *resultFunction = NULL;
        autoload_functions_map_t::const_iterator iter;
        for (iter = autoload_functions.begin(); iter != autoload_functions.end(); iter++)
        {
            /* Skip the skip */
            if (iter->first == skip) continue;
            
            /* Skip items that are still loading */
            if (is_loading(iter->first)) continue;
            
            /* Skip placeholder items */
            if (iter->second.is_placeholder) continue;
            
            /* Check cutoff_access */
            if (iter->second.load_time > cutoff_access) continue;

            /* Remember this if it was used earlier */
            if (resultFunction == NULL || iter->second.load_time < resultFunction->load_time) {
                resultName = &iter->first;
                resultFunction = &iter->second;
            }
        }
        return resultName;
    }
    
    void apply_handler_to_nonplaceholder_function_names(void (*handler)(const wchar_t *cmd)) const
    {
        autoload_functions_map_t::const_iterator iter;
        for (iter = autoload_functions.begin(); iter != autoload_functions.end(); iter++)
            handler(iter->first.c_str());
    }
    
	/**
	   A string containg the path used to find any files to load. If
	   this differs from the current environment variable, the
	   autoloader needs to drop all loaded files and reload them.
	*/
	wcstring old_path;

};


/**
   Set of files which have been autoloaded
*/

typedef std::map<wcstring, autoload_t> autoload_map_t;
static autoload_map_t all_loaded_map;

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

void parse_util_load_reset( const wchar_t *path_var_name,
							void (*on_load)(const wchar_t *cmd) )
{
	CHECK( path_var_name, );

    autoload_map_t::iterator condemned = all_loaded_map.find(path_var_name);
    if (condemned != all_loaded_map.end())
    {
        autoload_t &loaded = condemned->second;
        if (on_load) {
            /*  Call the on_load handler on each real function name. */
            loaded.apply_handler_to_nonplaceholder_function_names(on_load);
        }
        /* Empty the functino set */
        loaded.remove_all_functions();
        all_loaded_map.erase(condemned);
    }
	
}

int parse_util_unload( const wchar_t *cmd,
					   const wchar_t *path_var_name,
					   void (*on_load)(const wchar_t *cmd) )
{
	int result = 0;

	CHECK( path_var_name, 0 );
	CHECK( cmd, 0 );

    autoload_map_t::iterator iter = all_loaded_map.find(path_var_name);
    if (iter == all_loaded_map.end())
    {
        return 0;
    }
    
    autoload_t &loaded = iter->second;
    if (loaded.remove_function_with_name(cmd))
    {
        if (on_load)
            on_load(cmd);
        result = 1;
    }
    return result;
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
    autoload_map_t::iterator iter = all_loaded_map.find(path_var_name);
    if (iter == all_loaded_map.end())
    {
        return;
    }
    autoload_t &loaded = iter->second;
	
	if( loaded.function_count() >= AUTOLOAD_MAX )
	{
        time_t cutoff_access = time(0) - AUTOLOAD_MIN_AGE;
        const wcstring *lru = loaded.get_lru_function_name(skip, cutoff_access);
        if (lru)
            parse_util_unload( lru->c_str(), path_var_name, on_load );
    }
}

static int parse_util_load_internal( const wcstring &cmd,
                                     const struct builtin_script_t *builtin_scripts,
                                     size_t builtin_script_count,
									 void (*on_load)(const wchar_t *cmd),
									 int reload,
									 autoload_t &loaded,
									 const std::vector<wcstring> &path_list );


int parse_util_load( const wcstring &cmd,
					 const wcstring &path_var_name,
					 void (*on_load)(const wchar_t *cmd),
					 int reload )
{
	int res;
	int c, c2;
    autoload_t *loaded;
	
	CHECK_BLOCK( 0 );
	
//	debug( 0, L"Autoload %ls in %ls", cmd, path_var_name );

	parse_util_autounload( path_var_name.c_str(), cmd.c_str(), on_load );
	const env_var_t path_var = env_get_string( path_var_name.c_str() );	
	
	/*
	  Do we know where to look?
	*/
	if( path_var.empty() )
	{
		return 0;
	}

    autoload_map_t::iterator iter = all_loaded_map.find(path_var_name);
    if (iter != all_loaded_map.end())
    {
        loaded = &iter->second;
        
		/*
		  Check if the lookup path has changed. If so, drop all loaded
		  files and start from scratch.
		*/
		if( path_var != loaded->old_path )
		{
			parse_util_load_reset( path_var_name.c_str(), on_load);
			reload = parse_util_load( cmd, path_var_name, on_load, reload );
			return reload;
		}

		/**
		   Warn and fail on infinite recursion
		*/
        if (loaded->is_loading(cmd))
		{
			debug( 0, 
				   _( L"Could not autoload item '%ls', it is already being autoloaded. " 
					  L"This is a circular dependency in the autoloading scripts, please remove it."), 
				   cmd.c_str() );
			return 1;
		}

	}
	else
	{
		/*
		  We have never tried to autoload using this path name before,
		  set up initial data
		*/
//		debug( 0, L"Create brand new autoload_t for %ls->%ls", path_var_name, path_var.c_str() );
        loaded = &all_loaded_map[path_var_name];
		loaded->old_path = wcsdup(path_var.c_str());
	}

    std::vector<wcstring> path_list;
	tokenize_variable_array2( path_var, path_list );
	
	c = path_list.size();
	
    loaded->is_loading_set.insert(cmd);

    /* Figure out which builtin-scripts array to search (if any) */
    const builtin_script_t *builtins = NULL;
    size_t builtin_count = 0;
    if (path_var_name == L"fish_function_path")
    {
        builtins = internal_function_scripts;
        builtin_count = sizeof internal_function_scripts / sizeof *internal_function_scripts;
    }

	/*
	  Do the actual work in the internal helper function
	*/

	res = parse_util_load_internal( cmd, builtins, builtin_count, on_load, reload, *loaded, path_list );

    autoload_map_t::iterator iter2 = all_loaded_map.find(path_var_name);
    if (iter2 != all_loaded_map.end()) {
        autoload_t *loaded2 = &iter2->second;
        if( loaded2 == loaded )
        {
            /**
               Cleanup
            */
            std::set<wcstring>::iterator condemned = loaded->is_loading_set.find(cmd);
            assert(condemned != loaded->is_loading_set.end());
            loaded->is_loading_set.erase(condemned);
        }
    }
	
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

static int parse_util_load_internal( const wcstring &cmd,
                                     const builtin_script_t *builtin_scripts,
                                     size_t builtin_script_count,
									 void (*on_load)(const wchar_t *cmd),
									 int reload,
									 autoload_t &loaded,
									 const std::vector<wcstring> &path_list )
{

	size_t i;
	int reloaded = 0;

    /* Get the function */
    autoload_function_t * func = loaded.get_function_with_name(cmd);

    /* Return if already loaded and we are skipping reloading */
	if( !reload && func )
		return 0;

    /* Nothing to do if we just checked it */
    if (func && time(NULL) - func->load_time <= 1)
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
            struct stat buf;
            wcstring next = path_list.at(i);
            wcstring path = next + L"/" + cmd + L".fish";

            if( (wstat( path.c_str(), &buf )== 0) &&
                (waccess( path.c_str(), R_OK ) == 0) )
            {
                if( !func || (func->modification_time != buf.st_mtime ) )
                {
                    wcstring esc = escape_string(path, 1);
                    script_source = L". " + esc;
                    has_script_source = true;
                    
                    if( !func )
                        func = loaded.create_function_with_name(cmd);

                    func->modification_time = buf.st_mtime;
                    func->load_time = time(NULL);
                    
                    if( on_load )
                        on_load(cmd.c_str());
                    
                    reloaded = 1;
                }
                else if( func )
                {
                    /*
                      If we are rechecking an autoload file, and it hasn't
                      changed, update the 'last check' timestamp.
                    */
                    func->load_time = time(NULL);				
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
            func = loaded.create_function_with_name(cmd);
            func->load_time = time(NULL);
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

