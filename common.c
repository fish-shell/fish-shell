/** \file common.c
	
Various functions, mostly string utilities, that are used by most
parts of fish.
*/

#include "config.h"

#include <stdlib.h>
#include <termios.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>		
#include <signal.h>		
#include <locale.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>

#ifndef HOST_NAME_MAX
/**
   Maximum length of hostname return. It is ok if this is too short,
   getting the actual hostname is not critical, so long as the string
   is unique in the filesystem namespace.
*/
#define HOST_NAME_MAX 255
#endif

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#include <term.h>


#include "util.h"
#include "wutil.h"
#include "common.h"
#include "expand.h"
#include "proc.h"
#include "wildcard.h"
#include "parser.h"

/**
   Error message to show on string convertion error
*/
#define STR2WCS_MSG "fish: Invalid multibyte sequence \'"

/**
   The maximum number of minor errors to report. Further errors will be omitted.
*/
#define ERROR_MAX_COUNT 1

/**
   The number of milliseconds to wait between polls when attempting to acquire 
   a lockfile
*/
#define LOCKPOLLINTERVAL 10

struct termios shell_modes;      

/**
   Number of error encountered. This is reset after each command, and
   used to limit the number of error messages on commands with many
   string convertion problems.
*/
static int error_count=0;

int error_max=1;

wchar_t ellipsis_char;

static int c1=0, c2=0, c3=0, c4=0, c5;

char *profile=0;

wchar_t *program_name;

int debug_level=1;

/**
   This struct should be continually updated by signals as the term resizes, and as such always contain the correct current size.
*/
static struct winsize termsize;


/**
   Number of nested calls to the block function. Unblock when this reaches 0.
*/
static int block_count=0;

void common_destroy()
{
	debug( 3, L"Calls: wcsdupcat %d, wcsdupcat2 %d, wcsndup %d, str2wcs %d, wcs2str %d", c1, c2, c3, c4, c5 );
}



wchar_t **list_to_char_arr( array_list_t *l )
{
	wchar_t ** res = malloc( sizeof(wchar_t *)*(al_get_count( l )+1) );
	int i;
	if( res == 0 )
	{
		die_mem();
	}
	for( i=0; i<al_get_count( l ); i++ )
		res[i] = (wchar_t *)al_get(l,i);
	res[i]='\0';
	return res;	
}

void block()
{
	block_count++;
	if( block_count == 1 )
	{
		sigset_t chldset; 
		sigemptyset( &chldset );
		sigaddset( &chldset, SIGCHLD );
		sigprocmask(SIG_BLOCK, &chldset, 0);
	}
}


void unblock()
{
	block_count--;
	if( block_count == 0 )
	{
		sigset_t chldset; 
		sigemptyset( &chldset );
		sigaddset( &chldset, SIGCHLD );
		sigprocmask(SIG_UNBLOCK, &chldset, 0);
	}
}



int fgetws2( wchar_t **b, int *len, FILE *f )
{
	int i=0;
	wint_t c;
	
	wchar_t *buff = *b;

	/*
	  This is a kludge: We block SIGCHLD while reading, since I can't
	  get getwc to perform reliably when signals are flying. Even when
	  watching for EINTR errors, bytes are lost. 
	*/

	block();

	while( 1 )
	{
		/* Reallocate the buffer if necessary */
		if( i+1 >= *len )
		{
			int new_len = maxi( 128, (*len)*2);
			buff = realloc( buff, sizeof(wchar_t)*new_len );
			if( buff == 0 )
			{
				die_mem();
			}
			else
			{
				*len = new_len;
				*b = buff;
			}			
		}
		
		errno=0;		
		
		c = getwc( f );
		
		if( errno == EILSEQ )
		{

			getc( f );
			continue;
		}
	
//fwprintf( stderr, L"b\n" );
		
		switch( c )
		{
			/* End of line */ 
			case WEOF:
			case L'\n':
			case L'\0':
				buff[i]=L'\0';
				unblock();

				return i;				
				/* Ignore carriage returns */
			case L'\r':
				break;
				
			default:
				buff[i++]=c;
				break;
		}		


	}
	unblock();

}


