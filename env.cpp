/** \file env.c
	Functions for setting and getting environment variables.
*/
#include "config.h"

#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <pwd.h>
#include <set>
#include <map>
#include <algorithm>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <errno.h>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "proc.h"
#include "common.h"
#include "env.h"
#include "sanity.h"
#include "expand.h"
#include "history.h"
#include "reader.h"
#include "parser.h"
#include "env_universal.h"
#include "input_common.h"
#include "event.h"
#include "path.h"

#include "complete.h"

/**
   Command used to start fishd
*/
#define FISHD_CMD L"fishd ^/tmp/fishd.log.%s"

/**
   Value denoting a null string
*/
#define ENV_NULL L"\x1d"

/**
   At init, we read all the environment variables from this array.
*/
extern char **environ;
/**
   This should be the same thing as \c environ, but it is possible only one of the two work...
*/
extern char **__environ;

/**
   A variable entry. Stores the value of a variable and whether it
   should be exported. Obviously, it needs to be allocated large
   enough to fit the value string.
*/
struct var_entry_t
{
	bool exportv; /**< Whether the variable should be exported */
	wcstring val; /**< The value of the variable */

	var_entry_t() : exportv(false) { } 
};


/**
   Struct representing one level in the function variable stack
*/
struct env_node_t
{
	/** 
		Variable table 
	*/
	std::map<wcstring, var_entry_t*> env;
	/** 
		Does this node imply a new variable scope? If yes, all
		non-global variables below this one in the stack are
		invisible. If new_scope is set for the global variable node,
		the universe will explode.
	*/
	int new_scope;
	/**
	   Does this node contain any variables which are exported to subshells
	*/
	int exportv;
	
	/** 
		Pointer to next level
	*/
	struct env_node_t *next;


	env_node_t() : new_scope(0), exportv(0), next(NULL) { }
};

class variable_entry_t {
    bool exportv; /**< Whether the variable should be exported */
    wcstring value; /**< Value of the variable */
};

static pthread_mutex_t env_lock = PTHREAD_MUTEX_INITIALIZER;

/**
   Top node on the function stack
*/
static env_node_t *top=0;

/**
   Bottom node on the function stack
*/
static env_node_t *global_env = 0;


/**
   Table for global variables
*/
static std::map<wcstring, var_entry_t *> *global;

/**
   Table of variables that may not be set using the set command.
*/
static std::set<wcstring> env_read_only;

static bool is_read_only(const wcstring &key)
{
    return env_read_only.find(key) != env_read_only.end();
}

/**
   Table of variables whose value is dynamically calculated, such as umask, status, etc
*/
static std::set<wcstring> env_electric;

static bool is_electric(const wcstring &key)
{
    return env_electric.find(key) != env_electric.end();
}


/**
   Exported variable array used by execv
*/
static char **export_arr=0;

/**
   Buffer used for storing string contents for export_arr
*/
static buffer_t export_buffer;


/**
   Flag for checking if we need to regenerate the exported variable
   array
*/
static int has_changed = 1;

/**
   This string is used to store the value of dynamically
   generated variables, such as history.
*/
static wcstring dyn_var;

/**
   Variable used by env_get_names to communicate auxiliary information
   to add_key_to_string_set
*/
static int get_names_show_exported;
/**
   Variable used by env_get_names to communicate auxiliary information
   to add_key_to_string_set
*/
static int get_names_show_unexported;

/**
   List of all locale variable names
*/
static const wchar_t *locale_variable[] =
{
	L"LANG",
	L"LC_ALL",
	L"LC_COLLATE",
	L"LC_CTYPE",
	L"LC_MESSAGES",
	L"LC_MONETARY",
	L"LC_NUMERIC",
	L"LC_TIME",
	NULL
}
	;


/**
   When fishd isn't started, this function is provided to
   env_universal as a callback, it tries to start up fishd. It's
   implementation is a bit of a hack, since it evaluates a bit of
   shellscript, and it might be used at times when that might not be
   the best idea.
*/
static void start_fishd()
{
	struct passwd *pw = getpwuid(getuid());
	
	debug( 3, L"Spawning new copy of fishd" );
	
	if( !pw )
	{
		debug( 0, _( L"Could not get user information" ) );
		return;
	}
	
    wcstring cmd = format_string(FISHD_CMD, pw->pw_name);	
    parser_t &parser = parser_t::principal_parser();
	parser.eval( cmd, 0, TOP );
}

/**
   Return the current umask value.
*/
static mode_t get_umask()
{
	mode_t res;
	res = umask( 0 );
	umask( res );
	return res;
}

