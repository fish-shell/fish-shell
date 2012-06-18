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
#include <vector>


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
typedef std::vector<char> data_buffer_t;
static data_buffer_t *s_writeb_buffer=0;

static int s_writeb( char c );

/* Class to temporarily set s_writeb_buffer and the writer function in a scoped way */
class scoped_buffer_t {
    data_buffer_t * const old_buff;
    int (* const old_writer)(char);
    
    public:
    scoped_buffer_t(data_buffer_t *buff) : old_buff(s_writeb_buffer), old_writer(output_get_writer())
    {
        s_writeb_buffer = buff;
        output_set_writer(s_writeb);
    }
    
    ~scoped_buffer_t()
    {
        s_writeb_buffer = old_buff;
        output_set_writer(old_writer);
    }
};

/**
   Tests if the specified narrow character sequence is present at the
   specified position of the specified wide character string. All of
   \c seq must match, but str may be longer than seq.
*/
static int try_sequence( const char *seq, const wchar_t *str )
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

// PCA for term256 support, let's just detect the escape codes directly
static int is_term256_escape(const wchar_t *str) {
    // An escape code looks like this: \x1b[38;5;<num>m 
    // or like this: \x1b[48;5;<num>m 
    
    // parse out the required prefix
    int len = try_sequence("\x1b[38;5;", str);
    if (! len) len = try_sequence("\x1b[48;5;", str);
    if (! len) return 0;
    
    // now try parsing out a string of digits
    // we need at least one
    if (! iswdigit(str[len])) return 0;
    while (iswdigit(str[len])) len++;
    
    // look for the terminating m
    if (str[len++] != L'm') return 0;
    
    // success
    return len;
}

