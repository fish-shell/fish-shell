/** \file builtin_set.c Functions defining the set builtin

Functions used for implementing the set builtin. 

*/
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>

#include "config.h"
#include "util.h"
#include "builtin.h"
#include "env.h"
#include "expand.h"
#include "common.h"
#include "wgetopt.h"
#include "proc.h"
#include "parser.h"

/** 
	Extract the name from a destination argument of the form name[index1 index2...]
*/
static int parse_fill_name( string_buffer_t *name, 
							const wchar_t *src) 
{

	if (src == 0) 
	{
		return 0;
	}
	
	while (iswalnum(*src) || *src == L'_')
	{
		sb_append_char(name, *src++);
	}
	
	if (*src != L'[' && *src != L'\0') 
	{
		sb_printf( sb_err, BUILTIN_ERR_VARCHAR, L"set", *src );
		sb_append2(sb_err, parser_current_line(), L"\n", (void *)0 );
//		builtin_print_help( L"set", sb_err );

		return -1;
	}
	else 
	{
		return 0;
	}
}


/** 
	Extract indexes from a destination argument of the form name[index1 index2...]
*/
static int parse_fill_indexes( array_list_t *indexes,
							   const wchar_t *src)
{
	int count = 0;

	if (src == 0)
	{
		return 0;
	}
	
	while (*src != L'\0' && (iswalnum(*src) || *src == L'_'))
	{
		src++;
	}
	
	if (*src == L'\0') 
	{
		return 0;
	}
	
	if (*src++ != L'[')
	{
		return -1;
	}
	
	while (iswspace(*src)) 
	{
		src++;
	}
	
	while (*src != L']') 
	{
		wchar_t *end;
		long l_ind = wcstol(src, &end, 10);
		if (end == src) 
		{
			wchar_t sbuf[256];
			swprintf(sbuf, 255, L"Invalid index starting at %ls\n", src);
			sb_append(sb_err, sbuf);
			return -1;
		}

		int *ind = (int *) calloc(1, sizeof(int));
		*ind = (int) l_ind;
		al_push(indexes, ind);
		src = end;
		count++;
		while (iswspace(*src)) src++;
	}

	return count;
}


/**
   Update a list by writing the specified values at the specified indexes
*/
static int update_values( array_list_t *list, 
						  array_list_t *indexes,
						  array_list_t *values ) 
{
	int i;

	//fwprintf(stderr, L"Scan complete\n");
	/* Replace values where needed */
	for( i = 0; i < al_get_count(indexes); i++ ) 
	{
		int ind = *(int *) al_get(indexes, i) - 1;
		void *new = (void *) al_get(values, i);
		if (al_get(list, ind) != 0) 
		{
			free((void *) al_get(list, ind));
		}
		al_set(list, ind, new != 0 ? wcsdup(new) : wcsdup(L""));
	}
  
	return al_get_count(list);
}


/**
   Return 1 if an array list of int* pointers contains the specified
   value, 0 otherwise
*/
static int al_contains_int( array_list_t *list,
							int val)
{
	int i;

	for (i = 0; i < al_get_count(list); i++) 
	{
		int *current = (int *) al_get(list, i);
		if (current != 0 && *current == val) 
		{
			return 1;
		}
	}
	
	return 0;
}


/**
   Erase from a list values at specified indexes 
*/
static int erase_values(array_list_t *list, array_list_t *indexes) 
{
	int i;
	array_list_t result;

	//fwprintf(stderr, L"Starting with %d\n", al_get_count(list));

	al_init(&result);

	for (i = 0; i < al_get_count(list); i++) 
	{
		if (!al_contains_int(indexes, i + 1)) 
		{
			al_push(&result, al_get(list, i));
		}
		else 
		{
			free((void *) al_get(list, i));
		}
	}
	
	al_truncate(list,0);	
	al_push_all( list, &result );
	al_destroy(&result);

	//fwprintf(stderr, L"Remaining: %d\n", al_get_count(&result));

	return al_get_count(list);    
}


