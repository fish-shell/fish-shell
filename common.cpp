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
#include <algorithm>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
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
#include "complete.h"

#include "util.cpp"
#include "fallback.cpp"



struct termios shell_modes;      

// Note we foolishly assume that pthread_t is just a primitive. But it might be a struct.
static pthread_t main_thread_id = 0;
static bool thread_assertions_configured_for_testing = false;

wchar_t ellipsis_char;

char *profile=0;

const wchar_t *program_name;

int debug_level=1;

/**
   This struct should be continually updated by signals as the term resizes, and as such always contain the correct current size.
*/
static struct winsize termsize;


void show_stackframe() 
{
    /* Hack to avoid showing backtraces in the tester */
    if (program_name && ! wcscmp(program_name, L"(ignore)"))
        return;
    
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

int fgetws2(wcstring *s, FILE *f)
{
	int i=0;
	wint_t c;

	while( 1 )
	{
		errno=0;

		c = getwc( f );
		
		if( errno == EILSEQ )
		{
			continue;
		}
			
		switch( c )
		{
			/* End of line */ 
			case WEOF:
			case L'\n':
			case L'\0':
				return i;				
				/* Ignore carriage returns */
			case L'\r':
				break;
				
			default:
                i++;
				s->push_back((wchar_t)c);
				break;
		}
	}
}

wchar_t *str2wcs( const char *in )
{
	wchar_t *out;
	size_t len = strlen(in);
	
	out = (wchar_t *)malloc( sizeof(wchar_t)*(len+1) );

	if( !out )
	{
		DIE_MEM();
	}

	return str2wcs_internal( in, out );
}

wcstring str2wcstring( const char *in )
{
    wchar_t *tmp = str2wcs(in);
    wcstring result = tmp;
    free(tmp);
    return result;
}

wcstring str2wcstring( const std::string &in )
{
    wchar_t *tmp = str2wcs(in.c_str());
    wcstring result = tmp;
    free(tmp);
    return result;
}

wchar_t *str2wcs_internal( const char *in, wchar_t *out )
{
	size_t res=0;
	size_t in_pos=0;
	size_t out_pos = 0;
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
    if (! in)
        return NULL;
	char *out;
    size_t desired_size = MAX_UTF8_BYTES*wcslen(in)+1;
    char local_buff[512];
    if (desired_size <= sizeof local_buff / sizeof *local_buff) {
        // convert into local buff, then use strdup() so we don't waste malloc'd space
        char *result = wcs2str_internal(in, local_buff);
        if (result) {
            // It converted into the local buffer, so copy it
            result = strdup(result);
            if (! result) {
                DIE_MEM();
            }
        }
        return result;
        
    } else {
        // here we fall into the bad case of allocating a buffer probably much larger than necessary
        out = (char *)malloc( MAX_UTF8_BYTES*wcslen(in)+1 );
        if (!out) {
            DIE_MEM();
        }
        return wcs2str_internal( in, out );
    }

	return wcs2str_internal( in, out );
}

std::string wcs2string(const wcstring &input)
{
    char *tmp = wcs2str(input.c_str());
    std::string result = tmp;
    free(tmp);
    return result;
}

char *wcs2str_internal( const wchar_t *in, char *out )
{
	size_t res=0;
	size_t in_pos=0;
	size_t out_pos = 0;
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

char **wcsv2strv( const wchar_t * const *in )
{
	size_t i, count = 0;

	while( in[count] != 0 )
		count++;
	char **res = (char **)malloc( sizeof( char *)*(count+1));
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

wcstring format_string(const wchar_t *format, ...)
{
	va_list va;
	va_start( va, format );
    wcstring result = vformat_string(format, va);
	va_end( va );
    return result;
}

wcstring vformat_string(const wchar_t *format, va_list va_orig)
{    
    const int saved_err = errno;
    /*
      As far as I know, there is no way to check if a
      vswprintf-call failed because of a badly formated string
      option or because the supplied destination string was to
      small. In GLIBC, errno seems to be set to EINVAL either way. 

      Because of this, on failiure we try to
      increase the buffer size until the free space is
      larger than max_size, at which point it will
      conclude that the error was probably due to a badly
      formated string option, and return an error. Make
      sure to null terminate string before that, though.
    */
    const size_t max_size = (128*1024*1024);
    wchar_t static_buff[256];
    size_t size = 0;
    wchar_t *buff = NULL;
    int status = -1;
    while (status < 0) {
        /* Reallocate if necessary */
        if (size == 0) {
            buff = static_buff;
            size = sizeof static_buff;
        } else {
            size *= 2;
            if (size >= max_size) {
                buff[0] = '\0';
                break;
            }
            buff = (wchar_t *)realloc( (buff == static_buff ? NULL : buff), size);
            if (buff == NULL) {
                DIE_MEM();
            }
        }
                
        /* Try printing */
		va_list va;
		va_copy( va, va_orig );
        status = vswprintf(buff, size / sizeof(wchar_t), format, va);
        va_end(va);   
    }
    
    wcstring result = wcstring(buff);
    
    if (buff != static_buff)
        free(buff);
    
    errno = saved_err;
    return result;
}

void append_format(wcstring &str, const wchar_t *format, ...)
{
    /* Preserve errno across this call since it likes to stomp on it */
    int err = errno;
	va_list va;
	va_start( va, format );
    str.append(vformat_string(format, va));
	va_end( va );
    errno = err;
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

const wchar_t *wcsfuncname( const wchar_t *str )
{
	return wcschr( str, L'/' );
}


int wcsvarchr( wchar_t chr )
{
	return iswalnum(chr) || chr == L'_';
}


/** 
	The glibc version of wcswidth seems to hang on some strings. fish uses this replacement.
*/
int my_wcswidth( const wchar_t *c )
{
	return fish_wcswidth(c, wcslen(c));
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

				
wcstring wsetlocale(int category, const wchar_t *locale)
{

	char *lang = NULL;
	if (locale){
		lang = wcs2str( locale );
	}
	char * res = setlocale(category,lang);
	free( lang );

	/*
	  Use ellipsis if on known unicode system, otherwise use $
	*/
	char *ctype = setlocale( LC_CTYPE, NULL );
	ellipsis_char = (strstr( ctype, ".UTF")||strstr( ctype, ".utf") )?L'\x2026':L'$';	
		
	if( !res )
		return wcstring();
    else
		return format_string(L"%s", res);
}

bool contains_internal( const wchar_t *a, ... )
{
	const wchar_t *arg;
	va_list va;
	bool res = false;

	CHECK( a, 0 );
	
	va_start( va, a );
	while( (arg=va_arg(va, const wchar_t *) )!= 0 ) 
	{
		if( wcscmp( a,arg) == 0 )
		{
			res = true;
			break;
		}
		
	}
	va_end( va );
	return res;
}

/* wcstring variant of contains_internal. The first parameter is a wcstring, the rest are const wchar_t* */
__sentinel bool contains_internal( const wcstring &needle, ... )
{
	const wchar_t *arg;
	va_list va;
	int res = 0;

	va_start( va, needle );
	while( (arg=va_arg(va, const wchar_t *) )!= 0 ) 
	{
		if( needle == arg)
		{
			res=1;
			break;
		}
		
	}
	va_end( va );
	return res;
}

long read_blocked(int fd, void *buf, size_t count)
{
	ssize_t res;
	sigset_t chldset, oldset; 

	sigemptyset( &chldset );
	sigaddset( &chldset, SIGCHLD );
	VOMIT_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &chldset, &oldset));
	res = read( fd, buf, count );
	VOMIT_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &oldset, NULL));
	return res;
}

ssize_t write_loop(int fd, const char *buff, size_t count)
{
	ssize_t out=0;
	size_t out_cum=0;
	while( 1 ) 
	{
		out = write( fd, 
					 &buff[out_cum],
					 count - out_cum );
		if (out < 0) 
		{
			if(errno != EAGAIN && errno != EINTR) 
			{
				return -1;
			}
		}
        else 
		{
			out_cum += (size_t)out;
		}
		if( out_cum >= count ) 
		{
			break;
		}
	}						
	return (ssize_t)out_cum;
}

ssize_t read_loop(int fd, void *buff, size_t count)
{
    ssize_t result;
    do {
        result = read(fd, buff, count);
    } while (result < 0 && (errno == EAGAIN || errno == EINTR));
    return result;
}

static bool should_debug(int level)
{
	if( level > debug_level )
		return false;

    /* Hack to not print error messages in the tests */
    if ( program_name && ! wcscmp(program_name, L"(ignore)") )
        return false;
        
    return true;
}

static void debug_shared( const wcstring &msg )
{
    const wcstring sb = wcstring(program_name) + L": " + msg;
	wcstring sb2;
	write_screen( sb, sb2 );
	fwprintf( stderr, L"%ls", sb2.c_str() );	
}

void debug( int level, const wchar_t *msg, ... )
{
    if (! should_debug(level))
        return;
    int errno_old = errno;
    va_list va;
	va_start(va, msg);
    wcstring local_msg = vformat_string(msg, va);
	va_end(va);
    debug_shared(local_msg);
    errno = errno_old;
}

void debug( int level, const char *msg, ... )
{
    if (! should_debug(level))
        return;
    int errno_old = errno;
    char local_msg[512];
    va_list va;
	va_start(va, msg);
    vsnprintf(local_msg, sizeof local_msg, msg, va);
	va_end(va);
    debug_shared(str2wcstring(local_msg));
    errno = errno_old;
}


void debug_safe(int level, const char *msg, const char *param1, const char *param2, const char *param3, const char *param4, const char *param5, const char *param6, const char *param7, const char *param8, const char *param9, const char *param10, const char *param11, const char *param12)
{
    const char * const params[] = {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12};
    if (! msg)
        return;
        
    /* Can't call printf, that may allocate memory Just call write() over and over. */
    if (level > debug_level)
        return;
    int errno_old = errno;
    
    size_t param_idx = 0;
    const char *cursor = msg;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '%');
        if (end == NULL)
            end = cursor + strlen(cursor);
        
        write(STDERR_FILENO, cursor, end - cursor);

        if (end[0] == '%' && end[1] == 's') {
            /* Handle a format string */
            assert(param_idx < sizeof params / sizeof *params);
            const char *format = params[param_idx++];
            if (! format) 
                format = "(null)";
            write(STDERR_FILENO, format, strlen(format));
            cursor = end + 2;
        } else if (end[0] == '\0') {
            /* Must be at the end of the string */
            cursor = end;
        } else {
            /* Some other format specifier, just skip it */
            cursor = end + 1;
        }
    }
    
    // We always append a newline
    write(STDERR_FILENO, "\n", 1);
    
    errno = errno_old;
}

