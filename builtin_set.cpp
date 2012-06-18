/** \file builtin_set.c Functions defining the set builtin

Functions used for implementing the set builtin. 

*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "env.h"
#include "expand.h"
#include "common.h"
#include "wgetopt.h"
#include "proc.h"
#include "parser.h"

/* We know about these buffers */
extern wcstring stdout_buffer, stderr_buffer;

/**
   Error message for invalid path operations
*/
#define BUILTIN_SET_PATH_ERROR L"%ls: Could not add component %ls to %ls.\n"

/**
   Hint for invalid path operation with a colon
*/
#define BUILTIN_SET_PATH_HINT L"%ls: Did you mean 'set %ls $%ls %ls'?\n"

/**
   Error for mismatch between index count and elements
*/
#define BUILTIN_SET_ARG_COUNT L"%ls: The number of variable indexes does not match the number of values\n"

/**
   Test if the specified variable should be subject to path validation
*/
static int is_path_variable( const wchar_t *env )
{
	return contains(env, L"PATH", L"CDPATH" );
}

/**
   Call env_set. If this is a path variable, e.g. PATH, validate the
   elements. On error, print a description of the problem to stderr.
*/
static int my_env_set( const wchar_t *key, wcstring_list_t &val, int scope )
{
	size_t i;
	int retcode = 0;
	const wchar_t *val_str=NULL;
		
	if( is_path_variable( key ) )
	{
		int error = 0;
		
		for( i=0; i< val.size() ; i++ )
		{
			int show_perror = 0;
			int show_hint = 0;
			
			struct stat buff;
			const wchar_t *dir = val[i].c_str();
			
			if( wstat( dir, &buff ) )
			{
				error = 1;
				show_perror = 1;
			}

			if( !( S_ISDIR(buff.st_mode) ) )
			{
				error = 1;
				
			}
			
			if( error )
			{
				const wchar_t *colon;
                append_format(stderr_buffer, _(BUILTIN_SET_PATH_ERROR), L"set", dir, key);
				colon = wcschr( dir, L':' );
				
				if( colon && *(colon+1) ) 
				{
					show_hint = 1;
				}
				
			}
			
			if( show_perror )
			{
				builtin_wperror( L"set" );
			}
			
			if( show_hint )
			{
                append_format(stderr_buffer, _(BUILTIN_SET_PATH_HINT), L"set", key, key, wcschr( dir, L':' )+1);
			}
			
			if( error )
			{
				break;
			}
			
		}

		if( error )
		{
			return 1;
		}
		
	}

	wcstring sb;
	if(  val.size() )
	{
		for( i=0; i< val.size() ; i++ )
		{
            sb.append(val[i]);
			if( i<val.size() - 1 )
			{
				sb.append( ARRAY_SEP_STR );
			}
		}
		val_str = sb.c_str();
	}
	
	switch( env_set( key, val_str, scope | ENV_USER ) )
	{
		case ENV_PERM:
		{
            append_format(stderr_buffer, _(L"%ls: Tried to change the read-only variable '%ls'\n"), L"set", key);
			retcode=1;
			break;
		}
		
		case ENV_INVALID:
		{
			append_format(stderr_buffer, _(L"%ls: Unknown error"), L"set" );
			retcode=1;
			break;
		}
	}

	return retcode;
}



/** 
	Extract indexes from a destination argument of the form name[index1 index2...]

	\param indexes the list to insert the new indexes into
	\param src the source string to parse
	\param name the name of the element. Return null if the name in \c src does not match this name
	\param var_count the number of elements in the array to parse. 

	\return the total number of indexes parsed, or -1 on error
*/
static int parse_index( std::vector<long> &indexes,
						const wchar_t *src,
						const wchar_t *name,
						int var_count )
{
	size_t len;
	
	int count = 0;
	const wchar_t *src_orig = src;
	
	if (src == 0)
	{
		return 0;
	}
	
	while (*src != L'\0' && (iswalnum(*src) || *src == L'_'))
	{
		src++;
	}
	
	if (*src != L'[')
	{
		append_format(stderr_buffer, _(BUILTIN_SET_ARG_COUNT), L"set" );					
		return 0;
	}
	
	len = src-src_orig;
	
	if( (wcsncmp( src_orig, name, len )!=0) || (wcslen(name) != (len)) )
	{
		append_format(stderr_buffer, 
				   _(L"%ls: Multiple variable names specified in single call (%ls and %.*ls)\n"),
				   L"set", 
				   name,
				   len,
				   src_orig);
		return 0;
	}

	src++;	

	while (iswspace(*src)) 
	{
		src++;
	}
	
	while (*src != L']') 
	{
		wchar_t *end;
		
		long l_ind;

		errno = 0;
		
		l_ind = wcstol(src, &end, 10);
		
		if( end==src || errno ) 
		{
			append_format(stderr_buffer, _(L"%ls: Invalid index starting at '%ls'\n"), L"set", src);
			return 0;
		}

		if( l_ind < 0 )
		{
			l_ind = var_count+l_ind+1;
		}
		
		indexes.push_back( l_ind );
		src = end;
		count++;
		while (iswspace(*src)) src++;
	}

	return count;
}

