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

#ifdef SunOS
#include <procfs.h>
#endif

#include "config.h"
#include "util.h"
#include "common.h"
#include "wutil.h"
#include "env.h"
#include "proc.h"
#include "parser.h"
#include "expand.h"
#include "wildcard.h"
#include "exec.h"
#include "tokenizer.h"
#include "complete.h"

/**
   Description for child process
*/
#define COMPLETE_CHILD_PROCESS_DESC COMPLETE_SEP_STR L"Child process"

/**
   Description for non-child process
*/
#define COMPLETE_PROCESS_DESC COMPLETE_SEP_STR L"Process"

/**
   Description for long job
*/
#define COMPLETE_JOB_DESC COMPLETE_SEP_STR L"Job"

/**
   Description for short job. The job command is concatenated
*/
#define COMPLETE_JOB_DESC_VAL COMPLETE_SEP_STR, L"Job: "

/**
   Description for the shells own pid
*/
#define COMPLETE_SELF_DESC COMPLETE_SEP_STR L"Shell process"

/**
   Description for the shells own pid
*/
#define COMPLETE_LAST_DESC COMPLETE_SEP_STR L"Last background job"

/**
   String in process expansion denoting ourself
*/
#define SELF_STR L"self"

/**
   String in process expansion denoting last background job
*/
#define LAST_STR L"last"

/**
   Return the environment variable value for the string starting at in 
*/
static wchar_t *expand_var( wchar_t *in )
{
	if( !in )
		return 0;
	return (in[0] == VARIABLE_EXPAND )? env_get( expand_var(in+1) ) : env_get( in );
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
				al_push( out, wcsdup(start) );
				start=pos+1;
			}
		}
		al_push( out, wcsdup(start) );
	
		free(cpy);	
	}
}