void format_long_safe(char buff[128], long val) {
    if (val == 0) {
        strcpy(buff, "0");
    } else {
        /* Generate the string in reverse */
        size_t idx = 0;
        bool negative = (val < 0);
        
        /* Note that we can't just negate val if it's negative, because it may be the most negative value. We do rely on round-towards-zero division though. */

        while (val != 0) {
            long rem = val % 10;
            buff[idx++] = '0' + (rem < 0 ? -rem : rem);
            val /= 10;
        }
        if (negative)
            buff[idx++] = '-';
        buff[idx] = 0;
        
        size_t left = 0, right = idx - 1;
        while (left < right)  {
            char tmp = buff[left];
            buff[left++] = buff[right];
            buff[right--] = tmp;
        }
    }
}

void format_long_safe(wchar_t buff[128], long val) {
    if (val == 0) {
        wcscpy(buff, L"0");
    } else {
        /* Generate the string in reverse */
        size_t idx = 0;
        bool negative = (val < 0);

        while (val > 0) {
            long rem = val % 10;
            /* Here we're assuming that wide character digits are contiguous - is that a correct assumption? */
            buff[idx++] = L'0' + (wchar_t)(rem < 0 ? -rem : rem);
            val /= 10;
        }
        if (negative)
            buff[idx++] = L'-';
        buff[idx] = 0;
        
        size_t left = 0, right = idx - 1;
        while (left < right)  {
            wchar_t tmp = buff[left];
            buff[left++] = buff[right];
            buff[right--] = tmp;
        }
    }
}

