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
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <limits.h>

#include "util.h"
#include "common.h"
#include "wutil.h"

static char *tmp=0;
static size_t tmp_len=0;

int c = 0;

void wutil_destroy()
{
	free( tmp );
	tmp=0;
	tmp_len=0;
	debug( 3, L"wutil functions called %d times", c );
}

static char *wutil_wcs2str( const wchar_t *in )
{
	c++;
	
	size_t new_sz =MAX_UTF8_BYTES*wcslen(in)+1;
	if( tmp_len < new_sz )
	{
		free( tmp );
		tmp = malloc( new_sz );
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
		va_start( argp, flags );
		
		if( ! (flags & O_CREAT) )
			res = open(tmp, flags);
		else
			res = open(tmp, flags, va_arg(argp, int) );
		
		va_end( argp );
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
/*
  Here is my own implementation of *wprintf, included since NetBSD does
  not provide one of it's own.
*/

/**
   This function is defined to help vgwprintf when it wants to call
   itself recursively
*/
static int gwprintf( void (*writer)(wchar_t), 
					 const wchar_t *filter, 
					 ... );


/**
   Generic formatting function. All other formatting functions are
   secretly a wrapper around this function.
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
			int i;
			int is_long=0;
			int width = 0;
			filter++;
			int loop=1;
			int precision=INT_MAX;
			
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
					case L'.':
						/* 
						   Set precision.
						   Hasn't been tested enough yet, so I don't really trust it.
						*/
						filter++;
						if( *filter == L'*' )
						{
							precision = va_arg( va, int );
						}
						else
						{
							while( (*filter >= L'0') && (*filter <= L'9'))
							{
								precision=10*precision+(*filter - L'0');
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
					
					c = is_long?va_arg(va, wchar_t):btowc(va_arg(va, int));
					if( width )
					{
						int i;
						for( i=1; i<width; i++ )
						{
							writer( L' ' );
							count++;
						}
					}
					if( precision != 0 )
						writer( c );
					count++;
					
					break;
				}
				case L's':
				{
					wchar_t *ss = is_long?va_arg(va, wchar_t*):str2wcs(va_arg(va, char*));
					
					if( !ss )
						return -1;
					
					if( width )
					{
						int i;
						for( i=wcslen(ss); i<width; i++ )
						{
							writer( L' ' );
							count++;
						}
					}

					wchar_t *s=ss;
					int precount = count;
					
					while( *s )
					{
						if( (precision <= (count-precount) ) )
							break;

						writer( *(s++) );
						count++;
					}
					
					if( !is_long )
						free( ss );
					
					break;
				}

				case L'd':
				case L'i':
				{
					char str[32];

					switch( is_long )
					{
						case 0:
						{
							int d = va_arg( va, int );
							snprintf( str, 32, "%.*d", precision, d );
							break;
						}
						
						case 1:
						{
							long d = va_arg( va, long );
							snprintf( str, 32, "%.*ld", precision, d );
							break;
						}
						
						case 2:
						{
							long long d = va_arg( va, long long );
							snprintf( str, 32, "%.*lld", precision, d );
							break;
						}
						
						default:
							return -1;
					}
					
					if( width )
					{
						int i;
						for( i=strlen(str); i<width; i++ )
						{
							writer( L' ' );
							count++;
						}
					}
										
					int c = gwprintf( writer, L"%s", str );
					if( c==-1 )
						return -1;
					else
						count += c;
					
					break;
				}

				case L'u':
				{
					char str[32];
					

					switch( is_long )
					{
						case 0:
						{
							unsigned d = va_arg( va, unsigned );
							snprintf( str, 32, "%d", d );
							break;
						}
						
						case 1:
						{
							unsigned long d = va_arg( va, unsigned long );
							snprintf( str, 32, "%ld", d );
							break;
						}
						
						case 2:
						{
							unsigned long long d = va_arg( va, unsigned long long );
							snprintf( str, 32, "%lld", d );
							break;
						}
						
						default:
							return -1;
					}
					
					if( width )
					{
						int i;
						for( i=strlen(str); i<width; i++ )
						{
							writer( L' ' );
							count++;
						}
					}

					int c = gwprintf( writer, L"%s", str );
					if( c==-1 )
						return -1;
					else
						count += c;
					
					break;
				}

				case L'n':
				{
					int *n = va_arg( va, int *);
					
					*n = count;					
					break;
				}
				default:
					debug( 0, L"Unknown switch %lc in string %ls\n", *filter, filter_org );
					exit(1);
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

static int gwprintf( void (*writer)(wchar_t), 
					 const wchar_t *filter, 
					 ... )
{
	va_list va;
	va_start( va, filter );
	int written=vgwprintf( writer,
						   filter,
						   va );
	va_end( va );
	return written;
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
   Writer for swprintf
*/
static void sw_writer( wchar_t c )
{
	if( sw_data.count < sw_data.max )
		*(sw_data.pos++)=c;	
	sw_data.count++;
}


int swprintf( wchar_t *out, size_t n, const wchar_t *filter, ... )
{
	va_list va;
	va_start( va, filter );
	sw_data.pos=out;
	sw_data.max=n;
	sw_data.count=0;
	int written=vgwprintf( &sw_writer,
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
	
	va_end( va );
	return written;
}

/**
   Holds auxiliary data for fwprintf and wprintf writer
*/
static FILE *fw_data;

static void fw_writer( wchar_t c )
{
	putw( c, fw_data );
}

/**
   Writer for fwprintf and wprintf
*/
int fwprintf( FILE *f, const wchar_t *filter, ... )
{
	va_list va;
	va_start( va, filter );
	fw_data = f;
	
	int written=vgwprintf( &fw_writer, filter, va );
	va_end( va );
	return written;
}

int wprintf( const wchar_t *filter, ... )
{
	va_list va;
	va_start( va, filter );
	fw_data = stdout;
	
	int written=vgwprintf( &fw_writer, filter, va );
	va_end( va );
	return written;
}

#endif