wchar_t *expand_escape( const wchar_t *in,
						int escape_all )
{
	return escape( in, escape_all );
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
			sb_append( &buff, L"\'\'");
			break;			

		case 1:
		{
			wchar_t *el = (wchar_t *)al_get( &l, 0 );
						
			if( wcschr( el, L' ' ) && is_quotable( el ) )
			{
				sb_append2( &buff,
							L"\'",
							el,
							L"\'",
							(void *)0 );
			}
			else
			{
				wchar_t *val = expand_escape( el, 1 );								
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
								L"\'",
								el,
								L"\'",
								(void *)0 );
				}
				else
				{
					wchar_t *val = expand_escape( el, 1 );								
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

   If accept_incomplete is true, the remaining string for any matches are inserted.

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

		//	fwprintf( stderr, L"Numeric\n\n\n" );

		if( flags & ACCEPT_INCOMPLETE )
		{
			for( j=first_job; j != 0; j=j->next )
			{
				wchar_t jid[16];
				if( j->command == 0 )
					continue;

				swprintf( jid, 16, L"%d", j->job_id );
//				fwprintf( stderr, L"Jid %ls\n", jid );
				
				if( wcsncmp( proc, jid, wcslen(proc ) )==0 )
				{
					al_push( out, 
							 wcsdupcat2( jid+wcslen(proc), 
										 COMPLETE_JOB_DESC_VAL, j->command, 
										 0 ) );
					
					
				}
			}
			
		}
		else
		{
	
			int jid = wcstol( proc, 0, 10 );
		
			j = job_get( jid );
			if( (j != 0) && (j->command != 0 ) )
			{
				
				{
					result = malloc(sizeof(wchar_t)*16 );
					swprintf( result, 16, L"%d", j->pgid );
					//fwprintf( stderr, L"pid %d %ls\n", j->pgid, result );
					al_push( out, result );
					found = 1;
				}
			}		
		}
	}
	if( found )
		return 1;

	for( j=first_job; j != 0; j=j->next )
	{
//		fwprintf( stderr, L"..." );
		if( j->command == 0 )
			continue;
		
		//	fwprintf( stderr, L"match '%ls' '%ls'\n\n\n", j->command, proc );
		
		if( match_pid( j->command, proc, flags ) )
		{
			if( flags & ACCEPT_INCOMPLETE )
			{
				wchar_t *res = wcsdupcat( j->command + wcslen(proc), 
										  COMPLETE_JOB_DESC );
//				fwprintf( stderr, L"Woot %ls\n", res );
				
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
			
//		fwprintf( stderr, L"..." );
			if( p->actual_cmd == 0 )
				continue;
			
			//	fwprintf( stderr, L"match '%ls' '%ls'\n\n\n", j->command, proc );
			
			if( match_pid( p->actual_cmd, proc, flags ) )
			{
				if( flags & ACCEPT_INCOMPLETE )
				{
					wchar_t *res = wcsdupcat( p->actual_cmd + wcslen(proc), 
											  COMPLETE_CHILD_PROCESS_DESC );
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
			
			fgetws2( &cmd, &sz, cmdfile );		
			
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
					wchar_t *res = wcsdupcat( cmd + wcslen(proc),
											  COMPLETE_PROCESS_DESC );
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
			wchar_t *res = wcsdupcat( SELF_STR+wcslen(in+1), COMPLETE_SELF_DESC );
			al_push( out, res );			
		}		
		else if( wcsncmp( in+1, LAST_STR, wcslen(in+1) )==0 )
		{
			wchar_t *res = wcsdupcat( LAST_STR+wcslen(in+1), COMPLETE_LAST_DESC );
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
	
//	fwprintf( stderr, L"expand_pid() %ls\n", in );
	int prev = al_get_count( out );
	if( !find_process( in+1, flags, out ) )
		return 0;
	
	if( prev == al_get_count( out ) )
	{
//		fwprintf( stderr, L"no match\n" );
		
		if( flags & ACCEPT_INCOMPLETE )
			free( in );
		else
		{
			*in = L'%';
//			fwprintf( stderr, L"return %ls\n", in );
			al_push( out, in );
		}
	}	
	else
	{
//		fwprintf( stderr, L"match\n" );
		free( in );
	}
	
	return 1;
}


/**
   Expand all environment variables in the string *ptr. 
*/
static int expand_variables( wchar_t *in, array_list_t *out )
{
	wchar_t c;
	wchar_t prev_char=0;
	int i, j;
	int is_ok= 1;
	int empty=0;
	

	for( i=wcslen(in)-1; (i>=0) && is_ok && !empty; i-- )
	{
		c = in[i];
		if( c == VARIABLE_EXPAND )
		{
			int start_pos = i+1;
			int stop_pos;
			int var_len, new_len;
			wchar_t *var_name;
			wchar_t * var_val;
			wchar_t * new_in;			
			array_list_t l;
			
			

//			fwprintf( stderr, L"Expand %ls\n", in );
			
									
//			while (in[stop_pos]==VARIABLE_EXPAND)			 
//				stop_pos++;

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
			
/*			printf( "Stop for '%c'\n", in[stop_pos]);*/
			
			var_len = stop_pos - start_pos;
			
			if( !(var_name = malloc( sizeof(wchar_t)*(var_len+1) )))
			{
				die_mem();
			}
			else
			{
				wcsncpy( var_name, &in[start_pos], var_len );
				var_name[var_len]='\0';
/*			printf( "Variable name is %s, len is %d\n", var_name, var_len );*/
				wchar_t *var_val_orig = expand_var( var_name );
		
				if( var_val_orig && (var_val = wcsdup( var_val_orig) ) )
				{
					int all_vars=1;
					array_list_t idx;
					al_init( &idx );
					al_init( &l );
					
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
									   L"Expected integer or \']\'", 
									   -1 );
								is_ok = 0;
								break;
							}
							al_push( &idx, (void *)tmp );
							stop_pos = end-in;
						}
					}
							
					if( is_ok )
					{				
						expand_variable_array( var_val, &l );
						if( !all_vars )
						{	
							int j;
							for( j=0; j<al_get_count( &idx ); j++)
							{
								int tmp = (int)al_get( &idx, j );
								if( tmp < 1 || tmp > al_get_count( &l ) )
								{
									error( SYNTAX_ERROR, L"Array index out of bounds", -1 );
									is_ok=0;
									al_truncate( &idx, j );
									break;
								}				
								else
								{
									/* Move string from list l to list idx */
									al_set( &idx, j, al_get( &l, tmp-1 ) );
									al_set( &l, tmp-1, 0 );
								}
							}
							/* Free remaining strings in list l and truncate it */
							al_foreach( &l, (void (*)(const void *))&free );
							al_truncate( &l, 0 );
							/* Add items from list idx back to list l */							
							al_push_all( &l, &idx );							
						}
						free( var_val );
					}				

					for( j=0; j<al_get_count( &l); j++ )
					{
						wchar_t *next = (wchar_t *)al_get( &l, j );
						
						if( is_ok )
						{
							
							new_len = wcslen(in) - (stop_pos-start_pos+1) + wcslen( next) +2;
							
							if( !(new_in = malloc( sizeof(wchar_t)*new_len )))
							{
								error( OOM, L"Out of memory", -1 );				
								is_ok = 0;
							}
							else
							{
								
								wcsncpy( new_in, in, start_pos-1 );
								
								if(start_pos>1 && new_in[start_pos-2]!=VARIABLE_EXPAND)
								{									
									new_in[start_pos-1]=INTERNAL_SEPARATOR;
									new_in[start_pos]=L'\0';
								}
								else
									new_in[start_pos-1]=L'\0';
								
								wcscat( new_in, next );
								wcscat( new_in, &in[stop_pos] );
								
//								fwprintf( stderr, L"New value %ls\n", new_in );
								is_ok &= expand_variables( new_in, out );
							}
						}
						free( next );
					}					
					al_destroy( &l );
					al_destroy( &idx );
					free(in);
					free(var_name );
					return is_ok;					
				}
				else
				{
					empty = 1;
				}
				
				free(var_name );				
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

//	fwprintf( stderr, L"expand %ls\n", in );
	
	
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
		error( SYNTAX_ERROR, L"Mismatched brackets", -1 );
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
				wcsncpy( whole_item, in, len1 );
				wcsncpy( whole_item+len1, item_begin, item_len );	
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

int expand_locate_subshell( wchar_t *in, 
							wchar_t **begin, 
							wchar_t **end,
							int allow_incomplete )
{
	wchar_t *pos;
	wchar_t prev=0;
	int syntax_error=0;
	int paran_count=0;	

	wchar_t *paran_begin=0, *paran_end=0;

	for( pos=in; *pos; pos++ )
	{
		if( prev != '\\' )
		{
			if( wcschr( L"\'\"", *pos ) )
			{
				wchar_t *end = quote_end( pos );
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

	*begin = paran_begin;
	*end = paran_count?in+wcslen(in):paran_end;
	
	return 1;

}

/**
   Perform subshell expansion
*/
static int expand_subshell( wchar_t *in, array_list_t *out )
{
	wchar_t *paran_begin=0, *paran_end=0;
	int len1, len2;
	wchar_t prev=0;
	wchar_t *subcmd;
	array_list_t sub_res, tail_expand;
	int i, j;
	wchar_t *item_begin;

	switch( expand_locate_subshell(in, 
								   &paran_begin,
								   &paran_end,
								   0 ) )
	{
		case -1:
			error( SYNTAX_ERROR, L"Mismatched parans", -1 );
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
	
	wcsncpy( subcmd, paran_begin+1, paran_end-paran_begin-1 );
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
        sub_item2 = expand_escape( sub_item, 1 );
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


wchar_t *expand_unescape( const wchar_t * in, int escape_special )
{
	wchar_t *res = unescape( in, escape_special );
	if( !res )
		error( SYNTAX_ERROR, L"Unexpected end of string", -1 );
	return res;
}

/**
   Attempts tilde expansion. Of the string specified. If tilde
   expansion is performed, the argument is freed and a new string is
   allocated in its place. Horrible call signature. Should be
   altered. Fugly!
*/
static int tilde_expand( wchar_t **ptr )
{
	wchar_t *in = *ptr;
	
	if( in[0] == HOME_DIRECTORY )
	{
		int tilde_error = 0;
		wchar_t *home=0;
		wchar_t *new_in;
		wchar_t *old_in;
		
//		fwprintf( stderr, L"Tilde expand ~%ls\n", (*ptr)+1 );
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
		
		if( !tilde_error )
		{
			new_in = wcsdupcat( home, old_in ); 
			free( in );
			in = new_in;
			free(home);
			*ptr = in;
		}		
		
	}
	return 1;
} 

wchar_t *expand_tilde(wchar_t *in)
{
	if( in[0] == L'~' )
	{
		in[0] = HOME_DIRECTORY;
		tilde_expand( &in );
		return in;
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

//	int changed=0;

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
/*	if( changed )
	{
		fwprintf( stderr, L" -> %ls\n", s );
	}
*/
}


/**
   The real expansion function. All other expansion  functions are wrappers to this one.
*/
int expand_string( wchar_t *str,
				   array_list_t *end_out, 
				   int flags )
{
	array_list_t list1, list2;
	array_list_t *in, *out;
	
	int i;
	int subshell_ok = 1;
	
	al_init( &list1 );
	al_init( &list2 );

	if( EXPAND_SKIP_SUBSHELL & flags )
	{
		wchar_t *pos = str;
		
		while( 1 )
		{
			pos = wcschr( pos, L'(' );
			if( pos == 0 )
				break;
			
			if( (pos == str) || ( *(pos-1) != L'\\' ) )
			{
				error( SUBSHELL_ERROR, L"Subshells not allowed", -1 );
				free( str );
				al_destroy( &list1 );
				al_destroy( &list2 );
				return 0;
			}
			pos++;
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
		return 0;
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
				continue;			
			
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
				if(!expand_variables( next, out ))
				{
					al_destroy( in );
					al_destroy( out );
					return 0;	
				}
			}
		}
		
		al_truncate( in, 0 );

		in = &list2;
		out = &list1;
		
		for( i=0; i<al_get_count( in ); i++ )
		{
			wchar_t *next = (wchar_t *)al_get( in, i );
			if( !expand_brackets( next, flags, out ))
			{
				al_destroy( in );
				al_destroy( out );
				return 0;
			}
		}
		al_truncate( in, 0 );

		in = &list1;
		out = &list2;
		
		for( i=0; i<al_get_count( in ); i++ )
		{
			wchar_t *next = (wchar_t *)al_get( in, i );		
			if(!tilde_expand( &next ))
			{
				al_destroy( in );
				al_destroy( out );
				return 0;
			}
		
			if( flags & ACCEPT_INCOMPLETE )
			{
				if( *next == PROCESS_EXPAND )
				{
					expand_pid( next, flags, end_out );
					al_destroy( in );
					al_destroy( out );
					return 1;
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
					return 0;
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
						if( !(flags & ACCEPT_INCOMPLETE) )
						{	
							break;
						}
					case 1:
						sort_list( out );
						al_push_all( end_out, out );
						al_truncate( out, 0 );						
						break;
						
					default:
						fwprintf( stderr, L"error\n" );
						/*al_destroy( &list1 );*/
						/*al_destroy( &list2 );*/
						/*return 0;*/
				}			
			}
			else
			{
				if( flags & ACCEPT_INCOMPLETE)
					free( next );
				else
					al_push( end_out, next );
			}		
		}
		al_destroy( in );
		al_destroy( out );
	}

	return 1;
}


wchar_t *expand_one( wchar_t *string, int flags )
{
	array_list_t l;
	int res;
	wchar_t *one;
	
	al_init( &l );
	res = expand_string( string, &l, flags );
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
	return one;
}