void write_screen( const wcstring &msg, wcstring &buff )
{
	const wchar_t *start, *pos;
	int line_width = 0;
	int tok_width = 0;
	int screen_width = common_get_width();
	
	if( screen_width )
	{
		start = pos = msg.c_str();
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
				if((tok_width + fish_wcwidth(*pos)) > (screen_width-1))
				{
					overflow = 1;
					break;				
				}
			
				tok_width += fish_wcwidth( *pos );
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
                    buff.push_back(L'\n');
                buff.append(format_string(L"%ls-\n", token));
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
                    buff.push_back(L'\n');
					line_width=0;
				}
                buff.append(format_string(L"%ls%ls", line_width?L" ":L"", token ));
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
        buff.append(msg);
	}
    buff.push_back(L'\n');
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
	out = (wchar_t *)malloc( sizeof(wchar_t)*(len+3));
	if( !out )
		DIE_MEM();
	
	out[0] = L'\'';
	wcscpy(&out[1], in );
	out[len+1]=L'\'';
	out[len+2]=0;
	return out;
}

wchar_t *escape( const wchar_t *in_orig, escape_flags_t flags )
{
	const wchar_t *in = in_orig;
	
	bool escape_all = !! (flags & ESCAPE_ALL);
	bool no_quoted  = !! (flags & ESCAPE_NO_QUOTED);
    bool no_tilde = !! (flags & ESCAPE_NO_TILDE);
	
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
	
	
	out = (wchar_t *)malloc( sizeof(wchar_t)*(wcslen(in)*4 + 1));
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
            wchar_t c = *in;
			switch( c )
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
                    if (! no_tilde || c != L'~')
                    {
                        need_escape=1;
                        if( escape_all )
                            *pos++ = L'\\';
                    }
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

wcstring escape_string( const wcstring &in, escape_flags_t flags ) {
    wchar_t *tmp = escape(in.c_str(), flags);
    wcstring result(tmp);
    free(tmp);
    return result;
}

wchar_t *unescape( const wchar_t * orig, int flags )
{
	
	int mode = 0;
    int out_pos;
	size_t in_pos;
    size_t len;
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
                                    // note in_pod must be larger than 0 since we incremented it above
                                    assert(in_pos > 0);
									in_pos--;
									break;
								}								
							}
					
							for( i=0; i<chars; i++ )
							{
								long d = convert_digit( in[++in_pos],base);
								
								if( d < 0 )
								{
									in_pos--;
									break;
								}
								
								res=(res*base)|d;
							}

							if( (res <= max_val) )
							{
								in[out_pos] = (wchar_t)((byte?ENCODE_DIRECT_BASE:0)+res);
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
							{
								//We may ever escape a NULL character, but still appending a \ in case I am wrong.
								in[out_pos] = L'\\';
							}
						}
							break;	
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
								{
									//We probably don't need it since NULL character is always appended before ending this function.
									in[out_pos]=in[in_pos];
								}
							}
								break;		
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

