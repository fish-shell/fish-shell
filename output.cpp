/** \file output.c
 Generic output functions
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <sys/time.h>
#include <unistd.h>
#include <wctype.h>

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

#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <wchar.h>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "expand.h"
#include "common.h"
#include "output.h"
#include "highlight.h"
#include "env.h"

/**
 Number of color names in the col array
 */
#define COLORS (sizeof(col)/sizeof(wchar_t *))

static int writeb_internal( char c );

/**
 Names of different colors. 
 */
static const wchar_t *col[]=
{
	L"black",
	L"red",
	L"green",
	L"brown",
	L"yellow",
	L"blue",
	L"magenta",
	L"purple",
	L"cyan",
	L"white"
	L"normal"
}
;

/**
 Mapping from color name (the 'col' array) to color index as used in
 ANSI color terminals, and also the fish_color_* constants defined
 in highlight.h. Non-ANSI terminals will display the wrong colors,
 since they use a different mapping.
 */
static const int col_idx[]=
{
	0, 
	1,
	2,
	3,
	3,
	4,
	5,
	5,
	6,
	7,
	FISH_COLOR_NORMAL,
};

/**
 The function used for output
 */

static int (*out)(char c) = &writeb_internal;

/**
 Name of terminal
 */
static wcstring current_term;

/* Whether term256 is supported */
static bool support_term256 = false;


void output_set_writer( int (*writer)(char) )
{
	CHECK( writer, );
	out = writer;
}

int (*output_get_writer())(char)
{
	return out;
}

static bool term256_support_is_native(void) {
    /* Return YES if we think the term256 support is "native" as opposed to forced. */
    return max_colors == 256;
}

bool output_get_supports_term256() {
    return support_term256;
}

void output_set_supports_term256(bool val) {
    support_term256 = val;
}

static unsigned char index_for_color(rgb_color_t c) {
    if (c.is_named() || ! output_get_supports_term256()) {
        return c.to_name_index();
    } else {
        return c.to_term256_index();
    }
}


static bool write_color(char *todo, unsigned char idx, bool is_fg) {
    bool result = false;
    if (idx < 16 || term256_support_is_native()) {
        /* Use tparm */
        writembs( tparm( todo, idx ) );
        result = true;
    } else {
        /* We are attempting to bypass the term here. Generate the ANSI escape sequence ourself. */
        char stridx[128];
        format_long_safe(stridx, idx);
        char buff[128] = "\x1b[";
        strcat(buff, is_fg ? "38;5;" : "48;5;");
        strcat(buff, stridx);
        strcat(buff, "m");
        
        int (*writer)(char) = output_get_writer();
        if (writer) {
            for (size_t i=0; buff[i]; i++) {
                writer(buff[i]);
            }
        }
        
        result = true;
    }
    return result;
}

static bool write_foreground_color(unsigned char idx) {
    if (set_a_foreground && set_a_foreground[0]) {
        return write_color(set_a_foreground, idx, true);
    } else if (set_foreground && set_foreground[0]) {
        return write_color(set_foreground, idx, true);
    } else {
        return false;
    }
}

static bool write_background_color(unsigned char idx) {
    if (set_a_background && set_a_background[0]) {
        return write_color(set_a_background, idx, false);
    } else if (set_background && set_background[0]) {
        return write_color(set_background, idx, false);
    } else {
        return false;
    }
}


