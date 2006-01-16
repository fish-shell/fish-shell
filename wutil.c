/** \file wutil.c
	Wide character equivalents of various standard unix functions. 
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

/**
   Buffer for converting wide arguments to narrow arguments, used by
   the \c wutil_wcs2str() function.
*/
static char *tmp=0;
/**
   Length of the \c tmp buffer.
*/
static size_t tmp_len=0;

/**
   Counts the number of calls to the wutil wrapper functions
*/
static int wutil_calls = 0;

void wutil_init()
{
}

void wutil_destroy()
{
	free( tmp );
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
	wutil_calls++;
	
	size_t new_sz =MAX_UTF8_BYTES*wcslen(in)+1;
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
	
	wcstombs( tmp, in, tmp_len );
	return tmp;
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


#if !HAVE_WPRINTF

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
   implement the %f %d and %u filters.

   Currently supported functionality:

   - precision specification, both through .* and .N
   - width specification through * and N
   - long versions of all filters thorugh l and ll prefix
   - Character outout using %c
   - String output through %s
   - Floating point number output through %f
   - Integer output through %d or %i
   - Unsigned integer output through %u
   - Left padding using the - prefix

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

					c = is_long?va_arg(va, wchar_t):btowc(va_arg(va, int));
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
				{
					char str[32];
					char *pos;
					
					switch( is_long )
					{
						case 0:
						{
							int d = va_arg( va, int );
							if( precision >= 0 )
								snprintf( str, 32, "%.*d", precision, d );
							else
								snprintf( str, 32, "%d", d );
							
							break;
						}
						
						case 1:
						{
							long d = va_arg( va, long );
							if( precision >= 0 )
								snprintf( str, 32, "%.*ld", precision, d );
							else
								snprintf( str, 32, "%ld", d );
							break;
						}
						
						case 2:
						{
							long long d = va_arg( va, long long );
							if( precision >= 0 )
								snprintf( str, 32, "%.*lld", precision, d );
							else
								snprintf( str, 32, "%lld", d );
							break;
						}
						
						default:
							return -1;
					}
					
					if( (width >= 0) && pad_left )
					{
						pad( writer, width-strlen(str) );					
						count +=maxi(width-strlen(str), 0 );						
					}
					
					pos = str;
					
					while( *pos )
					{
						writer( *(pos++) );
						count++;
					}

					if( (width >= 0) && !pad_left )
					{
						pad( writer, width-strlen(str) );					
						count += maxi(width-strlen(str), 0 );						
					}
					
					break;
				}
				
				case L'u':
				{
					char str[32];
					char *pos;
					
					switch( is_long )
					{
						case 0:
						{
							unsigned d = va_arg( va, unsigned );
							if( precision >= 0 )
								snprintf( str, 32, "%.*u", precision, d );
							else
								snprintf( str, 32, "%u", d );
							break;
						}
						
						case 1:
						{
							unsigned long d = va_arg( va, unsigned long );
							if( precision >= 0 )
								snprintf( str, 32, "%.*lu", precision, d );
							else
								snprintf( str, 32, "%lu", d );
							break;
						}
						
						case 2:
						{
							unsigned long long d = va_arg( va, unsigned long long );
							if( precision >= 0 )
								snprintf( str, 32, "%.*llu", precision, d );
							else
								snprintf( str, 32, "%llu", d );
							break;
						}
						
						default:
							return -1;
					}

					if( (width >= 0) && pad_left )
					{
						pad( writer, width-strlen(str) );					
						count += maxi( width-strlen(str), 0 );						
					}
					
					pos = str;
					
					while( *pos )
					{
						writer( *(pos++) );
						count++;
					}
					
					if( (width >= 0) && !pad_left )
					{
						pad( writer, width-strlen(str) );					
						count += maxi( width-strlen(str), 0 );						
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
//					exit(1);
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