/**
   Checks if the specified variable is a locale variable
*/
static int is_locale( const wchar_t *key )
{
	int i;
	for( i=0; locale_variable[i]; i++ )
	{
		if( wcscmp(locale_variable[i], key ) == 0 )
		{
			return 1;
		}
	}
	return 0;
}

/**
  Properly sets all locale information
*/
static void handle_locale()
{
	const env_var_t lc_all = env_get_string( L"LC_ALL" );
	int i;
    const wcstring old_locale = wsetlocale( LC_MESSAGES, NULL );

	/*
	  Array of locale constants corresponding to the local variable names defined in locale_variable
	*/
	static const int cat[] = 
		{
			0, 
			LC_ALL, 
			LC_COLLATE,
			LC_CTYPE,
			LC_MESSAGES,
			LC_MONETARY,
			LC_NUMERIC,
			LC_TIME
		}
	;
	
	if( !lc_all.missing() )
	{
		wsetlocale( LC_ALL, lc_all.c_str() );
	}
	else
	{
		const env_var_t lang = env_get_string( L"LANG" );
		if( !lang.missing() )
		{
			wsetlocale( LC_ALL, lang.c_str() );
		}
		
		for( i=2; locale_variable[i]; i++ )
		{
			const env_var_t val = env_get_string( locale_variable[i] );

			if( !val.missing() )
			{
				wsetlocale(  cat[i], val.c_str() );
			}
		}
	}
	
    const wcstring new_locale = wsetlocale(LC_MESSAGES, NULL);
	if( old_locale != new_locale )
	{

		/* 
		   Try to make change known to gettext. Both changing
		   _nl_msg_cat_cntr and calling dcgettext might potentially
		   tell some gettext implementation that the translation
		   strings should be reloaded. We do both and hope for the
		   best.
		*/

		extern int _nl_msg_cat_cntr;
		_nl_msg_cat_cntr++;

		dcgettext( "fish", "Changing language to English", LC_MESSAGES );
		
		if( get_is_interactive() )
		{
			debug( 0, _(L"Changing language to English") );
		}
	}	
}


/**
   Universal variable callback function. This function makes sure the
   proper events are triggered when an event occurs.
*/
static void universal_callback( int type,
								const wchar_t *name, 
								const wchar_t *val )
{
	const wchar_t *str=0;
	
	if( is_locale( name ) )
	{
		handle_locale();
	}
	
	switch( type )
	{
		case SET:
		case SET_EXPORT:
		{
			str=L"SET";
			break;
		}
		
		case ERASE:
		{
			str=L"ERASE";
			break;
		}
	}
	
	if( str )
	{
		has_changed=1;
		
        event_t ev = event_t::variable_event(name);
        ev.arguments.reset(new wcstring_list_t());
        ev.arguments->push_back(L"VARIABLE");
        ev.arguments->push_back(str);
        ev.arguments->push_back(name);
		event_fire( &ev );
        ev.arguments.reset(NULL);
	}
}

/**
   Make sure the PATH variable contains the essential directories
*/
static void setup_path()
{	
    size_t i;
	int j;
	wcstring_list_t lst;
    
	const wchar_t *path_el[] = 
    {
        L"/bin",
        L"/usr/bin",
        PREFIX L"/bin",
        0
    }
	;
    
	env_var_t path = env_get_string( L"PATH" );
	
	if( !path.missing() )
	{
		tokenize_variable_array( path, lst );
	}
	
	for( j=0; path_el[j]; j++ )
	{
        
		int has_el=0;
		
		for( i=0; i<lst.size(); i++ )
		{
			wcstring el = lst.at(i);
			size_t len = el.size();
            
			while( (len > 0) && (el[len-1]==L'/') )
			{
				len--;
			}
            
			if( (wcslen( path_el[j] ) == len) && 
               (wcsncmp( el.c_str(), path_el[j], len)==0) )
			{
				has_el = 1;
			}
		}
		
		if( !has_el )
		{
			wcstring buffer;
            
			debug( 3, L"directory %ls was missing", path_el[j] );
            
			if( !path.missing() )
			{
                buffer += path;
			}
			
            buffer += ARRAY_SEP_STR;
            buffer += path_el[j];
			
			env_set( L"PATH", buffer.empty()?NULL:buffer.c_str(), ENV_GLOBAL | ENV_EXPORT );
            
			path = env_get_string( L"PATH" );
            lst.resize(0);
			tokenize_variable_array( path, lst );			
		}
	}
}

int env_set_pwd()
{
	wchar_t dir_path[4096];
	wchar_t *res = wgetcwd( dir_path, 4096 );
	if( !res )
	{
		return 0;
	}
	env_set( L"PWD", dir_path, ENV_EXPORT | ENV_GLOBAL );
	return 1;
}

