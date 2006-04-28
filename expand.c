/**\file expand.c

String expansion functions. These functions perform several kinds of
parameter expansion. 

*/

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <wctype.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include <assert.h>

#ifdef SunOS
#include <procfs.h>
#endif

#include "config.h"

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "env.h"
#include "proc.h"
#include "parser.h"
#include "expand.h"
#include "wildcard.h"
#include "exec.h"
#include "signal.h"
#include "tokenizer.h"
#include "complete.h"
#include "translate.h"
#include "parse_util.h"
#include "halloc_util.h"

/**
   Description for child process
*/
#define COMPLETE_CHILD_PROCESS_DESC _( L"Child process")

/**
   Description for non-child process
*/
#define COMPLETE_PROCESS_DESC _( L"Process")

/**
   Description for long job
*/
#define COMPLETE_JOB_DESC _( L"Job")

/**
   Description for short job. The job command is concatenated
*/
#define COMPLETE_JOB_DESC_VAL _( L"Job: ")

/**
   Description for the shells own pid
*/
#define COMPLETE_SELF_DESC _( L"Shell process")

/**
   Description for the shells own pid
*/
#define COMPLETE_LAST_DESC _( L"Last background job")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_DESC _( L"The '$' character begins a variable name. The character '%lc', which directly followed a '$', is not allowed as a part of a variable name, and variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'.")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_NULL_DESC _( L"The '$' begins a variable name. It was given at the end of an argument. Variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'.")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_BRACKET_DESC _( L"Did you mean {$VARIABLE}? The '$' character begins a variable name. A bracket, which directly followed a '$', is not allowed as a part of a variable name, and variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'." )

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_PARAN_DESC _( L"Did you mean (COMMAND)? In fish, the '$' character is only used for accessing variables. To learn more about command substitution in fish, type 'help expand-command-substitution'.")


/**
   String in process expansion denoting ourself
*/
#define SELF_STR L"self"

/**
   String in process expansion denoting last background job
*/
#define LAST_STR L"last"

/**
   Characters which make a string unclean if they are the first
   character of the string. See \c is_clean().
*/
#define UNCLEAN_FIRST L"~%"
/**
   Unclean characters. See \c is_clean().
*/
#define UNCLEAN L"$*?\\\"'({})"

/**
   Test if the specified argument is clean, i.e. it does not contain
   any tokens which need to be expanded or otherwise altered. Clean
   strings can be passed through expand_string and expand_one without
   changing them. About 90% of all strings are clean, so skipping
   expansion on them actually does save a small amount of time, since
   it avoids multiple memory allocations during the expansion process.
*/
static int is_clean( const wchar_t *in )
{

	const wchar_t * str = in;

	/*
	  Test characters that have a special meaning in the first character position
	*/
	if( wcschr( UNCLEAN_FIRST, *str ) )
		return 0;

	/*
	  Test characters that have a special meaning in any character position
	*/
	while( *str )
	{
		if( wcschr( UNCLEAN, *str ) )
			return 0;
		str++;
	}

	return 1;
}

/**
   Return the environment variable value for the string starting at \c in.
*/
static wchar_t *expand_var( wchar_t *in )
{
	if( !in )
		return 0;
	return env_get( in );
}

void expand_variable_array( const wchar_t *val, array_list_t *out )
{
	if( val )
	{
		wchar_t *cpy = wcsdup( val );
		wchar_t *pos, *start;

		if( !cpy )
		{
			die_mem();
		}

		for( start=pos=cpy; *pos; pos++ )
		{
			if( *pos == ARRAY_SEP )
			{
				*pos=0;
				al_push( out, start==cpy?cpy:wcsdup(start) );
				start=pos+1;
			}
		}
		al_push( out, start==cpy?cpy:wcsdup(start) );
	}
}


/**
   Test if the specified string does not contain character which can
   not be used inside a quoted string.
*/
static int is_quotable( wchar_t *str )
{
	switch( *str )
	{
		case 0:
			return 1;

		case L'\n':
		case L'\t':
		case L'\r':
		case L'\b':
		case L'\e':
			return 0;

		default:
			return is_quotable(str+1);
	}
	return 0;

}

