/** \file wildcard.c

    Fish needs it's own globbing implementation to support
	tab-expansion of globbed parameters. Also provides recursive
	wildcards using **.

*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "util.h"
#include "wutil.h"
#include "complete.h"
#include "common.h"
#include "wildcard.h"
#include "complete.h"
#include "reader.h"
#include "expand.h"

/**
   This flag is set in the flags parameter of wildcard_expand if the
   call is part of a recursiv wildcard search. It is used to make sure
   that the contents of subdirectories are only searched once.
*/
#define WILDCARD_RECURSIVE 64

/**
   The maximum length of a filename token. This is a fallback value,
   an attempt to find the true value using patchconf is always made. 
*/
#define MAX_FILE_LENGTH 1024

int wildcard_has( const wchar_t *str, int internal )
{
	wchar_t prev=0;
	if( internal )
	{
		for( ; *str; str++ )
		{
			if( ( *str == ANY_CHAR ) || (*str == ANY_STRING) || (*str == ANY_STRING_RECURSIVE) )
				return 1;
			prev = *str;
		}
	}
	else
	{
		for( ; *str; str++ )
		{
			if( ( (*str == L'*' ) || (*str == L'?' ) ) && (prev != L'\\') )
				return 1;
			prev = *str;
		}
	}
	
	return 0;
}

/**
   Check whether the string str matches the wildcard string wc.
  
   \param str String to be matched.
   \param wc The wildcard.
   \param is_first Whether files beginning with dots should not be matched against wildcards. 
   \param wc_unescaped Whether the unescaped special character ANY_CHAR abd ANY_STRING should be used instead of '?' and '*' for wildcard matching
*/


static int wildcard_match2( const wchar_t *str, 
							const wchar_t *wc, 
							int is_first )
{
	
	if( *str == 0 && *wc==0 )
		return 1;
	
	if( *wc == ANY_STRING || *wc == ANY_STRING_RECURSIVE)
	{		
		/* Ignore hidden file */
		if( is_first && *str == L'.' )
		{
			return 0;
		}
		
		/* Try all submatches */
		do
		{
			if( wildcard_match2( str, wc+1, 0 ) )
				return 1;
		}
		while( *(str++) != 0 );
		return 0;
	}

	if( *wc == ANY_CHAR )
	{
		if( is_first && *str == L'.' )
		{
			return 0;
		}
		
		return wildcard_match2( str+1, wc+1, 0 );
	}
	
	if( *wc == *str )
		return wildcard_match2( str+1, wc+1, 0 );

	return 0;
}

/**
   Matches the string against the wildcard, and if the wildcard is a
   possible completion of the string, the remainder of the string is
   inserted into the array_list_t.
*/
static int wildcard_complete_internal( const wchar_t *orig, 
									   const wchar_t *str, 
									   const wchar_t *wc, 
									   int is_first,
									   const wchar_t *desc,
									   const wchar_t *(*desc_func)(const wchar_t *),
									   array_list_t *out )
{
	if( *wc == 0 && 
		( ( *str != L'.') || (!is_first)) )
	{
		if( !out )
			return 1;
		
		wchar_t *new;
			
		if( wcschr( str, PROG_COMPLETE_SEP ) )
		{
			/*
			  This completion has an embedded description, du not use the generic description
			*/
			wchar_t *sep;
				
			new = wcsdup( str );
			sep = wcschr(new, PROG_COMPLETE_SEP );
			*sep = COMPLETE_SEP;
		}
		else if( desc_func )
		{
			/*
			  A descripton generating function is specified, use it
			*/
			new = wcsdupcat2( str, COMPLETE_SEP_STR, desc_func( orig ), 0);			
		}
		else
		{
			/*
			  Append generic description to item, if the description exists
			*/
			if( desc && wcslen(desc)>1 )
				new = wcsdupcat( str, desc );
			else
				new = wcsdup( str );
		}

		if( new )
		{
			al_push( out, new );
		}
		return 1;
	}
	
	
	if( *wc == ANY_STRING )
	{		
		int res=0;
		
		/* Ignore hidden file */
		if( is_first && str[0] == L'.' )
			return 0;
		
		/* Try all submatches */
		do
		{
			res |= wildcard_complete_internal( orig, str, wc+1, 0, desc, desc_func, out );
			if( res && !out )
				break;
		}
		while( *str++ != 0 );
		return res;
		
	}
	else if( *wc == ANY_CHAR )
	{
		return wildcard_complete_internal( orig, str+1, wc+1, 0, desc, desc_func, out );
	}	
	else if( *wc == *str )
	{
		return wildcard_complete_internal( orig, str+1, wc+1, 0, desc, desc_func, out );
	}
	return 0;	
}

