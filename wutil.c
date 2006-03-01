/** \file wutil.c
	Wide character equivalents of various standard unix
	functions.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <limits.h>


#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"

#define TMP_LEN_MIN 256

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 4096
#endif
#endif

/**
   Buffer for converting wide arguments to narrow arguments, used by
   the \c wutil_wcs2str() function.
*/
static char *tmp=0;
static wchar_t *tmp2=0;
/**
   Length of the \c tmp buffer.
*/
static size_t tmp_len=0;
static size_t tmp2_len=0;

/**
   Counts the number of calls to the wutil wrapper functions
*/
static int wutil_calls = 0;

static struct wdirent my_wdirent;

void wutil_init()
{
}

void wutil_destroy()
{
	free( tmp );
	free( tmp2 );
	tmp=0;
	tmp_len=0;
	debug( 3, L"wutil functions called %d times", wutil_calls );
}

/**
   Convert the specified wide aharacter string to a narrow character
   string. This function uses an internal temporary buffer for storing
   the result so subsequent results will overwrite previous results.
*/
static char *wutil_wcs2str( const wchar_t *in )
{
	size_t new_sz;
	
	wutil_calls++;
	
	new_sz =MAX_UTF8_BYTES*wcslen(in)+1;
	if( tmp_len < new_sz )
	{
		new_sz = maxi( new_sz, TMP_LEN_MIN );
		tmp = realloc( tmp, new_sz );
		if( !tmp )
		{
			die_mem();
		}
		tmp_len = new_sz;
	}

	return wcs2str_internal( in, tmp );
}


/**
   Convert the specified wide character string to a narrow character
   string. This function uses an internal temporary buffer for storing
   the result so subsequent results will overwrite previous results.
*/
static wchar_t *wutil_str2wcs( const char *in )
{
	size_t new_sz;
	
	wutil_calls++;
	
	new_sz = sizeof(wchar_t)*(strlen(in)+1);
	if( tmp2_len < new_sz )
	{
		new_sz = maxi( new_sz, TMP_LEN_MIN );
		tmp2 = realloc( tmp2, new_sz );
		if( !tmp2 )
		{
			die_mem();
		}
		tmp2_len = new_sz;
	}

	return str2wcs_internal( in, tmp2 );
}



struct wdirent *wreaddir(DIR *dir )
{
	struct dirent *d = readdir( dir );
	if( !d )
		return 0;

	my_wdirent.d_name = wutil_str2wcs( d->d_name );
	return &my_wdirent;
	
}


wchar_t *wgetcwd( wchar_t *buff, size_t sz )
{
	char buffc[sz*MAX_UTF8_BYTES];
	char *res = getcwd( buffc, sz*MAX_UTF8_BYTES );
	if( !res )
		return 0;
	
	if( (size_t)-1 == mbstowcs( buff, buffc, sizeof( wchar_t ) * sz ) )
	{
		return 0;		
	}	
	return buff;
}

int wchdir( const wchar_t * dir )
{
	char *tmp = wutil_wcs2str(dir);
	return chdir( tmp );
}

FILE *wfopen(const wchar_t *path, const char *mode)
{
	
	char *tmp =wutil_wcs2str(path);
	FILE *res=0;
	if( tmp )
	{
		res = fopen(tmp, mode);
		
	}
	return res;	
}

FILE *wfreopen(const wchar_t *path, const char *mode, FILE *stream)
{
	char *tmp =wutil_wcs2str(path);
	FILE *res=0;
	if( tmp )
	{
		res = freopen(tmp, mode, stream);
	}
	return res;	
}



int wopen(const wchar_t *pathname, int flags, ...)
{
	char *tmp =wutil_wcs2str(pathname);
	int res=-1;
	va_list argp;
	
	if( tmp )
	{
		
		if( ! (flags & O_CREAT) )
		{
			res = open(tmp, flags);
		}
		else
		{
			va_start( argp, flags );
			res = open(tmp, flags, va_arg(argp, int) );
			va_end( argp );
		}
	}
	
    return res;
}

int wcreat(const wchar_t *pathname, mode_t mode)
{
    char *tmp =wutil_wcs2str(pathname);
    int res = -1;
	if( tmp )
	{
		res= creat(tmp, mode);
	}
	
    return res;
}

DIR *wopendir(const wchar_t *name)
{
	char *tmp =wutil_wcs2str(name);
	DIR *res = 0;
	if( tmp ) 
	{
		res = opendir(tmp);
	}
	
	return res;
}

int wstat(const wchar_t *file_name, struct stat *buf)
{
	char *tmp =wutil_wcs2str(file_name);
	int res = -1;

	if( tmp )
	{
		res = stat(tmp, buf);
	}
	
	return res;	
}

int lwstat(const wchar_t *file_name, struct stat *buf)
{
	char *tmp =wutil_wcs2str(file_name);
	int res = -1;

	if( tmp )
	{
		res = lstat(tmp, buf);
	}
	
	return res;	
}


int waccess(const wchar_t *file_name, int mode)
{
	char *tmp =wutil_wcs2str(file_name);
	int res = -1;
	if( tmp )
	{
		res= access(tmp, mode);
	}	
	return res;	
}

void wperror(const wchar_t *s)
{
	if( s != 0 )
	{
		fwprintf( stderr, L"%ls: ", s );
	}
	fwprintf( stderr, L"%s\n", strerror( errno ) );
}

#ifdef HAVE_REALPATH_NULL

wchar_t *wrealpath(const wchar_t *pathname, wchar_t *resolved_path)
{
	char *tmp = wutil_wcs2str(pathname);
	char *narrow_res = realpath( tmp, 0 );	
	wchar_t *res;	

	if( !narrow_res )
		return 0;
		
	if( resolved_path )
	{
		wchar_t *tmp2 = str2wcs( narrow_res );
		wcslcpy( resolved_path, tmp2, PATH_MAX );
		free( tmp2 );
		res = resolved_path;		
	}
	else
	{
		res = str2wcs( narrow_res );
	}

    free( narrow_res );

	return res;
}

#else

wchar_t *wrealpath(const wchar_t *pathname, wchar_t *resolved_path)
{
	char *tmp =wutil_wcs2str(pathname);
	char narrow[PATH_MAX];
	char *narrow_res = realpath( tmp, narrow );
	wchar_t *res;	

	if( !narrow_res )
		return 0;
		
	if( resolved_path )
	{
		wchar_t *tmp2 = str2wcs( narrow_res );
		wcslcpy( resolved_path, tmp2, PATH_MAX );
		free( tmp2 );
		res = resolved_path;		
	}
	else
	{
		res = str2wcs( narrow_res );
	}
	return res;
}

#endif