/**
   Wrapper for wcsfilecmp
*/
static int completion_cmp( const void *a, const void *b )
{
	wchar_t *c= *((wchar_t **)a);
	wchar_t *d= *((wchar_t **)b);
	return wcsfilecmp( c, d );
}

void sort_list( array_list_t *comp )
{
	qsort( comp->arr, 
		   al_get_count( comp ),
		   sizeof( void*),
		   &completion_cmp );
}

wchar_t *str2wcs( const char *in )
{
	c4++;
	
	wchar_t *res;
	
	res = malloc( sizeof(wchar_t)*(strlen(in)+1) );
	
	if( !res )
	{
		die_mem();
		
	}
	
	if( (size_t)-1 == mbstowcs( res, in, sizeof(wchar_t)*(strlen(in)) +1) )
	{
		error_count++;
		if( error_count <=error_max )
		{
			fflush( stderr );			
			write( 2,
				   STR2WCS_MSG,
				   strlen(STR2WCS_MSG) );
			write( 2,
				   in,
				   strlen(in ));
			write( 2, 
				   "\'\n",
				   2 );
		}
		
		free(res);
		return 0;
	}	
	
	return res;
	
}

void error_reset()
{
	error_count=0;
}

char *wcs2str( const wchar_t *in )
{
	c5++;

	char *res = malloc( MAX_UTF8_BYTES*wcslen(in)+1 );
	if( res == 0 )
	{
		die_mem();
	}

	wcstombs( res, 
			  in,
			  MAX_UTF8_BYTES*wcslen(in)+1 );

//	res = realloc( res, strlen( res )+1 );

	return res;
}

char **wcsv2strv( const wchar_t **in )
{
	int count =0;
	int i;

	while( in[count] != 0 )
		count++;
	char **res = malloc( sizeof( char *)*(count+1));
	if( res == 0 )
	{
		die_mem();		
	}

	for( i=0; i<count; i++ )
	{
		res[i]=wcs2str(in[i]);
	}
	res[count]=0;
	return res;

}

wchar_t *wcsdupcat( const wchar_t *a, const wchar_t *b )
{
	c1++;

	return wcsdupcat2( a, b, 0 );
}

wchar_t *wcsdupcat2( const wchar_t *a, ... )
{
	c2++;
	
	int len=wcslen(a);
	int pos;
	va_list va, va2;
	wchar_t *arg;

	va_start( va, a );
	va_copy( va2, va );
	while( (arg=va_arg(va, wchar_t *) )!= 0 ) 
	{
		len += wcslen( arg );
	}
	va_end( va );

	wchar_t *res = malloc( sizeof(wchar_t)*(len +1 ));
	if( res == 0 )
	{
		die_mem();
	}
	
	wcscpy( res, a );
	pos = wcslen(a);
	while( (arg=va_arg(va2, wchar_t *) )!= 0 ) 
	{
		wcscpy( res+pos, arg );
		pos += wcslen(arg);
	}
	va_end( va2 );
	return res;	

}


wchar_t **strv2wcsv( const char **in )
{
	int count =0;
	int i;

	while( in[count] != 0 )
		count++;
	wchar_t **res = malloc( sizeof( wchar_t *)*(count+1));
	if( res == 0 )
	{
		die_mem();
	}

	for( i=0; i<count; i++ )
	{
		res[i]=str2wcs(in[i]);
	}
	res[count]=0;
	return res;

}



wchar_t *wcsndup( const wchar_t *in, int c )
{
	c3++;
	
	wchar_t *res = malloc( sizeof(wchar_t)*(c+1) );
	if( res == 0 )
	{
		die_mem();
	}
	wcsncpy( res, in, c );
	res[c] = L'\0';	
	return res;	
}

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

/**
   Fallback implementation if missing from libc
*/
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

/**
   Fallback implementation if missing from libc
*/
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

int wcsvarname( wchar_t *str )
{
	while( *str )
	{
		if( (!iswalnum(*str)) && (*str != L'_' ) )
		{
			return 0;
		}
		str++;
	}
	return 1;
	
	
}