int wildcard_complete( const wchar_t *str,
					   const wchar_t *wc,
					   const wchar_t *desc,						
					   const wchar_t *(*desc_func)(const wchar_t *),
					   array_list_t *out )
{
	return wildcard_complete_internal( str, str, wc, 1, desc, desc_func, out );	
}


int wildcard_match( const wchar_t *str, const wchar_t *wc )
{
	return wildcard_match2( str, wc, 1 );	
}

/**
   Creates a path from the specified directory and filename. 
*/
static wchar_t *make_path( const wchar_t *base_dir, const wchar_t *name )
{
	
	wchar_t *long_name;
	int base_len = wcslen( base_dir );
	if( !(long_name= malloc( sizeof(wchar_t)*(base_len+wcslen(name)+1) )))
	{
		die_mem();
	}					
	wcscpy( long_name, base_dir );
	wcscpy(&long_name[base_len], name );
	return long_name;
}

void get_desc( wchar_t *fn, string_buffer_t *sb, int is_cmd )
{
	const wchar_t *desc;
	
	struct stat buf;
	off_t sz;
	wchar_t *sz_name[]=
		{
			L"kB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB", 0
		}
	;
	
	
	sb_clear( sb );
	
	if( wstat( fn, &buf ) )
	{
		sz=-1;
	}
	else
	{
		sz = buf.st_size;
	}
						
	desc = complete_get_desc( fn );
	
	if( sz >= 0 && S_ISDIR(buf.st_mode) )
	{
		sb_append2( sb, desc, (void *)0 );							
	}
	else
	{							
		sb_append2( sb, desc, L", ", (void *)0 );
		if( sz < 0 )
		{
			sb_append( sb, L"unknown" );
		}
		else if( sz < 1 )
		{
			sb_append( sb, L"empty" );
		}
		else if( sz < 1024 )
		{
			sb_printf( sb, L"%dB", sz );
		}
		else
		{
			int i;
			
			for( i=0; sz_name[i]; i++ )
			{
				if( sz < (1024*1024) || !sz_name[i+1] )
				{
					int isz = sz/1024;
					if( isz > 9 )
						sb_printf( sb, L"%d%ls", isz, sz_name[i] );
					else
						sb_printf( sb, L"%.1f%ls", (double)sz/1024, sz_name[i] );
					
					break;
				}
				sz /= 1024;

			}
		}		
	}
}

/*
  Test if the file specified by the given filename matches the
  expansion flags specified. flags can be a combination of
  EXECUTABLES_ONLY and DIRECTORIES_ONLY.
*/
static int test_flags( wchar_t *filename,
					   int flags )
{
	if( !(flags & EXECUTABLES_ONLY) && !(flags & DIRECTORIES_ONLY) )
		return 1;
	
	struct stat buf;
	wstat( filename, &buf );
	
	if( S_IFDIR & buf.st_mode )
		return 1;
	
	if( flags & EXECUTABLES_ONLY )
		return ( waccess( filename, X_OK ) == 0);

	return 0;
}


