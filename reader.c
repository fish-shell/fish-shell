/** \file reader.c

Functions for reading data from stdin and passing to the
parser. If stdin is a keyboard, it supplies a killring, history,
syntax highlighting, tab-completion and various other interactive features.

Internally the interactive mode functions rely in the functions of the
input library to read individual characters of input.

Token search is handled incrementally. Actual searches are only done
on when searching backwards, since the previous results are saved. The
last search position is remembered and a new search continues from the
last search position. All search results are saved in the list
'search_prev'. When the user searches forward, i.e. presses Alt-down,
the list is consulted for previous search result, and subsequent
backwards searches are also handled by consultiung the list up until
the end of the list is reached, at which point regular searching will
commence.

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
#include <sys/wait.h>
#include <sys/poll.h>
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

#include <assert.h>

#include "util.h"
#include "wutil.h"
#include "highlight.h"
#include "reader.h"
#include "proc.h"
#include "parser.h"
#include "complete.h"
#include "history.h"
#include "common.h"
#include "sanity.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "tokenizer.h"
#include "kill.h"
#include "input_common.h"
#include "input.h"
#include "function.h"
#include "output.h"
#include "signal.h"
#include "translate.h"
#include "parse_util.h"

/**
   Maximum length of prefix string when printing completion
   list. Longer prefixes will be ellipsized.
*/
#define PREFIX_MAX_LEN 8

/**
   A simple prompt for reading shell commands that does not rely on
   fish specific commands, meaning it will work even if fish is not
   installed. This is used by read_i.
*/
#define DEFAULT_PROMPT L"whoami; echo @; hostname|cut -d . -f 1; echo \" \"; pwd; printf '> ';"

/**
   The default title for the reader. This is used by reader_readline.
*/
#define DEFAULT_TITLE L"echo $_ \" \"; pwd"

/**
   The maximum number of characters to read from the keyboard without
   repainting. Note that this readahead will only occur if new
   characters are avaialble for reading, fish will never block for
   more input without repainting.
*/
#define READAHEAD_MAX 256

/**
   A struct describing the state of the interactive reader. These
   states can be stacked, in case reader_readline is called from
   input_read().
*/
typedef struct reader_data
{
	/**
	   Buffer containing the current commandline
	*/
	wchar_t *buff;

	/**
	   The output string, may be different than buff if buff can't fit on one line.
	*/
	wchar_t *output;

	/**
	   The number of characters used by the prompt
	*/
	int prompt_width;

	/**
	   Buffer containing the current search item
	*/
	wchar_t *search_buff;
	/**
	   Saved position used by token history search
	*/
	int token_history_pos;

	/**
	   Saved search string for token history search. Not handled by check_size.
	*/
	const wchar_t *token_history_buff;

	/**
	   List for storing previous search results. Used to avoid duplicates.
	*/
	array_list_t search_prev;

	/**
	   The current position in search_prev
	*/

	int search_pos;

	/**
	   Current size of the buffers
	*/
	size_t buff_sz;

	/**
	   Length of the command in buff. (Not the length of buff itself)
	*/

	size_t buff_len;

	/**
	   The current position of the cursor in buff.
	*/
	size_t buff_pos;

	/**
	   The current position of the cursor in output buffer.
	*/
	size_t output_pos;

	/**
	   Name of the current application
	*/
	wchar_t *name;

	/** The prompt text */
	wchar_t *prompt;

	/**
	   Color is the syntax highlighting for buff.  The format is that
	   color[i] is the classification (according to the enum in
	   highlight.h) of buff[i].
	*/
	int *color;

	/**
	   New color buffer, used for syntax highlighting.
	*/
	int *new_color;

	/**
	   Color for the actual output string.
	*/
	int *output_color;

	/**
	   Should the prompt command be reexecuted on the next repaint
	*/
	int exec_prompt;

	/**
	   Function for tab completion
	*/
	void (*complete_func)( const wchar_t *,
						   array_list_t * );

	/**
	   Function for syntax highlighting
	*/
	void (*highlight_func)( wchar_t *,
							int *,
							int,
							array_list_t * );

	/**
	   Function for testing if the string can be returned
	*/
	int (*test_func)( wchar_t * );

	/**
	   When this is true, the reader will exit
	*/
	int end_loop;

	/**
	   Pointer to previous reader_data
	*/
	struct reader_data *next;
}
	reader_data_t;

/**
   The current interactive reading context
*/
static reader_data_t *data=0;


/**
   Flag for ending non-interactive shell
*/
static int end_loop = 0;

/**
   The list containing names of files that are being parsed
*/
static array_list_t current_filename;

/**
   These status buffers are used to check if any output has occurred
   other than from fish's main loop, in which case we need to redraw.
*/
static struct stat prev_buff_1, prev_buff_2, post_buff_1, post_buff_2;



/**
   List containing strings which make up the prompt
*/
static array_list_t prompt_list;

/**
   Stores the previous termios mode so we can reset the modes when
   we execute programs and when the shell exits.
*/
static struct termios saved_modes;


/**
   Store the pid of the parent process, so the exit function knows whether it should reset the terminal or not.
*/
static pid_t original_pid;

/**
   This variable is set to true by the signal handler when ^C is pressed
*/
static int interupted=0;

/**
   Original terminal mode when fish was started
*/
static struct termios old_modes;

/*
  Prototypes for a bunch of functions defined later on.
*/

static void reader_save_status();
static void reader_check_status();
static void reader_super_highlight_me_plenty( wchar_t * buff, int *color, int pos, array_list_t *error );


/**
   Give up control of terminal
*/
static void term_donate()
{
	tcgetattr(0,&old_modes);        /* get the current terminal modes */

	set_color(FISH_COLOR_NORMAL, FISH_COLOR_NORMAL);

	while( 1 )
	{
		if(	tcsetattr(0,TCSANOW,&saved_modes) )
		{
			if( errno != EINTR )
			{
				debug( 1, _( L"Could not set terminal mode for new job" ) );
				wperror( L"tcsetattr" );
				break;
			}
		}
		else
			break;
	}


}

