/** \file wutil.c
	Wide character equivalents of various standard unix
	functions. Also contains fallback implementations of a large number
	of wide character unix functions.
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

#if !HAVE_FWPRINTF

void pad( void (*writer)(wchar_t), int count)
{
	
	int i;
	if( count < 0 )
		return;
	
	for( i=0; i<count; i++ )
	{
		writer( L' ' );
	}
}

/**
   Generic formatting function. All other string formatting functions
   are secretly a wrapper around this function. vgprintf does not
   implement all the filters supported by printf, only those that are
   currently used by fish. vgprintf internally uses snprintf to
   implement the number outputs, such as %f and %x.

   Currently supported functionality:

   - precision specification, both through .* and .N
   - Padding through * and N
   - Right padding using the - prefix
   - long versions of all filters thorugh l and ll prefix
   - Character output using %c
   - String output through %s
   - Floating point number output through %f
   - Integer output through %d, %i, %u, %o, %x and %X

   For a full description on the usage of *printf, see use 'man 3 printf'.
*/
static int vgwprintf( void (*writer)(wchar_t), 
					  const wchar_t *filter, 
					  va_list va )
{
	const wchar_t *filter_org=filter;
	int count=0;
	
	for( ;*filter; filter++)
	{
		if(*filter == L'%')
		{
			int is_long=0;
			int width = -1;
			filter++;
			int loop=1;
			int precision=-1;
			int pad_left = 1;
			
			if( iswdigit( *filter ) )
			{
				width=0;
				while( (*filter >= L'0') && (*filter <= L'9'))
				{
					width=10*width+(*filter++ - L'0');
				}				
			}

			while( loop )
			{
				
				switch(*filter)
				{
					case L'l':
						/* Long variable */
						is_long++;
						filter++;
						break;

					case L'*':
						/* Set minimum field width */
						width = va_arg( va, int );
						filter++;
						break;

					case L'-':
						filter++;
						pad_left=0;
						break;
						
					case L'.':
						/* 
						   Set precision.
						*/
						filter++;
						if( *filter == L'*' )
						{
							precision = va_arg( va, int );
						}
						else
						{
							precision=0;
							while( (*filter >= L'0') && (*filter <= L'9'))
							{
								precision=10*precision+(*filter++ - L'0');
							}							
						}
						break;
						
					default:
						loop=0;
						break;
				}
			}

			switch( *filter )
			{
				case L'c':
				{
					wchar_t c;

					if( (width >= 0) && pad_left )
					{
						pad( writer, width-1 );					
						count += maxi( width-1, 0 );						
					}

					c = is_long?va_arg(va, wint_t):btowc(va_arg(va, int));
					if( precision != 0 )
						writer( c );


					if( (width >= 0) && !pad_left )
					{
						pad( writer, width-1 );
						count += maxi( width-1, 0 );		
					}
					
					count++;
					
					break;
				}
				case L's':
				{		
					
					wchar_t *ss=0;
					if( is_long )
					{
						ss = va_arg(va, wchar_t *);
					}
					else
					{
						char *ns = va_arg(va, char*);

						if( ns )
						{
							ss = str2wcs( ns );
						}						
					}
										
					if( !ss )
					{
						return -1;
					}
					
					if( (width >= 0) && pad_left )
					{
						pad( writer, width-wcslen(ss) );					
						count += maxi(width-wcslen(ss), 0);						
					}
					
					wchar_t *s=ss;
					int precount = count;
					
					while( *s )
					{
						if( (precision > 0) && (precision <= (count-precount) ) )
							break;
						
						writer( *(s++) );
						count++;
					}
					
					if( (width >= 0) && !pad_left )
					{
						pad( writer, width-wcslen(ss) );					
						count += maxi( width-wcslen(ss), 0 );						
					}
					
					if( !is_long )
						free( ss );
					
					break;
				}

				case L'd':
				case L'i':
				case L'o':
				case L'u':
				case L'x':
				case L'X':
				{
					char str[33];
					char *pos;
					char format[16];
					int len;
					
					format[0]=0;
					strcat( format, "%");
					if( precision >= 0 )
						strcat( format, ".*" );
					switch( is_long )
					{
						case 2:
							strcat( format, "ll" );
							break;
						case 1:
							strcat( format, "l" );
							break;
					}
					
					len = strlen(format);
					format[len++]=(char)*filter;
					format[len]=0;

					switch( *filter )
					{
						case L'd':
						case L'i':
						{
							
							switch( is_long )
							{
								case 0:
								{
									int d = va_arg( va, int );
									if( precision >= 0 )
										snprintf( str, 32, format, precision, d );
									else
										snprintf( str, 32, format, d );
									
									break;
								}
								
								case 1:
								{
									long d = va_arg( va, long );
									if( precision >= 0 )
										snprintf( str, 32, format, precision, d );
									else
										snprintf( str, 32, format, d );
									break;
								}
								
								case 2:
								{
									long long d = va_arg( va, long long );
									if( precision >= 0 )
										snprintf( str, 32, format, precision, d );
									else
										snprintf( str, 32, format, d );
									break;
								}
						
								default:
									debug( 0, L"Invalid length modifier in string %ls\n", filter_org );
									return -1;
							}
							break;
							
						}
						
						case L'u':
						case L'o':
						case L'x':
						case L'X':
						{
							
							switch( is_long )
							{
								case 0:
								{
									unsigned d = va_arg( va, unsigned );
									if( precision >= 0 )
										snprintf( str, 32, format, precision, d );
									else
										snprintf( str, 32, format, d );
									break;
								}
								
								case 1:
								{
									unsigned long d = va_arg( va, unsigned long );
									if( precision >= 0 )
										snprintf( str, 32, format, precision, d );
									else
										snprintf( str, 32, format, d );
									break;
								}
								
								case 2:
								{
									unsigned long long d = va_arg( va, unsigned long long );
									if( precision >= 0 )
										snprintf( str, 32, format, precision, d );
									else
										snprintf( str, 32, format, d );
									break;
								}
								
								default:
									debug( 0, L"Invalid length modifier in string %ls\n", filter_org );
									return -1;
							}
							break;
							
						}
						
						default:
							debug( 0, L"Invalid filter %ls in string %ls\n", *filter, filter_org );
							return -1;
							
					}
					
					if( (width >= 0) && pad_left )
					{
						int l = maxi(width-strlen(str), 0 );
						pad( writer, l );
						count += l;
					}
					
					pos = str;
					
					while( *pos )
					{
						writer( *(pos++) );
						count++;
					}
					
					if( (width >= 0) && !pad_left )
					{
						int l = maxi(width-strlen(str), 0 );
						pad( writer, l );
						count += l;
					}
					
					break;
				}

				case L'f':
				{
					char str[32];
					char *pos;
					double val = va_arg( va, double );
					
					if( precision>= 0 )
					{
						if( width>= 0 )
						{
							snprintf( str, 32, "%*.*f", width, precision, val );
						}
						else
						{
							snprintf( str, 32, "%.*f", precision, val );
						}
					}
					else
					{
						if( width>= 0 )
						{
							snprintf( str, 32, "%*f", width, val );
						}
						else
						{
							snprintf( str, 32, "%f", val );
						}
					}

					pos = str;
					
					while( *pos )
					{
						writer( *(pos++) );
						count++;
					}
					
					break;
				}

				case L'n':
				{
					int *n = va_arg( va, int *);
					
					*n = count;					
					break;
				}
				case L'%':
				{
					writer('%');
					count++;
					break;
				}
				default:
					debug( 0, L"Unknown switch %lc in string %ls\n", *filter, filter_org );
					return -1;
			}
		}
		else
		{
			writer( *filter );
			count++;
		}
	}

	return count;
}

