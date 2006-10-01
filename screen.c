#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

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

#include <wchar.h>

#include <assert.h>


#include "fallback.h"
#include "common.h"
#include "util.h"
#include "wutil.h"
#include "output.h"
#include "highlight.h"
#include "screen.h"

static buffer_t *s_writeb_buffer=0;

/**
   Tests if the specified narrow character sequence is present at the
   specified position of the specified wide character string. All of
   \c seq must match, but str may be longer than seq.
*/
static int try_sequence( char *seq, wchar_t *str )
{
	int i;

	for( i=0;; i++ )
	{
		if( !seq[i] )
			return i;

		if( seq[i] != str[i] )
			return 0;
	}

	return 0;
}

/**
   Calculate the width of the specified prompt. Does some clever magic
   to detect common escape sequences that may be embeded in a prompt,
   such as color codes.
*/
static int calc_prompt_width( wchar_t *prompt )
{
	int res = 0;
	int j, k;

	for( j=0; prompt[j]; j++ )
	{
		if( prompt[j] == L'\e' )
		{
			/*
			  This is the start of an escape code. Try to guess it's width.
			*/
				int l;
				int len=0;
				int found = 0;

				/*
				  Detect these terminfo color escapes with parameter
				  value 0..7, all of which don't move the cursor
				*/
				char * esc[] =
					{
						set_a_foreground,
						set_a_background,
						set_foreground,
						set_background,
					}
				;

				/*
				  Detect these semi-common terminfo escapes without any
				  parameter values, all of which don't move the cursor
				*/
				char *esc2[] =
					{
						enter_bold_mode,
						exit_attribute_mode,
						enter_underline_mode,
						exit_underline_mode,
						enter_standout_mode,
						exit_standout_mode,
						flash_screen,
						enter_subscript_mode,
						exit_subscript_mode,
						enter_superscript_mode,
						exit_superscript_mode,
						enter_blink_mode,
						enter_italics_mode,
						exit_italics_mode,
						enter_reverse_mode,
						enter_shadow_mode,
						exit_shadow_mode,
						enter_standout_mode,
						exit_standout_mode,
						enter_secure_mode
					}
				;

				for( l=0; l < (sizeof(esc)/sizeof(char *)) && !found; l++ )
				{
					if( !esc[l] )
						continue;

					for( k=0; k<8; k++ )
					{
						len = try_sequence( tparm(esc[l],k), &prompt[j] );
						if( len )
						{
							j += (len-1);
							found = 1;
							break;
						}
					}
				}

				for( l=0; l < (sizeof(esc2)/sizeof(char *)) && !found; l++ )
				{
					if( !esc2[l] )
						continue;
					/*
					  Test both padded and unpadded version, just to
					  be safe. Most versions of tparm don't actually
					  seem to do anything these days.
					*/
					len = maxi( try_sequence( tparm(esc2[l]), &prompt[j] ),
								try_sequence( esc2[l], &prompt[j] ));
					
					if( len )
					{
						j += (len-1);
						found = 1;
					}
				}
			}
			else if( prompt[j] == L'\t' )
			{
				/*
				  Assume tab stops every 8 characters if undefined
				*/
				if( init_tabs <= 0 )
					init_tabs = 8;
				
				res=( (res/init_tabs)+1 )*init_tabs;
			}
			else
			{
				/*
				  Ordinary decent character. Just add width.
				*/
				res += wcwidth( prompt[j] );
			}
		}
	return res;
}



static void free_line( void *l )
{
	line_t *line = (line_t *)l;
//	debug( 0, L"Free line at %d", l);
	al_destroy( &line->text );
	al_destroy( &line->color );
	free( line );
}


static void s_reset_arr( array_list_t *l )
{
//	debug( 0, L"I have %d lines", al_get_count( l ));
	
	al_foreach( l, &free_line );
	al_truncate( l, 0 );
}

void s_init( screen_t *s )
{
	memset( s, 0, sizeof(screen_t));
	sb_init( &s->prompt_buff );
}


void s_destroy( screen_t *s )
{
	s_reset_arr( &s->screen );
	al_destroy( &s->screen );
	s_reset_arr( &s->output );
	al_destroy( &s->output );
}