int wildcard_expand( const wchar_t *wc, 
					 const wchar_t *base_dir,
					 int flags,
					 array_list_t *out )
{
	
	/* Points to the end of the current wildcard segment */
	wchar_t *wc_end;

	/* Variables for traversing a directory */
	struct dirent *next;
	DIR *dir;
	
	/* The result returned */
	int res = 0;
	
	/* Length of the directory to search in */
	int base_len;

	/* Variables for testing for presense of recursive wildcards */
	wchar_t *wc_recursive;
	int is_recursive;

	/* Sligtly mangled version of base_dir */
	const wchar_t *dir_string;

	/* Description for completions */
	string_buffer_t sb_desc;
	
	//	debug( 3, L"WILDCARD_EXPAND %ls in %ls", wc, base_dir );

	if( flags & ACCEPT_INCOMPLETE )
	{	
		/* 
		   Avoid excessive number of returned matches for wc ending with a * 
		*/
		int len = wcslen(wc);
		if( len && (wc[len-1]==ANY_STRING) )
		{
			wchar_t * foo = wcsdup( wc );
			foo[len-1]=0;
			int res = wildcard_expand( foo, base_dir, flags, out );
			free( foo );
			return res;			
		}
	}

	/*
	  Initialize various variables
	*/

	dir_string = base_dir[0]==L'\0'?L".":base_dir;
	
	if( !(dir = wopendir( dir_string )))
	{
		return 0;
	}

	wc_end = wcschr(wc,L'/');
	base_len = wcslen( base_dir );

	/*
	  Test for recursive match string in current segment
	*/	
	wc_recursive = wcschr( wc, ANY_STRING_RECURSIVE );
	is_recursive = ( wc_recursive && (!wc_end || wc_recursive < wc_end));

	/*
	  This makes sure that the base
	  directory of the recursive search is
	  also searched for matching files.
	*/
	if( is_recursive && (wc_end==(wc+1)) && !(flags & WILDCARD_RECURSIVE ) )
	{
		wildcard_expand( wc_end + 1,
						 base_dir,
						 flags,
						 out );
	}
	
	if( flags & ACCEPT_INCOMPLETE )
		sb_init( &sb_desc );

	/*
	  Is this segment of the wildcard the last?
	*/
	if( !wc_end && !is_recursive )
	{
		/*
		  Wildcard segment is the last segment,

		  Insert all matching files/directories
		*/
		if( wc[0]=='\0' )
		{
			/*
			  The last wildcard segment is empty. Insert everything if
			  completing, the directory itself otherwise.
			*/
			if( flags & ACCEPT_INCOMPLETE )
			{
				while( (next=readdir(dir))!=0 )
				{
					if( next->d_name[0] != '.' )
					{
						wchar_t *name = str2wcs(next->d_name);
						if( name == 0 )
						{
							continue;
						}
						wchar_t *long_name = make_path( base_dir, name );
						
						if( test_flags( long_name, flags ) )
						{
							get_desc( long_name,
									  &sb_desc,
									  flags & EXECUTABLES_ONLY );
							al_push( out,
									 wcsdupcat(name, (wchar_t *)sb_desc.buff) );
						}
						
						free(name);

						free( long_name );
					}					
				}
			}
			else
			{								
				res = 1;
				al_push_check( out, wcsdup( base_dir ) );
			}							
		}
		else
		{
			/*
			  This is the last wildcard segment, and it is not empty. Match files/directories.
			*/
			while( (next=readdir(dir))!=0 )
			{
				wchar_t *name = str2wcs(next->d_name);
				if( name == 0 )
				{
					continue;
				}
				
				if( flags & ACCEPT_INCOMPLETE )
				{
					
					wchar_t *long_name = make_path( base_dir, name );

					/*
					  Test for matches before stating file, so as to minimize the number of calls to the much slower stat function 
					*/
					if( wildcard_complete( name,
										   wc,
										   L"",
										   0,
										   0 ) )
					{
						if( test_flags( long_name, flags ) )
						{
							get_desc( long_name,
									  &sb_desc, 
									  flags & EXECUTABLES_ONLY );
							
							wildcard_complete( name,
											   wc,
											   (wchar_t *)sb_desc.buff,
											   0,
											   out );
						}
					}
					
					free( long_name );
					
				}
				else
				{
					if( wildcard_match2( name, wc, 1 ) )
					{
						wchar_t *long_name = make_path( base_dir, name );
						
						al_push_check( out, long_name );
						res = 1;
					}
				}
				free( name );
			}
		}
	}
	else
	{
		/*
		  Wilcard segment is not the last segment.  Recursively call
		  wildcard_expand for all matching subdirectories.
		*/
		
		/*
		  wc_str is the part of the wildcarded string from the
		  beginning to the first slash
		*/
		wchar_t *wc_str;

		/*
		  new_dir is a scratch area containing the full path to a
		  file/directory we are iterating over
		*/
		wchar_t *new_dir;

		/*
		  The maximum length of a file element
		*/
		static size_t ln=MAX_FILE_LENGTH;
		char * narrow_dir_string = wcs2str( dir_string );
		
		if( narrow_dir_string )
		{
			/* 
			   Find out how long the filename can be in a worst case
			   scenario
			*/
			ln = pathconf( narrow_dir_string, _PC_NAME_MAX ); 

			/*
			  If not specified, use som large number as fallback
			*/
			if( ln < 0 )
				ln = MAX_FILE_LENGTH;		
			free( narrow_dir_string );
		}
		new_dir= malloc( sizeof(wchar_t)*(base_len+ln+2)  );

		wc_str = wc_end?wcsndup(wc, wc_end-wc):wcsdup(wc);

		if( (!new_dir) || (!wc_str) )
		{
			die_mem();
		}

		wcscpy( new_dir, base_dir );
		
		while( (next=readdir(dir))!=0 )
		{
			wchar_t *name = str2wcs(next->d_name);
			if( name == 0 )
			{
				continue;
			}			

			/*
			  Test if the file/directory name matches the whole
			  wildcard element, i.e. regular matching.
			*/
			int whole_match = wildcard_match2( name, wc_str, 1 );
			int partial_match = 0;
			
			/* 
			   If we are doing recursive matching, also check if this
			   directory matches the part up to the recusrive
			   wildcard, if so, then we can search all subdirectories
			   for matches.
			*/
			if( is_recursive )
			{
				wchar_t *end = wcschr( wc, ANY_STRING_RECURSIVE );
				wchar_t *wc_sub = wcsndup( wc, end-wc+1);
				partial_match = wildcard_match2( name, wc_sub, 1 );
				free( wc_sub );
			}			

			if( whole_match || partial_match )
			{
				int new_len;
				struct stat buf;			
				char *dir_str;
				int stat_res;

				wcscpy(&new_dir[base_len], name );
				dir_str = wcs2str( new_dir );
				
				if( dir_str )
				{
					stat_res = stat( dir_str, &buf );
					free( dir_str );
					
					if( !stat_res )
					{
						if( buf.st_mode & S_IFDIR )
						{
							new_len = wcslen( new_dir );
							new_dir[new_len] = L'/';
							new_dir[new_len+1] = L'\0';
							
							/*
							  Regular matching
							*/
							if( whole_match )
							{
								res |= wildcard_expand( wc_end?wc_end + 1:L"", 
														new_dir, 
														flags, 
														out );

							}
							
							/*
							  Recursive matching
							*/
							if( partial_match )
							{
								res |= wildcard_expand( wcschr( wc, ANY_STRING_RECURSIVE ), 
														new_dir,
														flags | WILDCARD_RECURSIVE, 
														out );
							}
						}								
					}
				}
			}
			free(name);
		}
		
		free( wc_str );
		free( new_dir );
	}
	closedir( dir );
	
	if( flags & ACCEPT_INCOMPLETE )
		sb_destroy( &sb_desc );
	
	return res;
}

void al_push_check( array_list_t *l, const wchar_t *new )
{
	int i;

	for( i = 0; i < al_get_count(l); i++ ) 
	{
		if( !wcscmp( al_get(l, i), new ) ) 
		{
			free( new );
			return;
		}
	}

	al_push( l, new );
}