wchar_t *expand_escape_variable( const wchar_t *in )
{

	array_list_t l;
	string_buffer_t buff;

	al_init( &l );
	expand_variable_array( in, &l );
	sb_init( &buff );

	switch( al_get_count( &l) )
	{
		case 0:
			sb_append( &buff, L"''");
			break;

		case 1:
		{
			wchar_t *el = (wchar_t *)al_get( &l, 0 );

			if( wcschr( el, L' ' ) && is_quotable( el ) )
			{
				sb_append2( &buff,
							L"'",
							el,
							L"'",
							(void *)0 );
			}
			else
			{
				wchar_t *val = escape( el, 1 );
				sb_append( &buff, val );
				free( val );
			}
			free( el );
			break;
		}
		default:
		{
			int j;
			for( j=0; j<al_get_count( &l ); j++ )
			{
				wchar_t *el = (wchar_t *)al_get( &l, j );
				if( j )
					sb_append( &buff, L"  " );

				if( is_quotable( el ) )
				{
					sb_append2( &buff,
								L"'",
								el,
								L"'",
								(void *)0 );
				}
				else
				{
					wchar_t *val = escape( el, 1 );
					sb_append( &buff, val );
					free( val );
				}

				free( el );
			}
		}
	}
	al_destroy( &l );

	return (wchar_t *)buff.buff;

}

/**
   Tests if all characters in the string are numeric
*/
static int isnumeric( const char *n )
{
	if( *n == '\0' )
		return 1;
	if( *n < '0' || *n > '9' )
		return 0;
	return isnumeric( n+1 );
}

/**
   Tests if all characters in the wide string are numeric
*/
static int iswnumeric( const wchar_t *n )
{
	if( *n == L'\0' )
		return 1;
	if( *n < L'0' || *n > L'9' )
		return 0;
	return iswnumeric( n+1 );
}

/**
   See if the process described by \c proc matches the commandline \c
   cmd
*/
static int match_pid( const wchar_t *cmd,
					  const wchar_t *proc,
					  int flags )
{
	/* Test for direct match */

	if( wcsncmp( cmd, proc, wcslen( proc ) ) == 0 )
		return 1;

	if( flags & ACCEPT_INCOMPLETE )
		return 0;

	/*
	  Test if the commandline is a path to the command, if so we try
	  to match against only the command part
	*/
	wchar_t *first_token = tok_first( cmd );
	if( first_token == 0 )
		return 0;

	wchar_t *start=0;
	wchar_t prev=0;
	wchar_t *p;

	/*
	  This should be done by basename(), if it wasn't for the fact
	  that is does not accept wide strings
	*/
	for( p=first_token; *p; p++ )
	{
		if( *p == L'/' && prev != L'\\' )
			start = p;
		prev = *p;
	}
	if( start )
	{

		if( wcsncmp( start+1, proc, wcslen( proc ) ) == 0 )
		{
			free( first_token );
			return 1;
		}
	}
	free( first_token );

	return 0;

}

/**
   Searches for a job with the specified job id, or a job or process
   which has the string \c proc as a prefix of its commandline.

   If accept_incomplete is true, the remaining string for any matches
   are inserted.

   If accept_incomplete is false, any job matching the specified
   string is matched, and the job pgid is returned. If no job
   matches, all child processes are searched. If no child processes
   match, and <tt>fish</tt> can understand the contents of the /proc
   filesystem, all the users processes are searched for matches.
*/

