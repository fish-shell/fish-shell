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
#include "halloc_util.h"
#include "highlight.h"

/**
   Number of color names in the col array
*/
#define COLORS (sizeof(col)/sizeof(wchar_t *))

static int writeb_internal( char c );

/**
   Names of different colors. 
*/
static wchar_t *col[]=
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
static int col_idx[]=
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
}
	;

/**
  Size of writestr_buff
*/
static size_t writestr_buff_sz=0;	

/**
   Temp buffer used for converting from wide to narrow strings
*/
static char *writestr_buff = 0;

/**
   The function used for output
*/

static int (*out)(char c) = &writeb_internal;

/**
   Name of terminal
 */
static wchar_t *current_term = 0;


/**
   Cleanup function. Run automatically through halloc
*/
static void output_destroy()
{
	free( writestr_buff );
}

void output_set_writer( int (*writer)(char) )
{
	CHECK( writer, );
	out = writer;
}

int (*output_get_writer())(char)
{
	return out;
}


void set_color( int c, int c2 )
{
	static int last_color = FISH_COLOR_NORMAL;
	static int last_color2 = FISH_COLOR_NORMAL;
	static int was_bold=0;
	static int was_underline=0;
	int bg_set=0, last_bg_set=0;
	char *fg = 0, *bg=0;

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
	

	is_bold |= (c&FISH_COLOR_BOLD)!=0;
	is_bold |= (c2&FISH_COLOR_BOLD)!=0;

	is_underline |= (c&FISH_COLOR_UNDERLINE)!=0;
	is_underline |= (c2&FISH_COLOR_UNDERLINE)!=0;

	c = c&(~(FISH_COLOR_BOLD|FISH_COLOR_UNDERLINE));
	c2 = c2&(~(FISH_COLOR_BOLD|FISH_COLOR_UNDERLINE));

	if( (set_a_foreground != 0) && (strlen( set_a_foreground) != 0 ) )
	{
		fg = set_a_foreground;
		bg = set_a_background;
	}
	else if( (set_foreground != 0) && (strlen( set_foreground) != 0 ) )
	{
		fg = set_foreground;
		bg = set_background;
	}

	if( (c == FISH_COLOR_RESET) || (c2 == FISH_COLOR_RESET))
	{
		c = c2 = FISH_COLOR_NORMAL;
		was_bold=0;
		was_underline=0;
		if( fg )
		{
			/*
			  If we exit attibute mode, we must first set a color, or
			  previously coloured text might lose it's
			  color. Terminals are weird...
			*/
			writembs( tparm( fg, 0 ) );
		}
		writembs( exit_attribute_mode );
		return;
	}

	if( was_bold && !is_bold )
	{
		/*
		  Only way to exit bold mode is a reset of all attributes. 
		*/
		writembs( exit_attribute_mode );
		last_color = FISH_COLOR_NORMAL;
		last_color2 = FISH_COLOR_NORMAL;
		was_bold=0;
		was_underline=0;
	}
	
	if( last_color2 != FISH_COLOR_NORMAL &&
		last_color2 != FISH_COLOR_RESET &&
		last_color2 != FISH_COLOR_IGNORE )
	{
		/*
		  Background was set
		*/
		last_bg_set=1;
	}

	if( c2 != FISH_COLOR_NORMAL &&
		c2 != FISH_COLOR_IGNORE )
	{
		/*
		  Background is set
		*/
		bg_set=1;
		c = (c2==FISH_COLOR_WHITE)?FISH_COLOR_BLACK:FISH_COLOR_WHITE;
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
			if( fg )
			{
				writembs( tparm( fg, 0 ) );
				last_color=0;
			}
		}
	}

	if( last_color != c )
	{
		if( c==FISH_COLOR_NORMAL )
		{
			if( fg )
			{
				writembs( tparm( fg, 0 ) );
			}
			writembs( exit_attribute_mode );

			last_color2 = FISH_COLOR_NORMAL;
			was_bold=0;
			was_underline=0;
		}
		else if( ( c >= 0 ) && ( c < FISH_COLOR_NORMAL ) )
		{
			if( fg )
			{
				writembs( tparm( fg, c ) );
			}
		}
	}

	last_color = c;

	if( last_color2 != c2 )
	{
		if( c2 == FISH_COLOR_NORMAL )
		{
			if( bg )
			{
				writembs( tparm( bg, 0 ) );
			}

			writembs( exit_attribute_mode );
			if( ( last_color != FISH_COLOR_NORMAL ) && fg )
			{
				if( fg )
				{
					writembs( tparm( fg, last_color ) );
				}
			}
			

			was_bold=0;
			was_underline=0;
			last_color2 = c2;
		}
		else if ( ( c2 >= 0 ) && ( c2 < FISH_COLOR_NORMAL ) )
		{
			if( bg )
			{
				writembs( tparm( bg, c2 ) );
			}
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
	int i;
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
	  Reallocate if needed
	*/
	if( writestr_buff_sz < len )
	{
		if( !writestr_buff )
		{
			halloc_register_function_void( global_context, &output_destroy );
		}
		
		writestr_buff = realloc( writestr_buff, len );
		if( !writestr_buff )
		{
			DIE_MEM();
		}
		writestr_buff_sz = len;
	}
	
	/*
	  Convert
	*/
	wcstombs( writestr_buff, 
			  str,
			  writestr_buff_sz );

	/*
	  Write
	*/
	for( pos = writestr_buff; *pos; pos++ )
	{
		out( *pos );
	}
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
		int w = wcwidth( *str );
		if( written+w+wcwidth( ellipsis_char )>max_width )
		{
			break;
		}
		written+=w;
		writech( *(str++) );
	}

	written += wcwidth( ellipsis_char );
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
		for( i=0; (written+wcwidth(out[i]))<=(max_len-1); i++ )
		{
			writech( out[i] );
			written += wcwidth( out[i] );
		}
		writech( ellipsis_char );
		written += wcwidth( ellipsis_char );
		
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


int output_color_code( const wchar_t *val )
{
	int j, i, color=FISH_COLOR_NORMAL;
	array_list_t el;
	int is_bold=0;
	int is_underline=0;
	
	if( !val )
		return FISH_COLOR_NORMAL;
	
	al_init( &el );
	tokenize_variable_array( val, &el );
	
	for( j=0; j<al_get_count( &el ); j++ )
	{
		wchar_t *next = (wchar_t *)al_get( &el, j );
		
		is_bold |= (wcsncmp( next, L"--bold", wcslen(next) ) == 0 ) && wcslen(next)>=3;
		is_bold |= wcscmp( next, L"-o" ) == 0;

		is_underline |= (wcsncmp( next, L"--underline", wcslen(next) ) == 0 ) && wcslen(next)>=3;
		is_underline |= wcscmp( next, L"-u" ) == 0;

		for( i=0; i<COLORS; i++ )
		{
			if( wcscasecmp( col[i], next ) == 0 )
			{
				color = col_idx[i];
				break;
			}
		}
		
	}
	
	al_foreach( &el, &free );
	al_destroy( &el );

	return color | (is_bold?FISH_COLOR_BOLD:0) | (is_underline?FISH_COLOR_UNDERLINE:0);
	
}

void output_set_term( wchar_t *term )
{
	current_term = halloc_wcsdup(global_context, term);
}

wchar_t *output_get_term()
{
	return current_term ? current_term : L"<unknown>";
}


