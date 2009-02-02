/**
   This file only contains fallback implementations of functions which
   have been found to be missing or broken by the configuration
   scripts.

   Many of these functions are more or less broken and
   incomplete. lrand28_r internally uses the regular (bad) rand_r
   function, the gettext function doesn't actually do anything, etc.
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
#include <assert.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include "fallback.h"
#include "util.h"


#ifndef HAVE___ENVIRON

char **__environ = 0;

#endif

#ifdef TPUTS_KLUDGE

int tputs(const char *str, int affcnt, int (*fish_putc)(tputs_arg_t))
{
	while( *str )
	{
		fish_putc( *str++ );
	}	
}

#endif

#ifdef TPARM_SOLARIS_KLUDGE

#undef tparm

/**
   Checks for known string values and maps to correct number of parameters.
*/
char *tparm_solaris_kludge( char *str, ... )
{
	long int param[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	va_list ap;
	va_start( ap, str );

	if( ( set_a_foreground && ! strcmp( str, set_a_foreground ) )
            || ( set_a_background && ! strcmp( str, set_a_background ) )
	    || ( set_foreground && ! strcmp( str, set_foreground ) ) 
            || ( set_background && ! strcmp( str, set_background ) )
	    || ( enter_underline_mode && ! strcmp( str, enter_underline_mode ) ) 
	    || ( exit_underline_mode && ! strcmp( str, exit_underline_mode ) ) 
	    || ( enter_standout_mode && ! strcmp( str, enter_standout_mode ) ) 
	    || ( exit_standout_mode && ! strcmp( str, exit_standout_mode ) ) 
	    || ( flash_screen && ! strcmp( str, flash_screen ) ) 
	    || ( enter_subscript_mode && ! strcmp( str, enter_subscript_mode ) ) 
	    || ( exit_subscript_mode && ! strcmp( str, exit_subscript_mode ) ) 
	    || ( enter_superscript_mode && ! strcmp( str, enter_superscript_mode ) ) 
	    || ( exit_superscript_mode && ! strcmp( str, exit_superscript_mode ) ) 
	    || ( enter_blink_mode && ! strcmp( str, enter_blink_mode ) ) 
	    || ( enter_italics_mode && ! strcmp( str, enter_italics_mode ) ) 
	    || ( exit_italics_mode && ! strcmp( str, exit_italics_mode ) ) 
	    || ( enter_reverse_mode && ! strcmp( str, enter_reverse_mode ) ) 
	    || ( enter_shadow_mode && ! strcmp( str, enter_shadow_mode ) ) 
	    || ( exit_shadow_mode && ! strcmp( str, exit_shadow_mode ) ) 
	    || ( enter_standout_mode && ! strcmp( str, enter_standout_mode ) ) 
	    || ( exit_standout_mode && ! strcmp( str, exit_standout_mode ) ) 
	    || ( enter_secure_mode && ! strcmp( str, enter_secure_mode ) ) 
	    || ( enter_bold_mode && ! strcmp ( str, enter_bold_mode ) ) )
	{
		param[0] = va_arg( ap, long int );
	}
	else if( cursor_address && ! strcmp( str, cursor_address ) )
	{
		param[0] = va_arg( ap, long int );
		param[1] = va_arg( ap, long int );
	}

	va_end( ap );


	return tparm( str, param[0], param[1], param[2], param[3],
		      param[4], param[5],  param[6], param[7], param[8] );
}

// Re-defining just to make sure nothing breaks further down in this file.
#define tparm tparm_solaris_kludge

#endif


#ifndef HAVE_FWPRINTF
#define INTERNAL_FWPRINTF 1
#endif

#ifdef HAVE_BROKEN_FWPRINTF
#define INTERNAL_FWPRINTF 1
#endif

#ifdef INTERNAL_FWPRINTF

/**
   Internal function for the wprintf fallbacks. USed to write the
   specified number of spaces.
*/
static void pad( void (*writer)(wchar_t), int count)
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
		return 0;
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
		return 0;
	}
	wcslcpy( res, in, c+1 );
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
#ifndef HAVE_WCSLCAT

/*$OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

size_t
wcslcat(wchar_t *dst, const wchar_t *src, size_t siz)
{
	
	register wchar_t *d = dst;
	register const wchar_t *s = src;
	register size_t n = siz;	
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	
	dlen = d - dst;
	n = siz - dlen;	

	if (n == 0)
		return(dlen + wcslen(s));

	while (*s != '\0') 
	{
		if (n != 1) 
		{
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));
	/* count does not include NUL */
}

#endif
#ifndef HAVE_WCSLCPY

/*$OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

size_t
wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz)
{
	register wchar_t *d = dst;
	register const wchar_t *s = src;
	register size_t n = siz;
	
	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) 
	{		
		do 
		{			
			if ((*d++ = *s++) == 0)
				break;
		}
		while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) 
	{		
		if (siz != 0)
			*d = '\0';
		/* NUL-terminate dst */
		while (*s++)
			;
	}
	return(s - src - 1);
	/* count does not include NUL */
}

#endif

#ifndef HAVE_LRAND48_R

int lrand48_r(struct drand48_data *buffer, long int *result)
{
	*result = rand_r( &buffer->seed );	
	return 0;
}

int srand48_r(long int seedval, struct drand48_data *buffer)
{
	buffer->seed = (int)seedval;
	return 0;
}

#endif

#ifndef HAVE_FUTIMES

int futimes(int fd, const struct timeval *times)
{
	errno = ENOSYS;
	return -1;
}

#endif

#ifndef HAVE_GETTEXT

char * gettext (const char * msgid)
{
	return (char *)msgid;
}

char * bindtextdomain (const char * domainname, const char * dirname)
{
	return 0;
}

char * textdomain (const char * domainname)
{
	return 0;
}

#endif

#ifndef HAVE_DCGETTEXT

char * dcgettext ( const char * domainname,
				   const char * msgid,
				   int category)
{
	return (char *)msgid;
}


#endif

#ifndef HAVE__NL_MSG_CAT_CNTR

int _nl_msg_cat_cntr=0;

#endif

#ifndef HAVE_KILLPG
int killpg( int pgr, int sig )
{
	assert( pgr > 1 );
	return kill( -pgr, sig );
}
#endif

#ifndef HAVE_WORKING_GETOPT_LONG

int getopt_long( int argc, 
				 char * const argv[],
				 const char *optstring,
				 const struct option *longopts, 
				 int *longindex )
{
	return getopt( argc, argv, optstring );
}


#endif

#ifndef HAVE_BACKTRACE
int backtrace (void **buffer, int size)
{
	return 0;
}
#endif

#ifndef HAVE_BACKTRACE_SYMBOLS
char ** backtrace_symbols (void *const *buffer, int size)
{
	return 0;
}
#endif

#ifndef HAVE_SYSCONF

long sysconf(int name)
{
	if( name == _SC_ARG_MAX )
	{
#ifdef ARG_MAX
		return ARG_MAX;
#endif
	}

	return -1;
	
}
#endif

#ifndef HAVE_NAN
double nan(char *tagp)
{
	return 0.0/0.0;
}
#endif