static int find_process( const wchar_t *proc,
						 int flags,
						 array_list_t *out )
{
	DIR *dir;
	struct dirent *next;
	char *pdir_name;
	char *pfile_name;
	wchar_t *cmd=0;
	int sz=0;
	int found = 0;
	wchar_t *result;

	job_t *j;

	if( iswnumeric(proc) || (wcslen(proc)==0) )
	{
		/*
		  This is a numeric job string, like '%2'
		*/

		if( flags & ACCEPT_INCOMPLETE )
		{
			for( j=first_job; j != 0; j=j->next )
			{
				wchar_t jid[16];
				if( j->command == 0 )
					continue;

				swprintf( jid, 16, L"%d", j->job_id );

				if( wcsncmp( proc, jid, wcslen(proc ) )==0 )
				{
					al_push( out,
							 wcsdupcat2( jid+wcslen(proc),
										 COMPLETE_SEP_STR,
										 COMPLETE_JOB_DESC_VAL,
										 j->command,
										 (void *)0 ) );


				}
			}

		}
		else
		{

			int jid = wcstol( proc, 0, 10 );
			if( jid > 0 )
			{
				j = job_get( jid );
				if( (j != 0) && (j->command != 0 ) )
				{
					
					{
						result = malloc(sizeof(wchar_t)*16 );
						swprintf( result, 16, L"%d", j->pgid );
						al_push( out, result );
						found = 1;
					}
				}
			}
		}
	}
	if( found )
		return 1;

	for( j=first_job; j != 0; j=j->next )
	{
		if( j->command == 0 )
			continue;

		if( match_pid( j->command, proc, flags ) )
		{
			if( flags & ACCEPT_INCOMPLETE )
			{
				wchar_t *res = wcsdupcat2( j->command + wcslen(proc),
										   COMPLETE_SEP_STR,
										   COMPLETE_JOB_DESC,
										   (void *)0 );
				al_push( out, res );
			}
			else
			{
				result = malloc(sizeof(wchar_t)*16 );
				swprintf( result, 16, L"%d", j->pgid );
				al_push( out, result );
				found = 1;
			}
		}
	}

	if( found )
	{
		return 1;
	}

	for( j=first_job; j; j=j->next )
	{
		process_t *p;
		if( j->command == 0 )
			continue;
		for( p=j->first_process; p; p=p->next )
		{

			if( p->actual_cmd == 0 )
				continue;

			if( match_pid( p->actual_cmd, proc, flags ) )
			{
				if( flags & ACCEPT_INCOMPLETE )
				{
					wchar_t *res = wcsdupcat2( p->actual_cmd + wcslen(proc),
											   COMPLETE_SEP_STR,
											   COMPLETE_CHILD_PROCESS_DESC,
											   (void *)0);
					al_push( out, res );
				}
				else
				{
					result = malloc(sizeof(wchar_t)*16 );
					swprintf( result, 16, L"%d", p->pid );
					al_push( out, result );
					found = 1;
				}
			}
		}
	}

	if( found )
	{
		return 1;
	}

	if( !(dir = opendir( "/proc" )))
	{
		/*
		  This system does not have a /proc filesystem.
		*/
		return 1;
	}

	pdir_name = malloc( 256 );
	pfile_name = malloc( 64 );
	strcpy( pdir_name, "/proc/" );

	while( (next=readdir(dir))!=0 )
	{
		char *name = next->d_name;
		struct stat buf;

		if( !isnumeric( name ) )
			continue;

		strcpy( pdir_name + 6, name );
		if( stat( pdir_name, &buf ) )
		{
			continue;
		}
		if( buf.st_uid != getuid() )
		{
			continue;
		}
		strcpy( pfile_name, pdir_name );
		strcat( pfile_name, "/cmdline" );

		if( !stat( pfile_name, &buf ) )
		{
			/*
			  the 'cmdline' file exists, it should contain the commandline
			*/
			FILE *cmdfile;

			if((cmdfile=fopen( pfile_name, "r" ))==0)
			{
				wperror( L"fopen" );
				continue;
			}

			signal_block();
			fgetws2( &cmd, &sz, cmdfile );
			signal_unblock();
			
			fclose( cmdfile );
		}
		else
		{
#ifdef SunOS
			strcpy( pfile_name, pdir_name );
			strcat( pfile_name, "/psinfo" );
			if( !stat( pfile_name, &buf ) )
			{
				psinfo_t info;

				FILE *psfile;

				if((psfile=fopen( pfile_name, "r" ))==0)
				{
					wperror( L"fopen" );
					continue;
				}

				if( fread( &info, sizeof(info), 1, psfile ) )
				{
					if( cmd != 0 )
						free( cmd );
					cmd = str2wcs( info.pr_fname );
				}
				fclose( psfile );
			}
			else
#endif
			{
				if( cmd != 0 )
				{
					*cmd = 0;
				}
			}
		}

		if( cmd != 0 )
		{
			if( match_pid( cmd, proc, flags ) )
			{
				if( flags & ACCEPT_INCOMPLETE )
				{
					wchar_t *res = wcsdupcat2( cmd + wcslen(proc),
											   COMPLETE_SEP_STR,
											   COMPLETE_PROCESS_DESC,
											   (void *)0);
					if( res )
						al_push( out, res );
				}
				else
				{
					wchar_t *res = str2wcs(name);
					if( res )
						al_push( out, res );
				}
			}
		}
	}

	if( cmd != 0 )
		free( cmd );

	free( pdir_name );
	free( pfile_name );

	closedir( dir );

	return 1;
}

/**
   Process id expansion
*/
static int expand_pid( wchar_t *in,
					   int flags,
					   array_list_t *out )
{
	if( *in != PROCESS_EXPAND )
	{
		al_push( out, in );
		return 1;
	}

	if( flags & ACCEPT_INCOMPLETE )
	{
		if( wcsncmp( in+1, SELF_STR, wcslen(in+1) )==0 )
		{
			wchar_t *res = wcsdupcat2( SELF_STR+wcslen(in+1), COMPLETE_SEP_STR, COMPLETE_SELF_DESC, (void *)0 );
			al_push( out, res );
		}
		else if( wcsncmp( in+1, LAST_STR, wcslen(in+1) )==0 )
		{
			wchar_t *res = wcsdupcat2( LAST_STR+wcslen(in+1), COMPLETE_SEP_STR, COMPLETE_LAST_DESC, (void *)0 );
			al_push( out, res );
		}
	}
	else
	{
		if( wcscmp( (in+1), SELF_STR )==0 )
		{
			wchar_t *str= malloc( sizeof(wchar_t)*32);
			free(in);
			swprintf( str, 32, L"%d", getpid() );
			al_push( out, str );

			return 1;
		}
		if( wcscmp( (in+1), LAST_STR )==0 )
		{
			wchar_t *str;

			if( proc_last_bg_pid > 0 )
			{
				str = malloc( sizeof(wchar_t)*32);
				free(in);
				swprintf( str, 32, L"%d", proc_last_bg_pid );
				al_push( out, str );
			}

			return 1;
		}
	}

	int prev = al_get_count( out );
	if( !find_process( in+1, flags, out ) )
		return 0;

	if( prev == al_get_count( out ) )
	{
		if( flags & ACCEPT_INCOMPLETE )
			free( in );
		else
		{
			*in = L'%';
			al_push( out, in );
		}
	}
	else
	{
		free( in );
	}

	return 1;
}