static line_t *s_create_line()
{
	line_t *current = malloc( sizeof( line_t ));
	al_init( &current->text );
	al_init( &current->color );
	return current;
}

/*
  Appends a character to the end of the line that the output cursor is on
*/
static void s_output_append_char( screen_t *s, wchar_t b, int c, int prompt_width )
{
	int line_no = s->output_cursor[1];
	
	switch( b )
	{
		case L'\n':
		{
			int i;
			line_t *current = s_create_line();
			al_push( &s->output, current );
			s->output_cursor[1]++;
			s->output_cursor[0]=0;
			for( i=0; i < prompt_width; i++ )
			{
				s_output_append_char( s, L' ', 0, prompt_width );
			}
			break;
		}
		
		case L'\r':
		{
			line_t *current;
			current = (line_t *)al_get( &s->output, line_no );
			al_truncate( &current->text, 0 );
			al_truncate( &current->color, 0 );			
			s->output_cursor[0]=0;
			break;			
		}		

		default:
		{
			line_t *current;
			current = (line_t *)al_get( &s->output, line_no );

			if( !current )
			{
				current = s_create_line();
				al_push( &s->output, current );
			}
			
			al_push_long( &current->text, b );
			al_push_long( &current->color, c );
			s->output_cursor[0]+= wcwidth(b);
			break;
		}
	}
	
}

static int s_writeb( char c )
{
	b_append( s_writeb_buffer, &c, 1 );
	return 0;
}


static void s_move( screen_t *s, buffer_t *b, int new_x, int new_y )
{
	int i;
	int x_steps, y_steps;
	
	int (*writer_old)(char) = output_get_writer();

	char *str;
/*
	debug( 0, L"move from %d %d to %d %d", 
		   s->screen_cursor[0], s->screen_cursor[1],  
		   new_x, new_y );
*/
	output_set_writer( &s_writeb );
	s_writeb_buffer = b;
	
	y_steps = new_y - s->screen_cursor[1];

	if( y_steps > 0 && (strcmp( cursor_down, "\n")==0))
	{	
		/*
		  This is very strange - it seems all (most) consoles use a
		  simple newline as the cursor down escape. This will of
		  course move the cursor to the beginning of the line as
		  well. The cursor_up does not have this behaviour...
		*/
		s->screen_cursor[0]=0;
	}
	
	if( y_steps < 0 )
	{
		str = cursor_up;
	}
	else
	{
		str = cursor_down;
		
	}
	
	for( i=0; i<abs(y_steps); i++)
	{
		writembs(str);
	}


	x_steps = new_x - s->screen_cursor[0];
	
	if( x_steps && new_x == 0 )
	{
		char c = '\r';
		b_append( b, &c, 1 );
		x_steps = 0;
//		debug( 0, L"return to first" );		
	}
		
	if( x_steps < 0 ){
		str = cursor_left;
	}
	else
	{
		str = cursor_right;
	}
	
	for( i=0; i<abs(x_steps); i++)
	{
		writembs(str);
	}


	s->screen_cursor[0] = new_x;
	s->screen_cursor[1] = new_y;
	
	output_set_writer( writer_old );
	
}

static void s_set_color( screen_t *s, buffer_t *b, int c )
{
	
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;
	
	set_color( highlight_get_color( c & 0xffff ),
			   highlight_get_color( (c>>16)&0xffff ) );
	
	output_set_writer( writer_old );
	
}

static void s_write_char( screen_t *s, buffer_t *b, wchar_t c )
{
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;
	s->screen_cursor[0]+=wcwidth( c );
	
	writech( c );
	
	output_set_writer( writer_old );
}

static void s_write_mbs( buffer_t *b, char *s )
{
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;

	writembs( s );
	
	output_set_writer( writer_old );
}


static void s_write_str( buffer_t *b, wchar_t *s )
{
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;

	writestr( s );
	
	output_set_writer( writer_old );
}