bool unescape_string(wcstring &str, int escape_special)
{
    bool success = false;
    wchar_t *result = unescape(str.c_str(), escape_special);
    if (result) {
        str.replace(str.begin(), str.end(), result);
        free(result);
        success = true;
    }
    return success;
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

void tokenize_variable_array( const wcstring &val, std::vector<wcstring> &out)
{
    size_t pos = 0, end = val.size();
    while (pos < end) {
        size_t next_pos = val.find(ARRAY_SEP, pos);
        if (next_pos == wcstring::npos) break;
        out.push_back(val.substr(pos, next_pos - pos));
        pos = next_pos + 1; //skip the separator
    }
    out.push_back(val.substr(pos, end - pos));
}

bool string_prefixes_string(const wchar_t *proposed_prefix, const wcstring &value)
{
    size_t prefix_size = wcslen(proposed_prefix);
    return prefix_size <= value.size() && value.compare(0, prefix_size, proposed_prefix) == 0;
}


bool string_prefixes_string(const wcstring &proposed_prefix, const wcstring &value)
{
    size_t prefix_size = proposed_prefix.size();
    return prefix_size <= value.size() && value.compare(0, prefix_size, proposed_prefix) == 0;
}

bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix, const wcstring &value) {
    size_t prefix_size = proposed_prefix.size();
    return prefix_size <= value.size() && wcsncasecmp(proposed_prefix.c_str(), value.c_str(), prefix_size) == 0;
}

bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value) {
    size_t suffix_size = proposed_suffix.size();
    return suffix_size <= value.size() && value.compare(value.size() - suffix_size, suffix_size, proposed_suffix) == 0;
}

bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value) {
    size_t suffix_size = wcslen(proposed_suffix);
    return suffix_size <= value.size() && value.compare(value.size() - suffix_size, suffix_size, proposed_suffix) == 0;
}

bool list_contains_string(const wcstring_list_t &list, const wcstring &str)
{
    return std::find(list.begin(), list.end(), str) != list.end();
}

int create_directory( const wcstring &d )
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
            wcstring dir = wdirname(d);
			if( !create_directory( dir ) )
			{
				if( !wmkdir( d, 0700 ) )
				{
					ok = 1;
				}
			}
		}
	}
	
	return ok?0:-1;
}

__attribute__((noinline))
void bugreport()
{
	debug( 1,
	       _( L"This is a bug. Break on bugreport to debug."
		  L"If you can reproduce it, please send a bug report to %s." ),
		PACKAGE_BUGREPORT );
}

wcstring format_size(long long sz)
{
    wcstring result;
	const wchar_t *sz_name[]= {
			L"kB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB", 0
    };

	if( sz < 0 )
	{
		result.append( L"unknown" );
	}
	else if( sz < 1 )
	{
		result.append( _( L"empty" ) );
	}
	else if( sz < 1024 )
	{
		result.append(format_string( L"%lldB", sz ));
	}
	else
	{
		int i;
		
		for( i=0; sz_name[i]; i++ )
		{
			if( sz < (1024*1024) || !sz_name[i+1] )
			{
				long isz = ((long)sz)/1024;
				if( isz > 9 )
					result.append( format_string( L"%d%ls", isz, sz_name[i] ));
				else
					result.append( format_string( L"%.1f%ls", (double)sz/1024, sz_name[i] ));
				break;
			}
			sz /= 1024;
			
		}
	}
    return result;
}

/* Crappy function to extract the most significant digit of an unsigned long long value */
static char extract_most_significant_digit(unsigned long long *xp) {
    unsigned long long place_value = 1;
    unsigned long long x = *xp;
    while (x >= 10) {
        x /= 10;
        place_value *= 10;
    }
    *xp -= (place_value * x);
    return x + '0';
}

void append_ull(char *buff, unsigned long long val, size_t *inout_idx, size_t max_len) {
    size_t idx = *inout_idx;
    while (val > 0 && idx < max_len)
        buff[idx++] = extract_most_significant_digit(&val);
    *inout_idx = idx;
}

void append_str(char *buff, const char *str, size_t *inout_idx, size_t max_len) {
    size_t idx = *inout_idx;
    while (*str && idx < max_len)
        buff[idx++] = *str++;
    *inout_idx = idx;
}