/**
   Set up default values for various variables if not defined.
 */
static void env_set_defaults()
{

	if( env_get_string(L"USER").missing() )
	{
		struct passwd *pw = getpwuid( getuid());
		wchar_t *unam = str2wcs( pw->pw_name );
		env_set( L"USER", unam, ENV_GLOBAL );
		free( unam );
	}

	if( env_get_string(L"HOME").missing() )
	{
		const env_var_t unam = env_get_string( L"USER" );
		char *unam_narrow = wcs2str( unam.c_str() );
		struct passwd *pw = getpwnam( unam_narrow );
		wchar_t *dir = str2wcs( pw->pw_dir );
		env_set( L"HOME", dir, ENV_GLOBAL );
		free( dir );		
		free( unam_narrow );
	}	

	env_set_pwd();
	
}

void env_init()
{
	char **p;
	struct passwd *pw;
	wchar_t *uname;
	wchar_t *version;
	
	b_init( &export_buffer );
	
	/*
	  env_read_only variables can not be altered directly by the user
	*/
    
    const wchar_t * const ro_keys[] = {
        L"status",
        L"history",
        L"version",
        L"_",
        L"LINES",
        L"COLUMNS",
        L"PWD",
        L"SHLVL",
    };
    for (size_t i=0; i < sizeof ro_keys / sizeof *ro_keys; i++) {
        env_read_only.insert(ro_keys[i]);
    }
	
	/*
	  HOME and USER should be writeable by root, since this can be a
	  convenient way to install software.
	*/
	if( getuid() != 0 )
	{
        env_read_only.insert(L"HOME");
        env_read_only.insert(L"USER");
	}
	
	/*
     Names of all dynamically calculated variables
     */
    env_electric.insert(L"history");
    env_electric.insert(L"status");
    env_electric.insert(L"umask");
    
	top = new env_node_t;
	global_env = top;
	global = &top->env;	
	
	/*
	  Now the environemnt variable handling is set up, the next step
	  is to insert valid data
	*/
	
	/*
	  Import environment variables
	*/
	for( p=environ?environ:__environ; p && *p; p++ )
	{
		wchar_t *key, *val;
		wchar_t *pos;
		
		key = str2wcs(*p);

		if( !key )
		{
			continue;
		}
		
		val = wcschr( key, L'=' );

		if( val == 0 )
		{
			env_set( key, L"", ENV_EXPORT );
		}
		else
		{ 
			*val = L'\0';
			val++;
			pos=val;
			//fwprintf( stderr, L"Set $%ls to %ls\n", key, val );
			while( *pos )
			{
				if( *pos == L':' )
				{
					*pos = ARRAY_SEP;
				}
				pos++;
			}

			env_set( key, val, ENV_EXPORT | ENV_GLOBAL );
		}		
		free(key);
	}
	
	/*
	  Set up the PATH variable
	*/
	setup_path();

	/*
	  Set up the USER variable
	*/
	pw = getpwuid( getuid() );
	if( pw )
	{
		uname = str2wcs( pw->pw_name );
		env_set( L"USER", uname, ENV_GLOBAL | ENV_EXPORT );
		free( uname );
	}

	/*
	  Set up the version variable
	*/
	version = str2wcs( PACKAGE_VERSION );
	env_set( L"version", version, ENV_GLOBAL );
	free( version );

	const env_var_t fishd_dir_wstr = env_get_string( L"FISHD_SOCKET_DIR");
	const env_var_t user_dir_wstr = env_get_string( L"USER" );

	wchar_t * fishd_dir = fishd_dir_wstr.missing()?NULL:const_cast<wchar_t*>(fishd_dir_wstr.c_str());
	wchar_t * user_dir = user_dir_wstr.missing()?NULL:const_cast<wchar_t*>(user_dir_wstr.c_str());

	env_universal_init(fishd_dir , user_dir , 
						&start_fishd,
						&universal_callback );

	/*
	  Set up SHLVL variable
	*/
	const env_var_t shlvl_str = env_get_string( L"SHLVL" );
	const wchar_t *shlvl = shlvl_str.missing() ? NULL : shlvl_str.c_str();

	if ( shlvl )
	{
		wchar_t *nshlvl, **end_nshlvl;
		/* add an extra space for digit dump (9+1=10) */
		size_t i =  wcslen( shlvl ) + 2 * sizeof(wchar_t);

		nshlvl = (wchar_t *)malloc(i);
		end_nshlvl = (wchar_t **)calloc( 1, sizeof(nshlvl) );
		if ( !nshlvl || !end_nshlvl )
				DIE_MEM();

		if ( nshlvl && swprintf( nshlvl, i,
								 L"%ld", wcstoul( shlvl, end_nshlvl, 10 )+1 ) != -1 )
		{
			env_set( L"SHLVL",
				     nshlvl,
				     ENV_GLOBAL | ENV_EXPORT );
		}
		free( end_nshlvl );
		free( nshlvl );
	}
	else
	{
		env_set( L"SHLVL",
				 L"1",
				 ENV_GLOBAL | ENV_EXPORT );
	}

	/*
	  Set correct defaults for e.g. USER and HOME variables
	*/
	env_set_defaults();
}