/**
   Fill a string buffer with values from a list, using ARRAY_SEP_STR to separate them 
*/
static int fill_buffer_from_list(string_buffer_t *sb, array_list_t *list) 
{
	int i;

	for (i = 0; i < al_get_count(list); i++) 
	{
		wchar_t *v = (wchar_t *) al_get(list, i);
		if (v != 0) 
		{
			// fwprintf(stderr, L".\n");
			// fwprintf(stderr, L"Collecting %ls from %d\n", v, i);
			sb_append(sb, v);
		}    
		if (i < al_get_count(list) - 1)
			sb_append(sb, ARRAY_SEP_STR);
	}
	return al_get_count(list);
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
		wchar_t *e_key = esc ? escape(key, 1) : wcsdup(key);

		sb_append(sb_out, e_key);
		
		if( include_values ) 
		{
			wchar_t *value = env_get(key);
			wchar_t *e_value;
			e_value = esc ? expand_escape_variable(value) : wcsdup(value);
			sb_append2(sb_out, L" ", e_value, (void *)0);
			free(e_value);
		}
		
		sb_append(sb_out, L"\n");
		free(e_key);
	}
  	al_destroy(&names);
}

/**
   The set builtin. Creates, updates and erases environment variables and environemnt variable arrays. 
*/

