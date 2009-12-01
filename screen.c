/** \file screen.c High level library for handling the terminal screen

The screen library allows the interactive reader to write its
output to screen efficiently by keeping an inetrnal representation
of the current screen contents and trying to find the most
efficient way for transforming that to the desired screen content.
*/

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
#include <time.h>

#include <assert.h>


#include "fallback.h"
#include "common.h"
#include "util.h"
#include "wutil.h"
#include "output.h"
#include "highlight.h"
#include "screen.h"
#include "env.h"

/**
   The number of characters to indent new blocks
 */
#define INDENT_STEP 4

/**
   Ugly kludge. The internal buffer used to store output of
   tputs. Since tputs external function can only take an integer and
   not a pointer as parameter we need a static storage buffer.
*/
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
   Returns the number of columns left until the next tab stop, given
   the current cursor postion.
 */
static int next_tab_stop( int in )
{
	/*
	  Assume tab stops every 8 characters if undefined
	*/
	if( init_tabs <= 0 )
		init_tabs = 8;
				
	return ( (in/init_tabs)+1 )*init_tabs;
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
		if( prompt[j] == L'\x1b' )
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
				
			if( !found )
			{
				if( prompt[j+1] == L'k' )
				{
					wchar_t *term_name = env_get( L"TERM" );
					if( term_name && wcsstr( term_name, L"screen" ) == term_name )
					{
						wchar_t *end;
						j+=2;
						found = 1;
						end = wcsstr( &prompt[j], L"\x1b\\" );
						if( end )
						{
							/*
							  You'd thing this should be
							  '(end-prompt)+2', in order to move j
							  past the end of the string, but there is
							  a 'j++' at the end of each lap, so j
							  should always point to the last menged
							  character, e.g. +1.
							*/
							j = (end-prompt)+1;
						}
						else
						{
							break;
						}
					}						
				}					
			}
				
		}
		else if( prompt[j] == L'\t' )
		{
			res = next_tab_stop( res );
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

/**
   Test if there is space between the time fields of struct stat to
   use for sub second information. If so, we assume this space
   contains the desired information.
*/
static int room_for_usec(struct stat *st)
{
	int res = ((&(st->st_atime) + 2) == &(st->st_mtime) &&
		   (&(st->st_atime) + 4) == &(st->st_ctime));
	return res;
}

/**
   Stat stdout and stderr and save result.

   This should be done before calling a function that may cause output.
*/

static void s_save_status( screen_t *s)
{

	/*
	  This futimes call tries to trick the system into using st_mtime
	  as a tampering flag. This of course only works on systems where
	  futimes is defined, but it should make the status saving stuff
	  failsafe.
	*/
	struct timeval t[]=
		{
			{
				time(0)-1,
				0
			}
			,
			{
				time(0)-1,
				0
			}
		}
	;

	/*
	  Don't check return value on these. We don't care if they fail,
	  really.  This is all just to make the prompt look ok, which is
	  impossible to do 100% reliably. We try, at least.
	*/
	futimes( 1, t );
	futimes( 2, t );

	fstat( 1, &s->prev_buff_1 );
	fstat( 2, &s->prev_buff_2 );
}

/**
   Stat stdout and stderr and compare result to previous result in
   reader_save_status. Repaint if modification time has changed.

   Unfortunately, for some reason this call seems to give a lot of
   false positives, at least under Linux.
*/

static void s_check_status( screen_t *s)
{
	fflush( stdout );
	fflush( stderr );

	fstat( 1, &s->post_buff_1 );
	fstat( 2, &s->post_buff_2 );

	int changed = ( s->prev_buff_1.st_mtime != s->post_buff_1.st_mtime ) ||
		( s->prev_buff_2.st_mtime != s->post_buff_2.st_mtime );

	if (room_for_usec( &s->post_buff_1))
	{
		changed = changed || ( (&s->prev_buff_1.st_mtime)[1] != (&s->post_buff_1.st_mtime)[1] ) ||
			( (&s->prev_buff_2.st_mtime)[1] != (&s->post_buff_2.st_mtime)[1] );
	}

	if( changed )
	{
		/*
		  Ok, someone has been messing with our screen. We will want
		  to repaint. However, we do not know where the cursor is. It
		  is our best bet that we are still on the same line, so we
		  move to the beginning of the line, reset the modelled screen
		  contents, and then set the modeled cursor y-pos to its
		  earlier value. 
		*/
		
		int prev_line = s->actual_cursor[1];
		write_loop( 1, "\r", 1 );
		s_reset( s, 0 );
		s->actual_cursor[1] = prev_line;
	}
}

/**
   Free all memory used by one line_t struct. 
*/
static void free_line( void *l )
{
	line_t *line = (line_t *)l;
	al_destroy( &line->text );
	al_destroy( &line->color );
	free( line );
}

/**
   Clear the specified array of line_t structs.
*/
static void s_reset_arr( array_list_t *l )
{
	al_foreach( l, &free_line );
	al_truncate( l, 0 );
}

void s_init( screen_t *s )
{
	CHECK( s, );
	
	memset( s, 0, sizeof(screen_t));
	sb_init( &s->actual_prompt );
}


void s_destroy( screen_t *s )
{
	CHECK( s, );

	s_reset_arr( &s->actual );
	al_destroy( &s->actual );
	s_reset_arr( &s->desired );
	al_destroy( &s->desired );
	sb_destroy( &s->actual_prompt );		
}

/**
   Allocate a new line_t struct.
*/
static line_t *s_create_line()
{
	line_t *current = malloc( sizeof( line_t ));
	al_init( &current->text );
	al_init( &current->color );
	return current;
}

/**
   Appends a character to the end of the line that the output cursor is
   on. This function automatically handles linebreaks and lines longer
   than the screen width. 
*/
static void s_desired_append_char( screen_t *s, 
				   wchar_t b,
				   int c, 
				   int indent,
				   int prompt_width )
{
	int line_no = s->desired_cursor[1];
	
	switch( b )
	{
		case L'\n':
		{
			int i;
			line_t *current = s_create_line();
			al_push( &s->desired, current );
			s->desired_cursor[1]++;
			s->desired_cursor[0]=0;
			for( i=0; i < prompt_width+indent*INDENT_STEP; i++ )
			{
				s_desired_append_char( s, L' ', 0, indent, prompt_width );
			}
			break;
		}
		
		case L'\r':
		{
			line_t *current;
			current = (line_t *)al_get( &s->desired, line_no );
			al_truncate( &current->text, 0 );
			al_truncate( &current->color, 0 );			
			s->desired_cursor[0]=0;
			break;			
		}		

		default:
		{
			line_t *current;
			int screen_width = common_get_width();
			int cw = wcwidth(b);
			int ew = wcwidth( ellipsis_char );
			int i;
			
			current = (line_t *)al_get( &s->desired, line_no );
			
			if( !current )
			{
				current = s_create_line();
				al_push( &s->desired, current );
			}

			/*
			  Check if we are at the end of the line. If so, print an
			  ellipsis character and continue on the next line.
			*/
			if( s->desired_cursor[0] + cw + ew > screen_width )
			{
				al_set_long( &current->text, s->desired_cursor[0], ellipsis_char );
				al_set_long( &current->color, s->desired_cursor[0], HIGHLIGHT_COMMENT );
								
				current = s_create_line();
				al_push( &s->desired, current );
				s->desired_cursor[1]++;
				s->desired_cursor[0]=0;
				for( i=0; i < (prompt_width-ew); i++ )
				{
					s_desired_append_char( s, L' ', 0, indent, prompt_width );
				}				
				s_desired_append_char( s, ellipsis_char, HIGHLIGHT_COMMENT, indent, prompt_width );
			}
			
			al_set_long( &current->text, s->desired_cursor[0], b );
			al_set_long( &current->color, s->desired_cursor[0], c );
			s->desired_cursor[0]+= cw;
			break;
		}
	}
	
}

/**
   The writeb function offered to tputs.
*/
static int s_writeb( char c )
{
	b_append( s_writeb_buffer, &c, 1 );
	return 0;
}

/**
   Write the bytes needed to move screen cursor to the specified
   position to the specified buffer. The actual_cursor field of the
   specified screen_t will be updated.
   
   \param s the screen to operate on
   \param b the buffer to send the output escape codes to
   \param new_x the new x position
   \param new_y the new y position
*/
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
	
	y_steps = new_y - s->actual_cursor[1];

	if( y_steps > 0 && (strcmp( cursor_down, "\n")==0))
	{	
		/*
		  This is very strange - it seems some (all?) consoles use a
		  simple newline as the cursor down escape. This will of
		  course move the cursor to the beginning of the line as well
		  as moving it down one step. The cursor_up does not have this
		  behaviour...
		*/
		s->actual_cursor[0]=0;
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


	x_steps = new_x - s->actual_cursor[0];
	
	if( x_steps && new_x == 0 )
	{
		char c = '\r';
		b_append( b, &c, 1 );
		x_steps = 0;
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


	s->actual_cursor[0] = new_x;
	s->actual_cursor[1] = new_y;
	
	output_set_writer( writer_old );
	
}

/**
   Set the pen color for the terminal
*/
static void s_set_color( screen_t *s, buffer_t *b, int c )
{
	
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;
	
	set_color( highlight_get_color( c & 0xffff ),
		   highlight_get_color( (c>>16)&0xffff ) );
	
	output_set_writer( writer_old );
	
}

/**
   Convert a wide character to a multibyte string and append it to the
   buffer.
*/
static void s_write_char( screen_t *s, buffer_t *b, wchar_t c )
{
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;
	s->actual_cursor[0]+=wcwidth( c );
	
	writech( c );
	
	output_set_writer( writer_old );
}

/**
   Send the specified string through tputs and append the output to
   the specified buffer.
*/
static void s_write_mbs( buffer_t *b, char *s )
{
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;

	writembs( s );
	
	output_set_writer( writer_old );
}

/**
   Convert a wide string to a multibyte string and append it to the
   buffer.
*/
static void s_write_str( buffer_t *b, wchar_t *s )
{
	int (*writer_old)(char) = output_get_writer();

	output_set_writer( &s_writeb );
	s_writeb_buffer = b;

	writestr( s );
	
	output_set_writer( writer_old );
}

/**
   Update the screen to match the desired output.
*/
static void s_update( screen_t *scr, wchar_t *prompt )
{
	int i, j, k;
	int prompt_width = calc_prompt_width( prompt );
	int current_width=0;
	int screen_width = common_get_width();
	int need_clear = scr->need_clear;
	buffer_t output;

	scr->need_clear = 0;
	
	b_init( &output );

	
	if( scr->actual_width != screen_width )
	{
		need_clear = 1;
		s_move( scr, &output, 0, 0 );
		scr->actual_width = screen_width;
		s_reset( scr, 0 );
	}
	
	if( wcscmp( prompt, (wchar_t *)scr->actual_prompt.buff ) )
	{
		s_move( scr, &output, 0, 0 );
		s_write_str( &output, prompt );
		sb_clear( &scr->actual_prompt );
		sb_append( &scr->actual_prompt, prompt );		
		scr->actual_cursor[0] = prompt_width;
	}
	
	for( i=0; i< al_get_count( &scr->desired ); i++ )
	{
		line_t *o_line = (line_t *)al_get( &scr->desired, i );
		line_t *s_line = (line_t *)al_get( &scr->actual, i );
		int start_pos = (i==0?prompt_width:0);
		current_width = start_pos;

		if( need_clear )
		{
			s_move( scr, &output, start_pos, i );
			s_write_mbs( &output, clr_eol);
			if( s_line )
			{
				al_truncate( &s_line->text, 0 );
				al_truncate( &s_line->color, 0 );
			}
		}
		
		if( !s_line )
		{
			s_line = s_create_line();
			al_push( &scr->actual, s_line );
		}
		
		for( j=start_pos; j<al_get_count( &o_line->text ); j++ )
		{
			wchar_t o = (wchar_t)(long)al_get( &o_line->text, j );
			int o_c = (int)(long)al_get( &o_line->color, j );

			
			if( !o )
				continue;
			
			if( al_get_count( &s_line->text ) == j )
			{
				s_move( scr, &output, current_width, i );
				s_set_color( scr, &output, o_c );
				s_write_char( scr, &output, o );
				al_set_long( &s_line->text, j, o );
				al_set_long( &s_line->color, j, o_c );				
			}
			else
			{
				wchar_t s = (wchar_t)(long)al_get( &s_line->text, j );
				int s_c = (int)(long)al_get( &s_line->color, j );
				if( o != s || o_c != s_c )
				{
					s_move( scr, &output, current_width, i );
					s_set_color( scr, &output, o_c );
					s_write_char( scr, &output, o );
					al_set_long( &s_line->text, current_width, o );
					al_set_long( &s_line->color, current_width, o_c );
					for( k=1; k<wcwidth(o); k++ )
						al_set_long( &s_line->text, current_width+k, L'\0' );
						
				}
			}
			current_width += wcwidth( o );
		}

		if( al_get_count( &s_line->text ) > al_get_count( &o_line->text ) )
		{
			s_move( scr, &output, current_width, i );
			s_write_mbs( &output, clr_eol);
			al_truncate( &s_line->text, al_get_count( &o_line->text ) );
		}
		
	}
	for( i=al_get_count( &scr->desired ); i< al_get_count( &scr->actual ); i++ )
	{
		line_t *s_line = (line_t *)al_get( &scr->actual, i );
		s_move( scr, &output, 0, i );
		s_write_mbs( &output, clr_eol);
		al_truncate( &s_line->text, 0 );
	}
	
	s_move( scr, &output, scr->desired_cursor[0], scr->desired_cursor[1] );
	
	s_set_color( scr, &output, 0xffffffff);

	if( output.used )
	{
		write_loop( 1, output.buff, output.used );
	}
	
	b_destroy( &output );
	
}

/**
   Returns non-zero if we are using a dumb terminal.
 */
static int is_dumb()
{
	return ( !cursor_up || !cursor_down || !cursor_left || !cursor_right );
}



void s_write( screen_t *s,
	      wchar_t *prompt,
	      wchar_t *b, 
	      int *c, 
	      int *indent,
	      int cursor )
{
	int i;
	int cursor_arr[2];

	int prompt_width;
	int screen_width;

	int max_line_width = 0;
	int current_line_width = 0;
	
	CHECK( s, );
	CHECK( prompt, );
	CHECK( b, );
	CHECK( c, );
	CHECK( indent, );

	/*
	  If we are using a dumb terminal, don't try any fancy stuff,
	  just print out the text.
	 */
	if( is_dumb() )
	{
		char *prompt_narrow = wcs2str( prompt );
		char *buffer_narrow = wcs2str( b );
		
		write_loop( 1, "\r", 1 );
		write_loop( 1, prompt_narrow, strlen( prompt_narrow ) );
		write_loop( 1, buffer_narrow, strlen( buffer_narrow ) );

		free( prompt_narrow );
		free( buffer_narrow );
		
		return;
	}
	
	prompt_width = calc_prompt_width( prompt );
	screen_width = common_get_width();

	s_check_status( s );

	/*
	  Ignore prompts wider than the screen - only print a two
	  character placeholder...

	  It would be cool to truncate the prompt, but because it can
	  contain escape sequences, this is harder than you'd think.
	*/
	if( prompt_width >= screen_width )
	{
		prompt = L"> ";
		prompt_width = 2;
	}
	
	/*
	  Completely ignore impossibly small screens
	*/
	if( screen_width < 4 )
	{
		return;
	}

	/*
	  Check if we are overflowing
	 */

	for( i=0; b[i]; i++ )
	{
		if( b[i] == L'\n' )
		{
			if( current_line_width > max_line_width )
				max_line_width = current_line_width;
			current_line_width = indent[i]*INDENT_STEP;
		}
		else
		{
			current_line_width += wcwidth(b[i]);
		}
	}
	if( current_line_width > max_line_width )
		max_line_width = current_line_width;

	s_reset_arr( &s->desired );
	s->desired_cursor[0] = s->desired_cursor[1] = 0;

	/*
	  If overflowing, give the prompt its own line to improve the
	  situation.
	 */
	if( max_line_width + prompt_width >= screen_width )
	{
		s_desired_append_char( s, L'\n', 0, 0, 0 );
		prompt_width=0;
	}
	else
	{
		for( i=0; i<prompt_width; i++ )
		{
			s_desired_append_char( s, L' ', 0, 0, prompt_width );
		}
	}
	

	
	for( i=0; b[i]; i++ )
	{
		int col = c[i];
		
		if( i == cursor )
		{
			col = 0;
		}
		
		if( i == cursor )
		{
			cursor_arr[0] = s->desired_cursor[0];
			cursor_arr[1] = s->desired_cursor[1];
		}
		
		s_desired_append_char( s, b[i], col, indent[i], prompt_width );
		
		if( i== cursor && s->desired_cursor[1] != cursor_arr[1] && b[i] != L'\n' )
		{
			/*
			   Ugh. We are placed exactly at the wrapping point of a
			   wrapped line, move cursor to the line below so the
			   cursor won't be on the ellipsis which looks
			   unintuitive.
			*/
			cursor_arr[0] = s->desired_cursor[0] - wcwidth(b[i]);
			cursor_arr[1] = s->desired_cursor[1];
		}
		
	}
	if( i == cursor )
	{
		memcpy(cursor_arr, s->desired_cursor, sizeof(int)*2);
	}
	
	memcpy( s->desired_cursor, cursor_arr, sizeof(int)*2 );
	s_update( s, prompt );
	s_save_status( s );
}

void s_reset( screen_t *s, int reset_cursor )
{
	CHECK( s, );

	int prev_line = s->actual_cursor[1];
	s_reset_arr( &s->actual );
	s->actual_cursor[0] = s->actual_cursor[1] = 0;
	sb_clear( &s->actual_prompt );
	s->need_clear=1;

	if( !reset_cursor )
	{
		/*
		  This should prevent reseting the cursor position during the
		  next repaint.
		*/
		write_loop( 1, "\r", 1 );
		s->actual_cursor[1] = prev_line;
	}
	fstat( 1, &s->prev_buff_1 );
	fstat( 2, &s->prev_buff_2 );
}