/**
   Expand all environment variables in the string *ptr.

   This function is slow, fragile and complicated. There are lots of
   little corner cases, like $$foo should do a double expansion,
   $foo$bar should not double expand bar, etc. Also, it's easy to
   accidentally leak memory on array out of bounds errors an various
   other situations. All in all, this function should be rewritten,
   split out into multiple logical units and carefully tested. After
   that, it can probably be optimized to do fewer memory allocations,
   fewer string scans and overall just less work. But until that
   happens, don't edit it unless you know exactly what you are doing,
   and do proper testing afterwards.
*/
static int expand_variables( wchar_t *in, array_list_t *out, int last_idx )
{
	wchar_t c;
	wchar_t prev_char=0;
	int i, j;
	int is_ok= 1;
	int empty=0;

	static string_buffer_t *var_tmp = 0;
	static array_list_t *var_idx_list = 0;

	if( !var_tmp )
	{
		var_tmp = sb_halloc( global_context );
		if( !var_tmp )
			die_mem();
	}
	else
	{
		sb_clear(var_tmp );
	}

	if( !var_idx_list )
	{
		var_idx_list = al_halloc( global_context );
		if( !var_idx_list )
			die_mem();
	}
	else
	{
		al_truncate( var_idx_list, 0 );
	}

	for( i=last_idx; (i>=0) && is_ok && !empty; i-- )
	{
		c = in[i];
		if( ( c == VARIABLE_EXPAND ) || (c == VARIABLE_EXPAND_SINGLE ) )
		{
			int start_pos = i+1;
			int stop_pos;
			int var_len, new_len;
			wchar_t * var_val;
			wchar_t * new_in;
			int is_single = (c==VARIABLE_EXPAND_SINGLE);
			int var_name_stop_pos;
			
			stop_pos = start_pos;

			while( 1 )
			{
				if( !(in[stop_pos ]) )
					break;
				if( !( iswalnum( in[stop_pos] ) ||
					   (wcschr(L"_", in[stop_pos])!= 0)  ) )
					break;

				stop_pos++;
			}
			var_name_stop_pos = stop_pos;
			
/*			printf( "Stop for '%c'\n", in[stop_pos]);*/

			var_len = stop_pos - start_pos;

			if( var_len == 0 )
			{
				switch( in[stop_pos] )
				{
					case BRACKET_BEGIN:
					{
						error( SYNTAX_ERROR,
							   -1,
							   COMPLETE_VAR_BRACKET_DESC );
						break;
					}

					case INTERNAL_SEPARATOR:
					{
						error( SYNTAX_ERROR,
							   -1,
							   COMPLETE_VAR_PARAN_DESC );
						break;
					}

					case 0:
					{
						error( SYNTAX_ERROR,
							   -1,
							   COMPLETE_VAR_NULL_DESC,
							   in[stop_pos] );
						break;
					}

					default:
					{
						error( SYNTAX_ERROR,
							   -1,
							   COMPLETE_VAR_DESC,
							   in[stop_pos] );
						break;
					}
				}


				is_ok = 0;
				break;
			}

			sb_append_substring( var_tmp, &in[start_pos], var_len );

			var_val = expand_var( (wchar_t *)var_tmp->buff );

			if( var_val )
			{
				int all_vars=1;
				array_list_t var_item_list;
				al_init( &var_item_list );
				
				if( in[stop_pos] == L'[' )
				{
					wchar_t *end;

					all_vars = 0;

					stop_pos++;
					while( 1 )
					{
						int tmp;

						while( iswspace(in[stop_pos]) || (in[stop_pos]==INTERNAL_SEPARATOR))
							stop_pos++;


						if( in[stop_pos] == L']' )
						{
							stop_pos++;
							break;
						}

						errno=0;
						tmp = wcstol( &in[stop_pos], &end, 10 );
						if( ( errno ) || ( end == &in[stop_pos] ) )
						{
							error( SYNTAX_ERROR,
								   -1,
								   L"Expected integer or \']\'" );

							is_ok = 0;
							break;
						}
						al_push( var_idx_list, (void *)tmp );
						stop_pos = end-in;
					}
				}

				if( is_ok )
				{
					expand_variable_array( var_val, &var_item_list );
					if( !all_vars )
					{
						int j;
						for( j=0; j<al_get_count( var_idx_list ); j++)
						{
							int tmp = (int)al_get( var_idx_list, j );
							if( tmp < 1 || tmp > al_get_count( &var_item_list ) )
							{
								error( SYNTAX_ERROR,
									   -1,
									   L"Array index out of bounds" );
								is_ok=0;
								al_truncate( var_idx_list, j );
								break;
							}
							else
							{
								/* Move string from list l to list idx */
								al_set( var_idx_list, j, al_get( &var_item_list, tmp-1 ) );
								al_set( &var_item_list, tmp-1, 0 );
							}
						}
						/* Free remaining strings in list l and truncate it */
						al_foreach( &var_item_list, (void (*)(const void *))&free );
						al_truncate( &var_item_list, 0 );
						/* Add items from list idx back to list l */
						al_push_all( &var_item_list, var_idx_list );
					}
				}

				if( is_ok )
				{
					
					if( is_single )
					{
						string_buffer_t res;
						in[i]=0;
						
						sb_init( &res );
						sb_append( &res, in );
						sb_append_char( &res, INTERNAL_SEPARATOR );

						for( j=0; j<al_get_count( &var_item_list); j++ )
						{
							wchar_t *next = (wchar_t *)al_get( &var_item_list, j );
							
							if( is_ok )
							{
								if( j != 0 )
									sb_append( &res, L" " );
								sb_append( &res, next );
							}
							free( next );
						}
						sb_append( &res, &in[stop_pos] );
						is_ok &= expand_variables( (wchar_t *)res.buff, out, i );
					}
					else
					{
						for( j=0; j<al_get_count( &var_item_list); j++ )
						{
							wchar_t *next = (wchar_t *)al_get( &var_item_list, j );
							if( is_ok && (i == 0) && (!in[stop_pos]) )
							{
								al_push( out, next );
							}
							else
							{
								
								if( is_ok )
								{
									new_len = wcslen(in) - (stop_pos-start_pos+1);
									new_len += wcslen( next) +2;
									
									if( !(new_in = malloc( sizeof(wchar_t)*new_len )))
									{
										die_mem();
									}
									else
									{
										
										wcslcpy( new_in, in, start_pos );

										if(start_pos>1 && new_in[start_pos-2]!=VARIABLE_EXPAND)
										{
											new_in[start_pos-1]=INTERNAL_SEPARATOR;
											new_in[start_pos]=L'\0';
										}
										else
											new_in[start_pos-1]=L'\0';
										
										wcscat( new_in, next );
										wcscat( new_in, &in[stop_pos] );
										
										is_ok &= expand_variables( new_in, out, i );
									}
								}
								free( next );
							}
							
						}
					}
				}
				
				free(in);
				al_destroy( &var_item_list );
				return is_ok;
			}
			else
			{
				/*
				  Expand a non-existing variable
				*/
				if( c == VARIABLE_EXPAND )
				{
					/*
					  Regular expansion, i.e. expand this argument to nothing
					*/
					empty = 1;
				}
				else
				{
					/*
					  Expansion to single argument.
					*/
					string_buffer_t res;
					sb_init( &res );

					in[i]=0;

					sb_append( &res, in );
					sb_append( &res, &in[stop_pos] );

					is_ok &= expand_variables( (wchar_t *)res.buff, out, i );
					free(in);
					return is_ok;
				}
			}


		}

		prev_char = c;
	}

	if( !empty )
	{
		al_push( out, in );
	}
	else
	{
		free( in );
	}

	return is_ok;
}