#if !HAVE_WCWIDTH
/**
   Return the number of columns used by a character. 

   In locales without a native wcwidth, Unicode is probably so broken
   that it isn't worth trying to implement a real wcwidth. This
   wcwidth assumes any printing character takes up one column.
*/
int wcwidth( wchar_t c )
{
	if( c < 32 )
		return 0;
	if ( c == 127 )
		return 0;
	return 1;
}
#endif

/** 
	The glibc version of wcswidth seems to hang on some strings. fish uses this replacement.
*/
int my_wcswidth( const wchar_t *c )
{
	int res=0;
	while( *c )
	{
		int w = wcwidth( *c++ );
		if( w < 0 )
			w = 1;
		if( w > 2 )
			w=1;
		
		res += w;		
	}
	return res;
}

wchar_t *quote_end( const wchar_t *in )
{
	switch( *in )
	{
		case '"':
		{
			while(1)
			{
				in = wcschr( in+1, L'"' );
				if( !in )
					return 0;
				if( *(in-1) != L'\\' )
					return (wchar_t *)in;
			}
		}
		case '\'':
		{
			return wcschr( in+1, L'\'' );
		}
	}
	return 0;
}


void fish_setlocale(int category, const wchar_t *locale)
{
	char *lang = wcs2str( locale );
	setlocale(category,lang);
	free( lang );
	/*
	  Use ellipsis if on known unicode system, otherwise use $
	*/
	if( wcslen( locale ) )
	{
		ellipsis_char = wcsstr( locale, L".UTF")?L'\u2026':L'$';	
	}
	else
	{
		char *lang = getenv( "LANG" );
		if( lang )
			ellipsis_char = strstr( lang, ".UTF")?L'\u2026':L'$';	
		else
			ellipsis_char = L'$';
	}
}

int contains_str( const wchar_t *a, ... )
{
	wchar_t *arg;
	va_list va;
	int res = 0;
	
	va_start( va, a );
	while( (arg=va_arg(va, wchar_t *) )!= 0 ) 
	{
		if( wcscmp( a,arg) == 0 )
		{
			res=1;
			break;
		}
		
	}
	va_end( va );
	return res;	
}

int read_blocked(int fd, void *buf, size_t count)
{
	int res;	
	sigset_t chldset, oldset; 

	sigemptyset( &chldset );
	sigaddset( &chldset, SIGCHLD );
	sigprocmask(SIG_BLOCK, &chldset, &oldset);
	res = read( fd, buf, count );
	sigprocmask( SIG_SETMASK, &oldset, 0 );
	return res;	
}

int writeb( tputs_arg_t b )
{
	write( 1, &b, 1 );
//	putc( b, stdout );
	return 0;
}

void die_mem()
{
	debug( 0, L"Out of memory, shutting down fish." );
	exit(1);
}

void debug( int level, wchar_t *msg, ... )
{
	va_list va;
	string_buffer_t sb;
	wchar_t *start, *pos;
	int line_width = 0;
	int tok_width = 0;
	int screen_width = common_get_width();
	
	if( level > debug_level )
		return;

	sb_init( &sb );
	va_start( va, msg );
	
	sb_printf( &sb, L"%ls: ", program_name );
	sb_vprintf( &sb, msg, va );
	va_end( va );	
	
	if( screen_width )
	{
		start = pos = (wchar_t *)sb.buff;
		while( 1 )
		{
			int overflow = 0;
		
			tok_width=0;

			/*
			  Tokenize on whitespace, and also calculate the width of the token
			*/
			while( *pos && ( !wcschr( L" \n\r\t", *pos ) ) )
			{

				/*
				  Check is token is wider than one line.
				  If so we mark it as an overflow and break the token.
				*/
				if((tok_width + wcwidth(*pos)) > (screen_width-1))
				{
					overflow = 1;
					break;				
				}
			
				tok_width += wcwidth( *pos );
				pos++;
			}

			/*
			  If token is zero character long, we don't do anything
			*/
			if( pos == start )
			{
				start = pos = pos+1;
			}
			else if( overflow )
			{
				/*
				  In case of overflow, we print a newline, except if we alreade are at position 0
				*/
				wchar_t *token = wcsndup( start, pos-start );
				if( line_width != 0 )
					putwc( L'\n', stderr );			
				fwprintf( stderr, L"%ls-\n", token );
				free( token );
				line_width=0;
			}
			else
			{
				/*
				  Print the token
				*/
				wchar_t *token = wcsndup( start, pos-start );
				if( (line_width + (line_width!=0?1:0) + tok_width) > screen_width )
				{
					putwc( L'\n', stderr );
					line_width=0;
				}
				fwprintf( stderr, L"%ls%ls", line_width?L" ":L"", token );
				free( token );
				line_width += (line_width!=0?1:0) + tok_width;
			}
			/*
			  Break on end of string
			*/
			if( !*pos )
				break;
		
			start=pos;
		}
	}
	else
	{
		fwprintf( stderr, L"%ls", sb.buff );
		
	}
	putwc( L'\n', stderr );
	sb_destroy( &sb );
}

