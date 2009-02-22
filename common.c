/** \file common.c
	
Various functions, mostly string utilities, that are used by most
parts of fish.
*/

#include "config.h"


#include <unistd.h>

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#include <stdlib.h>
#include <termios.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <wctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>		
#include <locale.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

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

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "common.h"
#include "expand.h"
#include "proc.h"
#include "wildcard.h"
#include "parser.h"

#include "util.c"
#include "halloc.c"
#include "halloc_util.c"
#include "fallback.c"

/**
   The number of milliseconds to wait between polls when attempting to acquire 
   a lockfile
*/
#define LOCKPOLLINTERVAL 10

struct termios shell_modes;      

wchar_t ellipsis_char;

char *profile=0;

wchar_t *program_name;

int debug_level=1;

/**
   This struct should be continually updated by signals as the term resizes, and as such always contain the correct current size.
*/
static struct winsize termsize;

/**
   String buffer used by the wsetlocale function
*/
static string_buffer_t *setlocale_buff=0;


void show_stackframe() 
{
	void *trace[32];
	char **messages = (char **)NULL;
	int i, trace_size = 0;

	trace_size = backtrace(trace, 32);
	messages = backtrace_symbols(trace, trace_size);

	if( messages )
	{
		debug( 0, L"Backtrace:" );
		for( i=0; i<trace_size; i++ )
		{
			fwprintf( stderr, L"%s\n", messages[i]);
		}
		free( messages );
	}
}


wchar_t **list_to_char_arr( array_list_t *l )
{
	wchar_t ** res = malloc( sizeof(wchar_t *)*(al_get_count( l )+1) );
	int i;
	if( res == 0 )
	{
		DIE_MEM();
	}
	for( i=0; i<al_get_count( l ); i++ )
	{		
		res[i] = (wchar_t *)al_get(l,i);
	}
	res[i]='\0';
	return res;	
}

int fgetws2( wchar_t **b, int *len, FILE *f )
{
	int i=0;
	wint_t c;
	
	wchar_t *buff = *b;

	while( 1 )
	{
		/* Reallocate the buffer if necessary */
		if( i+1 >= *len )
		{
			int new_len = maxi( 128, (*len)*2);
			buff = realloc( buff, sizeof(wchar_t)*new_len );
			if( buff == 0 )
			{
				DIE_MEM();
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
				return i;				
				/* Ignore carriage returns */
			case L'\r':
				break;
				
			default:
				buff[i++]=c;
				break;
		}		


	}

}


/**
   Wrapper for wcsfilecmp
*/
static int str_cmp( const void *a, const void *b )
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
		   &str_cmp );
}

wchar_t *str2wcs( const char *in )
{
	wchar_t *out;
	size_t len = strlen(in);
	
	out = malloc( sizeof(wchar_t)*(len+1) );

	if( !out )
	{
		DIE_MEM();
	}

	return str2wcs_internal( in, out );
}

wchar_t *str2wcs_internal( const char *in, wchar_t *out )
{
	size_t res=0;
	int in_pos=0;
	int out_pos = 0;
	mbstate_t state;
	size_t len;

	CHECK( in, 0 );
	CHECK( out, 0 );
		
	len = strlen(in);

	memset( &state, 0, sizeof(state) );

	while( in[in_pos] )
	{
		res = mbrtowc( &out[out_pos], &in[in_pos], len-in_pos, &state );

		if( ( ( out[out_pos] >= ENCODE_DIRECT_BASE) &&
		      ( out[out_pos] < ENCODE_DIRECT_BASE+256)) ||
		    ( out[out_pos] == INTERNAL_SEPARATOR ) )
		{
			out[out_pos] = ENCODE_DIRECT_BASE + (unsigned char)in[in_pos];
			in_pos++;
			memset( &state, 0, sizeof(state) );
			out_pos++;
		}
		else
		{
			
			switch( res )
			{
				case (size_t)(-2):
				case (size_t)(-1):
				{
					out[out_pos] = ENCODE_DIRECT_BASE + (unsigned char)in[in_pos];
					in_pos++;
					memset( &state, 0, sizeof(state) );
					break;
				}
				
				case 0:
				{
					return out;
				}
		
				default:
				{
					in_pos += res;
					break;
				}
			}
			out_pos++;
		}
		
	}
	out[out_pos] = 0;
	
	return out;	
}