void env_destroy()
{
	env_universal_destroy();
	
	b_destroy( &export_buffer );
	
	while( &top->env != global )
	{
		env_pop();
	}
	
    env_read_only.clear();
    env_electric.clear();

	
	std::map<wcstring, var_entry_t*>::iterator iter;
	for (iter = global->begin(); iter != global->end(); ++iter) {
		var_entry_t *entry = iter->second;	
		if( entry->exportv )
		{
			has_changed = 1;
		}

		delete entry;
	}

	delete top;
	free( export_arr ); 
}

/**
   Search all visible scopes in order for the specified key. Return
   the first scope in which it was found.
*/
static env_node_t *env_get_node( const wcstring &key )
{
	var_entry_t* res = NULL;
	env_node_t *env = top;


	while( env != 0 )
	{
		std::map<wcstring, var_entry_t*>::const_iterator result = env->env.find( key ); 

		if ( result != env->env.end() )
		{ 
			res  = result->second; 
			return env;
		}

		if( env->new_scope )
		{
			env = global_env;
		}
		else		
		{
			env = env->next;
		}
	}
	
	return 0;
}

int env_set( const wchar_t *key, 
			 const wchar_t *val, 
			 int var_mode )
{
	env_node_t *node = NULL;
	int has_changed_old = has_changed;
	int has_changed_new = 0;
	var_entry_t *e=0;	
	int done=0;
    
	int is_universal = 0;	
	
	CHECK( key, ENV_INVALID );
    
	if( val && contains( key, L"PWD", L"HOME" ) )
	{
        /* Canoncalize our path; if it changes, recurse and try again. */
        wcstring val_canonical = val;
        path_make_canonical(val_canonical);
        if (val != val_canonical) {
            return env_set( key, val_canonical.c_str(), var_mode );
        }
	}
    
	if( (var_mode & ENV_USER ) && is_read_only(key) )
	{
		return ENV_PERM;
	}
	
	if( wcscmp( key, L"umask" ) == 0)
	{
		wchar_t *end;
		int mask;
        
		/*
         Set the new umask
         */
		if( val && wcslen(val) )
		{				
			errno=0;
			mask = wcstol( val, &end, 8 );
            
			if( !errno && (!*end) && (mask <= 0777) && (mask >= 0) )
			{
				umask( mask );
			}
		}
		/*
         Do not actually create a umask variable, on env_get, it will
         be calculated dynamically
         */
		return 0;
	}
    
	/*
     Zero element arrays are internaly not coded as null but as this
     placeholder string
     */
	if( !val )
	{
		val = ENV_NULL;
	}
	
	if( var_mode & ENV_UNIVERSAL )
	{
		int exportv = 0;
        
		if( !(var_mode & ENV_EXPORT ) &&
           !(var_mode & ENV_UNEXPORT ) )
		{
			env_universal_get_export( key );
		}
		else 
		{
			exportv = (var_mode & ENV_EXPORT );
		}
		
		env_universal_set( key, val, exportv );
		is_universal = 1;
        
	}
	else
	{
		
		node = env_get_node( key );
		if( node )
		{
            
			std::map<wcstring, var_entry_t*>::iterator result = node->env.find(key);
			if ( result != node->env.end() ) { 
				e  = result->second; 
			}
			else {
				e = NULL;
			}
            
			if( e->exportv )
			{
				has_changed_new = 1;
			}
		}
        
		if( (var_mode & ENV_LOCAL) || 
           (var_mode & ENV_GLOBAL) )
		{
			node = ( var_mode & ENV_GLOBAL )?global_env:top;
		}
		else
		{
			if( node )
			{
				if( !(var_mode & ENV_EXPORT ) &&
                   !(var_mode & ENV_UNEXPORT ) )
				{				
					var_mode = e->exportv?ENV_EXPORT:0;
				}
			}
			else
			{
				if( !proc_had_barrier)
				{
					proc_had_barrier=1;
					env_universal_barrier();
				}
				
				if( env_universal_get( key ) )
				{
					int exportv = 0;
                    
					if( !(var_mode & ENV_EXPORT ) &&
                       !(var_mode & ENV_UNEXPORT ) )
					{
						env_universal_get_export( key );
					}
					else 
					{
						exportv = (var_mode & ENV_EXPORT );
					}
					
					env_universal_set( key, val, exportv );
					is_universal = 1;
					
					done = 1;
                    
				}
				else
				{
					/*
                     New variable with unspecified scope. The default
                     scope is the innermost scope that is shadowing,
                     which will be either the current function or the
                     global scope.				   
                     */
					node = top;
					while( node->next && !node->new_scope )
					{
						node = node->next;
					}
				}
			}
		}
        
		if( !done )
        {
		    var_entry_t *old_entry = NULL;
		    std::map<wcstring, var_entry_t*>::iterator result = node->env.find(key);
		    if ( result != node->env.end() )
		    {
				old_entry = result->second;
				node->env.erase(result);
		    }
            
			var_entry_t *entry = NULL;
			if( old_entry )
            {
			    entry = old_entry;
				
			    if( (var_mode & ENV_EXPORT) || entry->exportv )
                {
                    entry->exportv = !!(var_mode & ENV_EXPORT);
                    has_changed_new = 1;		
                }
            }	
			else
            {
			    entry = new var_entry_t;
                
			    if( var_mode & ENV_EXPORT)
                {
                    entry->exportv = 1;
                    has_changed_new = 1;		
                }
			    else
                {
                    entry->exportv = 0;
                }
				
            }
            
			entry->val = val;
			
			node->env.insert(std::pair<wcstring, var_entry_t*>(key, entry));
            
			if( entry->exportv )
            {
			    node->exportv=1;
            }
            
			has_changed = has_changed_old || has_changed_new;
        }
        
    }
    
    if( !is_universal )
    {
        event_t ev = event_t::variable_event(key);		
        ev.arguments.reset(new wcstring_list_t);
        ev.arguments->push_back(L"VARIABLE");
        ev.arguments->push_back(L"SET");
        ev.arguments->push_back(key);
		
        //	debug( 1, L"env_set: fire events on variable %ls", key );	
        event_fire( &ev );
        //	debug( 1, L"env_set: return from event firing" );	
        ev.arguments.reset(NULL);
    }
    
    if( is_locale( key ) )
    {
        handle_locale();
    }
    
    return 0;
}