wchar_t *escape( const wchar_t *in, 
				 int escape_all )
{
	wchar_t *out = malloc( sizeof(wchar_t)*(wcslen(in)*4 + 1));
	wchar_t *pos=out;
	if( !out )
		die_mem();
	
	while( *in != 0 )
	{
		switch( *in )
		{
			case L'\t':
				*(pos++) = L'\\';
				*(pos++) = L't';					
				break;
					
			case L'\n':
				*(pos++) = L'\\';
				*(pos++) = L'n';					
				break;
					
			case L'\b':
				*(pos++) = L'\\';
				*(pos++) = L'b';					
				break;
					
			case L'\r':
				*(pos++) = L'\\';
				*(pos++) = L'r';					
				break;
					
			case L'\e':
				*(pos++) = L'\\';
				*(pos++) = L'e';					
				break;
					
			case L'\\':
			case L'&':
			case L'$':
			case L' ':
			case L'#':
			case L'^':
			case L'<':
			case L'>':
			case L'@':
			case L'(':
			case L')':
			case L'{':
			case L'}':
			case L'?':
			case L'*':
			case L'|':
			case L';':
			case L':':
			case L'\'':
			case L'\"':
			case L'%':
				if( escape_all )
					*pos++ = L'\\';
				*pos++ = *in;
				break;
					
			default:
				if( *in < 32 )
				{
					int tmp = (*in)%16;
					*pos++ = L'\\';
					*pos++ = L'x';
					*pos++ = ((*in>15)? L'1' : L'0');
					*pos++ = tmp > 9? L'a'+(tmp-10):L'0'+tmp;
				}
				else
					*pos++ = *in;
				break;
		}
		in++;
	}
	*pos = 0;
	return out;
}


