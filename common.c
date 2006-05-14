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

/**
   The number of milliseconds to wait between polls when attempting to acquire 
   a lockfile
*/
#define LOCKPOLLINTERVAL 10

/**
  Highest legal ascii value
*/
#define ASCII_MAX 127u

/**
  Highest legal 16-bit unicode value
*/
#define UCS2_MAX 0xffffu

/**
  Highest legal byte value
*/
#define BYTE_MAX 0xffu

struct termios shell_modes;      

int error_max=1;

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

void common_destroy()
{
	
	if( setlocale_buff )
	{
		sb_destroy( setlocale_buff );
		free( setlocale_buff );
	}
}

void common_init()
{
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

	/*
	  This is a kludge: We block SIGCHLD while reading, since I can't
	  get getwc to perform reliably when signals are flying. Even when
	  watching for EINTR errors, bytes are lost. 
	*/

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
	wchar_t *out;
	size_t len = strlen(in);
	
	out = malloc( sizeof(wchar_t)*(len+1) );

	if( !out )
	{
		die_mem();
	}

	return str2wcs_internal( in, out );
}

wchar_t *str2wcs_internal( const char *in, wchar_t *out )
{
	size_t res=0;
	int in_pos=0;
	int out_pos = 0;
	mbstate_t state;
	size_t len = strlen(in);
	
	memset( &state, 0, sizeof(state) );
	
	while( in[in_pos] )
	{
		res = mbrtowc( &out[out_pos], &in[in_pos], len-in_pos, &state );

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
	out[out_pos] = 0;
	
	return out;	
}

char *wcs2str( const wchar_t *in )
{
	char *out;	
	
	out = malloc( MAX_UTF8_BYTES*wcslen(in)+1 );

	if( !out )
	{
		die_mem();
	}

	return wcs2str_internal( in, out );
}

char *wcs2str_internal( const wchar_t *in, char *out )
{
	size_t res=0;
	int in_pos=0;
	int out_pos = 0;
	mbstate_t state;
	memset( &state, 0, sizeof(state) );
	
	while( in[in_pos] )
	{
		if( ( in[in_pos] >= ENCODE_DIRECT_BASE) &&
			( in[in_pos] < ENCODE_DIRECT_BASE+256) )
		{
			out[out_pos++] = in[in_pos]- ENCODE_DIRECT_BASE;
		}
		else
		{
			res = wcrtomb( &out[out_pos], in[in_pos], &state );
			
			if( res == (size_t)(-1) )
			{
				debug( 1, L"Wide character has no narrow representation" );
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
	return wcsdupcat2( a, b, 0 );
}

wchar_t *wcsdupcat2( const wchar_t *a, ... )
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

wchar_t *wcsvarname( wchar_t *str )
{
	while( *str )
	{
		if( (!iswalnum(*str)) && (*str != L'_' ) )
		{
			return str;
		}
		str++;
	}
	return 0;
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

const wchar_t *quote_end( const wchar_t *pos )
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
		}
		else
		{
			if( *pos == c )
			{
				return pos;
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
		setlocale_buff = malloc( sizeof(string_buffer_t) );
		sb_init( setlocale_buff);
	}
	
	sb_clear( setlocale_buff );
	sb_printf( setlocale_buff, L"%s", res );
	
	return (wchar_t *)setlocale_buff->buff;	
}

int contains_str( const wchar_t *a, ... )
{
	wchar_t *arg;
	va_list va;
	int res = 0;

	if( !a )
	{
		debug( 1, L"Warning: Called contains_str with null argument. This is a bug." );		
		return 0;
	}
	
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


void die_mem()
{
	/*
	  Do not translate this message, and do not send it through the
	  usual channels. This increases the odds that the message gets
	  through correctly, even if we are out of memory.
	*/
	fwprintf( stderr, L"Out of memory, shutting down fish.\n" );
	exit(1);
}

void debug( int level, const wchar_t *msg, ... )
{
	va_list va;
	string_buffer_t sb;
	string_buffer_t sb2;
	
	if( level > debug_level )
		return;

	sb_init( &sb );
	sb_init( &sb2 );
	va_start( va, msg );
	
	sb_printf( &sb, L"%ls: ", program_name );
	sb_vprintf( &sb, msg, va );
	va_end( va );	

	write_screen( (wchar_t *)sb.buff, &sb2 );
	fwprintf( stderr, L"%ls", sb2.buff );	

	sb_destroy( &sb );	
	sb_destroy( &sb2 );	
}

void write_screen( const wchar_t *msg, string_buffer_t *buff )
{
	const wchar_t *start, *pos;
	int line_width = 0;
	int tok_width = 0;
	int screen_width = common_get_width();
	
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
				  In case of overflow, we print a newline, except if we alreade are at position 0
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
				break;
		
			start=pos;
		}
	}
	else
	{
		sb_printf( buff, L"%ls", msg );
	}
	sb_append_char( buff, L'\n' );
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
			
		}
		else
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
			case L':':
			case L'\'':
			case L'"':
			case L'%':
			case L'~':
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
						case L'X':
						case L'0':
						case L'1':
						case L'2':
						case L'3':
						case L'4':
						case L'5':
						case L'6':
						case L'7':
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
				if( c == L'\\' )
				{
					switch( in[++in_pos] )
					{
						case '\\':
						case L'\'':
						{
							in[out_pos]=in[in_pos];
							break;
						}
						
						case 0:
						{
							free(in);
							return 0;
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
							
							case '\\':
							case L'$':
							case '"':
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