/**
   Attempt to remove/free the specified key/value pair from the
   specified map.

   \return zero if the variable was not found, non-zero otherwise
*/
static int try_remove( env_node_t *n,
					   const wchar_t *key,
					   int var_mode )
{
	if( n == 0 )
	{
		return 0;
	}

	std::map<wcstring, var_entry_t*>::iterator result = n->env.find( key );  
	if ( result != n->env.end() )
	{
		var_entry_t *v = result->second;

		if( v->exportv )
		{
			has_changed = 1;
		}
        
		n->env.erase(result);
		delete v;
		return 1;
	}

	if( var_mode & ENV_LOCAL )
	{
		return 0;
	}
	
	if( n->new_scope )
	{
		return try_remove( global_env, key, var_mode );
	}
	else
	{
		return try_remove( n->next, key, var_mode );
	}
}


int env_remove( const wchar_t *key, int var_mode )
{
	env_node_t *first_node;
	int erased = 0;
	
	CHECK( key, 1 );
		
	if( (var_mode & ENV_USER ) && is_read_only(key) )
	{
		return 2;
	}
	
	first_node = top;
	
	if( ! (var_mode & ENV_UNIVERSAL ) )
	{
		
		if( var_mode & ENV_GLOBAL )
		{
			first_node = global_env;
		}
		
		if( try_remove( first_node, key, var_mode ) )
		{		
			event_t ev = event_t::variable_event(key);
            ev.arguments.reset(new wcstring_list_t);
            ev.arguments->push_back(L"VARIABLE");
            ev.arguments->push_back(L"ERASE");
            ev.arguments->push_back(key);
			
			event_fire( &ev );	
			
			ev.arguments.reset(NULL);
			erased = 1;
		}
	}
	
	if( !erased && 
		!(var_mode & ENV_GLOBAL) &&
		!(var_mode & ENV_LOCAL) ) 
	{
		erased = !env_universal_remove( key );
	}

	if( is_locale( key ) )
	{
		handle_locale();
	}
	
	return !erased;	
}

env_var_t env_var_t::missing_var(void) {
    env_var_t result(L"");
    result.is_missing = true;
    return result;
}

