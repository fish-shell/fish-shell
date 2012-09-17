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
static size_t next_tab_stop( size_t in )
{
	/*
	  Assume tab stops every 8 characters if undefined
	*/
	size_t tab_width = (init_tabs > 0 ? (size_t)init_tabs : 8);
	return ( (in/tab_width)+1 )*tab_width;
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
static size_t calc_prompt_width( const wchar_t *prompt )
{
	size_t res = 0;
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
			res += fish_wcwidth( prompt[j] );
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
		
		int prev_line = s->actual.cursor.y;
		write_loop( 1, "\r", 1 );
		s_reset( s, false );
		s->actual.cursor.y = prev_line;
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
                                  size_t prompt_width )
{
	int line_no = s->desired.cursor.y;
	
	switch( b )
	{
		case L'\n':
		{
			int i;
            s->desired.create_line(s->desired.line_count());
			s->desired.cursor.y++;
			s->desired.cursor.x=0;
			for( i=0; i < prompt_width+indent*INDENT_STEP; i++ )
			{
				s_desired_append_char( s, L' ', 0, indent, prompt_width );
			}
			break;
		}
            
		case L'\r':
		{
            line_t &current = s->desired.line(line_no);
            current.clear();
            s->desired.cursor.x = 0;
			break;
		}
            
		default:
		{
			int screen_width = common_get_width();
			int cw = fish_wcwidth(b);
			
            s->desired.create_line(line_no);
            
			/*
             Check if we are at the end of the line. If so, continue on the next line.
             */
			if( (s->desired.cursor.x + cw) > screen_width )
			{
                line_no = (int)s->desired.line_count();
                s->desired.add_line();
				s->desired.cursor.y++;
				s->desired.cursor.x=0;
                for( size_t i=0; i < prompt_width; i++ )
                {
                    s_desired_append_char( s, L' ', 0, indent, prompt_width );
                }
			}
			
            line_t &line = s->desired.line(line_no);
            line.append(b, c);
			s->desired.cursor.x+= cw;
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
    	
	y_steps = new_y - s->actual.cursor.y;

	if( y_steps > 0 && (strcmp( cursor_down, "\n")==0))
	{	
		/*
		  This is very strange - it seems some (all?) consoles use a
		  simple newline as the cursor down escape. This will of
		  course move the cursor to the beginning of the line as well
		  as moving it down one step. The cursor_up does not have this
		  behaviour...
		*/
		s->actual.cursor.x=0;
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


	x_steps = new_x - s->actual.cursor.x;
	
	if( x_steps && new_x == 0 )
	{
        b->push_back('\r');
		x_steps = 0;
	}
		
	if( x_steps < 0 )
    {
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


	s->actual.cursor.x = new_x;
	s->actual.cursor.y = new_y;
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
	s->actual.cursor.x+=fish_wcwidth( c );
	writech( c );
}

/**
   Convert a wide string to a multibyte string and append it to the
   buffer. Returns the width.
*/
static int s_write_string( screen_t *s, data_buffer_t *b, const wcstring &str )
{
	scoped_buffer_t scoped_buffer(b);
    int width = fish_wcswidth(str.c_str(), str.size());
	writestr(str.c_str());
    s->actual.cursor.x += width;
    return width;
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

/** Returns the length of the "shared prefix" of the two lines, which is the run of matching text and colors.
    If the prefix ends on a combining character, do not include the previous character in the prefix.
*/
static size_t line_shared_prefix(const line_t &a, const line_t &b)
{
    size_t idx, max = std::min(a.size(), b.size());
    for (idx=0; idx < max; idx++)
    {            
        wchar_t ac = a.char_at(idx), bc = b.char_at(idx);
        if (fish_wcwidth(ac) < 1 || fish_wcwidth(bc) < 1)
        {
            /* Possible combining mark, return one index prior */
            if (idx > 0) idx--;
            break;
        }
        
        /* We're done if the text or colors are different */
        if (ac != bc || a.color_at(idx) != b.color_at(idx))
            break;
    }
    return idx;
}

/**
   Update the screen to match the desired output.
*/
static void s_update( screen_t *scr, const wchar_t *prompt )
{
	size_t prompt_width = calc_prompt_width( prompt );
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
		scr->actual.cursor.x = (int)prompt_width;
	}
	
    for (size_t i=0; i < scr->desired.line_count(); i++)
	{
		const line_t &o_line = scr->desired.line(i);
		line_t &s_line = scr->actual.create_line(i);
		size_t start_pos = (i==0 ? prompt_width : 0);
        int current_width = 0;
        
		if( need_clear )
		{
			s_move( scr, &output, (int)start_pos, (int)i );
			s_write_mbs( &output, clr_eol);
            s_line.clear();
		}
        
        /* Note that skip_remaining is a width, not a character count */
        size_t skip_remaining = start_pos;
        
        /* Compute how much we should skip. At a minimum we skip over the prompt. But also skip over the shared prefix of what we want to output now, and what we output before, to avoid repeatedly outputting it. */
        size_t shared_prefix = line_shared_prefix(o_line, s_line);
        if (shared_prefix > 0)
        {
            int prefix_width = fish_wcswidth(&o_line.text.at(0), shared_prefix);
            if (prefix_width > skip_remaining)
                skip_remaining = prefix_width;
        }
        
        /* Skip over skip_remaining width worth of characters */
        size_t j = 0;
        for ( ; j < o_line.size(); j++)
        {
            int width = fish_wcwidth(o_line.char_at(j));
            if (skip_remaining <= width)
                break;
            skip_remaining -= width;
            current_width += width;
        }
        
        /* Skip over zero-width characters (e.g. combining marks at the end of the prompt) */
        for ( ; j < o_line.size(); j++)
        {
            int width = fish_wcwidth(o_line.char_at(j));
            if (width > 0)
                break;
        }
        
        /* Now actually output stuff */
        for ( ; j < o_line.size(); j++)
        {
            s_move( scr, &output, current_width, (int)i );
            s_set_color( scr, &output, o_line.color_at(j) );
            s_write_char( scr, &output, o_line.char_at(j) );
            current_width += fish_wcwidth(o_line.char_at(j));
        }
        
        /* If we wrote more on this line last time, clear it */
        int prev_length = (s_line.text.empty() ? 0 : fish_wcswidth(&s_line.text.at(0), s_line.text.size()));
        if (prev_length > current_width )
		{
			s_move( scr, &output, current_width, (int)i );
			s_write_mbs( &output, clr_eol);
		}
	}
    
    /* Clear remaining lines */
    for( size_t i=scr->desired.line_count(); i < scr->actual.line_count(); i++ )
	{
		s_move( scr, &output, 0, (int)i );
		s_write_mbs( &output, clr_eol);
	}
	
	s_move( scr, &output, scr->desired.cursor.x, scr->desired.cursor.y );
	s_set_color( scr, &output, 0xffffffff);

	if( ! output.empty() )
	{
		write_loop( 1, &output.at(0), output.size() );
	}
	
    /* We have now synced our actual screen against our desired screen. Note that this is a big assignment! */
    scr->actual = scr->desired;
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
	      const wchar_t *commandline, 
	      size_t explicit_len,
	      const int *c, 
	      const int *indent,
	      size_t cursor_pos )
{
	screen_data_t::cursor_t cursor_arr;

	size_t prompt_width;
	size_t screen_width;

	int current_line_width = 0, newline_count = 0, explicit_portion_width = 0;
    size_t max_line_width = 0;
	
	CHECK( s, );
	CHECK( prompt, );
	CHECK( commandline, );
	CHECK( c, );
	CHECK( indent, );

	/*
	  If we are using a dumb terminal, don't try any fancy stuff,
	  just print out the text.
	 */
	if( is_dumb() )
	{
		char *prompt_narrow = wcs2str( prompt );
		char *buffer_narrow = wcs2str( commandline );
		
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
    size_t last_char_that_fits = 0;
	for( size_t i=0; commandline[i]; i++ )
	{
		if( commandline[i] == L'\n' )
		{
			if( current_line_width > max_line_width )
				max_line_width = current_line_width;
			current_line_width = indent[i]*INDENT_STEP;
            newline_count++;
		}
		else
		{
            int width = fish_wcwidth(commandline[i]);
			current_line_width += width;
            if (i < explicit_len)
                explicit_portion_width += width;
                
            if (prompt_width + current_line_width < screen_width)
                last_char_that_fits = i;
		}
	}
	if( current_line_width > max_line_width )
		max_line_width = current_line_width;

    s->desired.resize(0);
	s->desired.cursor.x = s->desired.cursor.y = 0;
    
    /* If we cannot fit with the autosuggestion, but we can fit without it, truncate the autosuggestion. We limit this check to just one line to avoid confusion; not sure how well this would work with multiple lines */
    wcstring truncated_autosuggestion_line;
    if (newline_count == 0 && prompt_width + max_line_width >= screen_width && prompt_width + explicit_portion_width < screen_width)
    {
        assert(screen_width - prompt_width >= 1);
        max_line_width = screen_width - prompt_width - 1;
        truncated_autosuggestion_line = wcstring(commandline, 0, last_char_that_fits);
        commandline = truncated_autosuggestion_line.c_str();
    }
	for( size_t i=0; i<prompt_width; i++ )
	{
		s_desired_append_char( s, L' ', 0, 0, prompt_width );
	}

	/*
	  If overflowing, give the prompt its own line to improve the
	  situation.
	 */
	if( max_line_width + prompt_width >= screen_width )
	{
		s_desired_append_char( s, L'\n', 0, 0, 0 );
		prompt_width=0;
	}
	
    size_t i;
	for( i=0; commandline[i]; i++ )
	{
		int col = c[i];
		
		if( i == cursor_pos )
		{
			col = 0;
		}
		
		if( i == cursor_pos )
		{
            cursor_arr = s->desired.cursor;
		}
		
		s_desired_append_char( s, commandline[i], col, indent[i], prompt_width );
		
	}
	if( i == cursor_pos )
	{
        cursor_arr = s->desired.cursor;
	}
	
    s->desired.cursor = cursor_arr;
	s_update( s, prompt );
	s_save_status( s );
}

void s_reset( screen_t *s, bool reset_cursor )
{
	CHECK( s, );

	int prev_line = s->actual.cursor.y;
    s->actual.resize(0);
	s->actual.cursor.x = s->actual.cursor.y = 0;
    s->actual_prompt = L"";
	s->need_clear=1;

	if( !reset_cursor )
	{
		/*
		  This should prevent reseting the cursor position during the
		  next repaint.
		*/
		write_loop( 1, "\r", 1 );
		s->actual.cursor.y = prev_line;
	}
	fstat( 1, &s->prev_buff_1 );
	fstat( 2, &s->prev_buff_2 );
}