char *wcs2str( const wchar_t *in )
{
	char *out;	
	
	out = malloc( MAX_UTF8_BYTES*wcslen(in)+1 );

	if( !out )
	{
		DIE_MEM();
	}

	return wcs2str_internal( in, out );
}

char *wcs2str_internal( const wchar_t *in, char *out )
{
	size_t res=0;
	int in_pos=0;
	int out_pos = 0;
	mbstate_t state;

	CHECK( in, 0 );
	CHECK( out, 0 );
	
	memset( &state, 0, sizeof(state) );
	
	while( in[in_pos] )
	{
		if( in[in_pos] == INTERNAL_SEPARATOR )
		{
		}
		else if( ( in[in_pos] >= ENCODE_DIRECT_BASE) &&
			 ( in[in_pos] < ENCODE_DIRECT_BASE+256) )
		{
			out[out_pos++] = in[in_pos]- ENCODE_DIRECT_BASE;
		}
		else
		{
			res = wcrtomb( &out[out_pos], in[in_pos], &state );
			
			if( res == (size_t)(-1) )
			{
				debug( 1, L"Wide character %d has no narrow representation", in[in_pos] );
				memset( &state, 0, sizeof(state) );
			}
			else
			{
				out_pos += res;
			}
		}
		in_pos++;
	}
	out[out_pos] = 0;
	
	return out;	
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
		DIE_MEM();		
	}

	for( i=0; i<count; i++ )
	{
		res[i]=wcs2str(in[i]);
	}
	res[count]=0;
	return res;

}

wchar_t *wcsdupcat_internal( const wchar_t *a, ... )
{
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
		DIE_MEM();
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
		DIE_MEM();
	}

	for( i=0; i<count; i++ )
	{
		res[i]=str2wcs(in[i]);
	}
	res[count]=0;
	return res;

}

wchar_t *wcsvarname( const wchar_t *str )
{
	while( *str )
	{
		if( (!iswalnum(*str)) && (*str != L'_' ) )
		{
			return (wchar_t *)str;
		}
		str++;
	}
	return 0;
}

wchar_t *wcsfuncname( const wchar_t *str )
{
	return wcschr( str, L'/' );
}


int wcsvarchr( wchar_t chr )
{
	return ( (iswalnum(chr)) || (chr == L'_' ));
}


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

wchar_t *quote_end( const wchar_t *pos )
{
	wchar_t c = *pos;
	
	while( 1 )
	{
		pos++;
		
		if( !*pos )
			return 0;
		
		if( *pos == L'\\')
		{
			pos++;
			if( !*pos )
				return 0;
		}
		else
		{
			if( *pos == c )
			{
				return (wchar_t *)pos;
			}
		}
	}
	return 0;
	
}

				
const wchar_t *wsetlocale(int category, const wchar_t *locale)
{

	char *lang = locale?wcs2str( locale ):0;
	char * res = setlocale(category,lang);
	
	free( lang );

	/*
	  Use ellipsis if on known unicode system, otherwise use $
	*/
	char *ctype = setlocale( LC_CTYPE, (void *)0 );
	ellipsis_char = (strstr( ctype, ".UTF")||strstr( ctype, ".utf") )?L'\u2026':L'$';	
		
	if( !res )
		return 0;
	
	if( !setlocale_buff )
	{
		setlocale_buff = sb_halloc( global_context);
	}
	
	sb_clear( setlocale_buff );
	sb_printf( setlocale_buff, L"%s", res );
	
	return (wchar_t *)setlocale_buff->buff;	
}