/**
   Holds data for swprintf writer
*/
static struct
{
	int count;
	int max;
	wchar_t *pos;
}
sw_data;

/**
   Writers for string output
*/
static void sw_writer( wchar_t c )
{
	if( sw_data.count < sw_data.max )
		*(sw_data.pos++)=c;	
	sw_data.count++;
}

int vswprintf( wchar_t *out, size_t n, const wchar_t *filter, va_list va )
{
	int written;
	
	sw_data.pos=out;
	sw_data.max=n;
	sw_data.count=0;
	written=vgwprintf( &sw_writer,
					   filter,
					   va );
	if( written < n )
	{
		*sw_data.pos = 0;
	}
	else
	{
		written=-1;
	}
	
	return written;
}

int swprintf( wchar_t *out, size_t n, const wchar_t *filter, ... )
{
	va_list va;
	int written;
	
	va_start( va, filter );
	written = vswprintf( out, n, filter, va );
	va_end( va );
	return written;
}

/**
   Holds auxiliary data for fwprintf and wprintf writer
*/
static FILE *fw_data;

static void fw_writer( wchar_t c )
{
	putwc( c, fw_data );
}

/*
   Writers for file output
*/
int vfwprintf( FILE *f, const wchar_t *filter, va_list va )
{
	fw_data = f;
	return vgwprintf( &fw_writer, filter, va );
}