const wchar_t *env_var_t::c_str(void) const {
    assert(! is_missing);
    return wcstring::c_str();
}

env_var_t env_get_string( const wcstring &key )
{
    scoped_lock lock(env_lock);
    
	if( key == L"history" )
	{
        wcstring result;
		const wchar_t *current;
		int i;		
		int add_current=0;
		
		current = reader_get_buffer();
		if( current && wcslen( current ) )
		{
			add_current=1;
            result += current;
		}
		
		for( i=add_current;; i++ )
		{
            // PCA This looks bad!
			wchar_t *next = NULL;//history_get( i-add_current );
			if( !next )
			{
				break;
			}
			
			if( i!=0)
			{
                result += ARRAY_SEP_STR;
			}
            result += next;
		}
        
		return result;
	}
	else if( key == L"COLUMNS" )
	{
        return format_val((long)common_get_width());
	}	
	else if( key == L"LINES" )
	{
        return format_val((long)common_get_width());
	}
	else if( key == L"status" )
	{
        return format_val((long)proc_get_last_status());
	}
	else if( key == L"umask" )
	{
        return format_string(L"0%0.3o", get_umask() );
	}
	else {
        
        var_entry_t *res;
        env_node_t *env = top;
        wchar_t *item;
        wcstring result;
        
        while( env != 0 )
	  {
		std::map<wcstring, var_entry_t*>::iterator result =  env->env.find(key);
	    if ( result != env->env.end() ) 
	      {
			res = result->second; 
	      }
	    else
	      {
			res = 0;
	      }
	

	    if( res != 0 )
	      {
		if( res->val == ENV_NULL ) 
		  {
		    return wcstring(L"");
		  }
		else
		  {
		    return res->val;			
		  }
	      }
            
	    if( env->new_scope )
	      {
                env = global_env;
	      }
	    else
	      {
                env = env->next;
	      }
	  }	
        if( !proc_had_barrier)
        {
            proc_had_barrier=1;
            env_universal_barrier();
        }
        
        item = env_universal_get( key );
        
        if( !item || (wcscmp( item, ENV_NULL )==0))
        {
            return env_var_t::missing_var();
        }
        else
        {
            return item;
        }
    }
}

const wchar_t *env_get( const wchar_t *key )
{
    ASSERT_IS_MAIN_THREAD();

	var_entry_t *res;
	env_node_t *env = top;
	wchar_t *item;
	
	CHECK( key, 0 );

	if( wcscmp( key, L"history" ) == 0 )
	{
		const wchar_t *current;
		dyn_var.clear();
		
		current = reader_get_buffer();
		if( current && wcslen( current ) )
		{
            dyn_var.append(current);
            dyn_var.append(ARRAY_SEP_STR);
        }
        
        history_t *history = reader_get_history();
        if (history) {
            for (size_t idx = 1; idx < (size_t)(-1); idx++) {
                history_item_t item = history->item_at_index(idx);
                if (item.empty()) break;
                
                dyn_var.append(item.str());
                dyn_var.append(ARRAY_SEP_STR);
            }
        }
        
        /* We always have a trailing ARRAY_SEP_STR; get rid of it */
       if (dyn_var.size() >= wcslen(ARRAY_SEP_STR)) {
            dyn_var.resize(dyn_var.size() - wcslen(ARRAY_SEP_STR));
        }
        
		return dyn_var.c_str();
	}
	else if( wcscmp( key, L"COLUMNS" )==0 )
	{
        dyn_var = to_string<int>(common_get_width());
		return dyn_var.c_str();		
	}	
	else if( wcscmp( key, L"LINES" )==0 )
	{
        dyn_var = to_string<int>(common_get_height());
		return dyn_var.c_str();
	}
	else if( wcscmp( key, L"status" )==0 )
	{
        dyn_var = to_string<int>(proc_get_last_status());
		return dyn_var.c_str();
	}
	else if( wcscmp( key, L"umask" )==0 )
	{
        dyn_var = format_string(L"0%0.3o", get_umask());
		return dyn_var.c_str();
	}
	
	while( env != 0 )
	{
		std::map<wcstring, var_entry_t*>::iterator result =  env->env.find(key);
		if ( result != env->env.end() )
		{ 
			res  = result->second; 
		}
		else
		{
			res = 0;
		}


		if( res != 0 )
		{
			if( res->val == ENV_NULL ) 
			{
				return 0;
			}
			else
			{
				return res->val.c_str();			
			}
		}
		
		if( env->new_scope )
		{
			env = global_env;
		}
		else
		{
			env = env->next;
		}
	}	
	if( !proc_had_barrier)
	{
		proc_had_barrier=1;
		env_universal_barrier();
	}
	
	item = env_universal_get( key );
	
	if( !item || (wcscmp( item, ENV_NULL )==0))
	{
		return 0;
	}
	else
	{
		return item;
	}
}