/**
   Grab control of terminal
*/
static void term_steal()
{

	while( 1 )
	{
		if(	tcsetattr(0,TCSANOW,&shell_modes) )
		{
			if( errno != EINTR )
			{
				debug( 1, _( L"Could not set terminal mode for shell" ) );
				wperror( L"tcsetattr" );
				break;
			}
		}
		else
			break;
	}

	common_handle_winch(0 );

	if( tcsetattr(0,TCSANOW,&old_modes))/* return to previous mode */
	{
		wperror(L"tcsetattr");
		exit(1);
	}

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
   string_buffer used as temporary storage for the reader_readline function
*/
static string_buffer_t *readline_buffer=0;

void reader_handle_int( int sig )
{
	block_t *c = current_block;
	while( c )
	{
		c->skip=1;
		c=c->outer;
	}
	interupted = 1;

}

wchar_t *reader_current_filename()
{
	return al_get_count( &current_filename )?(wchar_t *)al_peek( &current_filename ):0;
}


void reader_push_current_filename( const wchar_t *fn )
{
	al_push( &current_filename, fn );
}


wchar_t *reader_pop_current_filename()
{
	return (wchar_t *)al_pop( &current_filename );
}


/**
   Make sure buffers are large enough to hold current data plus one extra character.
*/
static int check_size()
{
	if( data->buff_sz < data->buff_len + 2 )
	{
		data->buff_sz = maxi( 128, data->buff_len*2 );

		data->buff = realloc( data->buff,
							  sizeof(wchar_t)*data->buff_sz);
		data->search_buff = realloc( data->search_buff,
									 sizeof(wchar_t)*data->buff_sz);
		data->output = realloc( data->output,
								sizeof(wchar_t)*data->buff_sz);

		data->color = realloc( data->color,
							   sizeof(int)*data->buff_sz);
		data->new_color = realloc( data->new_color,
								   sizeof(int)*data->buff_sz);
		data->output_color = realloc( data->output_color,
									  sizeof(int)*data->buff_sz);

		if( data->buff==0 ||
			data->search_buff==0 ||
			data->color==0 ||
			data->new_color == 0 )
		{
			die_mem();

		}
	}
	return 1;
}

/**
   Check if the screen is not wide enough for the buffer, which means
   the buffer must be scrolled on input and cursor movement.
*/
static int force_repaint()
{
	int max_width = common_get_width() - data->prompt_width;
	int pref_width = my_wcswidth( data->buff ) + (data->buff_pos==data->buff_len);
	return pref_width >= max_width;
}


/**
   Calculate what part of the buffer should be visible

   \return returns 1 screen needs repainting, 0 otherwise
*/
static int calc_output()
{
	int max_width = common_get_width() - data->prompt_width;
	int pref_width = my_wcswidth( data->buff ) + (data->buff_pos==data->buff_len);
	if( pref_width <= max_width )
	{
		wcscpy( data->output, data->buff );
		memcpy( data->output_color, data->color, sizeof(int) * data->buff_len );
		data->output_pos=data->buff_pos;

		return 1;
	}
	else
	{
		int offset = data->buff_pos;
		int offset_end = data->buff_pos;
		int w = 0;
		wchar_t *pos=data->output;
		*pos=0;


		w = (data->buff_pos==data->buff_len)?1:wcwidth( data->buff[offset] );
		while( 1 )
		{
			int inc=0;
			int ellipsis_width;

			ellipsis_width = wcwidth(ellipsis_char)*((offset?1:0)+(offset_end<data->buff_len?1:0));

			if( offset > 0 && (ellipsis_width + w + wcwidth( data->buff[offset-1] ) <= max_width ) )
			{
				inc=1;
				offset--;
				w+= wcwidth( data->buff[offset]);
			}

			ellipsis_width = wcwidth(ellipsis_char)*((offset?1:0)+(offset_end<data->buff_len?1:0));

			if( offset_end < data->buff_len && (ellipsis_width + w + wcwidth( data->buff[offset_end+1] ) <= max_width ) )
			{
				inc = 1;
				offset_end++;
				w+= wcwidth( data->buff[offset_end]);
			}

			if( !inc )
				break;

		}

		data->output_pos = data->buff_pos - offset + (offset?1:0);

		if( offset )
		{
			data->output[0]=ellipsis_char;
			data->output[1]=0;

		}

		wcsncat( data->output,
				 data->buff+offset,
				 offset_end-offset );

		if( offset_end<data->buff_len )
		{
			int l = wcslen(data->output);

			data->output[l]=ellipsis_char;
			data->output[l+1]=0;

		}

		*data->output_color=HIGHLIGHT_NORMAL;

		memcpy( data->output_color+(offset?1:0),
				data->color+offset,
				sizeof(int) * (data->buff_len-offset) );
		return 1;
	}
}


/**
   Compare two completions, ignoring their description.
*/
static int fldcmp( wchar_t *a, wchar_t *b )
{
	while( 1 )
	{
		if( *a != *b )
			return *a-*b;
		if( ( (*a == COMPLETE_SEP) || (*a == L'\0') ) &&
			( (*b == COMPLETE_SEP) || (*b == L'\0') ) )
			return 0;
		a++;
		b++;
	}

}

/**
   Remove any duplicate completions in the list. This relies on the
   list first beeing sorted.
*/
static void remove_duplicates( array_list_t *l )
{
	int in, out;
	wchar_t *prev;
	if( al_get_count( l ) == 0 )
		return;

	prev = (wchar_t *)al_get( l, 0 );
	for( in=1, out=1; in < al_get_count( l ); in++ )
	{
		wchar_t *curr = (wchar_t *)al_get( l, in );
		if( fldcmp( prev, curr )==0 )
		{
			free( curr );
		}
		else
		{
			al_set( l, out++, curr );
			prev = curr;
		}
	}
	al_truncate( l, out );
}


/**
   Translate a highlighting code ()Such as  as returned by the highlight function
   into a color code which is then passed on to set_color.
*/
static void set_color_translated( int c )
{
	set_color( highlight_get_color( c & 0xff ),
			   highlight_get_color( (c>>8)&0xff ) );
}

int reader_interupted()
{
	int res=interupted;
	if( res )
		interupted=0;
	return res;
}

void reader_write_title()
{
	wchar_t *title;
	array_list_t l;
	wchar_t *term = env_get( L"TERM" );
	int was_interactive = is_interactive;

	/*
	  This is a pretty lame heuristic for detecting terminals that do
	  not support setting the title. If we recognise the terminal name
	  as that of a virtual terminal, we assume it supports setting the
	  title. Otherwise we check the ttyname.
	*/
	if( !term || !contains_str( term, L"xterm", L"screen", L"nxterm", L"rxvt", 0 ) )
	{
		char *n = ttyname( STDIN_FILENO );
		if( strstr( n, "tty" ) || strstr( n, "/vc/") )
			return;
	}

	title = function_exists( L"fish_title" )?L"fish_title":DEFAULT_TITLE;

	if( wcslen( title ) ==0 )
		return;

	al_init( &l );

	is_interactive = 0;
	if( exec_subshell( title, &l ) != -1 )
	{
		int i;
		writestr( L"\e]2;" );
		for( i=0; i<al_get_count( &l ); i++ )
		{
			writestr( (wchar_t *)al_get( &l, i ) );
		}
		writestr( L"\7" );
	}
	is_interactive = was_interactive;
	
	al_foreach( &l, (void (*)(const void *))&free );
	al_destroy( &l );
	set_color( FISH_COLOR_RESET, FISH_COLOR_RESET );
}

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
static int calc_prompt_width( array_list_t *arr )
{
	int res = 0;
	int i, j, k;

	for( i=0; i<al_get_count( arr ); i++ )
	{
		wchar_t *next = (wchar_t *)al_get( arr, i );

		for( j=0; next[j]; j++ )
		{
			if( next[j] == L'\e' )
			{
				/*
				  This is the start of an escape code. Try to guess it's width.
				*/
				int l;
				int len=0;
				int found = 0;

				/*
				  Test these color escapes with parameter value 0..7
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
				  Test these regular escapes without any parameter values
				*/
				char *esc2[] =
					{
						enter_bold_mode,
						exit_attribute_mode,
						enter_underline_mode,
						exit_underline_mode,
						enter_standout_mode,
						exit_standout_mode,
						flash_screen
					}
				;

				for( l=0; l < (sizeof(esc)/sizeof(char *)) && !found; l++ )
				{
					if( !esc[l] )
						continue;

					for( k=0; k<8; k++ )
					{
						len = try_sequence( tparm(esc[l],k), &next[j] );
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
					len = maxi( try_sequence( tparm(esc2[l]), &next[j] ),
								try_sequence( esc2[l], &next[j] ));

					if( len )
					{
						j += (len-1);
						found = 1;
					}
				}
			}
			else
			{
				/*
				  Ordinary decent character. Just add width.
				*/
				res += wcwidth( next[j] );
			}

		}
	}
	return res;
}


/**
   Write the prompt to screen. If data->exec_prompt is set, the prompt
   command is first evaluated, and the title will be reexecuted as
   well.
*/
static void write_prompt()
{
	int i;
	set_color( FISH_COLOR_NORMAL, FISH_COLOR_NORMAL );

	/*
	  Check if we need to reexecute the prompt command
	*/
	if( data->exec_prompt )
	{

		al_foreach( &prompt_list, (void (*)(const void *))&free );
		al_truncate( &prompt_list, 0 );

		if( data->prompt )
		{
			int was_interactive = is_interactive;
			is_interactive = 0;
			if( exec_subshell( data->prompt, &prompt_list ) == -1 )
			{
				/* If executing the prompt fails, make sure we at least don't print any junk */
				al_foreach( &prompt_list, (void (*)(const void *))&free );
				al_destroy( &prompt_list );
				al_init( &prompt_list );
			}
			is_interactive = was_interactive;			
		}

		data->prompt_width=calc_prompt_width( &prompt_list );

		data->exec_prompt = 0;
		reader_write_title();
	}

	/*
	  Write out the prompt strings
	*/

	for( i=0; i<al_get_count( &prompt_list); i++ )
	{
		writestr( (wchar_t *)al_get( &prompt_list, i ) );
	}
	set_color( FISH_COLOR_RESET, FISH_COLOR_RESET );

}

/**
   Write the whole command line (but not the prompt) to the screen. Do
   not set the cursor correctly afterwards.
*/
static void write_cmdline()
{
	int i;

	for( i=0; data->output[i]; i++ )
	{
		set_color_translated( data->output_color[i] );
		writech( data->output[i] );
	}
}

/**
   perm_left_cursor and parm_right_cursor don't seem to be defined as
   often as cursor_left and cursor_right, so we use this workalike.
*/
static void move_cursor( int steps )
{
	int i;

	if( steps < 0 ){
		for( i=0; i>steps; i--)
		{
			writembs(cursor_left);
		}
	}
	else
		for( i=0; i<steps; i++)
			writembs(cursor_right);
}


void reader_init()
{
	al_init( &current_filename);
}


void reader_destroy()
{
	al_destroy( &current_filename);
	if( readline_buffer )
	{
		sb_destroy( readline_buffer );
		free( readline_buffer );
		readline_buffer=0;
	}
}


void reader_exit( int do_exit )
{
	if( is_interactive )
		data->end_loop=do_exit;
	else
		end_loop=do_exit;
}

void repaint()
{
	int steps;

	calc_output();
	set_color( FISH_COLOR_RESET, FISH_COLOR_RESET );
	writech('\r');
	writembs(clr_eol);
	write_prompt();
	write_cmdline();

/*
  fwprintf( stderr, L"Width of \'%ls\' (length is %d): ",
  &data->buff[data->buff_pos],
  wcslen(&data->buff[data->buff_pos]));
  fwprintf( stderr, L"%d\n", my_wcswidth(&data->buff[data->buff_pos]));
*/

	steps = my_wcswidth( &data->output[data->output_pos]);
	if( steps )
		move_cursor( -steps );

	set_color( FISH_COLOR_NORMAL, -1 );
	reader_save_status();
}

/**
   Make sure color values are correct, and repaint if they are not.
*/
static void check_colors()
{
	reader_super_highlight_me_plenty( data->buff, data->new_color, data->buff_pos, 0 );
	if( memcmp( data->new_color, data->color, sizeof(int)*data->buff_len )!=0 )
	{
		memcpy( data->color, data->new_color,  sizeof(int)*data->buff_len );

		repaint();
	}
}

/**
   Stat stdout and stderr and save result.

   This should be done before calling a function that may cause output.
*/

static void reader_save_status()
{

#ifdef HAVE_FUTIMES
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

	futimes( 1, t );
	futimes( 2, t );
#endif

	fstat( 1, &prev_buff_1 );
	fstat( 2, &prev_buff_2 );
}

/**
   Stat stdout and stderr and compare result to previous result in
   reader_save_status. Repaint if modification time has changed.

   Unfortunately, for some reason this call seems to give a lot of
   false positives, at least under Linux.
*/

static void reader_check_status()
{
	fflush( stdout );
	fflush( stderr );

	fstat( 1, &post_buff_1 );
	fstat( 2, &post_buff_2 );

	int changed = ( prev_buff_1.st_mtime != post_buff_1.st_mtime ) ||
		( prev_buff_2.st_mtime != post_buff_2.st_mtime );

	if (room_for_usec( &post_buff_1))
	{
		changed = changed || ( (&prev_buff_1.st_mtime)[1] != (&post_buff_1.st_mtime)[1] ) ||
			( (&prev_buff_2.st_mtime)[1] != (&post_buff_2.st_mtime)[1] );
	}

	if( changed )
	{
		repaint();
		set_color( FISH_COLOR_RESET, FISH_COLOR_RESET );
	}
}

/**
   Remove the previous character in the character buffer and on the
   screen using syntax highlighting, etc.
*/
static void remove_backward()
{
	int wdt;

	if( data->buff_pos <= 0 )
		return;

	if( data->buff_pos < data->buff_len )
	{
		memmove( &data->buff[data->buff_pos-1],
				 &data->buff[data->buff_pos],
				 sizeof(wchar_t)*(data->buff_len-data->buff_pos+1) );

		memmove( &data->color[data->buff_pos-1],
				 &data->color[data->buff_pos],
				 sizeof(wchar_t)*(data->buff_len-data->buff_pos+1) );
	}
	data->buff_pos--;
	data->buff_len--;

	wdt=wcwidth(data->buff[data->buff_pos]);
	move_cursor(-wdt);
	data->buff[data->buff_len]='\0';
//	wcscpy(data->search_buff,data->buff);

	reader_super_highlight_me_plenty( data->buff,
									  data->new_color,
									  data->buff_pos,
									  0 );
	if( (!force_repaint()) && ( memcmp( data->new_color,
										data->color,
										sizeof(int)*data->buff_len )==0 ) &&
		( delete_character != 0) && (wdt==1) )
	{
		/*
		  Only do this if delete mode functions, and only for a column
		  wide characters, since terminfo seems to break for other
		  characters. This last check should be removed when terminfo
		  is fixed.
		*/
		if( enter_delete_mode != 0 )
			writembs(enter_delete_mode);
		writembs(delete_character);
		if( exit_delete_mode != 0 )
			writembs(exit_delete_mode);
	}
	else
	{
		memcpy( data->color,
				data->new_color,
				sizeof(int) * data->buff_len );

		repaint();
	}
}

/**
   Remove the current character in the character buffer and on the
   screen using syntax highlighting, etc.
*/
static void remove_forward()
{
	if( data->buff_pos >= data->buff_len )
		return;

	move_cursor(wcwidth(data->buff[data->buff_pos]));
	data->buff_pos++;

	remove_backward();
}

/**
   Insert the character into the command line buffer and print it to
   the screen using syntax highlighting, etc.
*/
static int insert_char( int c )
{

	if( !check_size() )
		return 0;

	/* Insert space for extra character at the right position */
	if( data->buff_pos < data->buff_len )
	{
		memmove( &data->buff[data->buff_pos+1],
				 &data->buff[data->buff_pos],
				 sizeof(wchar_t)*(data->buff_len-data->buff_pos) );

		memmove( &data->color[data->buff_pos+1],
				 &data->color[data->buff_pos],
				 sizeof(int)*(data->buff_len-data->buff_pos) );
	}
	/* Set character */
	data->buff[data->buff_pos]=c;

    /* Update lengths, etc */
	data->buff_pos++;
	data->buff_len++;
	data->buff[data->buff_len]='\0';

	/* Syntax highlight */

	reader_super_highlight_me_plenty( data->buff,
									  data->new_color,
									  data->buff_pos-1,
									  0 );
	data->color[data->buff_pos-1] = data->new_color[data->buff_pos-1];

	/* Check if the coloring has changed */
	if( (!force_repaint()) && ( memcmp( data->new_color,
										data->color,
										sizeof(int)*data->buff_len )==0 ) &&
		( insert_character ||
		  ( data->buff_pos == data->buff_len ) ||
		  enter_insert_mode) )
	{
		/*
		  Colors look ok, so we set the right color and insert a
		  character
		*/
		set_color_translated( data->color[data->buff_pos-1] );
		if( data->buff_pos < data->buff_len )
		{
			if( enter_insert_mode != 0 )
				writembs(enter_insert_mode);
			else
				writembs(insert_character);
			writech(c);
			if( insert_padding != 0 )
				writembs(insert_padding);
			if( exit_insert_mode != 0 )
				writembs(exit_insert_mode);
		}
		else
			writech(c);
		set_color( FISH_COLOR_NORMAL, -1 );
	}
	else
	{
		/* Nope, colors are off, so we repaint the entire command line */
		memcpy( data->color, data->new_color, sizeof(int) * data->buff_len );

		repaint();
	}
//	wcscpy(data->search_buff,data->buff);
	return 1;
}

/**
   Insert the characters of the string into the command line buffer
   and print them to the screen using syntax highlighting, etc.
*/
static int insert_str(wchar_t *str)
{
	int len = wcslen( str );
	if( len < 4 )
	{
		while( (*str)!=0 )
			if(!insert_char( *str++ ))
				return 0;
	}
	else
	{
		int old_len = data->buff_len;

		data->buff_len += len;
		check_size();

		/* Insert space for extra characters at the right position */
		if( data->buff_pos < old_len )
		{
			memmove( &data->buff[data->buff_pos+len],
					 &data->buff[data->buff_pos],
					 sizeof(wchar_t)*(data->buff_len-data->buff_pos) );
		}
		memmove( &data->buff[data->buff_pos], str, sizeof(wchar_t)*len );
		data->buff_pos += len;
		data->buff[data->buff_len]='\0';

		/* Syntax highlight */

		reader_super_highlight_me_plenty( data->buff,
										  data->new_color,
										  data->buff_pos-1,
										  0 );
		memcpy( data->color, data->new_color, sizeof(int) * data->buff_len );

		/* repaint */

		repaint();

	}
	return 1;
}

/**
   Calculate the length of the common prefix substring of two strings.
*/
static int comp_len( wchar_t *a, wchar_t *b )
{
	int i;
	for( i=0;
		 a[i] != '\0' && b[i] != '\0' && a[i]==b[i];
		 i++ )
		;
	return i;
}

/**
   Find the outermost quoting style of current token. Returns 0 if token is not quoted.

*/
static wchar_t get_quote( wchar_t *cmd, int l )
{
	int i=0;
	wchar_t res=0;

//	fwprintf( stderr, L"Woot %ls\n", cmd );

	while( 1 )
	{
		if( !cmd[i] )
			break;

		if( cmd[i] == L'\'' || cmd[i] == L'\"' )
		{
			wchar_t *end = quote_end( &cmd[i] );
			//fwprintf( stderr, L"Jump %d\n",  end-cmd );
			if(( end == 0 ) || (!*end) || (end-cmd > l))
			{
				res = cmd[i];
				break;
			}
			i = end-cmd+1;
		}
		else
			i++;

	}
	return res;
}

/**
   Calculates information on the parameter at the specified index.

   \param cmd The command to be analyzed
   \param pos An index in the string which is inside the parameter
   \param quote If not 0, store the type of quote this parameter has, can be either ', " or \\0, meaning the string is not quoted.
   \param offset If not 0, get_param will store a pointer to the beginning of the parameter.
   \param string If not 0, get_parm will store a copy of the parameter string as returned by the tokenizer.
   \param type If not 0, get_param will store the token type as returned by tok_last.
*/
static void get_param( wchar_t *cmd,
					   int pos,
					   wchar_t *quote,
					   wchar_t **offset,
					   wchar_t **string,
					   int *type )
{
	int prev_pos=0;
	wchar_t last_quote = '\0';
	int unfinished;

	tokenizer tok;
	tok_init( &tok, cmd, TOK_ACCEPT_UNFINISHED );

	for( ; tok_has_next( &tok ); tok_next( &tok ) )
	{
		if( tok_get_pos( &tok ) > pos )
			break;

		if( tok_last_type( &tok ) == TOK_STRING )
			last_quote = get_quote( tok_last( &tok ),
									pos - tok_get_pos( &tok ) );

		if( type != 0 )
			*type = tok_last_type( &tok );
		if( string != 0 )
			wcscpy( *string, tok_last( &tok ) );

		prev_pos = tok_get_pos( &tok );
	}

	tok_destroy( &tok );

	wchar_t c = cmd[pos];
	cmd[pos]=0;
	int cmdlen = wcslen( cmd );
	unfinished = (cmdlen==0);
	if( !unfinished )
	{
		unfinished = (quote != 0);

		if( !unfinished )
		{
			if( wcschr( L" \t\n\r", cmd[cmdlen-1] ) != 0 )
			{
				if( ( cmdlen == 1) || (cmd[cmdlen-2] != L'\\') )
				{
					unfinished=1;
				}
			}
		}
	}

	if( quote )
		*quote = last_quote;

	if( offset != 0 )
	{
		if( !unfinished )
		{
			while( (cmd[prev_pos] != 0) && (wcschr( L";|",cmd[prev_pos])!= 0) )
				prev_pos++;

			*offset = cmd + prev_pos;
		}
		else
		{
			*offset = cmd + pos;
		}
	}
	cmd[pos]=c;
}

/**
   Insert the string at the current cursor position. The function
   checks if the string is quoted or not and correctly escapes the
   string.

   \param val the string to insert
   \param is_complete Whether this completion is the whole string or
   just the common prefix of several completions. If the former, end by
   printing a space (and an end quote if the parameter is quoted).
*/
static void completion_insert( wchar_t *val, int is_complete )
{
	wchar_t *replaced;

	wchar_t quote;

	get_param( data->buff,
			   data->buff_pos,
			   &quote,
			   0, 0, 0 );

	if( quote == L'\0' )
	{
		replaced = escape( val, 1 );
	}
	else
	{
		int unescapable=0;

		wchar_t *pin, *pout;

		replaced = pout =
			malloc( sizeof(wchar_t)*(wcslen(val) + 1) );

		for( pin=val; *pin; pin++ )
		{
			switch( *pin )
			{
				case L'\n':
				case L'\t':
				case L'\b':
				case L'\r':
					unescapable=1;
					break;
				default:
					*pout++ = *pin;
					break;
			}
		}
		if( unescapable )
		{
			free( replaced );
			wchar_t *tmp = escape( val, 1 );
			replaced = wcsdupcat( L" ", tmp );
			free( tmp);
			replaced[0]=quote;
		}
		else
			*pout = 0;
	}

	if( insert_str( replaced ) )
	{

		if( is_complete ) /* Print trailing space since this is the only completion */
		{

			if( (quote) &&
				(data->buff[data->buff_pos] != quote ) ) /* This is a quoted parameter, first print a quote */
			{
				insert_char( quote );
			}
			insert_char( L' ' );
		}
	}

	free(replaced);
}

/**
   Run the fish_pager command to display the completion list, and
   insert the result into the backbuffer.
*/

static void run_pager( wchar_t *prefix, int is_quoted, array_list_t *comp )
{
	int i;
	string_buffer_t cmd;
	wchar_t * prefix_esc;

	if( !prefix || (wcslen(prefix)==0))
		prefix_esc = wcsdup(L"\"\"");
	else
		prefix_esc = escape( prefix,1);

	sb_init( &cmd );
	sb_printf( &cmd,
			   L"fish_pager %d %ls",
			   is_quoted,
			   prefix_esc );

	free( prefix_esc );

	for( i=0; i<al_get_count( comp); i++ )
	{
		wchar_t *el = escape( (wchar_t*)al_get( comp, i ),1);
		sb_printf( &cmd, L" %ls", el );
		free(el);
	}

	term_donate();

	io_data_t *out = io_buffer_create();

	eval( (wchar_t *)cmd.buff, out, TOP);
	term_steal();

	io_buffer_read( out );

	sb_destroy( &cmd );


	int nil=0;
	b_append( out->param2.out_buffer, &nil, 1 );

	wchar_t *tmp;
	wchar_t *str = str2wcs((char *)out->param2.out_buffer->buff);

	if( str )
	{
		for( tmp = str + wcslen(str)-1; tmp >= str; tmp-- )
		{
			input_unreadch( *tmp );
		}
		free( str );
	}

	io_buffer_destroy( out);

}

/**
   Handle the list of completions. This means the following:

   - If the list is empty, flash the terminal.
   - If the list contains one element, write the whole element, and if
   the element does not end on a '/', '@', ':', or a '=', also write a trailing
   space.
   - If the list contains multiple elements with a common prefix, write
   the prefix.
   - If the list contains multiple elements without
   a common prefix, call run_pager to display a list of completions

   \param comp the list of completion strings
*/


static int handle_completions( array_list_t *comp )
{
	int i;

	if( al_get_count( comp ) == 0 )
	{
		if( flash_screen != 0 )
			writembs( flash_screen );
		return 0;
	}
	else if( al_get_count( comp ) == 1 )
	{
		wchar_t *comp_str = wcsdup((wchar_t *)al_get( comp, 0 ));
		wchar_t *woot = wcschr( comp_str, COMPLETE_SEP );
		if( woot != 0 )
			*woot = L'\0';
		completion_insert( comp_str,
						   ( wcslen(comp_str) == 0 ) ||
						   ( wcschr( L"/=@:",
									 comp_str[wcslen(comp_str)-1] ) == 0 ) );
		free( comp_str );
		return 1;
	}
	else
	{
		wchar_t *base = wcsdup( (wchar_t *)al_get( comp, 0 ) );
		int len = wcslen( base );
		for( i=1; i<al_get_count( comp ); i++ )
		{
			int new_len = comp_len( base, (wchar_t *)al_get( comp, i ) );
			len = new_len < len ? new_len: len;
		}
		if( len > 0 )
		{
			base[len]=L'\0';
			wchar_t *woot = wcschr( base, COMPLETE_SEP );
			if( woot != 0 )
				*woot = L'\0';
			completion_insert(base, 0);
		}
		else
		{
			/*
			  There is no common prefix in the completions, and show_list
			  is true, so we print the list
			*/
			int len;
			wchar_t * prefix;
			wchar_t * prefix_start;
			get_param( data->buff,
					   data->buff_pos,
					   0,
					   &prefix_start,
					   0,
					   0 );

			len = &data->buff[data->buff_pos]-prefix_start+1;

			if( len <= PREFIX_MAX_LEN )
			{
				prefix = malloc( sizeof(wchar_t)*(len+1) );
				wcslcpy( prefix, prefix_start, len );
				prefix[len]=L'\0';
			}
			else
			{
				wchar_t tmp[2]=
					{
						ellipsis_char,
						0
					}
				;

				prefix = wcsdupcat( tmp,
									prefix_start + (len - PREFIX_MAX_LEN) );
				prefix[PREFIX_MAX_LEN] = 0;

			}

			{
				int is_quoted;

				wchar_t quote;
				get_param( data->buff, data->buff_pos, &quote, 0, 0, 0 );
				is_quoted = (quote != L'\0');

				writech(L'\n');

				run_pager( prefix, is_quoted, comp );


				/*
				  Try to print a list of completions. First try with five
				  columns, then four, etc. completion_try_print always
				  succeeds with one column.
				*/
/*
 */
			}

			free( prefix );
			repaint();

		}

		free( base );
		return len;
	}
}

/**
   Reset the terminal. This function is placed in the list of
   functions to call when exiting by using the atexit function. It
   checks whether it is the original parent process that is exiting
   and not a subshell, and if it is the parent, it restores the
   terminal.
*/
static void exit_func()
{
	if( getpid() == original_pid )
		tcsetattr(0, TCSANOW, &saved_modes);
}

/**
   Initialize data for interactive use
*/
static void reader_interactive_init()
{
	/* See if we are running interactively.  */
	pid_t shell_pgid;

	input_init();
	kill_init();
	shell_pgid = getpgrp ();

	/*
	  This should enable job control on fish, even if our parent did
	  not enable it for us.
	*/

	/* Loop until we are in the foreground.  */
	while (tcgetpgrp( 0 ) != shell_pgid)
	{
		kill (- shell_pgid, SIGTTIN);
	}

	/* Put ourselves in our own process group.  */
	shell_pgid = getpid ();
	if( getpgrp() != shell_pgid )
	{
		if (setpgid (shell_pgid, shell_pgid) < 0)
		{
			debug( 1,
				   _( L"Couldn't put the shell in its own process group" ));
			wperror( L"setpgid" );
			exit (1);
		}
	}

	/* Grab control of the terminal.  */
	if( tcsetpgrp (STDIN_FILENO, shell_pgid) )
	{
		debug( 1,
			   _( L"Couldn't grab control of terminal" ) );
		wperror( L"tcsetpgrp" );
		exit(1);
	}


	al_init( &prompt_list );
	history_init();

	common_handle_winch(0);

	tcgetattr(0,&shell_modes);        /* get the current terminal modes */
	memcpy( &saved_modes,
			&shell_modes,
			sizeof(saved_modes));     /* save a copy so we can reset the terminal later */

	shell_modes.c_lflag &= ~ICANON;   /* turn off canonical mode */
	shell_modes.c_lflag &= ~ECHO;     /* turn off echo mode */
    shell_modes.c_cc[VMIN]=1;
    shell_modes.c_cc[VTIME]=0;

    if( tcsetattr(0,TCSANOW,&shell_modes))      /* set the new modes */
    {
        wperror(L"tcsetattr");
        exit(1);
    }

	/* 
	   We need to know our own pid so we'll later know if we are a
	   fork 
	*/
	original_pid = getpid();

	if( atexit( &exit_func ) )
		debug( 1, _( L"Could not set exit function" ) );

	env_set( L"_", L"fish", ENV_GLOBAL );
}

/**
   Destroy data for interactive use
*/
static void reader_interactive_destroy()
{
	kill_destroy();
	al_foreach( &prompt_list, (void (*)(const void *))&free );
	al_destroy( &prompt_list );
	history_destroy();

	writestr( L"\n" );
	set_color( FISH_COLOR_RESET, FISH_COLOR_RESET );
	input_destroy();
}


void reader_sanity_check()
{
	if( is_interactive)
	{
		if(!( data->buff_pos <= data->buff_len ))
			sanity_lose();
		if(!( data->buff_len == wcslen( data->buff ) ))
			sanity_lose();
	}
}

void reader_replace_current_token( wchar_t *new_token )
{

	const wchar_t *begin, *end;
	string_buffer_t sb;
	int new_pos;

	/*
	  Find current token
	*/
	parse_util_token_extent( data->buff, data->buff_pos, &begin, &end, 0, 0 );

	if( !begin || !end )
		return;

//	fwprintf( stderr, L"%d %d, %d\n", begin-data->buff, end-data->buff, data->buff_len );

	/*
	  Make new string
	*/
	sb_init( &sb );
	sb_append_substring( &sb, data->buff, begin-data->buff );
	sb_append( &sb, new_token );
	sb_append( &sb, end );

	new_pos = (begin-data->buff) + wcslen(new_token);

	reader_set_buffer( (wchar_t *)sb.buff, new_pos );
	sb_destroy( &sb );

}


/**
   Set the specified string from the history as the current buffer. Do
   not modify prefix_width.
*/
static void handle_history( const wchar_t *new_str )
{
	data->buff_len = wcslen( new_str );
	check_size();
	wcscpy( data->buff, new_str );
	data->buff_pos=wcslen(data->buff);
	reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );

	repaint();
}

/**
   Check if the specified string is contained in the list, using
   wcscmp as a comparison function
*/
static int contains( const wchar_t *needle,
					 array_list_t *haystack )
{
	int i;
	for( i=0; i<al_get_count( haystack ); i++ )
	{
		if( !wcscmp( needle, al_get( haystack, i ) ) )
			return 1;
	}
	return 0;

}

/**
   Reset the data structures associated with the token search
*/
static void reset_token_history()
{
	const wchar_t *begin, *end;

	parse_util_token_extent( data->buff, data->buff_pos, &begin, &end, 0, 0 );
	if( begin )
	{
		wcslcpy(data->search_buff, begin, end-begin+1);
	}
	else
		data->search_buff[0]=0;

	data->token_history_pos = -1;
	data->search_pos=0;
	al_foreach( &data->search_prev, (void (*)(const void *))&free );
	al_truncate( &data->search_prev, 0 );
	al_push( &data->search_prev, wcsdup( data->search_buff ) );
}


/**
   Handles a token search command.

   \param forward if the search should be forward or reverse
   \param reset whether the current token should be made the new search token
*/
static void handle_token_history( int forward, int reset )
{
	wchar_t *str=0;
	int current_pos;
	tokenizer tok;

	if( reset )
	{
		/*
		  Start a new token search using the current token
		*/
		reset_token_history();

	}


	current_pos  = data->token_history_pos;

	if( forward || data->search_pos < (al_get_count( &data->search_prev )-1) )
	{
		if( forward )
		{
			if( data->search_pos > 0 )
			{
				data->search_pos--;
			}
			str = (wchar_t *)al_get( &data->search_prev, data->search_pos );
		}
		else
		{
			data->search_pos++;
			str = (wchar_t *)al_get( &data->search_prev, data->search_pos );
		}

		reader_replace_current_token( str );
		reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );
		repaint();
	}
	else
	{
		if( current_pos == -1 )
		{
			/*
			  Move to previous line
			*/
			free( (void *)data->token_history_buff );
			data->token_history_buff = wcsdup( history_prev_match(L"") );
			current_pos = wcslen(data->token_history_buff);
		}

		if( ! wcslen( data->token_history_buff ) )
		{
			/*
			  We have reached the end of the history - check if the
			  history already contains the search string itself, if so
			  return, otherwise add it.
			*/

			const wchar_t *last = al_get( &data->search_prev, al_get_count( &data->search_prev ) -1 );
			if( wcscmp( last, data->search_buff ) )
			{
				str = wcsdup(data->search_buff);
			}
			else
			{
				return;
			}
		}
		else
		{

			debug( 3, L"new '%ls'", data->token_history_buff );

			for( tok_init( &tok, data->token_history_buff, TOK_ACCEPT_UNFINISHED );
				 tok_has_next( &tok);
				 tok_next( &tok ))
			{
				switch( tok_last_type( &tok ) )
				{
					case TOK_STRING:
					{
						if( wcsstr( tok_last( &tok ), data->search_buff ) )
						{
							debug( 3, L"Found token at pos %d\n", tok_get_pos( &tok ) );
							if( tok_get_pos( &tok ) >= current_pos )
							{
								break;
							}
							debug( 3, L"ok pos" );

							if( !contains( tok_last( &tok ), &data->search_prev ) )
							{
								free(str);
								data->token_history_pos = tok_get_pos( &tok );
								str = wcsdup(tok_last( &tok ));
							}

						}
					}
				}
			}

			tok_destroy( &tok );
		}

		if( str )
		{
			reader_replace_current_token( str );
			reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );
			repaint();
			al_push( &data->search_prev, str );
			data->search_pos = al_get_count( &data->search_prev )-1;
		}
		else
		{
			data->token_history_pos=-1;
			handle_token_history( 0, 0 );
		}
	}
}