static int update_values( wcstring_list_t &list, 
						  std::vector<long> &indexes,
						  wcstring_list_t &values ) 
{
	size_t i;

	/* Replace values where needed */
	for( i = 0; i < indexes.size(); i++ ) 
	{
		/*
		  The '- 1' below is because the indices in fish are
		  one-based, but the vector uses zero-based indices
		*/
		long ind = indexes[i] - 1;
		const wcstring newv = values[ i ];
		if( ind < 0 )
		{
			return 1;
		}
		
//		free((void *) al_get(list, ind));
		list[ ind ] = newv; 
	}
  
	return 0;
}

/**
   Erase from a list of wcstring values at specified indexes 
*/
static void erase_values(wcstring_list_t &list, const std::vector<long> &indexes) 
{
    // Make a set of indexes.
    // This both sorts them into ascending order and removes duplicates.
    const std::set<long> indexes_set(indexes.begin(), indexes.end());
    
    // Now walk the set backwards, so we encounter larger indexes first, and remove elements at the given (1-based) indexes.
    std::set<long>::const_reverse_iterator iter;
    for (iter = indexes_set.rbegin(); iter != indexes_set.rend(); iter++) {
        long val = *iter;
        if (val > 0 && val <= list.size()) {
            // One-based indexing!
            list.erase(list.begin() + val - 1);
        }
    }
}


/**
   Print the names of all environment variables in the scope, with or without shortening,
   with or without values, with or without escaping
*/
static void print_variables(int include_values, int esc, bool shorten_ok, int scope) 
{
    wcstring_list_t names = env_get_names(scope);
    sort(names.begin(), names.end());
	
	for( size_t i = 0; i < names.size(); i++ )
	{
		const wcstring key = names.at(i);
        const wcstring e_key = escape_string(key, 0);

		stdout_buffer.append(e_key);
		
		if( include_values ) 
		{
			env_var_t value = env_get_string(key);
			if( !value.missing() )
			{
				int shorten = 0;
				
				if( shorten_ok && value.length() > 64 )
				{
					shorten = 1;
					value.resize(60);
				}
				
				wcstring e_value = esc ? expand_escape_variable(value) : value;
				
                stdout_buffer.append(L" ");
                stdout_buffer.append(e_value);
				
				if( shorten )
				{
					stdout_buffer.append(L"\u2026");
				}

			}
		}
		
		stdout_buffer.append(L"\n");
	}
}