int env_exist( const wchar_t *key, int mode )
{
	var_entry_t *res;
	env_node_t *env;
	wchar_t *item=0;

	CHECK( key, 0 );
		
	/*
	  Read only variables all exist, and they are all global. A local
	  version can not exist.
	*/
	if( ! (mode & ENV_LOCAL) && ! (mode & ENV_UNIVERSAL) )
	{
		if( is_read_only(key) || is_electric(key) )
		{
			return 1;
		}
	}

	if( ! (mode & ENV_UNIVERSAL) )
	{
		env = (mode & ENV_GLOBAL)?global_env:top;
					
		while( env != 0 )
		{
			std::map<wcstring, var_entry_t*>::iterator result = env->env.find( key );
			if ( result != env->env.end() )
			{ 
				res  = result->second; 
			}
			else
			{
				res = 0;
			}

			if( res != 0 )
			{
				return 1;
			}
			
			if( mode & ENV_LOCAL )
			{
				break;
			}
			
			if( env->new_scope )
			{
				env = global_env;
			}
			else
			{
				env = env->next;
			}
		}	
	}
	
	if( ! (mode & ENV_LOCAL) && ! (mode & ENV_GLOBAL) )
	{
		if( !proc_had_barrier)
		{
			proc_had_barrier=1;
			env_universal_barrier();
		}
		
		item = env_universal_get( key );
	
	}
	return item != 0;

}

/**
   Returns true if the specified scope or any non-shadowed non-global subscopes contain an exported variable.
*/
static int local_scope_exports( env_node_t *n )
{
	
	if( n==global_env )
		return 0;
	
	if( n->exportv )
		return 1;
	
	if( n->new_scope )
		return 0;
	
	return local_scope_exports( n->next );
}

void env_push( int new_scope )
{
	env_node_t *node = new env_node_t;
	node->next = top;
	node->new_scope=new_scope;
	
	if( new_scope )
	{
		has_changed |= local_scope_exports(top);
	}
	top = node;	

}


void env_pop()
{
	if( &top->env != global )
	{
		int i;
		int locale_changed = 0;
		
		env_node_t *killme = top;

		for( i=0; locale_variable[i]; i++ )
		{
			std::map<wcstring, var_entry_t*>::iterator result =  killme->env.find( locale_variable[i] ); 
			if ( result != killme->env.end() )
			{
				locale_changed = 1;
				break;
			}
		}

		if( killme->new_scope )
		{
			has_changed |= killme->exportv || local_scope_exports( killme->next );
		}
		
		top = top->next;

		std::map<wcstring, var_entry_t*>::iterator iter;
		for (iter = killme->env.begin(); iter != killme->env.end(); ++iter) 
		{
			var_entry_t *entry = iter->second;	
			if( entry->exportv )
			{
				has_changed = 1;
			}
			delete entry;
		}

		delete killme;

		if( locale_changed )
			handle_locale();
		
	}
	else
	{
		debug( 0,
			   _( L"Tried to pop empty environment stack." ) );
		sanity_lose();
	}	
}

/**
   Function used with to insert keys of one table into a set::set<wcstring>
*/
static void add_key_to_string_set(const std::map<wcstring, var_entry_t*> &envs, std::set<wcstring> &strSet)
{
	std::map<wcstring, var_entry_t*>::const_iterator iter;
	for (iter = envs.begin(); iter != envs.end(); ++iter)
	{
		var_entry_t *e = iter->second;

		if( ( e->exportv && get_names_show_exported) || 
			( !e->exportv && get_names_show_unexported) )
		{
		/*Insert Key*/
   	     strSet.insert(iter->first);
		}

	}
}