void set_color(rgb_color_t c, rgb_color_t c2)
{
    
#if 0
    wcstring tmp = c.description();
    wcstring tmp2 = c2.description();
    printf("set_color %ls : %ls\n", tmp.c_str(), tmp2.c_str());
#endif    
    ASSERT_IS_MAIN_THREAD();
    
    const rgb_color_t normal = rgb_color_t::normal();
    static rgb_color_t last_color = rgb_color_t::normal();
	static rgb_color_t last_color2 = rgb_color_t::normal();
	static int was_bold=0;
	static int was_underline=0;
	int bg_set=0, last_bg_set=0;
    
	int is_bold = 0;
	int is_underline = 0;
    
	/*
     Test if we have at least basic support for setting fonts, colors
     and related bits - otherwise just give up...
     */
	if( !exit_attribute_mode )
	{
		return;
	}
	
    
	is_bold |= c.is_bold();
	is_bold |= c2.is_bold();
    
	is_underline |= c.is_underline();
	is_underline |= c2.is_underline();
    
	if( c.is_reset() || c2.is_reset())
	{
		c = c2 = normal;
		was_bold=0;
		was_underline=0;
        /*
         If we exit attibute mode, we must first set a color, or
         previously coloured text might lose it's
         color. Terminals are weird...
         */
        write_foreground_color(0);
		writembs( exit_attribute_mode );
		return;
	}
    
	if( was_bold && !is_bold )
	{
		/*
         Only way to exit bold mode is a reset of all attributes. 
         */
		writembs( exit_attribute_mode );
		last_color = normal;
		last_color2 = normal;
		was_bold=0;
		was_underline=0;
	}
	
	if( ! last_color2.is_normal() &&
       ! last_color2.is_reset() &&
       ! last_color2.is_ignore() )
	{
		/*
         Background was set
         */
		last_bg_set=1;
	}
    
	if( ! c2.is_normal() &&
       ! c2.is_ignore())
	{
		/*
         Background is set
         */
		bg_set=1;
        if ( c==c2 )
            c = (c2==rgb_color_t::white())?rgb_color_t::black():rgb_color_t::white();
	}
    
	if( (enter_bold_mode != 0) && (strlen(enter_bold_mode) > 0))
	{
		if(bg_set && !last_bg_set)
		{
			/*
             Background color changed and is set, so we enter bold
             mode to make reading easier. This means bold mode is
             _always_ on when the background color is set.
             */
			writembs( enter_bold_mode );
		}
		if(!bg_set && last_bg_set)
		{
			/*
             Background color changed and is no longer set, so we
             exit bold mode
             */
			writembs( exit_attribute_mode );
			was_bold=0;
			was_underline=0;
			/*
             We don't know if exit_attribute_mode resets colors, so
             we set it to something known.
             */
			if( write_foreground_color(0))
			{
				last_color=rgb_color_t::black();
			}
		}
	}
    
	if( last_color != c )
	{
		if( c.is_normal() )
		{
			write_foreground_color(0);
			writembs( exit_attribute_mode );
            
			last_color2 = rgb_color_t::normal();
			was_bold=0;
			was_underline=0;
		}
		else if( ! c.is_special() )
		{
            write_foreground_color(index_for_color(c));
		}
	}
    
	last_color = c;
    
	if( last_color2 != c2 )
	{
		if( c2.is_normal() )
		{
            write_background_color(0);
            
			writembs( exit_attribute_mode );
			if( ! last_color.is_normal())
			{
                write_foreground_color(index_for_color(last_color));
			}
			
            
			was_bold=0;
			was_underline=0;
			last_color2 = c2;
		}
		else if ( ! c2.is_special() )
		{
            write_background_color(index_for_color(c2));
			last_color2 = c2;
		}
	}
    
	/*
     Lastly, we set bold mode and underline mode correctly
     */
	if( (enter_bold_mode != 0) && (strlen(enter_bold_mode) > 0) && !bg_set )
	{
		if( is_bold && !was_bold )
		{
			if( enter_bold_mode )
			{
				writembs( tparm( enter_bold_mode ) );
			}
		}
		was_bold = is_bold;	
	}
    
	if( was_underline && !is_underline )
	{
		writembs( exit_underline_mode );		
	}
	
	if( !was_underline && is_underline )
	{
		writembs( enter_underline_mode );		
	}
	was_underline = is_underline;
	
}

/**
 Default output method, simply calls write() on stdout
 */
static int writeb_internal( char c )
{
	write_loop( 1, &c, 1 );
	return 0;
}

int writeb( tputs_arg_t b )
{
	out( b );
	return 0;
}

int writembs_internal( char *str )
{
	CHECK( str, 1 );
    
	return tputs(str,1,&writeb)==ERR?1:0;
}

int writech( wint_t ch )
{
	mbstate_t state;
	size_t i;
	char buff[MB_LEN_MAX+1];
	size_t bytes;
    
	if( ( ch >= ENCODE_DIRECT_BASE) &&
       ( ch < ENCODE_DIRECT_BASE+256) )
	{
		buff[0] = ch - ENCODE_DIRECT_BASE;
		bytes=1;
	}
	else
	{
		memset( &state, 0, sizeof(state) );
		bytes= wcrtomb( buff, ch, &state );
		
		switch( bytes )
		{
			case (size_t)(-1):
			{
				return 1;
			}
		}
	}
	
	for( i=0; i<bytes; i++ )
	{
		out( buff[i] );
	}
	return 0;
}

void writestr( const wchar_t *str )
{
	char *pos;
    
	CHECK( str, );
	
    //	while( *str )
    //		writech( *str++ );
    
	/*
     Check amount of needed space
     */
	size_t len = wcstombs( 0, str, 0 );
    
	if( len == (size_t)-1 )
	{
		debug( 1, L"Tried to print invalid wide character string" );
		return;
	}
	
	len++;
    
	/*
     Convert
     */
    char *buffer, static_buffer[256];
    if (len <= sizeof static_buffer)
        buffer = static_buffer;
    else
        buffer = new char[len];
    
	wcstombs( buffer, 
             str,
             len );
    
	/*
     Write
     */
	for( pos = buffer; *pos; pos++ )
	{
		out( *pos );
	}
    
    if (buffer != static_buffer)
        delete[] buffer;
}


