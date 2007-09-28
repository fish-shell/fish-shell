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
	return contains( env,
						 L"PATH",
						 L"CDPATH" );
}

/**
   Call env_set. If this is a path variable, e.g. PATH, validate the
   elements. On error, print a description of the problem to stderr.
*/
static int my_env_set( const wchar_t *key, array_list_t *val, int scope )
{
	string_buffer_t sb;
	int i;
	int retcode = 0;
	wchar_t *val_str=0;
		
	if( is_path_variable( key ) )
	{
		int error = 0;
		
		for( i=0; i<al_get_count( val ); i++ )
		{
			int show_perror = 0;
			int show_hint = 0;
			
			struct stat buff;
			wchar_t *dir = (wchar_t *)al_get( val, i );
			
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
				wchar_t *colon;
				
				sb_printf( sb_err, 
						   _(BUILTIN_SET_PATH_ERROR),
						   L"set", 
						   dir, 
						   key );
				
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
				sb_printf( sb_err, 
						   _(BUILTIN_SET_PATH_HINT),
						   L"set",
						   key,
						   key,
						   wcschr( dir, L':' )+1);
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

	sb_init( &sb );

	if( al_get_count( val ) )
	{
		for( i=0; i<al_get_count( val ); i++ )
		{
			wchar_t *next =(wchar_t *)al_get( val, i );
			sb_append( &sb, next?next:L"" );
			if( i<al_get_count( val )-1 )
			{
				sb_append( &sb, ARRAY_SEP_STR );
			}
		}
		val_str = (wchar_t *)sb.buff;
		
	}
	
	switch( env_set( key, val_str, scope | ENV_USER ) )
	{
		case ENV_PERM:
		{
			sb_printf( sb_err, _(L"%ls: Tried to change the read-only variable '%ls'\n"), L"set", key );
			retcode=1;
			break;
		}
		
		case ENV_INVALID:
		{
			sb_printf( sb_err, _(L"%ls: Unknown error"), L"set" );
			retcode=1;
			break;
		}
	}

	sb_destroy( &sb );

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
static int parse_index( array_list_t *indexes,
						const wchar_t *src,
						const wchar_t *name,
						int var_count )
{
	int len;
	
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
		sb_printf( sb_err, _(BUILTIN_SET_ARG_COUNT), L"set" );					
		return 0;
	}
	
	len = src-src_orig;
	
	if( (wcsncmp( src_orig, name, len )!=0) || (wcslen(name) != (len)) )
	{
		sb_printf( sb_err, 
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
			sb_printf(sb_err, _(L"%ls: Invalid index starting at '%ls'\n"), L"set", src);
			return 0;
		}

		if( l_ind < 0 )
		{
			l_ind = var_count+l_ind+1;
		}
		
		al_push_long(indexes, l_ind);
		src = end;
		count++;
		while (iswspace(*src)) src++;
	}

	return count;
}


/**
   Update a list \c list by writing copies (using wcsdup) of the
   values specified by \c values to the indexes specified by \c
   indexes. The previous entries at the specidied position will be
   free'd.

   \return 0 if the operation was successfull, non-zero otherwise
*/
static int update_values( array_list_t *list, 
						  array_list_t *indexes,
						  array_list_t *values ) 
{
	int i;

	/* Replace values where needed */
	for( i = 0; i < al_get_count(indexes); i++ ) 
	{
		/*
		  The '- 1' below is because the indices in fish are
		  one-based, but the array_list_t uses zero-based indices
		*/
		long ind = al_get_long(indexes, i) - 1;
		void *new = (void *) al_get(values, i);
		if( ind < 0 )
		{
			return 1;
		}
		
		free((void *) al_get(list, ind));
		al_set(list, ind, new != 0 ? wcsdup(new) : wcsdup(L""));
	}
  
	return 0;
}


/**
   Return 1 if an array list of longs contains the specified
   value, 0 otherwise
*/
static int al_contains_long( array_list_t *list,
							long val)
{
	int i;

	for (i = 0; i < al_get_count(list); i++) 
	{
		long current = al_get_long(list, i);
		if( current != 0 && current == val ) 
		{
			return 1;
		}
	}
	
	return 0;
}


/**
   Erase from a list values at specified indexes 
*/
static void erase_values(array_list_t *list, array_list_t *indexes) 
{
	long i;
	array_list_t result;

	al_init(&result);

	for (i = 0; i < al_get_count(list); i++) 
	{
		if (!al_contains_long(indexes, i + 1)) 
		{
			al_push(&result, al_get(list, i));
		}
		else 
		{
			free( (void *)al_get(list, i));
		}
	}
	
	al_truncate(list,0);	
	al_push_all( list, &result );
	al_destroy(&result);
}


/**
   Print the names of all environment variables in the scope, with or without values,
   with or without escaping
*/
static void print_variables(int include_values, int esc, int scope) 
{
	array_list_t names;
	int i;
	
	al_init( &names );
	env_get_names( &names, scope );
	
	sort_list( &names );
	
	for( i = 0; i < al_get_count(&names); i++ )
	{
		wchar_t *key = (wchar_t *)al_get( &names, i );
		wchar_t *e_key = esc ? escape(key, 0) : wcsdup(key);

		sb_append(sb_out, e_key);
		
		if( include_values ) 
		{
			wchar_t *value = env_get(key);
			wchar_t *e_value;
			if( value )
			{
				int shorten = 0;
				
				if( wcslen( value ) > 64 )
				{
					shorten = 1;
					value = wcsndup( value, 60 );
					if( !value )
					{
						DIE_MEM();
					}
				}
				
				e_value = esc ? expand_escape_variable(value) : wcsdup(value);
				
				sb_append(sb_out, L" ", e_value, (void *)0);
				free(e_value);
				
				if( shorten )
				{
					sb_append(sb_out, L"\u2026");
					free( value );
				}

			}
		}
		
		sb_append(sb_out, L"\n");
		free(e_key);
	}
  	al_destroy(&names);
}



/**
   The set builtin. Creates, updates and erases environment variables
   and environemnt variable arrays.
*/
static int builtin_set( wchar_t **argv ) 
{
	
	/**
	   Variables used for parsing the argument list
	*/
	const static struct woption
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
	
	const wchar_t *short_options = L"+xglenuUqh";

	int argc = builtin_count_args(argv);

	/*
	  Flags to set the work mode
	*/
	int local = 0, global = 0, export = 0;
	int erase = 0, list = 0, unexport=0;
	int universal = 0, query=0;
	

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
				export = 1;
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

			case 'q':
				query = 1;
				break;

			case 'h':
				builtin_print_help( argv[0], sb_out );
				return 0;

			case '?':
				builtin_unknown_option( argv[0], argv[woptind-1] );
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

	if( query && (erase || list || global || local || universal || export || unexport ) )
	{
		sb_printf(sb_err,
				  BUILTIN_ERR_COMBO,
				  argv[0] );
		
		builtin_print_help( argv[0], sb_err );
		return 1;
	}
	

	/* We can't both list and erase varaibles */
	if( erase && list ) 
	{
		sb_printf(sb_err,
				  BUILTIN_ERR_COMBO,
				  argv[0] );		

		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	/*
	  Variables can only have one scope
	*/
	if( local + global + universal > 1 ) 
	{
		sb_printf( sb_err,
				   BUILTIN_ERR_GLOCAL,
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	/*
	  Variables can only have one export status
	*/
	if( export && unexport ) 
	{
		sb_printf( sb_err,
				   BUILTIN_ERR_EXPUNEXP,
				   argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	/*
	  Calculate the scope value for variable assignement
	*/
	scope = (local ? ENV_LOCAL : 0) | (global ? ENV_GLOBAL : 0) | (export ? ENV_EXPORT : 0) | (unexport ? ENV_UNEXPORT : 0) | (universal ? ENV_UNIVERSAL:0) | ENV_USER; 

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
				array_list_t indexes;
				array_list_t result;
				int j;
				
				al_init( &result );
				al_init( &indexes );

				tokenize_variable_array( env_get( dest ), &result );
								
				if( !parse_index( &indexes, arg, dest, al_get_count( &result ) ) )
				{
					builtin_print_help( argv[0], sb_err );
					retcode = 1;
					break;
				}
				for( j=0; j<al_get_count( &indexes ); j++ )
				{
					long idx = al_get_long( &indexes, j );
					if( idx < 1 || idx > al_get_count( &result ) )
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
		print_variables(0, 0, scope);
		return 0;
	} 
	
	if( woptind == argc )
	{
		/*
		  Print values of variables
		*/

		if( erase ) 
		{
			sb_printf( sb_err,
					   _(L"%ls: Erase needs a variable name\n%ls\n"), 
					   argv[0] );
			
			builtin_print_help( argv[0], sb_err );
			retcode = 1;
		}
		else
		{
			print_variables( 1, 1, scope );
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
		sb_printf( sb_err, BUILTIN_ERR_VARNAME_ZERO, argv[0] );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}
	
	if( (bad_char = wcsvarname( dest ) ) )
	{
		sb_printf( sb_err, BUILTIN_ERR_VARCHAR, argv[0], *bad_char );
		builtin_print_help( argv[0], sb_err );
		free( dest );
		return 1;
	}
	
	if( slice && erase && (scope != ENV_USER) )
	{
		free( dest );
		sb_printf( sb_err, _(L"%ls: Can not specify scope when erasing array slice\n"), argv[0] );
		builtin_print_help( argv[0], sb_err );
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
		array_list_t values;
		array_list_t indexes;
		array_list_t result;
		
		al_init(&values);
		al_init(&indexes);
		al_init(&result);
		
		tokenize_variable_array( env_get(dest), &result );
		
		for( ; woptind<argc; woptind++ )
		{			
			if( !parse_index( &indexes, argv[woptind], dest, al_get_count( &result ) ) )
			{
				builtin_print_help( argv[0], sb_err );
				retcode = 1;
				break;
			}
			
			val_count = argc-woptind-1;
			idx_count = al_get_count( &indexes );

			if( !erase )
			{
				if( val_count < idx_count )
				{
					sb_printf( sb_err, _(BUILTIN_SET_ARG_COUNT), argv[0] );
					builtin_print_help( argv[0], sb_err );
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
				erase_values(&result, &indexes);
				my_env_set( dest, &result, scope);
			}
			else
			{
				array_list_t value;
				al_init(&value);

				while( woptind < argc ) 
				{
					al_push(&value, argv[woptind++]);
				}

				if( update_values( &result, 
								   &indexes,
								   &value ) )
				{
					sb_printf( sb_err, L"%ls: ", argv[0] );
					sb_printf( sb_err, ARRAY_BOUNDS_ERR );
					sb_append( sb_err, L"\n" );
				}
				
				my_env_set(dest,
						   &result,
						   scope);
								
				al_destroy( &value );
								
			}			
		}

		al_foreach( &result, &free );
		al_destroy( &result );

		al_destroy(&indexes);
		al_destroy(&values);
		
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
				sb_printf( sb_err, 
						   _(L"%ls: Values cannot be specfied with erase\n"),
						   argv[0] );
				builtin_print_help( argv[0], sb_err );
				retcode=1;
			}
			else
			{
				retcode = env_remove( dest, scope );
			}
		}
		else
		{
			array_list_t val;
			al_init( &val );
			
			for( i=woptind; i<argc; i++ )
			{
				al_push( &val, argv[i] );
			}

			retcode = my_env_set( dest, &val, scope );
						
			al_destroy( &val );			
			
		}		
	}
	
	free( dest );
	
	return retcode;

}