/**
   Move buffer position one word or erase one word. This function
   updates both the internal buffer and the screen. It is used by
   M-left, M-right and ^W to do block movement or block erase.

   \param dir Direction to move/erase. 0 means move left, 1 means move right.
   \param erase Whether to erase the characters along the way or only move past them.

*/
static void move_word( int dir, int erase )
{
	int end_buff_pos=data->buff_pos;
	int mode=0;
	int step = dir?1:-1;

	while( mode < 2 )
	{
		if( !dir )
		{
			if( end_buff_pos == 0 )
				break;
		}
		else
		{
			if( end_buff_pos == data->buff_len )
				break;
		}
		end_buff_pos+=step;

		if( end_buff_pos < data->buff_len )
		{
			switch( mode )
			{
				case 0:
					if( iswalnum(data->buff[end_buff_pos] ) )
						mode++;
					break;

				case 1:
					if( !iswalnum(data->buff[end_buff_pos] ) )
					{
						if( !dir )
							end_buff_pos -= step;
						mode++;
					}
					break;
/*
  case 2:
  if( !iswspace(data->buff[end_buff_pos] ) )
  {
  mode++;
  if( !dir )
  end_buff_pos-=step;
  }
  break;
*/
			}
		}

		if( mode==2)
			break;

	}

	if( erase )
	{
		int remove_count = abs(data->buff_pos - end_buff_pos);
		int first_char = mini( data->buff_pos, end_buff_pos );
		wchar_t *woot = wcsndup( data->buff + first_char, remove_count);
//		fwprintf( stderr, L"Remove from %d to %d\n", first_char, first_char+remove_count );

		kill_add( woot );
		free( woot );
		memmove( data->buff + first_char, data->buff + first_char+remove_count, sizeof(wchar_t)*(data->buff_len-first_char-remove_count) );
		data->buff_len -= remove_count;
		data->buff_pos = first_char;
		data->buff[data->buff_len]=0;

		reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );

		repaint();
	}
	else
	{
/*		move_cursor(end_buff_pos-data->buff_pos);
  data->buff_pos = end_buff_pos;
*/
		if( end_buff_pos < data->buff_pos )
		{
			while( data->buff_pos != end_buff_pos )
			{
				data->buff_pos--;
				move_cursor( -wcwidth(data->buff[data->buff_pos]));
			}
		}
		else
		{
			while( data->buff_pos != end_buff_pos )
			{
				move_cursor( wcwidth(data->buff[data->buff_pos]));
				data->buff_pos++;
				check_colors();
			}
		}

		repaint();
//		check_colors();
	}
}