void format_size_safe(char buff[128], unsigned long long sz) {
    const size_t buff_size = 128;
    const size_t max_len = buff_size - 1; //need to leave room for a null terminator
    bzero(buff, buff_size);
    size_t idx = 0;
	const char * const sz_name[]= {
        "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", NULL
    };
	if (sz < 1)
    {
        strncpy(buff, "empty", buff_size);
    }
    else if (sz < 1024)
    {
        append_ull(buff, sz, &idx, max_len);
        append_str(buff, "B", &idx, max_len);
    }
    else
    {
		for( size_t i=0; sz_name[i]; i++ )
		{
			if( sz < (1024*1024) || !sz_name[i+1] )
			{
				unsigned long long isz = sz/1024;
				if( isz > 9 )
                {
                    append_ull(buff, isz, &idx, max_len);
                }
				else
                {
                    if (isz == 0)
                    {
                        append_str(buff, "0", &idx, max_len);
                    }
                    else
                    {
                        append_ull(buff, isz, &idx, max_len);
                    }
                    
                    // Maybe append a single fraction digit
                    unsigned long long remainder = sz % 1024;
                    if (remainder > 0)
                    {
                        char tmp[3] = {'.', extract_most_significant_digit(&remainder), 0};
                        append_str(buff, tmp, &idx, max_len);
                    }
                }
                append_str(buff, sz_name[i], &idx, max_len);
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

void exit_without_destructors(int code) {
    _exit(code);
}

/* Helper function to convert from a null_terminated_array_t<wchar_t> to a null_terminated_array_t<char_t> */
null_terminated_array_t<char> convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &wide_arr) {
    const wchar_t *const *arr = wide_arr.get();
    if (! arr)
        return null_terminated_array_t<char>();
        
    std::vector<std::string> list;
    for (size_t i=0; arr[i]; i++) {
        list.push_back(wcs2string(arr[i]));
    }
    return null_terminated_array_t<char>(list);
}

void append_path_component(wcstring &path, const wcstring &component)
{
    if (path.empty() || component.empty()) {
        path.append(component);
    } else {
        size_t path_len = path.size();
        bool path_slash = path.at(path_len-1) == L'/';
        bool comp_slash = component.at(0) == L'/';
        if (! path_slash && ! comp_slash) {
            // Need a slash
            path.push_back(L'/');
        } else if (path_slash && comp_slash) {
            // Too many slashes
            path.erase(path_len - 1, 1);
        }
        path.append(component);
    }
}

extern "C" {
__attribute__((noinline)) void debug_thread_error(void) {     while (1) sleep(9999999); }
}

 
void set_main_thread() {
 	main_thread_id = pthread_self();
}

void configure_thread_assertions_for_testing(void) {
    thread_assertions_configured_for_testing = true;
}

/* Notice when we've forked */
static pid_t initial_pid = 0;

bool is_forked_child(void) {
    /* Just bail if nobody's called setup_fork_guards - e.g. fishd */
    if (! initial_pid) return false;
    
    bool is_child_of_fork = (getpid() != initial_pid);
    if (is_child_of_fork) {
        printf("Uh-oh: %d\n", getpid());
        while (1) sleep(10000);
    }
    return is_child_of_fork;
}

void setup_fork_guards(void) {
    /* Notice when we fork by stashing our pid. This seems simpler than pthread_atfork(). */
    initial_pid = getpid();
}

bool is_main_thread() {
 	assert (main_thread_id != 0);
 	return main_thread_id == pthread_self();
}

void assert_is_main_thread(const char *who)
{
    if (! is_main_thread() && ! thread_assertions_configured_for_testing) {
        fprintf(stderr, "Warning: %s called off of main thread. Break on debug_thread_error to debug.\n", who);
        debug_thread_error();
    }
}

void assert_is_not_forked_child(const char *who)
{
    if (is_forked_child()) {
        fprintf(stderr, "Warning: %s called in a forked child. Break on debug_thread_error to debug.\n", who);
        debug_thread_error();
    }
}

void assert_is_background_thread(const char *who)
{
    if (is_main_thread() && ! thread_assertions_configured_for_testing) {
        fprintf(stderr, "Warning: %s called on the main thread (may block!). Break on debug_thread_error to debug.\n", who);
        debug_thread_error();
    }
}

void assert_is_locked(void *vmutex, const char *who, const char *caller)
{
    pthread_mutex_t *mutex = static_cast<pthread_mutex_t*>(vmutex);
    if (0 == pthread_mutex_trylock(mutex)) {
        fprintf(stderr, "Warning: %s is not locked when it should be in '%s'. Break on debug_thread_error to debug.\n", who, caller);
        debug_thread_error();
        pthread_mutex_unlock(mutex);
    }
}

void scoped_lock::lock(void) {
    assert(! locked);
    assert(! is_forked_child());
    VOMIT_ON_FAILURE(pthread_mutex_lock(lock_obj));
    locked = true;
}

void scoped_lock::unlock(void) {
    assert(locked);
    assert(! is_forked_child());
    VOMIT_ON_FAILURE(pthread_mutex_unlock(lock_obj));
    locked = false;
}

scoped_lock::scoped_lock(pthread_mutex_t &mutex) : lock_obj(&mutex), locked(false) {
    this->lock();
}

scoped_lock::~scoped_lock() {
    if (locked) this->unlock();
}

wcstokenizer::wcstokenizer(const wcstring &s, const wcstring &separator) :
    buffer(),
    str(),
    state(),
    sep(separator)
{
    buffer = wcsdup(s.c_str());
    str = buffer;
    state = NULL;
}

bool wcstokenizer::next(wcstring &result) {
    wchar_t *tmp = wcstok(str, sep.c_str(), &state);
    str = NULL;
    if (tmp) result = tmp;
    return tmp != NULL;
}
    
wcstokenizer::~wcstokenizer() {
    free(buffer);
}