int fwprintf( FILE *f, const wchar_t *filter, ... )
{
	va_list va;
	int written;
	
	va_start( va, filter );
	written = vfwprintf( f, filter, va );
	va_end( va );
	return written;
}

int vwprintf( const wchar_t *filter, va_list va )
{
	return vfwprintf( stdout, filter, va );
}

int wprintf( const wchar_t *filter, ... )
{
	va_list va;
	int written;
	
	va_start( va, filter );
	written=vwprintf( filter, va );
	va_end( va );
	return written;
}

#endif

#ifndef HAVE_FGETWC

wint_t fgetwc(FILE *stream)
{
	wchar_t res=0;
	mbstate_t state;
	memset (&state, '\0', sizeof (state));

	while(1)
	{
		int b = fgetc( stream );
		char bb;
			
		int sz;
			
		if( b == EOF )
			return WEOF;

		bb=b;
			
		sz = mbrtowc( &res, &bb, 1, &state );
			
		switch( sz )
		{
			case -1:
				memset (&state, '\0', sizeof (state));
				return WEOF;

			case -2:
				break;
			case 0:
				return 0;
			default:
				return res;
		}
	}

}


wint_t getwc(FILE *stream)
{
	return fgetwc( stream );
}


#endif

#ifndef HAVE_FPUTWC

wint_t fputwc(wchar_t wc, FILE *stream)
{
	int res;
	char s[MB_CUR_MAX+1];
	memset( s, 0, MB_CUR_MAX+1 );
	wctomb( s, wc );
	res = fputs( s, stream );
	return res==EOF?WEOF:wc;
}

wint_t putwc(wchar_t wc, FILE *stream)
{
	return fputwc( wc, stream );
}

#endif

#ifndef HAVE_WCSTOK

/*
  Used by fallback wcstok. Borrowed from glibc
*/
static size_t fish_wcsspn (const wchar_t *wcs,
					  const wchar_t *accept )
{
	register const wchar_t *p;
	register const wchar_t *a;
	register size_t count = 0;

	for (p = wcs; *p != L'\0'; ++p)
    {
		for (a = accept; *a != L'\0'; ++a)
			if (*p == *a)
				break;
		
		if (*a == L'\0')
			return count;
		else
			++count;
    }
	return count;	
}

/*
  Used by fallback wcstok. Borrowed from glibc
*/
static wchar_t *fish_wcspbrk (const wchar_t *wcs, const wchar_t *accept)
{
	while (*wcs != L'\0')
		if (wcschr (accept, *wcs) == NULL)
			++wcs;
		else
			return (wchar_t *) wcs;
	return NULL;	
}

