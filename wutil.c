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
#include <errno.h>
#include <fcntl.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <limits.h>
#include <libgen.h>

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "halloc.h"
#include "halloc_util.h"

/**
   Minimum length of the internal covnersion buffers
*/
#define TMP_LEN_MIN 256

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
/**
   Fallback length of MAXPATHLEN. Just a hopefully sane value...
*/
#define PATH_MAX 4096
#endif
#endif

/**
   Buffer for converting wide arguments to narrow arguments, used by
   the \c wutil_wcs2str() function.
*/
static char *tmp=0;

/**
   Buffer for converting narrow results to wide ones, used by the \c
   wutil_str2wcs() function. Avoid usign this without thinking about
   it, since subsequent calls will overwrite previous values.
*/
static wchar_t *tmp2;

/**
   Length of the \c tmp buffer.
*/
static size_t tmp_len=0;

/**
   Length of the \c tmp2 buffer
*/
static size_t tmp2_len;

/**
   Counts the number of calls to the wutil wrapper functions
*/
static int wutil_calls = 0;

/**
   Storage for the wreaddir function
*/
static struct wdirent my_wdirent;


/**
   For wgettext: Number of string_buffer_t in the ring of buffers
*/
#define BUFF_COUNT 4

/**
   For wgettext: The ring of string_buffer_t
*/
static string_buffer_t buff[BUFF_COUNT];
/**
   For wgettext: Current position in the ring
*/
static int curr_buff=0;

/**
   For wgettext: Buffer used by translate_wcs2str
*/
static char *wcs2str_buff=0;
/**
   For wgettext: Size of buffer used by translate_wcs2str
*/
static size_t wcs2str_buff_count=0;

/**
   For wgettext: Flag to tell whether the translation library has been initialized
*/
static int wgettext_is_init = 0;




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
			DIE_MEM();
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
			DIE_MEM();
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
	char *buffc = malloc( sz*MAX_UTF8_BYTES);
	char *res;
	wchar_t *ret = 0;
		
	if( !buffc )
	{
		errno = ENOMEM;
		return 0;
	}
	
	res = getcwd( buffc, sz*MAX_UTF8_BYTES );
	if( res )
	{
		if( (size_t)-1 != mbstowcs( buff, buffc, sizeof( wchar_t ) * sz ) )
		{
			ret = buff;
		}	
	}
	
	free( buffc );
	
	return ret;
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
	int e = errno;
	if( s != 0 )
	{
		fwprintf( stderr, L"%ls: ", s );
	}
	fwprintf( stderr, L"%s\n", strerror( e ) );
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
	char *tmp = wutil_wcs2str(pathname);
	char narrow_buff[PATH_MAX];
	char *narrow_res = realpath( tmp, narrow_buff );
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


wchar_t *wdirname( wchar_t *path )
{
	static string_buffer_t *sb = 0;
	if( sb )
		sb_clear(sb);
	else 
		sb = sb_halloc( global_context );
	
	char *tmp =wutil_wcs2str(path);
	char *narrow_res = dirname( tmp );
	if( !narrow_res )
		return 0;
	
	sb_printf( sb, L"%s", narrow_res );
	wcscpy( path, (wchar_t *)sb->buff );
	return path;
}

wchar_t *wbasename( const wchar_t *path )
{
	static string_buffer_t *sb = 0;
	if( sb )
		sb_clear(sb);
	else 
		sb = sb_halloc( global_context );
	
	char *tmp =wutil_wcs2str(path);
	char *narrow_res = basename( tmp );
	if( !narrow_res )
		return 0;
	
	sb_printf( sb, L"%s", narrow_res );
	return (wchar_t *)sb->buff;
}



/**
   For wgettext: Internal shutdown function. Automatically called on shutdown if the library has been initialized.
*/
static void wgettext_destroy()
{
	int i;

	if( !wgettext_is_init )
		return;
	
	wgettext_is_init = 0;
	
	for(i=0; i<BUFF_COUNT; i++ )
		sb_destroy( &buff[i] );

	free( wcs2str_buff );
}

/**
   For wgettext: Internal init function. Automatically called when a translation is first requested.
*/
static void wgettext_init()
{
	int i;
	
	wgettext_is_init = 1;
	
	for( i=0; i<BUFF_COUNT; i++ )
	{
		sb_init( &buff[i] );
	}
	
	halloc_register_function_void( global_context, &wgettext_destroy );
	
	bindtextdomain( PACKAGE_NAME, LOCALEDIR );
	textdomain( PACKAGE_NAME );
}


/**
   For wgettext: Wide to narrow character conversion. Internal implementation that
   avoids exessive calls to malloc
*/
static char *wgettext_wcs2str( const wchar_t *in )
{
	size_t len = MAX_UTF8_BYTES*wcslen(in)+1;
	if( len > wcs2str_buff_count )
	{
		wcs2str_buff = realloc( wcs2str_buff, len );
		if( !wcs2str_buff )
		{
			DIE_MEM();
		}
	}
	
	return wcs2str_internal( in, wcs2str_buff);
}

const wchar_t *wgettext( const wchar_t *in )
{
	if( !in )
		return in;
	
	if( !wgettext_is_init )
		wgettext_init();
	
	char *mbs_in = wgettext_wcs2str( in );	
	char *out = gettext( mbs_in );
	wchar_t *wres=0;
	
	sb_clear( &buff[curr_buff] );
	
	sb_printf( &buff[curr_buff], L"%s", out );
	wres = (wchar_t *)buff[curr_buff].buff;
	curr_buff = (curr_buff+1)%BUFF_COUNT;
	
	return wres;
}

wchar_t *wgetenv( const wchar_t *name )
{
	char *name_narrow =wutil_wcs2str(name);
	char *res_narrow = getenv( name_narrow );
	static string_buffer_t *out = 0;

	if( !res_narrow )
		return 0;
	
	if( !out )
	{
		out = sb_halloc( global_context );
	}
	else
	{
		sb_clear( out );
	}

	sb_printf( out, L"%s", res_narrow );
	
	return (wchar_t *)out->buff;
	
}

int wmkdir( const wchar_t *name, int mode )
{
	char *name_narrow =wutil_wcs2str(name);
	return mkdir( name_narrow, mode );
}

int wrename( const wchar_t *old, const wchar_t *new )
{
	char *old_narrow =wutil_wcs2str(old);
	char *new_narrow =wcs2str(new);
	int res;
	
	res = rename( old_narrow, new_narrow );
	free( new_narrow );

	return res;
}