wchar_t *reader_get_buffer()
{
	return data?data->buff:0;
}

void reader_set_buffer( wchar_t *b, int p )
{
	int l = wcslen( b );

	if( !data )
		return;

	data->buff_len = l;
	check_size();
	wcscpy( data->buff, b );

	if( p>=0 )
	{
		data->buff_pos=p;
	}
	else
	{
		data->buff_pos=l;
//		fwprintf( stderr, L"Pos %d\n", l );
	}

	reader_super_highlight_me_plenty( data->buff,
									  data->color,
									  data->buff_pos,
									  0 );
}


int reader_get_cursor_pos()
{
	if( !data )
		return -1;

	return data->buff_pos;
}


void reader_run_command( wchar_t *cmd )
{

	wchar_t *ft;

	ft= tok_first( cmd );

	if( ft != 0 )
		env_set( L"_", ft, ENV_GLOBAL );
	free(ft);

	reader_write_title();

	term_donate();

	eval( cmd, 0, TOP );
	job_reap( 1 );

	term_steal();

	env_set( L"_", L"fish", ENV_GLOBAL );

#ifdef HAVE__PROC_SELF_STAT
	proc_update_jiffies();
#endif


}


/**
   Test if the given shell command contains errors. Uses parser_test
   for testing.
*/