/**
   Perform bracket expansion
*/
static int expand_brackets( wchar_t *in, int flags, array_list_t *out )
{
	wchar_t *pos;
	int syntax_error=0;
	int bracket_count=0;

	wchar_t *bracket_begin=0, *bracket_end=0;
	wchar_t *last_sep=0;

	wchar_t *item_begin;
	int len1, len2, tot_len;

	for( pos=in;
		 (!bracket_end) && (*pos) && !syntax_error;
		 pos++ )
	{
		switch( *pos )
		{
			case BRACKET_BEGIN:
			{
				if(( bracket_count == 0)&&(bracket_begin==0))
					bracket_begin = pos;

				bracket_count++;
				break;

			}
			case BRACKET_END:
			{
				bracket_count--;
				if( (bracket_count == 0) && (bracket_end == 0) )
					bracket_end = pos;

				if( bracket_count < 0 )
				{
					syntax_error = 1;
				}
				break;
			}
			case BRACKET_SEP:
			{
				if( bracket_count == 1 )
					last_sep = pos;
			}
		}
	}

	if( bracket_count > 0 )
	{
		if( !(flags & ACCEPT_INCOMPLETE) )
			syntax_error = 1;
		else
		{

			string_buffer_t mod;
			sb_init( &mod );
			if( last_sep )
			{
				sb_append_substring( &mod, in, bracket_begin-in+1 );
				sb_append( &mod, last_sep+1 );
				sb_append_char( &mod, BRACKET_END );
			}
			else
			{
				sb_append( &mod, in );
				sb_append_char( &mod, BRACKET_END );
			}




			return expand_brackets( (wchar_t*)mod.buff, 1, out );
		}
	}

	if( syntax_error )
	{
		error( SYNTAX_ERROR,
			   -1,
			   _(L"Mismatched brackets") );
		return 0;
	}

	if( bracket_begin == 0 )
	{
		al_push( out, in );
		return 1;
	}

	len1 = (bracket_begin-in);
	len2 = wcslen(bracket_end)-1;
	tot_len = len1+len2;
	item_begin = bracket_begin+1;
	for( pos=(bracket_begin+1); 1; pos++ )
	{
		if( bracket_count == 0 )
		{
			if( (*pos == BRACKET_SEP) || (pos==bracket_end) )
			{
				wchar_t *whole_item;
				int item_len = pos-item_begin;

				whole_item = malloc( sizeof(wchar_t)*(tot_len + item_len + 1) );
				wcslcpy( whole_item, in, len1+1 );
				wcslcpy( whole_item+len1, item_begin, item_len+1 );
				wcscpy( whole_item+len1+item_len, bracket_end+1 );

				expand_brackets( whole_item, flags, out );

				item_begin = pos+1;
				if( pos == bracket_end )
					break;
			}
		}

		if( *pos == BRACKET_BEGIN )
		{
			bracket_count++;
		}

		if( *pos == BRACKET_END )
		{
			bracket_count--;
		}
	}
	free(in);
	return 1;
}