int builtin_set( wchar_t **argv ) 
{
	const static struct woption
		long_options[] = 
		{
			{ 
				L"export", no_argument, 0, 'x' 
			},
			{ 
				L"global", no_argument, 0, 'g' 
			},
			{ 
				L"local", no_argument, 0, 'l'  
			},
			{ 
				L"erase", no_argument, 0, 'e'  
			},
			{ 
				L"names", no_argument, 0, 'n' 
			} ,
			{ 
				L"unexport", no_argument, 0, 'u' 
			} ,
			{ 
				L"universal", no_argument, 0, 'U' 
			} ,
			{ 
				L"query", no_argument, 0, 'q' 
			} ,
			{ 
				0, 0, 0, 0 
			}
		}
	;
	
	wchar_t short_options[] = L"+xglenuUq";

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
	array_list_t values;
	string_buffer_t name_sb;
	int retcode=0;
	wchar_t *name;
	array_list_t indexes;
	int retval;


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
			case '?':
				return 1;
			default:
				break;
		}
	}

	if( query && (erase || list || global || local || universal || export || unexport ) )
	{
		sb_append2(sb_err,
				  argv[0],
				  BUILTIN_ERR_COMBO,
				  L"\n",
				  parser_current_line(),
				  L"\n",
				  (void *)0);
		builtin_print_help( argv[0], sb_err );
		return 1;
	}
	

	/* Check operation and modifiers sanity */
	if( erase && list ) 
	{
		sb_append2(sb_err,
				  argv[0],
				  BUILTIN_ERR_COMBO,
				  L"\n",
				  parser_current_line(),
				  L"\n",
				  (void *)0);
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( local + global + universal > 1 ) 
	{
		sb_printf( sb_err,
				   L"%ls%ls\n%ls\n",
				   argv[0],
				   BUILTIN_ERR_GLOCAL,
				   parser_current_line() );
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( export && unexport ) 
	{
		sb_append2(sb_err,
				   argv[0],
				   BUILTIN_ERR_EXPUNEXP,
				   L"\n",
				   parser_current_line(),
				   L"\n", 
				   (void *)0);
		builtin_print_help( argv[0], sb_err );
		return 1;
	}

	if( query )
	{
		/*
		  Query mode. Return number of specified variables that do not exist.
		*/
		int i;
		for( i=woptind; i<argc; i++ )
		{
			if( !env_exist( argv[i] ) )
				retcode++;
			
		}
		return retcode;
	}
	

	/* Parse destination */
	if( woptind < argc ) 
	{
		dest = wcsdup(argv[woptind++]);
		//fwprintf(stderr, L"Dest: %ls\n", dest);

		if( !wcslen( dest ) )
		{
			free( dest );
			sb_printf( sb_err, BUILTIN_ERR_VARNAME_ZERO, argv[0] );
			return 1;
		}		
	}

	/* Parse values */
	// wchar_t **values = woptind < argc ? (wchar_t **) calloc(argc - woptind, sizeof(wchar_t *)) : 0;

	al_init(&values);
	while( woptind < argc ) 
	{
		al_push(&values, argv[woptind++]);
		//    fwprintf(stderr, L"Val: %ls\n", argv[woptind - 1]);
	}

	/* Extract variable name and indexes */

	sb_init(&name_sb);
	retval = parse_fill_name(&name_sb, dest);

	
	if( retval < 0 ) 
		retcode=1;

	if( !retcode )
	{
		name = (wchar_t *) name_sb.buff;
		//fwprintf(stderr, L"Name is %ls\n", name);

		al_init(&indexes);
		retval = parse_fill_indexes(&indexes, dest);
		if (retval < 0) 
			retcode = 1;
	}
	
	if( !retcode )
	{
		
		int i;
		int finished=0;
		
		/* Do the actual work */
		int scope = (local ? ENV_LOCAL : 0) | (global ? ENV_GLOBAL : 0) | (export ? ENV_EXPORT : 0) | (unexport ? ENV_UNEXPORT : 0) | (universal ? ENV_UNIVERSAL:0) | ENV_USER; 
		if( list ) 
		{
			/* Maybe we should issue an error if there are any other arguments */
			print_variables(0, 0, scope);
			finished=1;			
		} 
		
		if( (!finished ) && 
			 (name == 0 || wcslen(name) == 0))
		{
			/* No arguments -- display name & value for all variables in scope */
			if( erase ) 
			{
				sb_append2( sb_err,
							argv[0],
							L": Erase needs a variable name\n", 
							parser_current_line(), 
							L"\n",
							(void *)0 );
				builtin_print_help( argv[0], sb_err );
				retcode = 1;
			}
			else 
			{
				print_variables( 1, 1, scope );
			}

			finished=1;			
		} 


		if( !finished )
		{
			if( al_get_count( &values ) == 0 && 
				al_get_count( &indexes ) == 0 &&
				!erase &&
				!list )
			{
				/*
				  Only update the variable scope
				*/
				env_set( name, 0, scope );
				finished = 1;
			}
		}
		
		if( !finished )
		{
			/* There are some arguments, we have at least a variable name */
			if( erase && al_get_count(&values) != 0 ) 
			{
				sb_append2( sb_err, 
							argv[0],
							L": Values cannot be specfied with erase\n",
							parser_current_line(),
							L"\n",
							(void *)0 );
				builtin_print_help( argv[0], sb_err );
				retcode = 1;
			} 
			else
			{    
				/* All ok, we can alter the specified variable */
				array_list_t val_l;
				al_init(&val_l);

				void *old=0;
				
				if (al_get_count(&indexes) == 0) 
				{
					/* We will act upon the entire variable */
	
 					al_push( &val_l, wcsdup(L"") );
					old = val_l.arr;
					
					/* Build indexes for all variable or all new values */
					int end_index = erase ? al_get_count(&val_l) : al_get_count(&values);
					for (i = 0; i < end_index; i++) 
					{
						int *ind = (int *) calloc(1, sizeof(int));
						*ind = i + 1;
						al_push(&indexes, ind);
					}
				}
				else 
				{
					/* We will act upon some specific indexes */
					expand_variable_array( env_get(name), &val_l );
				}
				
				string_buffer_t result_sb;
				sb_init(&result_sb);
				if (erase) 
				{
					int rem = erase_values(&val_l, &indexes);
					if (rem == 0) 
					{
						env_remove(name, ENV_USER);
					}
					else 
					{
						fill_buffer_from_list(&result_sb, &val_l);
						env_set(name, (wchar_t *) result_sb.buff, scope);
					}     
				}
				else
				{
					
					update_values( &val_l, 
								   &indexes,
								   &values );

					fill_buffer_from_list( &result_sb,
										   &val_l );

					env_set(name,
							(wchar_t *) result_sb.buff,
							scope);
				}
				
				al_foreach( &val_l, (void (*)(const void *))&free );
				al_destroy(&val_l);
				sb_destroy(&result_sb); 
			}
		}

		al_foreach( &indexes, (void (*)(const void *))&free );
		al_destroy(&indexes);
	}
	
/* Common cleanup */
//fwprintf(stderr, L"Cleanup\n");
	free(dest);		
	sb_destroy(&name_sb);
	al_destroy( &values );
	
	return retcode;
}