static int shell_test( wchar_t *b )
{
	return !wcslen(b);
}

/**
   Test if the given string contains error. Since this is the error
   detection for general purpose, there are no invalid strings, so
   this function always returns false.
*/
static int default_test( wchar_t *b )
{
	return 0;
}

void reader_push( wchar_t *name )
{
	reader_data_t *n = calloc( 1, sizeof( reader_data_t ) );
	n->name = wcsdup( name );
	n->next = data;
	data=n;
	check_size();
	data->buff[0]=data->search_buff[0]=0;
	data->exec_prompt=1;

	if( data->next == 0 )
	{
		reader_interactive_init();
	}
	reader_set_highlight_function( &highlight_universal );
	reader_set_test_function( &default_test );
	reader_set_prompt( L"" );
	history_set_mode( name );

	al_init( &data->search_prev );
	data->token_history_buff=0;

}

void reader_pop()
{
	reader_data_t *n = data;

	if( data == 0 )
	{
		debug( 0, _( L"Pop null reader block" ) );
		sanity_lose();
		return;
	}

	data=data->next;

	free(n->name );
	free( n->prompt );
	free( n->buff );
	free( n->color );
	free( n->new_color );
	free( n->search_buff );
	free( n->output );
	free( n->output_color );

	/*
	  Clean up after history search
	*/
	al_foreach( &n->search_prev, (void (*)(const void *))&free );
	al_destroy( &n->search_prev );
    free( (void *)n->token_history_buff);

	free(n);

	if( data == 0 )
	{
		reader_interactive_destroy();
	}
	else
	{
		history_set_mode( data->name );
		data->exec_prompt=1;
	}
}