/**
   Calculate the width of the specified prompt. Does some clever magic
   to detect common escape sequences that may be embeded in a prompt,
   such as color codes.
*/
static int calc_prompt_width( const wchar_t *prompt )
{
	int res = 0;
	size_t j, k;

	for( j=0; prompt[j]; j++ )
	{
		if( prompt[j] == L'\x1b' )
		{
			/*
			  This is the start of an escape code. Try to guess it's width.
			*/
			size_t p;
			int len=0;
			bool found = false;
			
			/*
			  Detect these terminfo color escapes with parameter
			  value 0..7, all of which don't move the cursor
			*/
			char * const esc[] =
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
			char * const esc2[] =
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

			for( p=0; p < sizeof esc / sizeof *esc && !found; p++ )
			{
				if( !esc[p] )
					continue;

				for( k=0; k<8; k++ )
				{
					len = try_sequence( tparm(esc[p],k), &prompt[j] );
					if( len )
					{
						j += (len-1);
						found = true;
						break;
					}
				}
			}
            
            // PCA for term256 support, let's just detect the escape codes directly
            if (! found) {
                len = is_term256_escape(&prompt[j]);
                if (len) {
                    j += (len - 1);
                    found = true;
                }
            }


			for( p=0; p < (sizeof(esc2)/sizeof(char *)) && !found; p++ )
			{
				if( !esc2[p] )
					continue;
				/*
				  Test both padded and unpadded version, just to
				  be safe. Most versions of tparm don't actually
				  seem to do anything these days.
				*/
				len = maxi( try_sequence( tparm(esc2[p]), &prompt[j] ),
					    try_sequence( esc2[p], &prompt[j] ));
					
				if( len )
				{
					j += (len-1);
					found = true;
				}
			}
				
			if( !found )
			{
				if( prompt[j+1] == L'k' )
				{
					const env_var_t term_name = env_get_string( L"TERM" );
					if( !term_name.missing() && wcsstr( term_name.c_str(), L"screen" ) == term_name )
					{
						const wchar_t *end;
						j+=2;
						found = true;
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
		else if( prompt[j] == L'\n' )
		{
			res = 0;
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

    // PCA Let's not do this futimes stuff, because sudo dumbly uses the
    // tty's ctime as part of its tty_tickets feature
    // Disabling this should fix https://github.com/fish-shell/fish-shell/issues/122
#if 0
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
#endif

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
		
		int prev_line = s->actual.cursor[1];
		write_loop( 1, "\r", 1 );
		s_reset( s, false );
		s->actual.cursor[1] = prev_line;
	}
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
	int line_no = s->desired.cursor[1];
	
	switch( b )
	{
		case L'\n':
		{
			int i;
            s->desired.add_line();
			s->desired.cursor[1]++;
			s->desired.cursor[0]=0;
			for( i=0; i < prompt_width+indent*INDENT_STEP; i++ )
			{
				s_desired_append_char( s, L' ', 0, indent, prompt_width );
			}
			break;
		}
		
		case L'\r':
		{
            line_t &current = s->desired.line(line_no);
            current.resize(0);
            s->desired.cursor[0] = 0;
			break;			
		}		

		default:
		{
			int screen_width = common_get_width();
			int cw = wcwidth(b);
			int ew = wcwidth( ellipsis_char );
			int i;
			
            s->desired.create_line(line_no);

			/*
			  Check if we are at the end of the line. If so, print an
			  ellipsis character and continue on the next line.
			*/
			if( s->desired.cursor[0] + cw + ew > screen_width )
			{
                line_entry_t &entry = s->desired.line(line_no).create_entry(s->desired.cursor[0]);
                entry.text = ellipsis_char;
                entry.color = HIGHLIGHT_COMMENT;
                
                line_no = s->desired.line_count();
                s->desired.add_line();
				s->desired.cursor[1]++;
				s->desired.cursor[0]=0;
				for( i=0; i < (prompt_width-ew); i++ )
				{
					s_desired_append_char( s, L' ', 0, indent, prompt_width );
				}				
				s_desired_append_char( s, ellipsis_char, HIGHLIGHT_COMMENT, indent, prompt_width );
			}
			
            line_t &line = s->desired.line(line_no);
            line.create_entry(s->desired.cursor[0]).text = b;
            line.create_entry(s->desired.cursor[0]).color = c;
			s->desired.cursor[0]+= cw;
			break;
		}
	}
	
}

/**
   The writeb function offered to tputs.
*/
static int s_writeb( char c )
{
    s_writeb_buffer->push_back(c);
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
static void s_move( screen_t *s, data_buffer_t *b, int new_x, int new_y )
{
	int i;
	int x_steps, y_steps;
	
	char *str;
/*
  debug( 0, L"move from %d %d to %d %d", 
  s->screen_cursor[0], s->screen_cursor[1],  
  new_x, new_y );
*/
    scoped_buffer_t scoped_buffer(b);
    	
	y_steps = new_y - s->actual.cursor[1];

	if( y_steps > 0 && (strcmp( cursor_down, "\n")==0))
	{	
		/*
		  This is very strange - it seems some (all?) consoles use a
		  simple newline as the cursor down escape. This will of
		  course move the cursor to the beginning of the line as well
		  as moving it down one step. The cursor_up does not have this
		  behaviour...
		*/
		s->actual.cursor[0]=0;
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


	x_steps = new_x - s->actual.cursor[0];
	
	if( x_steps && new_x == 0 )
	{
        b->push_back('\r');
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


	s->actual.cursor[0] = new_x;
	s->actual.cursor[1] = new_y;
}

/**
   Set the pen color for the terminal
*/
static void s_set_color( screen_t *s, data_buffer_t *b, int c )
{
    scoped_buffer_t scoped_buffer(b);
    
    unsigned int uc = (unsigned int)c;
	set_color( highlight_get_color( uc & 0xffff, false ),
		   highlight_get_color( (uc>>16)&0xffff, true ) );	
}

/**
   Convert a wide character to a multibyte string and append it to the
   buffer.
*/
static void s_write_char( screen_t *s, data_buffer_t *b, wchar_t c )
{
	scoped_buffer_t scoped_buffer(b);
	s->actual.cursor[0]+=wcwidth( c );	
	writech( c );
}

/**
   Send the specified string through tputs and append the output to
   the specified buffer.
*/
static void s_write_mbs( data_buffer_t *b, char *s )
{
    scoped_buffer_t scoped_buffer(b);
	writembs( s );
}

/**
   Convert a wide string to a multibyte string and append it to the
   buffer.
*/
static void s_write_str( data_buffer_t *b, const wchar_t *s )
{
    scoped_buffer_t scoped_buffer(b);
	writestr( s );
}

/**
   Update the screen to match the desired output.
*/
static void s_update( screen_t *scr, const wchar_t *prompt )
{
	size_t i, j;
	int prompt_width = calc_prompt_width( prompt );
	int current_width=0;
	int screen_width = common_get_width();
	int need_clear = scr->need_clear;
	data_buffer_t output;

	scr->need_clear = 0;
		
	if( scr->actual_width != screen_width )
	{
		need_clear = 1;
		s_move( scr, &output, 0, 0 );
		scr->actual_width = screen_width;
		s_reset( scr, false );
	}
	
	if( wcscmp( prompt, scr->actual_prompt.c_str() ) )
	{
		s_move( scr, &output, 0, 0 );
		s_write_str( &output, prompt );
        scr->actual_prompt = prompt;
		scr->actual.cursor[0] = prompt_width;
	}
	
    for (i=0; i < scr->desired.line_count(); i++)
	{
		line_t &o_line = scr->desired.line(i);
		line_t &s_line = scr->actual.create_line(i);
		int start_pos = (i==0?prompt_width:0);
		current_width = start_pos;

		if( need_clear )
		{
			s_move( scr, &output, start_pos, i );
			s_write_mbs( &output, clr_eol);
            s_line.resize(0);
		}

        for( j=start_pos; j<o_line.entry_count(); j++) 
		{
            line_entry_t &entry = o_line.entry(j);
			wchar_t o = entry.text;
			int o_c = entry.color;
			if( !o )
				continue;
			
			if( s_line.entry_count() == j )
			{
				s_move( scr, &output, current_width, i );
				s_set_color( scr, &output, o_c );
				s_write_char( scr, &output, o );
                s_line.create_entry(j).text = o;
                s_line.create_entry(j).color = o_c;
			}
			else
			{
                line_entry_t &entry = s_line.create_entry(j);
                wchar_t s = entry.text;
                int s_c = entry.color;

				if( o != s || o_c != s_c )
				{
					s_move( scr, &output, current_width, i );
					s_set_color( scr, &output, o_c );
					s_write_char( scr, &output, o );
                    
                    s_line.create_entry(current_width).text = o;
                    s_line.create_entry(current_width).color = o_c;
					for( int k=1; k<wcwidth(o); k++ )
                        s_line.create_entry(current_width+k).text = L'\0';
						
				}
			}
			current_width += wcwidth( o );
		}

        if ( s_line.entry_count() > o_line.entry_count() )
		{
			s_move( scr, &output, current_width, i );
			s_write_mbs( &output, clr_eol);
            s_line.resize(o_line.entry_count());
		}
		
	}
    for( i=scr->desired.line_count(); i < scr->actual.line_count(); i++ )
	{
        line_t &s_line = scr->actual.create_line(i);
		s_move( scr, &output, 0, i );
		s_write_mbs( &output, clr_eol);
        s_line.resize(0);
	}
	
	s_move( scr, &output, scr->desired.cursor[0], scr->desired.cursor[1] );
	
	s_set_color( scr, &output, 0xffffffff);

	if( ! output.empty() )
	{
		write_loop( 1, &output.at(0), output.size() );
	}
	
}

/**
   Returns non-zero if we are using a dumb terminal.
 */
static int is_dumb()
{
	return ( !cursor_up || !cursor_down || !cursor_left || !cursor_right );
}



void s_write( screen_t *s,
	      const wchar_t *prompt,
	      const wchar_t *b, 
	      const int *c, 
	      const int *indent,
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

    s->desired.resize(0);
	s->desired.cursor[0] = s->desired.cursor[1] = 0;

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
			cursor_arr[0] = s->desired.cursor[0];
			cursor_arr[1] = s->desired.cursor[1];
		}
		
		s_desired_append_char( s, b[i], col, indent[i], prompt_width );
		
		if( i== cursor && s->desired.cursor[1] != cursor_arr[1] && b[i] != L'\n' )
		{
			/*
			   Ugh. We are placed exactly at the wrapping point of a
			   wrapped line, move cursor to the line below so the
			   cursor won't be on the ellipsis which looks
			   unintuitive.
			*/
			cursor_arr[0] = s->desired.cursor[0] - wcwidth(b[i]);
			cursor_arr[1] = s->desired.cursor[1];
		}
		
	}
	if( i == cursor )
	{
		memcpy(cursor_arr, s->desired.cursor, sizeof(int)*2);
	}
	
	memcpy( s->desired.cursor, cursor_arr, sizeof(int)*2 );
	s_update( s, prompt );
	s_save_status( s );
}

void s_reset( screen_t *s, bool reset_cursor )
{
	CHECK( s, );

	int prev_line = s->actual.cursor[1];
    s->actual.resize(0);
	s->actual.cursor[0] = s->actual.cursor[1] = 0;
    s->actual_prompt = L"";
	s->need_clear=1;

	if( !reset_cursor )
	{
		/*
		  This should prevent reseting the cursor position during the
		  next repaint.
		*/
		write_loop( 1, "\r", 1 );
		s->actual.cursor[1] = prev_line;
	}
	fstat( 1, &s->prev_buff_1 );
	fstat( 2, &s->prev_buff_2 );
}