/**
   Perform subshell expansion
*/
static int expand_subshell( wchar_t *in, array_list_t *out )
{
	const wchar_t *paran_begin=0, *paran_end=0;
	int len1, len2;
	wchar_t prev=0;
	wchar_t *subcmd;
	array_list_t sub_res, tail_expand;
	int i, j;
	const wchar_t *item_begin;

	if( !in )
	{
		debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
		return 0;		
	}

	if( !out )
	{
		debug( 2, L"Got null pointer on line %d of file %s", __LINE__, __FILE__ );
		return 0;		
	}

	switch( parse_util_locate_cmdsubst(in,
									   &paran_begin,
									   &paran_end,
									   0 ) )
	{
		case -1:
			error( SYNTAX_ERROR,
				   -1,
				   L"Mismatched parans" );
			return 0;
		case 0:
			al_push( out, in );
			return 1;
		case 1:

			break;
	}

	len1 = (paran_begin-in);
	len2 = wcslen(paran_end)-1;
	prev=0;
	item_begin = paran_begin+1;

	al_init( &sub_res );
	if( !(subcmd = malloc( sizeof(wchar_t)*(paran_end-paran_begin) )))
	{
		al_destroy( &sub_res );
		return 0;
	}

	wcslcpy( subcmd, paran_begin+1, paran_end-paran_begin );
	subcmd[ paran_end-paran_begin-1]=0;

	if( exec_subshell( subcmd, &sub_res)==-1 )
	{
		al_foreach( &sub_res, (void (*)(const void *))&free );
		al_destroy( &sub_res );
		free( subcmd );
		return 0;
	}

	al_init( &tail_expand );
	expand_subshell( wcsdup(paran_end+1), &tail_expand );

    for( i=0; i<al_get_count( &sub_res ); i++ )
    {
        wchar_t *sub_item, *sub_item2;
        sub_item = (wchar_t *)al_get( &sub_res, i );
        sub_item2 = escape( sub_item, 1 );
		free(sub_item);
        int item_len = wcslen( sub_item2 );

        for( j=0; j<al_get_count( &tail_expand ); j++ )
        {
            string_buffer_t whole_item;
            wchar_t *tail_item = (wchar_t *)al_get( &tail_expand, j );

            sb_init( &whole_item );

            sb_append_substring( &whole_item, in, len1 );
            sb_append_char( &whole_item, INTERNAL_SEPARATOR );
            sb_append_substring( &whole_item, sub_item2, item_len );
			sb_append_char( &whole_item, INTERNAL_SEPARATOR );
            sb_append( &whole_item, tail_item );
			
			al_push( out, whole_item.buff );
        }
		
        free( sub_item2 );
    }
	free(in);

	al_destroy( &sub_res );

	al_foreach( &tail_expand, (void (*)(const void *))&free );
	al_destroy( &tail_expand );

	free( subcmd );
	return 1;
}


