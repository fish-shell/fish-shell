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
#include <string>

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "halloc.h"
#include "halloc_util.h"

typedef std::string cstring;

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
}

std::wstring *wreaddir(DIR *dir, std::wstring &outPath)
{
    struct dirent *d = readdir( dir );
    if ( !d ) return 0;
    
    outPath = str2wcstring(d->d_name);
    return &outPath;
}


wchar_t *wgetcwd( wchar_t *buff, size_t sz )
{
	char *buffc = (char *)malloc( sz*MAX_UTF8_BYTES);
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
    cstring tmp = wcs2string(dir);
	return chdir( tmp.c_str() );
}

FILE *wfopen(const wchar_t *path, const char *mode)
{
	cstring tmp = wcs2string(path);
	return fopen(tmp.c_str(), mode);
}

FILE *wfreopen(const wchar_t *path, const char *mode, FILE *stream)
{
    cstring tmp = wcs2string(path);
    return freopen(tmp.c_str(), mode, stream);
}

int wopen(const wchar_t *pathname, int flags, ...)
{
    cstring tmp = wcs2string(pathname);
	int res=-1;
	va_list argp;	
    if( ! (flags & O_CREAT) )
    {
        res = open(tmp.c_str(), flags);
    }
    else
    {
        va_start( argp, flags );
        res = open(tmp.c_str(), flags, va_arg(argp, int) );
        va_end( argp );
    }
    return res;
}

int wcreat(const wchar_t *pathname, mode_t mode)
{
    cstring tmp = wcs2string(pathname);
    return creat(tmp.c_str(), mode);
}

DIR *wopendir(const wchar_t *name)
{
    cstring tmp = wcs2string(name);
    return opendir(tmp.c_str());
}

int wstat(const wchar_t *file_name, struct stat *buf)
{
    cstring tmp = wcs2string(file_name);
    return stat(tmp.c_str(), buf);
}

int lwstat(const wchar_t *file_name, struct stat *buf)
{
   // fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
    cstring tmp = wcs2string(file_name);
    return lstat(tmp.c_str(), buf);
}


int waccess(const wchar_t *file_name, int mode)
{
    cstring tmp = wcs2string(file_name);
    return access(tmp.c_str(), mode);
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
	cstring tmp = wcs2string(pathname);
	char *narrow_res = realpath( tmp.c_str(), 0 );	
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
    cstring tmp = wcs2string(pathname);
	char narrow_buff[PATH_MAX];
	char *narrow_res = realpath( tmp.c_str(), narrow_buff );
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


wcstring wdirname( const wcstring &path )
{
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = dirname( tmp );
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

wcstring wbasename( const wcstring &path )
{
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = basename( tmp );
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
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
		wcs2str_buff = (char *)realloc( wcs2str_buff, len );
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
    cstring name_narrow = wcs2string(name);
	char *res_narrow = getenv( name_narrow.c_str() );
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
	cstring name_narrow = wcs2string(name);
	return mkdir( name_narrow.c_str(), mode );
}

int wrename( const wchar_t *old, const wchar_t *newv )
{
    cstring old_narrow = wcs2string(old);
	cstring new_narrow =wcs2string(newv);
	return rename( old_narrow.c_str(), new_narrow.c_str() );
}