wchar_t *unescape( const wchar_t * orig, int unescape_special )
{
	
	int mode = 0; 
	int in_pos, out_pos, len = wcslen( orig );
	int c;
	int bracket_count=0;
	wchar_t prev=0;	
	wchar_t *in = wcsdup(orig);
	if( !in )
		die_mem();
	
	for( in_pos=0, out_pos=0; 
		 in_pos<len; 
		 (prev=(out_pos>=0)?in[out_pos]:0), out_pos++, in_pos++ )
	{
		c = in[in_pos];
		switch( mode )
		{

			/*
			  Mode 0 means unquoted string
			*/
			case 0:
			{
				if( c == L'\\' )
				{
					switch( in[++in_pos] )
					{
						case L'\0':
						{
							free(in);
							return 0;
						}
						
						case L'n':
						{
							in[out_pos]=L'\n';
							break;
						}
						
						case L'r':
						{
							in[out_pos]=L'\r';
							break;
						}

						case L't':
						{
							in[out_pos]=L'\t';
							break;
						}

						case L'b':
						{
							in[out_pos]=L'\b';
							break;
						}
						
						case L'e':
						{
							in[out_pos]=L'\e';
							break;
						}
						
						case L'u':
						case L'U':
						case L'x':
						case L'o':
						{
							int i;
							wchar_t res=0;
							int chars=2;
							int base=16;
					
							switch( in[in_pos] )
							{
								case L'u':
								{
									base=16;
									chars=4;
									break;
								}
								
								case L'U':
								{
									base=16;
									chars=8;
									break;
								}
								
								case L'x':
								{
									base=16;
									chars=2;
									break;
								}
								
								case L'o':
								{
									base=8;
									chars=3;
									break;
								}
								
							}
					
							for( i=0; i<chars; i++ )
							{
								int d = convert_digit( in[++in_pos],base);
								if( d < 0 )
								{
									in_pos--;
									break;
								}
						
								res=(res*base)|d;
						
							}
					
							in[out_pos] = res;
					
							break;
						}

						default:
						{
							in[out_pos]=in[in_pos];
							break;
						}
					}
				}
				else 
				{
					switch( in[in_pos]){
						case L'~':
						{
							if( unescape_special && (in_pos == 0) )
							{
								in[out_pos]=HOME_DIRECTORY;
							}
							else
							{
								in[out_pos] = L'~';
							}
							break;
						}

						case L'%':
						{
							if( unescape_special && (in_pos == 0) )
							{
								in[out_pos]=PROCESS_EXPAND;
							}
							else
							{
								in[out_pos]=in[in_pos];						
							}
							break;
						}

						case L'*':
						{
							if( unescape_special )
							{
								if( out_pos > 0 && in[out_pos-1]==ANY_STRING )
								{
									out_pos--;
									in[out_pos] = ANY_STRING_RECURSIVE;
								}
								else
									in[out_pos]=ANY_STRING;
							}
							else
							{
								in[out_pos]=in[in_pos];						
							}
							break;
						}

						case L'?':
						{
							if( unescape_special )
							{
								in[out_pos]=ANY_CHAR;
							}
							else
							{
								in[out_pos]=in[in_pos];						
							}
							break;					
						}

						case L'$':
						{
							if( unescape_special )
							{
								in[out_pos]=VARIABLE_EXPAND;
							}
							else
							{
								in[out_pos]=in[in_pos];											
							}
							break;					
						}

						case L'{':
						{
							if( unescape_special )
							{
								bracket_count++;
								in[out_pos]=BRACKET_BEGIN;
							}
							else
							{
								in[out_pos]=in[in_pos];						
							}
							break;					
						}
						
						case L'}':
						{
							if( unescape_special )
							{
								bracket_count--;
								in[out_pos]=BRACKET_END;
							}
							else
							{
								in[out_pos]=in[in_pos];						
							}
							break;					
						}
						
						case L',':
						{
							if( unescape_special && bracket_count && prev!=BRACKET_SEP)
							{
								in[out_pos]=BRACKET_SEP;
							}
							else
							{
								in[out_pos]=in[in_pos];						
							}
							break;					
						}
						
						case L'\'':
						{
							mode = 1;
							in[out_pos] = INTERNAL_SEPARATOR;							
							break;					
						}
				
						case L'\"':
						{
							mode = 2;
							in[out_pos] = INTERNAL_SEPARATOR;							
							break;
						}

						default:
						{
							in[out_pos] = in[in_pos];
							break;
						}
					}
				}		
				break;
			}

			/*
			  Mode 1 means single quoted string, i.e 'foo'
			*/
			case 1:
			{
				if( c == L'\'' )
				{
					in[out_pos] = INTERNAL_SEPARATOR;							
					mode = 0;
				}
				else
				{
					in[out_pos] = in[in_pos];
				}
				
				break;
			}

			/*
			  Mode 2 means double quoted string, i.e. "foo"
			*/
			case 2:
			{
				switch( c )
				{
					case '"':
					{
						mode = 0;
						in[out_pos] = INTERNAL_SEPARATOR;							
						break;
					}
				
					case '\\':
					{
						switch( in[++in_pos] )
						{
							case L'\0':
							{
								free(in);
								return 0;
							}
							
							case L'$':
							{
								in[out_pos]=in[in_pos];
								break;
							}
							
							default:
							{
								in[out_pos++] = L'\\';
								in[out_pos] = in[in_pos];
								break;
							}
						}
						break;
					}
					
					case '$':
					{
						if( unescape_special )
						{
							in[out_pos]=VARIABLE_EXPAND_SINGLE;
						}
						else
						{
							in[out_pos]=in[in_pos];
						}
						break;
					}
			
					default:
					{
						in[out_pos] = in[in_pos];
						break;
					}
				
				}						
				break;
			}
		}
	}
	in[out_pos]=L'\0';
	return in;	
}