static wchar_t *expand_unescape( const wchar_t * in, int escape_special )
{
	wchar_t *res = unescape( in, escape_special );
	if( !res )
		error( SYNTAX_ERROR, -1, L"Unexpected end of string" );
	return res;
}

/**
   Attempts tilde expansion. Of the string specified. If tilde
   expansion is performed, the original string is freed and a new
   string allocated using malloc is returned, otherwise, the original
   string is returned.
*/
static wchar_t * expand_tilde_internal( wchar_t *in )
{

	if( in[0] == HOME_DIRECTORY )
	{
		int tilde_error = 0;
		wchar_t *home=0;
		wchar_t *new_in=0;
		wchar_t *old_in=0;

		if( in[1] == '/' || in[1] == '\0' )
		{
			/* Current users home directory */

			home = env_get( L"HOME" );
			if( home )
				home = wcsdup(home);
			else
				home = wcsdup(L"");

			if( !home )
			{
				*in = L'~';
				tilde_error = 1;
			}

			old_in = &in[1];
		}
		else
		{
			/* Some other users home directory */
			wchar_t *name;
			wchar_t *name_end = wcschr( in, L'/' );
			if( name_end == 0 )
			{
				name_end = in+wcslen( in );
			}
			name = wcsndup( in+1, name_end-in-1 );
			old_in = name_end;

			char *name_str = wcs2str( name );

			struct passwd *userinfo =
				getpwnam( name_str );

			if( userinfo == 0 )
			{
				tilde_error = 1;
				*in = L'~';
			}
			else
			{
				home = str2wcs(userinfo->pw_dir);
				if( !home )
				{
					*in = L'~';
					tilde_error = 1;
				}
			}

			free( name_str );
			free(name);
		}

		if( !tilde_error && home && old_in )
		{
			new_in = wcsdupcat( home, old_in );
		}
		free(home);
		free( in );
		return new_in;
	}
	return in;
}

wchar_t *expand_tilde( wchar_t *in)
{
	if( in[0] == L'~' )
	{
		in[0] = HOME_DIRECTORY;
		return expand_tilde_internal( in );
	}
	return in;
}

/**
   Remove any internal separators. Also optionally convert wildcard characters to
   regular equivalents. This is done to support EXPAN_SKIP_WILDCARDS.
*/
static void remove_internal_separator( const void *s, int conv )
{
	wchar_t *in = (wchar_t *)s;
	wchar_t *out=in;

	while( *in )
	{
		switch( *in )
		{
			case INTERNAL_SEPARATOR:
				in++;
				break;
				
			case ANY_CHAR:
				in++;
				*out++ = conv?L'?':ANY_CHAR;
				break;

			case ANY_STRING:
				in++;
				*out++ = conv?L'*':ANY_STRING;
				break;

			default:
				*out++ = *in++;
		}
	}
	*out=0;
}