int contains_internal( const wchar_t *a, ... )
{
	wchar_t *arg;
	va_list va;
	int res = 0;

	CHECK( a, 0 );
	
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

ssize_t write_loop(int fd, char *buff, size_t count)
{
	ssize_t out=0;
	ssize_t out_cum=0;
	while( 1 ) 
	{
		out = write( fd, 
					 &buff[out_cum],
					 count - out_cum );
		if (out == -1) 
		{
			if( errno != EAGAIN &&
				errno != EINTR ) 
			{
				return -1;
			}
		} else 
		{
			out_cum += out;
		}
		if( out_cum >= count ) 
		{
			break;
		}
	}						
	return out_cum;
}



void debug( int level, const wchar_t *msg, ... )
{
	va_list va;

	string_buffer_t sb;
	string_buffer_t sb2;

	int errno_old = errno;
	
	if( level > debug_level )
		return;

	CHECK( msg, );
		
	sb_init( &sb );
	sb_init( &sb2 );

	sb_printf( &sb, L"%ls: ", program_name );

	va_start( va, msg );	
	sb_vprintf( &sb, msg, va );
	va_end( va );	

	write_screen( (wchar_t *)sb.buff, &sb2 );
	fwprintf( stderr, L"%ls", sb2.buff );	

	sb_destroy( &sb );	
	sb_destroy( &sb2 );	

	errno = errno_old;
}

void write_screen( const wchar_t *msg, string_buffer_t *buff )
{
	const wchar_t *start, *pos;
	int line_width = 0;
	int tok_width = 0;
	int screen_width = common_get_width();
	
	CHECK( msg, );
	CHECK( buff, );
		
	if( screen_width )
	{
		start = pos = msg;
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
				  In case of overflow, we print a newline, except if we already are at position 0
				*/
				wchar_t *token = wcsndup( start, pos-start );
				if( line_width != 0 )
					sb_append_char( buff, L'\n' );
				sb_printf( buff, L"%ls-\n", token );
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
					sb_append_char( buff, L'\n' );
					line_width=0;
				}
				sb_printf( buff, L"%ls%ls", line_width?L" ":L"", token );
				free( token );
				line_width += (line_width!=0?1:0) + tok_width;
			}
			
			/*
			  Break on end of string
			*/
			if( !*pos )
			{
				break;
			}
			
			start=pos;
		}
	}
	else
	{
		sb_printf( buff, L"%ls", msg );
	}
	sb_append_char( buff, L'\n' );
}

/**
   Perform string escaping of a strinng by only quoting it. Assumes
   the string has already been checked for characters that can not be
   escaped this way.
 */
static wchar_t *escape_simple( const wchar_t *in )
{
	wchar_t *out;
	size_t len = wcslen(in);
	out = malloc( sizeof(wchar_t)*(len+3));
	if( !out )
		DIE_MEM();
	
	out[0] = L'\'';
	wcscpy(&out[1], in );
	out[len+1]=L'\'';
	out[len+2]=0;
	return out;
}