wcstring_list_t env_get_names( int flags )
{
    scoped_lock lock(env_lock);
    
    wcstring_list_t result;
    std::set<wcstring> names;
    int show_local = flags & ENV_LOCAL;
	int show_global = flags & ENV_GLOBAL;
	int show_universal = flags & ENV_UNIVERSAL;

	env_node_t *n=top;

	
	get_names_show_exported = 
		(flags & ENV_EXPORT) || !(flags & ENV_UNEXPORT);
	get_names_show_unexported = 
		(flags & ENV_UNEXPORT) || !(flags & ENV_EXPORT);

	if( !show_local && !show_global && !show_universal )
	{
		show_local =show_universal = show_global=1;
	}
	
	if( show_local )
	{
		while( n )
		{
			if( n == global_env )
				break;
			
			add_key_to_string_set(n->env, names);
			if( n->new_scope )
				break;		
			else
				n = n->next;

		}
	}
	
	if( show_global )
	{
		add_key_to_string_set(global_env->env, names);
		if( get_names_show_unexported ) {
            result.insert(result.end(), env_electric.begin(), env_electric.end());
        }
		
		if( get_names_show_exported )
		{
            result.push_back(L"COLUMNS");
            result.push_back(L"LINES");
		}
		
	}
	
	if( show_universal )
	{
    
        wcstring_list_t uni_list;
        env_universal_get_names2(uni_list,
                                 get_names_show_exported,
                                 get_names_show_unexported);                                 
        names.insert(uni_list.begin(), uni_list.end());
	}
	
    result.insert(result.end(), names.begin(), names.end());
    return result;
}

/**
	Get list of all exported variables
*/

static void get_exported2( const env_node_t *n, std::map<wcstring, wcstring> &h )
{
	if( !n )
		return;
	
	if( n->new_scope )
		get_exported2( global_env, h );
	else
		get_exported2( n->next, h );

	std::map<wcstring, var_entry_t*>::const_iterator iter;
	for (iter = n->env.begin(); iter != n->env.end(); ++iter)
	{
		const wcstring &key = iter->first;
		var_entry_t *val_entry = iter->second;	
		if( val_entry->exportv && (val_entry->val != ENV_NULL ) )
		{	
			h.insert(std::pair<wcstring, wcstring>(key, val_entry->val));
		}
	}
}

/**
	Function used by env_export_arr to iterate over map of variables
*/
static void export_func2(std::map<wcstring, wcstring> &envs, buffer_t* out)
{
	std::map<wcstring, wcstring>::const_iterator iter;
	for (iter = envs.begin(); iter != envs.end(); ++iter)
	{
		char* ks = wcs2str(iter->first.c_str());
		char* vs = wcs2str(iter->second.c_str()); 
		char *pos = vs;
		while( *pos )
		{
			if( *pos == ARRAY_SEP )
				*pos = ':';			
			pos++;
		}
		int nil = 0;

		b_append( out, ks, strlen(ks) );
		b_append( out, "=", 1 );
		b_append( out, vs, strlen(vs) );
		b_append( out, &nil, 1 );

		free(ks);
		free(vs);
	}	
}

char **env_export_arr( int recalc )
{
	if( recalc && !proc_had_barrier)
	{
		proc_had_barrier=1;
		env_universal_barrier();
	}
	
	if( has_changed )
	{
		std::map<wcstring, wcstring> vals;
		int prev_was_null=1;
		int pos=0;		
		size_t i;

		debug( 4, L"env_export_arr() recalc" );
				
		get_exported2( top, vals );
		
        wcstring_list_t uni;
		env_universal_get_names2( uni, 1, 0 );
		for( i=0; i<uni.size(); i++ )
		{
			const wcstring &key = uni.at(i);
			const wchar_t *val = env_universal_get( key.c_str() );

			std::map<wcstring, wcstring>::iterator result = vals.find( key ); 
			if( wcscmp( val, ENV_NULL) && ( result == vals.end() ) )
			{
				vals.insert(std::pair<wcstring, wcstring>(key, val));
			}

		}

		export_buffer.used=0;
		
		export_func2(vals, &export_buffer );
		
		export_arr = (char **)realloc( export_arr,
							  sizeof(char *)*(1 + vals.size())); //add 1 for null termination
		
		for( i=0; i<export_buffer.used; i++ )
		{
			if( prev_was_null )
			{
				export_arr[pos++]= &export_buffer.buff[i];
				debug( 3, L"%s", &export_buffer.buff[i]);
			}
			prev_was_null = (export_buffer.buff[i]==0);
		}
		export_arr[pos]=NULL;
		has_changed=0;

	}
	return export_arr;	
}

env_vars::env_vars(const wchar_t * const *keys)
{
    ASSERT_IS_MAIN_THREAD();
    for (size_t i=0; keys[i]; i++) {
        const env_var_t val = env_get_string(keys[i]);
        if (!val.missing()) {
            vars[keys[i]] = val;
        }
    }
}

env_vars::env_vars() { }

const wchar_t *env_vars::get(const wchar_t *key) const
{
    std::map<wcstring, wcstring>::const_iterator iter = vars.find(key);
    return (iter == vars.end() ? NULL : iter->second.c_str());
}

const wchar_t * const env_vars::highlighting_keys[] = {L"PATH", L"HIGHLIGHT_DELAY", L"fish_function_path", NULL};