/**
   The real expansion function. expand_one is just a wrapper around this one.
*/
int expand_string( void *context,
				   wchar_t *str,
				   array_list_t *end_out,
				   int flags )
{
	array_list_t list1, list2;
	array_list_t *in, *out;

	int i;
	int subshell_ok = 1;
	int res = EXPAND_OK;
	int start_count = al_get_count( end_out );

//	debug( 1, L"Expand %ls", str );

	if( (!(flags & ACCEPT_INCOMPLETE)) && is_clean( str ) )
	{
		halloc_register( context, str );
		al_push( end_out, str );
		return EXPAND_OK;
	}

	al_init( &list1 );
	al_init( &list2 );

	if( EXPAND_SKIP_SUBSHELL & flags )
	{
		const wchar_t *begin, *end;
		
		if( parse_util_locate_cmdsubst( str,
										&begin,
										&end,
										1 ) != 0 )
		{
			error( SUBSHELL_ERROR, -1, L"Subshells not allowed" );
			free( str );
			al_destroy( &list1 );
			al_destroy( &list2 );
			return EXPAND_ERROR;
		}
		al_push( &list1, str );
	}
	else
	{
		subshell_ok = expand_subshell( str, &list1 );
	}

	if( !subshell_ok )
	{
		al_destroy( &list1 );
		return EXPAND_ERROR;
	}
	else
	{
		in = &list1;
		out = &list2;

		for( i=0; i<al_get_count( in ); i++ )
		{
			wchar_t *next;

			next = expand_unescape( (wchar_t *)al_get( in, i ),
									1);

			free( (void *)al_get( in, i ) );

			if( !next )
			{
				debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
				continue;				
			}

			if( EXPAND_SKIP_VARIABLES & flags )
			{
				wchar_t *tmp;
				for( tmp=next; *tmp; tmp++ )
					if( *tmp == VARIABLE_EXPAND )
						*tmp = L'$';
				al_push( out, next );
			}
			else
			{
				if(!expand_variables( next, out, wcslen(next)-1 ))
				{
					al_destroy( in );
					al_destroy( out );
					return EXPAND_ERROR;
				}
			}
		}

		al_truncate( in, 0 );

		in = &list2;
		out = &list1;

		for( i=0; i<al_get_count( in ); i++ )
		{
			wchar_t *next = (wchar_t *)al_get( in, i );

			if( !next )
			{
				debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
				continue;				
			}	

			if( !expand_brackets( next, flags, out ))
			{
				al_destroy( in );
				al_destroy( out );
				return EXPAND_ERROR;
			}
		}
		al_truncate( in, 0 );

		in = &list1;
		out = &list2;

		for( i=0; i<al_get_count( in ); i++ )
		{
			wchar_t *next = (wchar_t *)al_get( in, i );

			if( !next )
			{
				debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
				continue;				
			}	

			if( !(next=expand_tilde_internal( next ) ) )
			{
				al_destroy( in );
				al_destroy( out );
				return EXPAND_ERROR;
			}

			if( flags & ACCEPT_INCOMPLETE )
			{
				if( *next == PROCESS_EXPAND )
				{
					/*
					  If process expansion matches, we are not
					  interested in other completions, so we
					  short-circut and return
					*/
					expand_pid( next, flags, end_out );
					al_destroy( in );
					al_destroy( out );
					return EXPAND_OK;
				}
				else
					al_push( out, next );
			}
			else
			{
				if( !expand_pid( next, flags, out ) )
				{
					al_destroy( in );
					al_destroy( out );
					return EXPAND_ERROR;
				}
			}
		}
		al_truncate( in, 0 );

		in = &list2;
		out = &list1;

		for( i=0; i<al_get_count( in ); i++ )
		{
			wchar_t *next = (wchar_t *)al_get( in, i );
			int wc_res;

			if( !next )
			{
				debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
				continue;				
			}	

			remove_internal_separator( next, EXPAND_SKIP_WILDCARDS & flags );
			
			if( ((flags & ACCEPT_INCOMPLETE) && (!(flags & EXPAND_SKIP_WILDCARDS))) ||
				wildcard_has( next, 1 ) )
			{

				if( next[0] == '/' )
				{
					wc_res = wildcard_expand( &next[1], L"/",flags, out );
				}
				else
				{
					wc_res = wildcard_expand( next, L"", flags, out );
				}
				free( next );
				switch( wc_res )
				{
					case 0:
					{
						if( !(flags & ACCEPT_INCOMPLETE) )
						{
							if( res == EXPAND_OK )
								res = EXPAND_WILDCARD_NO_MATCH;
							break;
						}
					}
					
					case 1:
					{
						int j;
						res = EXPAND_WILDCARD_MATCH;
						sort_list( out );

						for( j=0; j<al_get_count( out ); j++ )
						{
							wchar_t *next = (wchar_t *)al_get( out, j );
							if( !next )
							{
								debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
								continue;				
							}	
							al_push( end_out, next );
						}
						al_truncate( out, 0 );
						break;
					}
				}
			}
			else
			{
				if( flags & ACCEPT_INCOMPLETE)
				{
					free( next );
				}
				else
				{
					al_push( end_out, next );
				}
			}
		}
		al_destroy( in );
		al_destroy( out );
	}

	if( context )
	{
		for( i=start_count; i<al_get_count( end_out ); i++ )
		{
			halloc_register( context, (void *)al_get( end_out, i ) );
		}
	}
	
	return res;

}


wchar_t *expand_one( void *context, wchar_t *string, int flags )
{
	array_list_t l;
	int res;
	wchar_t *one;

	if( (!(flags & ACCEPT_INCOMPLETE)) &&  is_clean( string ) )
	{
		halloc_register( context, string );
		return string;
	}
	
	al_init( &l );
	res = expand_string( 0, string, &l, flags );
	if( !res )
	{
		one = 0;
	}
	else
	{
		if( al_get_count( &l ) != 1 )
		{
			one=0;
		}
		else
		{
			one = (wchar_t *)al_get( &l, 0 );
			al_set( &l, 0, 0 );
		}
	}

	al_foreach( &l, (void(*)(const void *))&free );
	al_destroy( &l );

	halloc_register( context, one );
	return one;
}