void writestr_ellipsis( const wchar_t *str, int max_width )
{
	int written=0;
	int tot;
    
	CHECK( str, );
	
	tot = my_wcswidth(str);
    
	if( tot <= max_width )
	{
		writestr( str );
		return;
	}
    
	while( *str != 0 )
	{
		int w = fish_wcwidth( *str );
		if( written+w+fish_wcwidth( ellipsis_char )>max_width )
		{
			break;
		}
		written+=w;
		writech( *(str++) );
	}
    
	written += fish_wcwidth( ellipsis_char );
	writech( ellipsis_char );
    
	while( written < max_width )
	{
		written++;
		writestr( L" " );
	}
}

int write_escaped_str( const wchar_t *str, int max_len )
{
    
	wchar_t *out;
	int i;
	int len;
	int written=0;
    
	CHECK( str, 0 );
	
	out = escape( str, 1 );
	len = my_wcswidth( out );
    
	if( max_len && (max_len < len))
	{
		for( i=0; (written+fish_wcwidth(out[i]))<=(max_len-1); i++ )
		{
			writech( out[i] );
			written += fish_wcwidth( out[i] );
		}
		writech( ellipsis_char );
		written += fish_wcwidth( ellipsis_char );
		
		for( i=written; i<max_len; i++ )
		{
			writech( L' ' );
			written++;
		}
	}
	else
	{
		written = len;
		writestr( out );
	}
    
	free( out );
	return written;
}


int output_color_code( const wcstring &val, bool is_background ) {
	size_t i;
    int color=FISH_COLOR_NORMAL;
	int is_bold=0;
	int is_underline=0;
	
	if (val.empty())
		return FISH_COLOR_NORMAL;
	
    wcstring_list_t el;
	tokenize_variable_array( val, el );
	
	for(size_t j=0; j < el.size(); j++ ) {
        const wcstring &next = el.at(j);
        wcstring color_name;
        if (is_background) {
            // look for something like "--background=red"
            const wcstring prefix = L"--background=";
            if (string_prefixes_string(prefix, next)) {
                color_name = wcstring(next, prefix.size());
            }
            
        } else {
            if (next == L"--bold" || next == L"-o")
                is_bold = true;
            else if (next == L"--underline" || next == L"-u")
                is_underline = true;
            else
                color_name = next;
        }
        
        if (! color_name.empty()) {
            for( i=0; i<COLORS; i++ )
            {
                if( wcscasecmp( col[i], color_name.c_str() ) == 0 )
                {
                    color = col_idx[i];
                    break;
                }
            }
        }
		
	}
	
	return color | (is_bold?FISH_COLOR_BOLD:0) | (is_underline?FISH_COLOR_UNDERLINE:0);	
}

rgb_color_t parse_color( const wcstring &val, bool is_background ) {
    int is_bold=0;
	int is_underline=0;
    
    std::vector<rgb_color_t> candidates;
    
    wcstring_list_t el;
	tokenize_variable_array( val, el );
	
	for(size_t j=0; j < el.size(); j++ ) {
        const wcstring &next = el.at(j);
        wcstring color_name;
        if (is_background) {
            // look for something like "--background=red"
            const wcstring prefix = L"--background=";
            if (string_prefixes_string(prefix, next)) {
                color_name = wcstring(next, prefix.size());
            }
        } else {
            if (next == L"--bold" || next == L"-o")
                is_bold = true;
            else if (next == L"--underline" || next == L"-u")
                is_underline = true;
            else
                color_name = next;
        }
        
        if (! color_name.empty()) {
            rgb_color_t color = rgb_color_t(color_name);
            if (! color.is_none()) {
                candidates.push_back(color);
            }
        }
    }
    
    // Pick the best candidate
    rgb_color_t first_rgb = rgb_color_t::none(), first_named = rgb_color_t::none();
    for (size_t i=0; i < candidates.size(); i++) {
        const rgb_color_t &color = candidates.at(i);
        if (color.is_rgb() && first_rgb.is_none())
            first_rgb = color;
        if (color.is_named() && first_named.is_none())
            first_named = color;
    }
    
    // If we have both RGB and named colors, then prefer rgb if term256 is supported
    rgb_color_t result;
    if ((!first_rgb.is_none() && output_get_supports_term256()) || first_named.is_none()) {
        result = first_rgb;
    } else {
        result = first_named;
    }
    
    if (result.is_none())
        result = rgb_color_t::normal();
    
    result.set_bold(is_bold);
    result.set_underline(is_underline);
    
#if 0
    wcstring desc = result.description();
    printf("Parsed %ls from %ls (%s)\n", desc.c_str(), val.c_str(), is_background ? "background" : "foreground");
#endif
    
    return result;
}

void output_set_term( const wchar_t *term )
{
	current_term = term;
}

const wchar_t *output_get_term()
{
    return current_term.empty() ? L"<unknown>" : current_term.c_str();
}