void reader_set_prompt( wchar_t *new_prompt )
{
 	free( data->prompt );
	data->prompt=wcsdup(new_prompt);
}

void reader_set_complete_function( void (*f)( const wchar_t *,
											  array_list_t * ) )
{
	data->complete_func = f;
}

void reader_set_highlight_function( void (*f)( wchar_t *,
											   int *,
											   int,
											   array_list_t * ) )
{
	data->highlight_func = f;
}

void reader_set_test_function( int (*f)( wchar_t * ) )
{
	data->test_func = f;
}

/**
   Call specified external highlighting function and then do search
   highlighting.
*/
static void reader_super_highlight_me_plenty( wchar_t * buff, int *color, int pos, array_list_t *error )
{
	data->highlight_func( buff, color, pos, error );
	if( wcslen(data->search_buff) )
	{
		wchar_t * match = wcsstr( buff, data->search_buff );
		if( match )
		{
			int start = match-buff;
			int count = wcslen(data->search_buff );
			int i;
//			fwprintf( stderr, L"WEE color from %d to %d\n", start, start+count );

			for( i=0; i<count; i++ )
			{
				/*
				  Do not overwrite previous highlighting color
				*/
				if( color[start+i]>>8 == 0 )
				{
					color[start+i] |= HIGHLIGHT_SEARCH_MATCH<<8;
				}
			}
		}
	}
}