wchar_t *escape( const wchar_t *in_orig, 
		 int flags )
{
	const wchar_t *in = in_orig;
	
	int escape_all = flags & ESCAPE_ALL;
	int no_quoted  = flags & ESCAPE_NO_QUOTED;
	
	wchar_t *out;
	wchar_t *pos;

	int need_escape=0;
	int need_complex_escape=0;

	if( !in )
	{
		debug( 0, L"%s called with null input", __func__ );
		FATAL_EXIT();
	}

	if( !no_quoted && (wcslen( in ) == 0) )
	{
		out = wcsdup(L"''");
		if( !out )
			DIE_MEM();
		return out;
	}
	
	
	out = malloc( sizeof(wchar_t)*(wcslen(in)*4 + 1));
	pos = out;
	
	if( !out )
		DIE_MEM();
	
	while( *in != 0 )
	{

		if( ( *in >= ENCODE_DIRECT_BASE) &&
			( *in < ENCODE_DIRECT_BASE+256) )
		{
			int val = *in - ENCODE_DIRECT_BASE;
			int tmp;
			
			*(pos++) = L'\\';
			*(pos++) = L'X';
			
			tmp = val/16;			
			*pos++ = tmp > 9? L'a'+(tmp-10):L'0'+tmp;
			
			tmp = val%16;			
			*pos++ = tmp > 9? L'a'+(tmp-10):L'0'+tmp;
			need_escape=need_complex_escape=1;
			
		}
		else
		{
			
			switch( *in )
			{
				case L'\t':
					*(pos++) = L'\\';
					*(pos++) = L't';					
					need_escape=need_complex_escape=1;
					break;
					
				case L'\n':
					*(pos++) = L'\\';
					*(pos++) = L'n';					
					need_escape=need_complex_escape=1;
					break;
					
				case L'\b':
					*(pos++) = L'\\';
					*(pos++) = L'b';					
					need_escape=need_complex_escape=1;
					break;
					
				case L'\r':
					*(pos++) = L'\\';
					*(pos++) = L'r';					
					need_escape=need_complex_escape=1;
					break;
					
				case L'\x1b':
					*(pos++) = L'\\';
					*(pos++) = L'e';					
					need_escape=need_complex_escape=1;
					break;
					

				case L'\\':
				case L'\'':
				{
					need_escape=need_complex_escape=1;
					if( escape_all )
						*pos++ = L'\\';
					*pos++ = *in;
					break;
				}

				case L'&':
				case L'$':
				case L' ':
				case L'#':
				case L'^':
				case L'<':
				case L'>':
				case L'(':
				case L')':
				case L'[':
				case L']':
				case L'{':
				case L'}':
				case L'?':
				case L'*':
				case L'|':
				case L';':
				case L'"':
				case L'%':
				case L'~':
				{
					need_escape=1;
					if( escape_all )
						*pos++ = L'\\';
					*pos++ = *in;
					break;
				}
				
				default:
				{
					if( *in < 32 )
					{
						if( *in <27 && *in > 0 )
						{
							*(pos++) = L'\\';
							*(pos++) = L'c';
							*(pos++) = L'a' + *in -1;
							
							need_escape=need_complex_escape=1;
							break;
								
						}
						

						int tmp = (*in)%16;
						*pos++ = L'\\';
						*pos++ = L'x';
						*pos++ = ((*in>15)? L'1' : L'0');
						*pos++ = tmp > 9? L'a'+(tmp-10):L'0'+tmp;
						need_escape=need_complex_escape=1;
					}
					else
					{
						*pos++ = *in;
					}
					break;
				}
			}
		}
		
		in++;
	}
	*pos = 0;

	/*
	  Use quoted escaping if possible, since most people find it
	  easier to read. 
	 */
	if( !no_quoted && need_escape && !need_complex_escape && escape_all )
	{
		free( out );
		out = escape_simple( in_orig );
	}
	
	return out;
}