/*
  Fallback wcstok implementation. Borrowed from glibc.
*/
wchar_t *wcstok(wchar_t *wcs, const wchar_t *delim, wchar_t **save_ptr)
{
	wchar_t *result;

	if (wcs == NULL)
    {
		if (*save_ptr == NULL)
        {
			errno = EINVAL;
			return NULL;
        }
		else
			wcs = *save_ptr;
    }

	/* Scan leading delimiters.  */
	wcs += fish_wcsspn (wcs, delim);
	
	if (*wcs == L'\0')
    {
		*save_ptr = NULL;		
		return NULL;
    }

	/* Find the end of the token.  */
	result = wcs;
	
	wcs = fish_wcspbrk (result, delim);
	
	if (wcs == NULL)
	{
		/* This token finishes the string.  */
		*save_ptr = NULL;
	}
	else
    {
		/* Terminate the token and make *SAVE_PTR point past it.  */
		*wcs = L'\0';
		*save_ptr = wcs + 1;
    }
	return result;
}

#endif

#ifndef HAVE_WCSDUP
wchar_t *wcsdup( const wchar_t *in )
{
	size_t len=wcslen(in);
	wchar_t *out = malloc( sizeof( wchar_t)*(len+1));
	if( out == 0 )
	{
		die_mem();
	}

	memcpy( out, in, sizeof( wchar_t)*(len+1));
	return out;
	
}
#endif

#ifndef HAVE_WCSLEN
size_t wcslen(const wchar_t *in)
{
	const wchar_t *end=in;
	while( *end )
		end++;
	return end-in;
}
#endif


#ifndef HAVE_WCSCASECMP
int wcscasecmp( const wchar_t *a, const wchar_t *b )
{
	if( *a == 0 )
	{
		return (*b==0)?0:-1;
	}
	else if( *b == 0 )
	{
		return 1;
	}
	int diff = towlower(*a)-towlower(*b);
	if( diff != 0 )
		return diff;
	else
		return wcscasecmp( a+1,b+1);
}
#endif


#ifndef HAVE_WCSNCASECMP
int wcsncasecmp( const wchar_t *a, const wchar_t *b, int count )
{
	if( count == 0 )
		return 0;
	
	if( *a == 0 )
	{
		return (*b==0)?0:-1;
	}
	else if( *b == 0 )
	{
		return 1;
	}
	int diff = towlower(*a)-towlower(*b);
	if( diff != 0 )
		return diff;
	else
		return wcsncasecmp( a+1,b+1, count-1);
}
#endif

#ifndef HAVE_WCWIDTH
int wcwidth( wchar_t c )
{
	if( c < 32 )
		return 0;
	if ( c == 127 )
		return 0;
	return 1;
}
#endif

#ifndef HAVE_WCSNDUP
wchar_t *wcsndup( const wchar_t *in, int c )
{
	wchar_t *res = malloc( sizeof(wchar_t)*(c+1) );
	if( res == 0 )
	{
		die_mem();
	}
	wcsncpy( res, in, c+1 );
	res[c] = L'\0';	
	return res;	
}
#endif

long convert_digit( wchar_t d, int base )
{
	long res=-1;
	if( (d <= L'9') && (d >= L'0') )
	{
		res = d - L'0';
	}
	else if( (d <= L'z') && (d >= L'a') )
	{
		res = d + 10 - L'a';		
	}
	else if( (d <= L'Z') && (d >= L'A') )
	{
		res = d + 10 - L'A';		
	}
	if( res >= base )
	{
		res = -1;
	}
	
	return res;
}

#ifndef HAVE_WCSTOL
long wcstol(const wchar_t *nptr, 
			wchar_t **endptr,
			int base)
{
	long long res=0;
	int is_set=0;
	if( base > 36 )
	{
		errno = EINVAL;
		return 0;
	}

	while( 1 )
	{
		long nxt = convert_digit( *nptr, base );
		if( endptr != 0 )
			*endptr = (wchar_t *)nptr;
		if( nxt < 0 )
		{
			if( !is_set )
			{
				errno = EINVAL;
			}
			return res;			
		}
		res = (res*base)+nxt;
		is_set = 1;
		if( res > LONG_MAX )
		{
			errno = ERANGE;
			return LONG_MAX;
		}
		if( res < LONG_MIN )
		{
			errno = ERANGE;
			return LONG_MIN;
		}
		nptr++;
	}
}
#endif