/**
   The set builtin. Creates, updates and erases environment variables
   and environemnt variable arrays.
*/
static int builtin_set( parser_t &parser, wchar_t **argv ) 
{
	
	/**
	   Variables used for parsing the argument list
	*/
	static const struct woption
		long_options[] = 
		{
			{ 
				L"export", no_argument, 0, 'x' 
			}
			,
			{ 
				L"global", no_argument, 0, 'g' 
			}
			,
			{ 
				L"local", no_argument, 0, 'l'  
			}
			,
			{ 
				L"erase", no_argument, 0, 'e'  
			}
			,
			{ 
				L"names", no_argument, 0, 'n' 
			} 
			,
			{ 
				L"unexport", no_argument, 0, 'u' 
			} 
			,
			{ 
				L"universal", no_argument, 0, 'U'
			}
            ,
			{ 
				L"long", no_argument, 0, 'L'
			} 
			,
			{ 
				L"query", no_argument, 0, 'q' 
			} 
			,
			{ 
				L"help", no_argument, 0, 'h' 
			} 
			,
			{ 
				0, 0, 0, 0 
			}
		}
	;
	
	const wchar_t *short_options = L"+xglenuULqh";

	int argc = builtin_count_args(argv);

	/*
	  Flags to set the work mode
	*/
	int local = 0, global = 0, exportv = 0;
	int erase = 0, list = 0, unexport=0;
	int universal = 0, query=0;
	bool shorten_ok = true;

	/*
	  Variables used for performing the actual work
	*/
	wchar_t *dest = 0;
	int retcode=0;
	int scope;
	int slice=0;
	int i;
	
	wchar_t *bad_char;
	
	
	/* Parse options to obtain the requested operation and the modifiers */
	woptind = 0;
	while (1) 
	{
		int c = wgetopt_long(argc, argv, short_options, long_options, 0);

		if (c == -1) 
		{
			break;
		}
    
		switch(c) 
		{
			case 0:
				break;

			case 'e':
				erase = 1;
				break;

			case 'n':
				list = 1;
				break;

			case 'x':
				exportv = 1;
				break;

			case 'l':
				local = 1;
				break;

			case 'g':
				global = 1;
				break;

			case 'u':
				unexport = 1;
				break;

			case 'U':
				universal = 1;
				break;
            
            case 'L':
                shorten_ok = false;
                break;

			case 'q':
				query = 1;
				break;

			case 'h':
				builtin_print_help( parser, argv[0], stdout_buffer );
				return 0;

			case '?':
				builtin_unknown_option( parser, argv[0], argv[woptind-1] );
				return 1;

			default:
				break;
		}
	}

	/*
	  Ok, all arguments have been parsed, let's validate them
	*/

	/*
	  If we are checking the existance of a variable (-q) we can not
	  also specify scope
	*/

	if( query && (erase || list) )
	{
		append_format(stderr_buffer,
				  BUILTIN_ERR_COMBO,
				  argv[0] );
		
		builtin_print_help( parser, argv[0], stderr_buffer );
		return 1;
	}
	

	/* We can't both list and erase varaibles */
	if( erase && list ) 
	{
		append_format(stderr_buffer,
				  BUILTIN_ERR_COMBO,
				  argv[0] );		

		builtin_print_help( parser, argv[0], stderr_buffer );
		return 1;
	}

	/*
	  Variables can only have one scope
	*/
	if( local + global + universal > 1 ) 
	{
		append_format(stderr_buffer,
				   BUILTIN_ERR_GLOCAL,
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return 1;
	}

	/*
	  Variables can only have one export status
	*/
	if( exportv && unexport ) 
	{
		append_format(stderr_buffer,
				   BUILTIN_ERR_EXPUNEXP,
				   argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return 1;
	}

	/*
	  Calculate the scope value for variable assignement
	*/
	scope = (local ? ENV_LOCAL : 0) | (global ? ENV_GLOBAL : 0) | (exportv ? ENV_EXPORT : 0) | (unexport ? ENV_UNEXPORT : 0) | (universal ? ENV_UNIVERSAL:0) | ENV_USER; 

	if( query )
	{
		/*
		  Query mode. Return the number of variables that do not exist
		  out of the specified variables.
		*/
		int i;
		for( i=woptind; i<argc; i++ )
		{
			wchar_t *arg = argv[i];
			int slice=0;

			if( !(dest = wcsdup(arg)))
			{
				DIE_MEM();		
			}

			if( wcschr( dest, L'[' ) )
			{
				slice = 1;
				*wcschr( dest, L'[' )=0;
			}
			
			if( slice )
			{
				std::vector<long> indexes;
				wcstring_list_t result;
				size_t j;
				
                env_var_t dest_str = env_get_string(dest);
                if (! dest_str.missing())
                    tokenize_variable_array( dest_str, result );
								
				if( !parse_index( indexes, arg, dest, result.size() ) )
				{
					builtin_print_help( parser, argv[0], stderr_buffer );
					retcode = 1;
					break;
				}
				for( j=0; j < indexes.size() ; j++ )
				{
					long idx = indexes[j];
					if( idx < 1 || (size_t)idx > result.size() )
					{
						retcode++;
					}
				}
			}
			else
			{
				if( !env_exist( arg, scope ) )
				{
					retcode++;
				}
			}
			
			free( dest );
			
		}
		return retcode;
	}
	
	if( list ) 
	{
		/* Maybe we should issue an error if there are any other arguments? */
		print_variables(0, 0, shorten_ok, scope);
		return 0;
	} 
	
	if( woptind == argc )
	{
		/*
		  Print values of variables
		*/

		if( erase ) 
		{
			append_format(stderr_buffer,
					   _(L"%ls: Erase needs a variable name\n"), 
					   argv[0] );
			
			builtin_print_help( parser, argv[0], stderr_buffer );
			retcode = 1;
		}
		else
		{
			print_variables( 1, 1, shorten_ok, scope );
		}
		
		return retcode;
	}

	if( !(dest = wcsdup(argv[woptind])))
	{
		DIE_MEM();		
	}

	if( wcschr( dest, L'[' ) )
	{
		slice = 1;
		*wcschr( dest, L'[' )=0;
	}
	
	if( !wcslen( dest ) )
	{
		free( dest );
		append_format(stderr_buffer, BUILTIN_ERR_VARNAME_ZERO, argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return 1;
	}
	
	if( (bad_char = wcsvarname( dest ) ) )
	{
		append_format(stderr_buffer, BUILTIN_ERR_VARCHAR, argv[0], *bad_char );
		builtin_print_help( parser, argv[0], stderr_buffer );
		free( dest );
		return 1;
	}
	
	if( slice && erase && (scope != ENV_USER) )
	{
		free( dest );
		append_format(stderr_buffer, _(L"%ls: Can not specify scope when erasing array slice\n"), argv[0] );
		builtin_print_help( parser, argv[0], stderr_buffer );
		return 1;
	}
	
	/*
	  set assignment can work in two modes, either using slices or
	  using the whole array. We detect which mode is used here.
	*/
	
	if( slice )
	{

		/*
		  Slice mode
		*/
		int idx_count, val_count;
		wcstring_list_t values;
		std::vector<long> indexes;
		wcstring_list_t result;
		
        const env_var_t dest_str = env_get_string(dest);
        if (! dest_str.missing())
            tokenize_variable_array( dest_str, result );
		
		for( ; woptind<argc; woptind++ )
		{			
			if( !parse_index( indexes, argv[woptind], dest, result.size() ) )
			{
				builtin_print_help( parser, argv[0], stderr_buffer );
				retcode = 1;
				break;
			}
			
			val_count = argc-woptind-1;
			idx_count = indexes.size();

			if( !erase )
			{
				if( val_count < idx_count )
				{
					append_format(stderr_buffer, _(BUILTIN_SET_ARG_COUNT), argv[0] );
					builtin_print_help( parser, argv[0], stderr_buffer );
					retcode=1;
					break;
				}
				if( val_count == idx_count )
				{
					woptind++;
					break;
				}
			}
		}		

		if( !retcode )
		{
			/*
			  Slice indexes have been calculated, do the actual work
			*/

			if( erase )
			{
				erase_values(result, indexes);
				my_env_set( dest, result, scope);
			}
			else
			{
				wcstring_list_t value;
//				al_init(&value);

				while( woptind < argc ) 
				{
					value.push_back( argv[woptind++] );
				}

				if( update_values( result, 
								   indexes,
								   value ) )
				{
					append_format(stderr_buffer, L"%ls: ", argv[0] );
					append_format(stderr_buffer, ARRAY_BOUNDS_ERR );
					stderr_buffer.push_back(L'\n');
				}
				
				my_env_set(dest, result, scope);
								
//				al_destroy( &value );
								
			}			
		}

//		al_foreach( &result, &free );
//		al_destroy( &result );

//		al_destroy(&indexes);
//		al_destroy(&values);
		
	}
	else
	{
		woptind++;
		
		/*
		  No slicing
		*/
		if( erase )
		{
			if( woptind != argc )
			{
				append_format(stderr_buffer, 
						   _(L"%ls: Values cannot be specfied with erase\n"),
						   argv[0] );
				builtin_print_help( parser, argv[0], stderr_buffer );
				retcode=1;
			}
			else
			{
				retcode = env_remove( dest, scope );
			}
		}
		else
		{
            wcstring_list_t val;
			for( i=woptind; i<argc; i++ )
                val.push_back(argv[i]);
			retcode = my_env_set( dest, val, scope );
		}		
	}
	
	free( dest );
	
	return retcode;

}