int exit_status()
{
	if( is_interactive )
		return first_job == 0 && data->end_loop;
	else
		return end_loop;
}

/**
   Read interactively. Read input from stdin while providing editing
   facilities.
*/
static int read_i()
{
	int prev_end_loop=0;

	reader_push(L"fish");
	reader_set_complete_function( &complete );
	reader_set_highlight_function( &highlight_shell );
	reader_set_test_function( &shell_test );

	data->prompt_width=60;

	while( (!data->end_loop) && (!sanity_check()) )
	{
		wchar_t *tmp;

		if( function_exists( L"fish_prompt" ) )
			reader_set_prompt( L"fish_prompt" );
		else
			reader_set_prompt( DEFAULT_PROMPT );

		/*
		  Put buff in temporary string and clear buff, so
		  that we can handle a call to reader_set_buffer
		  during evaluation.
		*/

		tmp = wcsdup( reader_readline() );

		data->buff_pos=data->buff_len=0;
		data->buff[data->buff_len]=L'\0';
		reader_run_command( tmp );
		free( tmp );

		if( data->end_loop)
		{
			if( !prev_end_loop && first_job != 0 )
			{
				writestr(_( L"There are stopped jobs\n" ));
				write_prompt();
				data->end_loop = 0;
				prev_end_loop=1;
			}
		}
		else
		{
			prev_end_loop=0;
		}
		error_reset();

	}
	reader_pop();
	return 0;
}

/**
   Test if there are bytes available for reading on the specified file
   descriptor
*/
static int can_read( int fd )
{
	struct timeval can_read_timeout = { 0, 0 };
	fd_set fds;

	FD_ZERO(&fds);
    FD_SET(fd, &fds);
    return select(fd + 1, &fds, 0, 0, &can_read_timeout) == 1;
}

/**
   Test if the specified character is in the private use area that
   fish uses to store internal characters
*/
static int wchar_private( wchar_t c )
{
	return ( (c >= 0xe000) && (c <= 0xf8ff ) );
}

wchar_t *reader_readline()
{

	wint_t c;
	int i;
	int last_char=0, yank=0;
	wchar_t *yank_str;
	array_list_t comp;
	int comp_empty=1;
	int finished=0;
	struct termios old_modes;

	check_size();
	data->search_buff[0]=data->buff[data->buff_len]='\0';


	al_init( &comp );

	data->exec_prompt=1;

	reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );
	repaint();

	tcgetattr(0,&old_modes);        /* get the current terminal modes */
	if( tcsetattr(0,TCSANOW,&shell_modes))      /* set the new modes */
	{
        wperror(L"tcsetattr");
        exit(1);
    }

	while( !finished && !data->end_loop)
	{

		/*
		  Save the terminal status so we know if we have to redraw
		*/

		reader_save_status();

		/*
		  Sometimes strange input sequences seem to generate a zero
		  byte. I believe these simply mean a character was pressed
		  but it should be ignored. (Example: Trying to add a tilde
		  (~) to digit)
		*/
		while( 1 )
		{
			c=input_readch();
			if( ( (!wchar_private(c))) && (c>31) && (c != 127) )
			{
				if( can_read(0) )
				{

					wchar_t arr[READAHEAD_MAX+1];
					int i;

					memset( arr, 0, sizeof( arr ) );
					arr[0] = c;

					for( i=1; i<READAHEAD_MAX; i++ )
					{

						if( !can_read( 0 ) )
						{
							c = 0;
							break;
						}
						c = input_readch();
						if( (!wchar_private(c)) && (c>31) && (c != 127) )
						{
							arr[i]=c;
							c=0;
						}
						else
							break;
					}

					insert_str( arr );

				}
			}

			if( c != 0 )
				break;
		}

		reader_check_status();

		if( (last_char == R_COMPLETE) && (c != R_COMPLETE) && (!comp_empty) )
		{
			al_foreach( &comp, (void (*)(const void *))&free );
			al_truncate( &comp, 0 );
			comp_empty = 1;
		}

		if( last_char != R_YANK && last_char != R_YANK_POP )
			yank=0;

		switch( c )
		{

			/* go to beginning of line*/
			case R_BEGINNING_OF_LINE:
			{
				data->buff_pos = 0;

				repaint();
				break;
			}

			/* go to EOL*/
			case R_END_OF_LINE:
			{
				data->buff_pos = data->buff_len;

				repaint();
				break;
			}

			case R_NULL:
			{
				data->exec_prompt=1;
				repaint();
				break;
			}

			/* complete */
			case R_COMPLETE:
			{

//					fwprintf( stderr, L"aaa\n" );
				if( !data->complete_func )
					break;

				if( !comp_empty && last_char == R_COMPLETE )
					break;

 				if( comp_empty )
				{
					const wchar_t *begin, *end;
					wchar_t *buffcpy;

					parse_util_cmdsubst_extent( data->buff, data->buff_pos, &begin, &end );

					int len = data->buff_pos - (data->buff - begin);
					buffcpy = wcsndup( begin, len );

					//fwprintf( stderr, L"String is %ls\n", buffcpy );

					reader_save_status();
					data->complete_func( buffcpy, &comp );
					reader_check_status();

					sort_list( &comp );
					remove_duplicates( &comp );

					free( buffcpy );
				}
				if( (comp_empty =
					 handle_completions( &comp ) ) )
				{
					al_foreach( &comp, (void (*)(const void *))&free );
					al_truncate( &comp, 0 );
				}

				break;
			}

			/* kill*/
			case R_KILL_LINE:
			{
				kill_add( &data->buff[data->buff_pos] );
				data->buff_len = data->buff_pos;
				data->buff[data->buff_len]=L'\0';


				repaint();
//				wcscpy(data->search_buff,data->buff);
				break;
			}

			case R_BACKWARD_KILL_LINE:
			{
				wchar_t *str = wcsndup( data->buff, data->buff_pos );
				if( !str )
					die_mem();

				kill_add( str );
				free( str );

				data->buff_len = wcslen(data->buff +data->buff_pos);
				memmove( data->buff, data->buff +data->buff_pos, sizeof(wchar_t)*data->buff_len );
				data->buff[data->buff_len]=L'\0';
				data->buff_pos=0;
				reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );

				repaint();
//				wcscpy(data->search_buff,data->buff);
				break;
			}

			case R_KILL_WHOLE_LINE:
			{
				kill_add( data->buff );
				data->buff_len = data->buff_pos = 0;
				data->buff[data->buff_len]=L'\0';
				reader_super_highlight_me_plenty( data->buff, data->color, data->buff_pos, 0 );

				repaint();
//				wcscpy(data->search_buff,data->buff);
				break;
			}

			/* yank*/
			case R_YANK:
			{	yank_str = kill_yank();
				insert_str( yank_str );
				yank = wcslen( yank_str );
//				wcscpy(data->search_buff,data->buff);
				break;
			}

			/* rotate killring*/
			case R_YANK_POP:
			{
				if( yank )
				{
					for( i=0; i<yank; i++ )
						remove_backward();

					yank_str = kill_yank_rotate();
					insert_str(yank_str);
					yank = wcslen(yank_str);
				}
				break;
			}

			/* Escape was pressed */
			case L'\e':
			{
				if( *data->search_buff )
				{
					if( data->token_history_pos==-1 )
					{
						history_reset();
						reader_set_buffer( data->search_buff,
										   wcslen(data->search_buff ) );
					}
					else
					{
						reader_replace_current_token( data->search_buff );
					}
					*data->search_buff=0;
					check_colors();

				}

				break;
			}

			/* delete backward*/
			case R_BACKWARD_DELETE_CHAR:
			{
				remove_backward();
				break;
			}

			/* delete forward*/
			case R_DELETE_CHAR:
			{
				remove_forward();
				break;
			}

			/* exit, but only if line is empty */
			case R_EXIT:
			{
				if( data->buff_len == 0 )
				{
					writestr( L"\n" );
					data->end_loop=1;
				}
				break;
			}

			/* Newline, evaluate*/
			case L'\n':
			{
				data->buff[data->buff_len]=L'\0';

				if( !data->test_func( data->buff ) )
				{

					if( wcslen( data->buff ) )
					{
//						wcscpy(data->search_buff,L"");
						history_add( data->buff );
					}
					finished=1;
					data->buff_pos=data->buff_len;
					check_colors();
					writestr( L"\n" );
				}
				else
				{
					writech('\r');
					writembs(clr_eol);
					writech('\n');
					repaint();
				}

				break;
			}

			/* History up */
			case R_HISTORY_SEARCH_BACKWARD:
			{
				if( (last_char != R_HISTORY_SEARCH_BACKWARD) &&
					(last_char != R_HISTORY_SEARCH_FORWARD) &&
					(last_char != R_FORWARD_CHAR) &&
					(last_char != R_BACKWARD_CHAR) )

				{
					wcscpy(data->search_buff, data->buff );
					data->search_buff[data->buff_pos]=0;
				}

				handle_history(history_prev_match(data->search_buff));
				break;
			}

			/* History down */
			case R_HISTORY_SEARCH_FORWARD:
			{
				if( (last_char != R_HISTORY_SEARCH_BACKWARD) &&
					(last_char != R_HISTORY_SEARCH_FORWARD) &&
					(last_char != R_FORWARD_CHAR) &&
					(last_char != R_BACKWARD_CHAR) )
				{
					wcscpy(data->search_buff, data->buff );
					data->search_buff[data->buff_pos]=0;
				}

				handle_history(history_next_match(data->search_buff));
				break;
			}

			/* Token search for a earlier match */
			case R_HISTORY_TOKEN_SEARCH_BACKWARD:
			{
				int reset=0;
				if( (last_char != R_HISTORY_TOKEN_SEARCH_BACKWARD) &&
					(last_char != R_HISTORY_TOKEN_SEARCH_FORWARD) )
				{
					reset=1;
				}

				handle_token_history( 0, reset );

				break;
			}

			/* Token search for a later match */
			case R_HISTORY_TOKEN_SEARCH_FORWARD:
			{
				int reset=0;

				if( (last_char != R_HISTORY_TOKEN_SEARCH_BACKWARD) &&
					(last_char != R_HISTORY_TOKEN_SEARCH_FORWARD) )
				{
					reset=1;
				}

				handle_token_history( 1, reset );

				break;
			}


			/* Move left*/
			case R_BACKWARD_CHAR:
				if( data->buff_pos > 0 )
				{
					data->buff_pos--;
					if( !force_repaint() )
					{
						move_cursor( -wcwidth(data->buff[data->buff_pos]));
						check_colors();
					}
					else
					{
						repaint();
					}
				}
				break;

				/* Move right*/
			case R_FORWARD_CHAR:
			{
				if( data->buff_pos < data->buff_len )
				{
					if( !force_repaint() )
					{
						move_cursor( wcwidth(data->buff[data->buff_pos]));
						data->buff_pos++;
						check_colors();
					}
					else
					{
						data->buff_pos++;

						repaint();
					}
				}
				break;
			}

			case R_DELETE_LINE:
			{
				data->buff[0]=0;
				data->buff_len=0;
				data->buff_pos=0;
				repaint();
				break;
			}

			/* kill one word left */
			case R_BACKWARD_KILL_WORD:
			{
				move_word(0,1);
				break;
			}

			/* kill one word right */
			case R_KILL_WORD:
			{
				move_word(1,1);
				break;
			}

			/* move one word left*/
			case R_BACKWARD_WORD:
			{
				move_word(0,0);
				break;
			}

			/* move one word right*/
			case R_FORWARD_WORD:
			{
				move_word( 1,0);
				break;
			}

			case R_CLEAR_SCREEN:
			{
				writembs( clear_screen );

				repaint();
				break;
			}

			case R_BEGINNING_OF_HISTORY:
			{
				history_first();
				break;
			}

			case R_END_OF_HISTORY:
			{
				history_reset();
				break;
			}

			/* Other, if a normal character, we add it to the command */
			default:
			{
				if( (!wchar_private(c)) && (c>31) && (c != 127) )
					insert_char( c );
				else
					debug( 0, _( L"Unknown keybinding %d" ), c );
				break;
			}

		}

		if( (c != R_HISTORY_SEARCH_BACKWARD) &&
			(c != R_HISTORY_SEARCH_FORWARD) &&
			(c != R_HISTORY_TOKEN_SEARCH_BACKWARD) &&
			(c != R_HISTORY_TOKEN_SEARCH_FORWARD) &&
			(c != R_FORWARD_CHAR) &&
			(c != R_BACKWARD_CHAR) )
		{
			data->search_buff[0]=0;
			history_reset();
			data->token_history_pos=-1;
		}


		last_char = c;
	}

	al_destroy( &comp );
    if( tcsetattr(0,TCSANOW,&old_modes))      /* return to previous mode */
    {
        wperror(L"tcsetattr");
        exit(1);
    }

	set_color( FISH_COLOR_RESET, FISH_COLOR_RESET );

	return data->buff;
}