wchar_t *unescape( const wchar_t * orig, int flags )
{
	
	int mode = 0; 
	int in_pos, out_pos, len;
	int c;
	int bracket_count=0;
	wchar_t prev=0;	
	wchar_t *in;
	int unescape_special = flags & UNESCAPE_SPECIAL;
	int allow_incomplete = flags & UNESCAPE_INCOMPLETE;
	
	CHECK( orig, 0 );
		
	len = wcslen( orig );
	in = wcsdup( orig );

	if( !in )
		DIE_MEM();
	
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
						
						/*
						  A null character after a backslash is an
						  error, return null
						*/
						case L'\0':
						{
							if( !allow_incomplete )
							{
								free(in);
								return 0;
							}
						}
												
						/*
						  Numeric escape sequences. No prefix means
						  octal escape, otherwise hexadecimal.
						*/
						
						case L'0':
						case L'1':
						case L'2':
						case L'3':
						case L'4':
						case L'5':
						case L'6':
						case L'7':
						case L'u':
						case L'U':
						case L'x':
						case L'X':
						{
							int i;
							long long res=0;
							int chars=2;
							int base=16;
							
							int byte = 0;
							wchar_t max_val = ASCII_MAX;
							
							switch( in[in_pos] )
							{
								case L'u':
								{
									chars=4;
									max_val = UCS2_MAX;
									break;
								}
								
								case L'U':
								{
									chars=8;
									max_val = WCHAR_MAX;
									break;
								}
								
								case L'x':
								{
									break;
								}
								
								case L'X':
								{
									byte=1;
									max_val = BYTE_MAX;
									break;
								}
								
								default:
								{
									base=8;
									chars=3;
									in_pos--;
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

							if( (res <= max_val) )
							{
								in[out_pos] = (byte?ENCODE_DIRECT_BASE:0)+res;
							}
							else
							{	
								free(in);	
								return 0;
							}
							
							break;
						}

						/*
						  \a means bell (alert)
						*/
						case L'a':
						{
							in[out_pos]=L'\a';
							break;
						}
						
						/*
						  \b means backspace
						*/
						case L'b':
						{
							in[out_pos]=L'\b';
							break;
						}
						
						/*
						  \cX means control sequence X
						*/
						case L'c':
						{
							in_pos++;
							if( in[in_pos] >= L'a' &&
								in[in_pos] <= (L'a'+32) )
							{
								in[out_pos]=in[in_pos]-L'a'+1;
							}
							else if( in[in_pos] >= L'A' &&
									 in[in_pos] <= (L'A'+32) )
							{
								in[out_pos]=in[in_pos]-L'A'+1;
							}
							else
							{
								free(in);	
								return 0;
							}
							break;
							
						}
						
						/*
						  \x1b means escape
						*/
						case L'e':
						{
							in[out_pos]=L'\x1b';
							break;
						}
						
						/*
						  \f means form feed
						*/
						case L'f':
						{
							in[out_pos]=L'\f';
							break;
						}

						/*
						  \n means newline
						*/
						case L'n':
						{
							in[out_pos]=L'\n';
							break;
						}
						
						/*
						  \r means carriage return
						*/
						case L'r':
						{
							in[out_pos]=L'\r';
							break;
						}
						
						/*
						  \t means tab
						 */
						case L't':
						{
							in[out_pos]=L'\t';
							break;
						}

						/*
						  \v means vertical tab
						*/
						case L'v':
						{
							in[out_pos]=L'\v';
							break;
						}
						
						default:
						{
							if( unescape_special )
								in[out_pos++] = INTERNAL_SEPARATOR;							
							in[out_pos]=in[in_pos];
							break;
						}
					}
				}
				else 
				{
					switch( in[in_pos])
					{
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
							if( unescape_special )
								in[out_pos] = INTERNAL_SEPARATOR;							
							else
								out_pos--;						
							break;					
						}
				
						case L'\"':
						{
							mode = 2;
							if( unescape_special )
								in[out_pos] = INTERNAL_SEPARATOR;							
							else
								out_pos--;						
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
				if( c == L'\\' )
				{
					switch( in[++in_pos] )
					{
						case '\\':
						case L'\'':
						case L'\n':
						{
							in[out_pos]=in[in_pos];
							break;
						}
						
						case 0:
						{
							if( !allow_incomplete )
							{
								free(in);
								return 0;
							}
							else
								out_pos--;						
						}
						
						default:
						{
							in[out_pos++] = L'\\';
							in[out_pos]= in[in_pos];
						}
					}
					
				}
				if( c == L'\'' )
				{
					if( unescape_special )
						in[out_pos] = INTERNAL_SEPARATOR;							
					else
						out_pos--;						
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
						if( unescape_special )
							in[out_pos] = INTERNAL_SEPARATOR;							
						else
							out_pos--;						
						break;
					}
				
					case '\\':
					{
						switch( in[++in_pos] )
						{
							case L'\0':
							{
								if( !allow_incomplete )
								{
									free(in);
									return 0;
								}
								else
									out_pos--;						
							}
							
							case '\\':
							case L'$':
							case '"':
							case '\n':
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

	if( !allow_incomplete && mode )
	{
		free( in );
		return 0;
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
	if( ( fd = open( linkfile, O_CREAT|O_RDONLY, 0600 ) ) == -1 )
	{
		debug( 1, L"acquire_lock_file: open: %s", strerror( errno ) );
		goto done;
	}
	/*
	  Don't need to check exit status of close on read-only file descriptors
	*/
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
#ifdef HAVE_WINSIZE
	if (ioctl(1,TIOCGWINSZ,&termsize)!=0)
	{
		return;
	}
#else
	termsize.ws_col = 80;
	termsize.ws_row = 24;
#endif
}

int common_get_width()
{
	return termsize.ws_col;
}


int common_get_height()
{
	return termsize.ws_row;
}

void tokenize_variable_array( const wchar_t *val, array_list_t *out )
{
	if( val )
	{
		wchar_t *cpy = wcsdup( val );
		wchar_t *pos, *start;
		wchar_t *next;

		if( !cpy )
		{
			DIE_MEM();
		}

		for( start=pos=cpy; *pos; pos++ )
		{
			if( *pos == ARRAY_SEP )
			{
				
				*pos=0;
				next = start==cpy?cpy:wcsdup(start);
				if( !next )
					DIE_MEM();
				al_push( out, next );
				start=pos+1;
			}
		}
		next = start==cpy?cpy:wcsdup(start);
		if( !next )
			DIE_MEM();
		al_push( out, next );
	}
}


int create_directory( wchar_t *d )
{
	int ok = 0;
	struct stat buf;
	int stat_res = 0;
	
	while( (stat_res = wstat(d, &buf ) ) != 0 )
	{
		if( errno != EAGAIN )
			break;
	}
	
	if( stat_res == 0 )
	{
		if( S_ISDIR( buf.st_mode ) )
		{
			ok = 1;
		}
	}
	else
	{
		if( errno == ENOENT )
		{
			wchar_t *dir = wcsdup( d );
			dir = wdirname( dir );
			if( !create_directory( dir ) )
			{
				if( !wmkdir( d, 0700 ) )
				{
					ok = 1;
				}
			}
			free(dir);
		}
	}
	
	return ok?0:-1;
}

void bugreport()
{
	debug( 1,
	       _( L"This is a bug. "
		  L"If you can reproduce it, please send a bug report to %s." ),
		PACKAGE_BUGREPORT );
}


void sb_format_size( string_buffer_t *sb,
		     long long sz )
{
	wchar_t *sz_name[]=
		{
			L"kB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB", 0
		}
	;

	if( sz < 0 )
	{
		sb_append( sb, L"unknown" );
	}
	else if( sz < 1 )
	{
		sb_append( sb, _( L"empty" ) );
	}
	else if( sz < 1024 )
	{
		sb_printf( sb, L"%lldB", sz );
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

double timef()
{
	int time_res;
	struct timeval tv;
	
	time_res = gettimeofday(&tv, 0);
	
	if( time_res ) 
	{
		/*
		  Fixme: What on earth is the correct parameter value for NaN?
		  The man pages and the standard helpfully state that this
		  parameter is implementation defined. Gcc gives a warning if
		  a null pointer is used. But not even all mighty Google gives
		  a hint to what value should actually be returned.
		 */
		return nan("");
	}
	
	return (double)tv.tv_sec + 0.000001*tv.tv_usec;
}
