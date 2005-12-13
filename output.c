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
#include <sys/ioctl.h>
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

#include <term.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <wchar.h>

#include "util.h"
#include "wutil.h"
#include "expand.h"
#include "common.h"
#include "output.h"
#include "highlight.h"

/**
   Number of color names in the col array
*/
#define COLORS (sizeof(col)/sizeof(wchar_t *))

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

void output_init()
{
}

void output_destroy()
{
	free( writestr_buff );
}


void set_color( int c, int c2 )
{
	static int last_color = FISH_COLOR_NORMAL;
	static int last_color2 = FISH_COLOR_NORMAL;
	int bg_set=0, last_bg_set=0;
	char *fg = 0, *bg=0;

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
		if( fg )
			writembs( tparm( fg, 0 ) );
		writembs( exit_attribute_mode );
		return;
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
		c2 != FISH_COLOR_RESET &&
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
			  mode to make reading easier
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
				writembs( tparm( fg, 0 ) );
			writembs( exit_attribute_mode );

			last_color2 = FISH_COLOR_NORMAL;
		}
		else if( ( c >= 0) && ( c < FISH_COLOR_NORMAL ) )
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

			writembs(exit_attribute_mode);
			if(( last_color != FISH_COLOR_NORMAL ) && fg )
			{
				writembs(tparm( fg, last_color ));
			}

			last_color2 = c2;
		}
		else if ((c2 >= 0 ) &&(c2 < FISH_COLOR_NORMAL))
		{
			if( bg )
			{
				writembs( tparm( bg, c2 ) );
			}
			last_color2 = c2;
		}
	}
}

int writembs( char *str )
{
#ifdef TPUTS_KLUDGE
	write( 1, str, strlen(str));
#else
	tputs(str,1,&writeb);
#endif
	return 0;
}

int writech( wint_t ch )
{
	static mbstate_t out_state;
	char buff[MB_CUR_MAX];
	size_t bytes = wcrtomb( buff, ch, &out_state );
	int err;
	
	while( (err =write( 1, buff, bytes ) ) )
	{
		if( err >= 0 )
			break;
		
		if( errno == EINTR )
			continue;
		
		wperror( L"write" );
		return 1;
	}
	
	return 0;
}

void writestr( const wchar_t *str )
{
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
		writestr_buff = realloc( writestr_buff, len );
		if( !writestr_buff )
			die_mem();
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
	write( 1, writestr_buff, strlen( writestr_buff ) );	

}


void writestr_ellipsis( const wchar_t *str, int max_width )
{
	int written=0;
	int tot = my_wcswidth(str);

	if( tot <= max_width )
	{
		writestr( str );
		return;
	}

	while( *str != 0 )
	{
		int w = wcwidth( *str );
		if( written+w+wcwidth( ellipsis_char )>max_width )
			break;
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

	wchar_t *out = escape( str, 1 );
	int i;
	int len = my_wcswidth( out );
	int written=0;

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


int writespace( int c )
{
	if( repeat_char && strlen(repeat_char) )
	{
		writembs( tparm( repeat_char, ' ', c ) );
	}
	else
	{
		write( 1, "        ", mini(c,8) );
		if( c>8)
		{
			writespace( c-8);
		}
	}
	
	return 0;
}


int output_color_code( const wchar_t *val )
{
	int i, color=-1;
	
	for( i=0; i<COLORS; i++ )
	{
		if( wcscasecmp( col[i], val ) == 0 )
		{
			color = col_idx[i];
			break;
		}
	}

	if( color >= 0 )
		return color;
	else
		return FISH_COLOR_NORMAL;
}