static void s_update( screen_t *scr, wchar_t *prompt )
{
	int i, j;
	int prompt_width = calc_prompt_width( prompt );
	
	buffer_t output;
	b_init( &output );

	if( wcscmp( prompt, (wchar_t *)scr->prompt_buff.buff ) )
	{
		s_move( scr, &output, 0, 0 );
		s_write_str( &output, prompt );
		sb_clear( &scr->prompt_buff );
		sb_append( &scr->prompt_buff, prompt );		
//		debug( 0, L"YAY %d", prompt_width );
		scr->screen_cursor[0] = prompt_width;
	}
	
	for( i=0; i< al_get_count( &scr->output ); i++ )
	{
		line_t *o_line = (line_t *)al_get( &scr->output, i );
		line_t *s_line = (line_t *)al_get( &scr->screen, i );
		
		if( !s_line )
		{
			s_line = s_create_line();
			al_push( &scr->screen, s_line );
		}
		for( j=(i==0?prompt_width:0); j<al_get_count( &o_line->text ); j++ )
		{
			wchar_t o = (wchar_t)al_get( &o_line->text, j );
			int o_c = (int)al_get( &o_line->color, j );
			if( al_get_count( &s_line->text ) == j )
			{
				s_move( scr, &output, j, i );
				s_set_color( scr, &output, o_c );
				s_write_char( scr, &output, o );
				al_set_long( &s_line->text, j, o );
				al_set_long( &s_line->color, j, o_c );				
			}
			else
			{
				wchar_t s = (wchar_t)al_get( &s_line->text, j );
				int s_c = (int)al_get( &s_line->color, j );
				if( o != s || o_c != s_c )
				{
					s_move( scr, &output, j, i );
					s_set_color( scr, &output, o_c );
					s_write_char( scr, &output, o );
					al_set_long( &s_line->text, j, o );
					al_set_long( &s_line->color, j, o_c );
				}
			}
		}
//		debug( 0, L"frum frum %d %d", al_get_count( &o_line->text ), al_get_count( &s_line->text ) );
		if( al_get_count( &s_line->text ) > al_get_count( &o_line->text ) )
		{
			s_move( scr, &output, al_get_count( &o_line->text ), i );
			s_write_mbs( &output, clr_eol);
			al_truncate( &s_line->text, al_get_count( &o_line->text ) );
//			debug( 0, L"YAY DELETE from %d", al_get_count( &o_line->text ) );			
		}
		
	}
	for( i=al_get_count( &scr->output ); i< al_get_count( &scr->screen ); i++ )
	{
		line_t *s_line = (line_t *)al_get( &scr->screen, i );
		s_move( scr, &output, 0, i );
		s_write_mbs( &output, clr_eol);
		al_truncate( &s_line->text, 0 );
	}
	
	s_move( scr, &output, scr->output_cursor[0], scr->output_cursor[1] );
	
	s_set_color( scr, &output, 0xffffffff);

	if( output.used )
	{
		write( 1, output.buff, output.used );
	}
	
	b_destroy( &output );
	
}


void s_write( screen_t *s, wchar_t *prompt, wchar_t *b, int *c, int cursor, int flags )
{
	int i;
	int cursor_arr[2];

	int prompt_width = calc_prompt_width( prompt );

//	debug( 0, L"Prompt width is %d", prompt_width );
	
	s_reset_arr( &s->output );
	s->output_cursor[0] = s->output_cursor[1] = 0;
	
	for( i=0; i<prompt_width; i++ )
	{
		s_output_append_char( s, L' ', 0, prompt_width );
	}
	
	for( i=0; b[i]; i++ )
	{
		int col = c[i];
		
		if( i == cursor )
		{
			memcpy(cursor_arr, s->output_cursor, sizeof(int)*2);
			col = 0;
		}

		s_output_append_char( s, b[i], col, prompt_width );
		
	}
	if( i == cursor )
	{
		memcpy(cursor_arr, s->output_cursor, sizeof(int)*2);
	}
	
	memcpy( s->output_cursor, cursor_arr, sizeof(int)*2 );
	s_update( s, prompt );
	
}

void s_reset( screen_t *s )
{
	s_reset_arr( &s->screen );
	s->screen_cursor[0] = s->screen_cursor[1] = 0;
	sb_clear( &s->prompt_buff );
}