/**
   Writes a pid_t in decimal representation to str.
   str must contain sufficient space.
   The conservatively approximate maximum number of characters a pid_t will 
   represent is given by: (int)(0.31 * sizeof(pid_t) + CHAR_BIT + 1)
   Returns the length of the string
*/
static int sprint_pid_t( pid_t pid, char *str )
{
	int len, i = 0;
	int dig;

	/* Store digits in reverse order into string */
	while( pid != 0 )
	{
		dig = pid % 10;
		str[i] = '0' + dig;
		pid = ( pid - dig ) / 10;
		i++;
	}
	len = i;
	/* Reverse digits */
	i /= 2;
	while( i )
	{
		i--;
		dig = str[i];
		str[i] = str[len -  1 - i];
		str[len - 1 - i] = dig;
	}
	return len;
}

/**
   Writes a pseudo-random number (between one and maxlen) of pseudo-random
   digits into str.
   str must point to an allocated buffer of size of at least maxlen chars.
   Returns the number of digits written.
   Since the randomness in part depends on machine time it has _some_ extra
   strength but still not enough for use in concurrent locking schemes on a
   single machine because gettimeofday may not return a different value on
   consecutive calls when:
   a) the OS does not support fine enough resolution
   b) the OS is running on an SMP machine.
   Additionally, gettimeofday errors are ignored.
   Excludes chars other than digits since ANSI C only guarantees that digits
   are consecutive.
*/
static int sprint_rand_digits( char *str, int maxlen )
{
	int i, max;
	struct timeval tv;
	
	/* 
	   Seed the pseudo-random generator based on time - this assumes
	   that consecutive calls to gettimeofday will return different values
	   and ignores errors returned by gettimeofday.
	   Cast to unsigned so that wrapping occurs on overflow as per ANSI C.
	*/
	(void)gettimeofday( &tv, NULL );
	srand( (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec * 1000000UL );
	max = 1 + (maxlen - 1) * (rand() / (RAND_MAX + 1.0));
	for( i = 0; i < max; i++ )
	{
		str[i] = '0' + 10 * (rand() / (RAND_MAX + 1.0));
	}
	return i;
}

/**
   Generate a filename unique in an NFS namespace by creating a copy of str and
   appending .{hostname}.{pid} to it.  If gethostname() fails then a pseudo-
   random string is substituted for {hostname} - the randomness of the string
   should be strong enough across different machines.  The main assumption 
   though is that gethostname will not fail and this is just a "safe enough"
   fallback.
   The memory returned should be freed using free().
*/
static char *gen_unique_nfs_filename( const char *filename )
{
	int pidlen, hnlen, orglen = strlen( filename );
	char hostname[HOST_NAME_MAX + 1];
	char *newname;
	
	if ( gethostname( hostname, HOST_NAME_MAX + 1 ) == 0 )
	{
		hnlen = strlen( hostname );
	}
	else
	{
		hnlen = sprint_rand_digits( hostname, HOST_NAME_MAX );
		hostname[hnlen] = '\0';
	}
	newname = malloc( orglen + 1 /* period */ + hnlen + 1 /* period */ +
					  /* max possible pid size: 0.31 ~= log(10)2 */
					  (int)(0.31 * sizeof(pid_t) * CHAR_BIT + 1)
					  + 1 /* '\0' */ );
	
	if ( newname == NULL )
	{
		debug( 1, L"gen_unique_nfs_filename: %s", strerror( errno ) );
		return newname;
	}
	memcpy( newname, filename, orglen );
	newname[orglen] = '.';
	memcpy( newname + orglen + 1, hostname, hnlen );
	newname[orglen + 1 + hnlen] = '.';
	pidlen = sprint_pid_t( getpid(), newname + orglen + 1 + hnlen + 1 );
	newname[orglen + 1 + hnlen + 1 + pidlen] = '\0';
/*	debug( 1, L"gen_unique_nfs_filename returning with: newname = \"%s\"; "
	L"HOST_NAME_MAX = %d; hnlen = %d; orglen = %d; "
	L"sizeof(pid_t) = %d; maxpiddigits = %d; malloc'd size: %d", 
	newname, (int)HOST_NAME_MAX, hnlen, orglen, 
	(int)sizeof(pid_t), 
	(int)(0.31 * sizeof(pid_t) * CHAR_BIT + 1),
	(int)(orglen + 1 + hnlen + 1 + 
	(int)(0.31 * sizeof(pid_t) * CHAR_BIT + 1) + 1) ); */
	return newname;
}

int acquire_lock_file( const char *lockfile, const int timeout, int force )
{
	int fd, timed_out = 0;
	int ret = 0; /* early exit returns failure */
	struct timespec pollint;
	struct timeval start, end;
	double elapsed;
	struct stat statbuf;

	/*
	  (Re)create a unique file and check that it has one only link.
	*/
	char *linkfile = gen_unique_nfs_filename( lockfile );
	if( linkfile == NULL )
	{
		goto done;
	}
	(void)unlink( linkfile );
	if( ( fd = open( linkfile, O_CREAT|O_RDONLY ) ) == -1 )
	{
		debug( 1, L"acquire_lock_file: open: %s", strerror( errno ) );
		goto done;
	}
	close( fd );
	if( stat( linkfile, &statbuf ) != 0 )
	{
		debug( 1, L"acquire_lock_file: stat: %s", strerror( errno ) );
		goto done;
	}
	if ( statbuf.st_nlink != 1 )
	{
		debug( 1, L"acquire_lock_file: number of hardlinks on unique "
			   L"tmpfile is %d instead of 1.", (int)statbuf.st_nlink );
		goto done;
	}
	if( gettimeofday( &start, NULL ) != 0 )
	{
		debug( 1, L"acquire_lock_file: gettimeofday: %s", strerror( errno ) );
		goto done;
	}
	end = start;
	pollint.tv_sec = 0;
	pollint.tv_nsec = LOCKPOLLINTERVAL * 1000000;
	do 
	{
		/*
		  Try to create a hard link to the unique file from the 
		  lockfile.  This will only succeed if the lockfile does not 
		  already exist.  It is guaranteed to provide race-free 
		  semantics over NFS which the alternative of calling 
		  open(O_EXCL|O_CREAT) on the lockfile is not.  The lock 
		  succeeds if the call to link returns 0 or the link count on 
		  the unique file increases to 2.
		*/
		if( link( linkfile, lockfile ) == 0 ||
		    ( stat( linkfile, &statbuf ) == 0 && 
			  statbuf.st_nlink == 2 ) )
		{
			/* Successful lock */
			ret = 1;
			break;
		}
		elapsed = end.tv_sec + end.tv_usec/1000000.0 - 
			( start.tv_sec + start.tv_usec/1000000.0 );
		/* 
		   The check for elapsed < 0 is to deal with the unlikely event 
		   that after the loop is entered the system time is set forward
		   past the loop's end time.  This would otherwise result in a 
		   (practically) infinite loop.
		*/
		if( timed_out || elapsed >= timeout || elapsed < 0 )
		{
			if ( timed_out == 0 && force )
			{
				/*
				  Timed out and force was specified - attempt to
				  remove stale lock and try a final time
				*/
				(void)unlink( lockfile );
				timed_out = 1;
				continue;
			}
			else
			{
				/*
				  Timed out and final try was unsuccessful or 
				  force was not specified
				*/
				debug( 1, L"acquire_lock_file: timed out "
					   L"trying to obtain lockfile %s using "
					   L"linkfile %s", lockfile, linkfile );
				break;
			}
		}
		nanosleep( &pollint, NULL );
	} while( gettimeofday( &end, NULL ) == 0 );
  done:
	/* The linkfile is not needed once the lockfile has been created */
	(void)unlink( linkfile );
	free( linkfile );
	return ret;
}

void common_handle_winch( int signal )
{
	if (ioctl(1,TIOCGWINSZ,&termsize)!=0)
	{
		return;
	}
}

int common_get_width()
{
	return termsize.ws_col;
}


int common_get_height()
{
	return termsize.ws_row;
}