/**
   Read non-interactively.  Read input from stdin without displaying
   the prompt, using syntax highlighting. This is used for reading
   scripts and init files.
*/
static int read_ni( int fd )
{
	FILE *in_stream;
	wchar_t *buff=0;
	buffer_t acc;

	int des = fd == 0 ? dup(0) : fd;
	int res=0;

	if (des == -1)
	{
		wperror( L"dup" );
		return 1;
	}

	b_init( &acc );

	in_stream = fdopen( des, "r" );
	if( in_stream != 0 )
	{
		wchar_t *str;
		int acc_used;

		while(!feof( in_stream ))
		{
			char buff[4096];
			int c;

			c = fread(buff, 1, 4096, in_stream);
			if( ferror( in_stream ) )
			{
				debug( 1,
					   _( L"Error while reading commands" ) );

				/*
				  Reset buffer. We won't evaluate incomplete files.
				*/
				acc.used=0;
				break;
			}

			b_append( &acc, buff, c );
		}
		b_append( &acc, "\0", 1 );
		acc_used = acc.used;
		str = str2wcs( acc.buff );
		b_destroy( &acc );

		if(	fclose( in_stream ))
		{
			debug( 1,
				   _( L"Error while closing input stream" ) );
			wperror( L"fclose" );
			res = 1;
		}

//		fwprintf( stderr, L"Woot is %d chars\n", wcslen( acc.buff ) );

		if( str )
		{
			if( !parser_test( str, 1 ) )
			{
				//fwprintf( stderr, L"We parse it\n" );
				eval( str, 0, TOP );
			}
			else
			{
				/*
				  No error reporting - parser_test did that for us
				*/
				res = 1;
			}
			free( str );
		}
		else
		{
			if( acc_used > 1 )
			{
				debug( 1,
					   _( L"Could not convert input. Read %d bytes." ),
					   acc_used-1 );
			}
			else
			{
				debug( 1,
					   _( L"Could not read input stream" ) );
			}
			res=1;
		}

	}
	else
	{
		debug( 1,
			   _( L"Error while opening input stream" ) );
		wperror( L"fdopen" );
		free( buff );
		res=1;
	}
	error_reset();
	return res;
}

int reader_read( int fd )
{
	int res;
	/*
	  If reader_read is called recursively through the '.' builtin,
	  we need to preserve is_interactive, so we save the
	  original state. We also update the signal handlers.
	*/
	int shell_was_interactive = is_interactive;

	is_interactive = ((fd == 0) && isatty(STDIN_FILENO));
	signal_set_handlers();

	res= is_interactive?read_i():read_ni( fd );

	/*
	  If the exit command was called in a script, only exit the
	  script, not the program
	*/
	end_loop = 0;

	is_interactive = shell_was_interactive;
	signal_set_handlers();
	return res;
}
