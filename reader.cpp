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
#include <algorithm>

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

#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>
#include <wctype.h>
#include <stack>

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

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <wchar.h>

#include <assert.h>


#include "fallback.h"
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
#include "screen.h"
#include "iothread.h"
#include "intern.h"
#include "path.h"

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
#define DEFAULT_PROMPT L"echo \"$USER@\"; hostname|cut -d . -f 1; echo \" \"; pwd; printf '> ';"

/**
   The name of the function that prints the fish prompt
 */
#define PROMPT_FUNCTION_NAME L"fish_prompt"

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
   A mode for calling the reader_kill function. In this mode, the new
   string is appended to the current contents of the kill buffer.
 */
#define KILL_APPEND 0
/**
   A mode for calling the reader_kill function. In this mode, the new
   string is prepended to the current contents of the kill buffer.
 */
#define KILL_PREPEND 1

/**
   History search mode. This value means that no search is currently
   performed.
 */
#define NO_SEARCH 0
/**
   History search mode. This value means that we are perforing a line
   history search.
 */
#define LINE_SEARCH 1
/**
   History search mode. This value means that we are perforing a token
   history search.
 */
#define TOKEN_SEARCH 2

/**
   History search mode. This value means we are searching backwards.
 */
#define SEARCH_BACKWARD 0
/**
   History search mode. This value means we are searching forwards.
 */
#define SEARCH_FORWARD 1

/* Any time the contents of a buffer changes, we update the generation count. This allows for our background highlighting thread to notice it and skip doing work that it would otherwise have to do. */
static unsigned int s_generation_count;

/* A color is an int */
typedef int color_t;

/**
   A struct describing the state of the interactive reader. These
   states can be stacked, in case reader_readline() calls are
   nested. This happens when the 'read' builtin is used.
*/
class reader_data_t
{
    public:
    
	/** String containing the whole current commandline */
	wcstring command_line;
    
    /** String containing the autosuggestion */
    wcstring autosuggestion;

    /** When backspacing, we suppress autosuggestions */
    bool suppress_autosuggestion;

	/** The representation of the current screen contents */
	screen_t screen;
    
    /** The history */
    history_t *history;

	/**
	   String containing the current search item
	*/
	wcstring search_buff;

    /* History search */
    history_search_t history_search;

	/**
	   Saved position used by token history search
	*/
	int token_history_pos;

	/**
	   Saved search string for token history search. Not handled by command_line_changed.
	*/
	wcstring token_history_buff;

	/**
	   List for storing previous search results. Used to avoid duplicates.
	*/
	wcstring_list_t search_prev;

	/** The current position in search_prev */
	int search_pos;
    
    /** Length of the command */
    size_t command_length() const { return command_line.size(); }
    
    /** Do what we need to do whenever our command line changes */
    void command_line_changed(void);

	/** The current position of the cursor in buff. */
	size_t buff_pos;

	/** Name of the current application */
	wcstring app_name;

	/** The prompt command */
	wcstring prompt;

	/** The output of the last evaluation of the prompt command */
	wcstring prompt_buff;
	
	/**
	   Color is the syntax highlighting for buff.  The format is that
	   color[i] is the classification (according to the enum in
	   highlight.h) of buff[i].
	*/
    std::vector<color_t> colors;

	/** An array defining the block level at each character. */
	std::vector<int> indents;

	/**
	   Function for tab completion
	*/
    complete_function_t complete_func;

	/**
	   Function for syntax highlighting
	*/
	highlight_function_t highlight_function;

	/**
	   Function for testing if the string can be returned
	*/
	int (*test_func)( const wchar_t * );

	/**
	   When this is true, the reader will exit
	*/
	bool end_loop;

	/**
	   If this is true, exit reader even if there are running
	   jobs. This happens if we press e.g. ^D twice.
	*/
	bool prev_end_loop;

	/** The current contents of the top item in the kill ring.  */
	wcstring kill_item;

	/**
	   Pointer to previous reader_data
	*/
	reader_data_t *next;

	/**
	   This variable keeps state on if we are in search mode, and
	   if yes, what mode
	 */
	int search_mode;

	/**
	   Keep track of whether any internal code has done something
	   which is known to require a repaint.
	 */
	bool repaint_needed;
    
    /** Whether the a screen reset is needed after a repaint. */
    bool screen_reset_needed;
};

/**
   The current interactive reading context
*/
static reader_data_t *data=0;

/**
   This flag is set to true when fish is interactively reading from
   stdin. It changes how a ^C is handled by the fish interrupt
   handler.
*/
static int is_interactive_read;

/**
   Flag for ending non-interactive shell
*/
static int end_loop = 0;

/** The stack containing names of files that are being parsed */
static std::stack<const wchar_t *, std::list<const wchar_t *> > current_filename;


/**
   Store the pid of the parent process, so the exit function knows whether it should reset the terminal or not.
*/
static pid_t original_pid;

/**
   This variable is set to true by the signal handler when ^C is pressed
*/
static int interrupted=0;


/*
  Prototypes for a bunch of functions defined later on.
*/

/**
   Stores the previous termios mode so we can reset the modes when
   we execute programs and when the shell exits.
*/
static struct termios saved_modes;

static void reader_super_highlight_me_plenty( int pos );

/**
   Variable to keep track of forced exits - see \c reader_exit_forced();
*/
static int exit_forced;


/**
   Give up control of terminal
*/
static void term_donate()
{
	set_color(rgb_color_t::normal(), rgb_color_t::normal());

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

}

int reader_exit_forced()
{
	return exit_forced;
}

/**
   Repaint the entire commandline. This means reset and clear the
   commandline, write the prompt, perform syntax highlighting, write
   the commandline and move the cursor.
*/

static void reader_repaint()
{
    //Update the indentation
	parser_t::principal_parser().test( data->command_line.c_str(), &data->indents[0], 0, 0 );
	    
#if 0
	s_write( &data->screen,
		 data->prompt_buff.c_str(),
		 data->command_line.c_str(),
		 &data->colors[0],
		 &data->indents[0], 
		 data->buff_pos );
#else

    wcstring full_line = (data->autosuggestion.empty() ? data->command_line : data->autosuggestion);
    size_t len = std::max((size_t)1, full_line.size());
    
    std::vector<color_t> colors = data->colors;
    colors.resize(len, HIGHLIGHT_AUTOSUGGESTION);
    
    std::vector<int> indents = data->indents;
    indents.resize(len);

	s_write( &data->screen,
		 data->prompt_buff.c_str(),
		 full_line.c_str(),
		 &colors[0],
		 &indents[0], 
		 data->buff_pos );
#endif
	data->repaint_needed = false;
}

/**
   Internal helper function for handling killing parts of text.
*/
static void reader_kill( size_t begin_idx, int length, int mode, int newv )
{
    const wchar_t *begin = data->command_line.c_str() + begin_idx;
	if( newv )
	{
        data->kill_item = wcstring(begin, length);
		kill_add(data->kill_item);
	}
	else
	{

        wcstring old = data->kill_item;
		if( mode == KILL_APPEND )
		{
            data->kill_item.append(begin, length);
		}
		else
		{
            data->kill_item = wcstring(begin, length);
            data->kill_item.append(old);
		}

		
		kill_replace( old, data->kill_item );
	}

	if( data->buff_pos > begin_idx ) {
		data->buff_pos = maxi( begin_idx, data->buff_pos-length );
	}
	
    data->command_line.erase(begin_idx, length);
    data->command_line_changed();
	
	reader_super_highlight_me_plenty( data->buff_pos );
	reader_repaint();
	
}

/* This is called from a signal handler! */
void reader_handle_int( int sig )
{
	if( !is_interactive_read )
	{
        parser_t::skip_all_blocks();
	}
	
	interrupted = 1;
	
}

const wchar_t *reader_current_filename()
{
    ASSERT_IS_MAIN_THREAD();
    return current_filename.empty() ? NULL : current_filename.top();
}


void reader_push_current_filename( const wchar_t *fn )
{
    ASSERT_IS_MAIN_THREAD();
    current_filename.push(intern(fn));
}


void reader_pop_current_filename()
{
    ASSERT_IS_MAIN_THREAD();
	current_filename.pop();
}


/** Make sure buffers are large enough to hold the current string length */
void reader_data_t::command_line_changed() {
    ASSERT_IS_MAIN_THREAD();
    size_t len = command_length();
    colors.resize(len);
    indents.resize(len);
    
    /* Update the gen count */
    s_generation_count++;
}


/** Remove any duplicate completions in the list. This relies on the list first beeing sorted. */
static void remove_duplicates(std::vector<completion_t> &l) {
	
	l.erase(std::unique( l.begin(), l.end()), l.end());
}

int reader_interrupted()
{
	int res=interrupted;
	if( res )
		interrupted=0;
	return res;
}

void reader_write_title()
{
	const wchar_t *title;
	const env_var_t term_str = env_get_string( L"TERM" );

	/*
	  This is a pretty lame heuristic for detecting terminals that do
	  not support setting the title. If we recognise the terminal name
	  as that of a virtual terminal, we assume it supports setting the
	  title. If we recognise it as that of a console, we assume it
	  does not support setting the title. Otherwise we check the
	  ttyname and see if we belive it is a virtual terminal.

	  One situation in which this breaks down is with screen, since
	  screen supports setting the terminal title if the underlying
	  terminal does so, but will print garbage on terminals that
	  don't. Since we can't see the underlying terminal below screen
	  there is no way to fix this.
	*/
	if ( term_str.missing() )
		return;

	const wchar_t *term = term_str.c_str();
    bool recognized = false;
    recognized = recognized || contains( term, L"xterm", L"screen", L"nxterm", L"rxvt" );
    recognized = recognized || ! wcsncmp(term, L"xterm-", wcslen(L"xterm-"));

	if( ! recognized )
	{
		char *n = ttyname( STDIN_FILENO );


		if( contains( term, L"linux" ) )
		{
			return;
		}

		if( strstr( n, "tty" ) || strstr( n, "/vc/") )
			return;
		
			
	}

	title = function_exists( L"fish_title" )?L"fish_title":DEFAULT_TITLE;

	if( wcslen( title ) ==0 )
		return;

    wcstring_list_t lst;

	proc_push_interactive(0);
	if( exec_subshell( title, lst ) != -1 )
	{
		size_t i;
		if( lst.size() > 0 )
		{
			writestr( L"\x1b];" );
			for( i=0; i<lst.size(); i++ )
			{
				writestr( lst.at(i).c_str() );
			}
			writestr( L"\7" );
		}
	}
	proc_pop_interactive();		
	set_color( rgb_color_t::reset(), rgb_color_t::reset() );
}

/**
   Reexecute the prompt command. The output is inserted into data->prompt_buff.
*/
static void exec_prompt()
{
	size_t i;

    wcstring_list_t prompt_list;
	
	if( data->prompt.size() )
	{
		proc_push_interactive( 0 );
		
		if( exec_subshell( data->prompt, prompt_list ) == -1 )
		{
			/* If executing the prompt fails, make sure we at least don't print any junk */
            prompt_list.clear();
		}
		proc_pop_interactive();
	}
	
	reader_write_title();
	
    data->prompt_buff.clear();
	
	for( i = 0; i < prompt_list.size(); i++ )
	{
        if (i > 0) data->prompt_buff += L'\n';
        data->prompt_buff += prompt_list.at(i);
	}	
}

void reader_init()
{

	tcgetattr(0,&shell_modes);        /* get the current terminal modes */
	memcpy( &saved_modes,
			&shell_modes,
			sizeof(saved_modes));     /* save a copy so we can reset the terminal later */
	
	shell_modes.c_lflag &= ~ICANON;   /* turn off canonical mode */
	shell_modes.c_lflag &= ~ECHO;     /* turn off echo mode */
    shell_modes.c_cc[VMIN]=1;
    shell_modes.c_cc[VTIME]=0;
    
    // PCA disable VDSUSP (typically control-Y), which is a funny job control
    // function available only on OS X and BSD systems
    // This lets us use control-Y for yank instead
    #ifdef VDSUSP
    shell_modes.c_cc[VDSUSP] = _POSIX_VDISABLE;  
    #endif
    
    /* Repaint if necessary before each byte is read. This lets us react immediately to universal variable color changes. */
    input_common_set_poll_callback(reader_repaint_if_needed);
}


void reader_destroy()
{
	tcsetattr(0, TCSANOW, &saved_modes);
}


void reader_exit( int do_exit, int forced )
{
	if( data )
		data->end_loop=do_exit;
	end_loop=do_exit;
	if( forced )
		exit_forced = 1;
	
}

void reader_repaint_needed()
{
	if (data) {
		data->repaint_needed = true;
	}
}

void reader_repaint_if_needed() {
    if (data && data->screen_reset_needed) {
        s_reset( &data->screen, false);
        data->screen_reset_needed = false;
    }

    if (data && data->repaint_needed) {
        reader_repaint();
        /* reader_repaint clears repaint_needed */
    }
}

void reader_react_to_color_change() {
	if (data) {
        data->repaint_needed = true;
        data->screen_reset_needed = true;
	}
}


/**
   Remove the previous character in the character buffer and on the
   screen using syntax highlighting, etc.
*/
static void remove_backward()
{

	if( data->buff_pos <= 0 )
		return;
        
    data->command_line.erase(data->buff_pos-1, 1);    
	data->buff_pos--;
    data->command_line_changed();
    data->suppress_autosuggestion = true;

	reader_super_highlight_me_plenty( data->buff_pos );

	reader_repaint();

}


/**
   Insert the characters of the string into the command line buffer
   and print them to the screen using syntax highlighting, etc.
*/
static int insert_string(const wcstring &str)
{
    size_t len = str.size();
    if (len == 0)
        return 0;
        
    data->command_line.insert(data->buff_pos, str);
    data->buff_pos += len;
    data->command_line_changed();
    data->suppress_autosuggestion = false;
    
	/* Syntax highlight  */
	reader_super_highlight_me_plenty( data->buff_pos-1 );
	
	reader_repaint();
	return 1;
}


/**
   Insert the character into the command line buffer and print it to
   the screen using syntax highlighting, etc.
*/
static int insert_char( wchar_t c )
{
	return insert_string(wcstring(&c, 1));
}


/**
   Calculate the length of the common prefix substring of two strings.
*/
static int comp_len( const wchar_t *a, const wchar_t *b )
{
	int i;
	for( i=0;
		 a[i] != '\0' && b[i] != '\0' && a[i]==b[i];
		 i++ )
		;
	return i;
}

/**
   Calculate the case insensitive length of the common prefix substring of two strings.
*/
static int comp_ilen( const wchar_t *a, const wchar_t *b )
{
	int i;
	for( i=0;
		 a[i] != '\0' && b[i] != '\0' && towlower(a[i])==towlower(b[i]);
		 i++ )
		;
	return i;
}

/**
   Find the outermost quoting style of current token. Returns 0 if
   token is not quoted.

*/
static wchar_t get_quote( wchar_t *cmd, int len )
{
	int i=0;
	wchar_t res=0;

	while( 1 )
	{
		if( !cmd[i] )
			break;

		if( cmd[i] == L'\\' )
		{
			i++;
			if( !cmd[i] )
				break;
			i++;			
		}
		else
		{
			if( cmd[i] == L'\'' || cmd[i] == L'\"' )
			{
				const wchar_t *end = quote_end( &cmd[i] );
				//fwprintf( stderr, L"Jump %d\n",  end-cmd );
				if(( end == 0 ) || (!*end) || (end-cmd > len))
				{
					res = cmd[i];
					break;
				}
				i = end-cmd+1;
			}
			else
				i++;
		}
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
static void get_param( const wchar_t *cmd,
					   const int pos,
					   wchar_t *quote,
					   const wchar_t **offset,
					   wchar_t **string,
					   int *type )
{
	int prev_pos=0;
	wchar_t last_quote = '\0';
	int unfinished;

	tokenizer tok;
	tok_init( &tok, cmd, TOK_ACCEPT_UNFINISHED | TOK_SQUASH_ERRORS );

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
    
    wchar_t *cmd_tmp = wcsdup(cmd);
	cmd_tmp[pos]=0;
	int cmdlen = wcslen( cmd_tmp );
	unfinished = (cmdlen==0);
	if( !unfinished )
	{
		unfinished = (quote != 0);

		if( !unfinished )
		{
			if( wcschr( L" \t\n\r", cmd_tmp[cmdlen-1] ) != 0 )
			{
				if( ( cmdlen == 1) || (cmd_tmp[cmdlen-2] != L'\\') )
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
			while( (cmd_tmp[prev_pos] != 0) && (wcschr( L";|",cmd_tmp[prev_pos])!= 0) )
				prev_pos++;

			*offset = cmd + prev_pos;
		}
		else
		{
			*offset = cmd + pos;
		}
	}
}

/**
   Insert the string in the given command line at the given cursor
   position. The function checks if the string is quoted or not and
   correctly escapes the string.
   \param val the string to insert
   \param flags A union of all flags describing the completion to insert. See the completion_t struct for more information on possible values.
   \param command_line The command line into which we will insert
   \param inout_cursor_pos On input, the location of the cursor within the command line. On output, the new desired position.
   \return The completed string
*/
static wcstring completion_apply_to_command_line(const wcstring &val_str, int flags, const wcstring &command_line, size_t *inout_cursor_pos)
{
    const wchar_t *val = val_str.c_str();
	bool add_space = !(flags & COMPLETE_NO_SPACE);
	bool do_replace = !!(flags & COMPLETE_NO_CASE);
	bool do_escape = !(flags & COMPLETE_DONT_ESCAPE);
    const size_t cursor_pos = *inout_cursor_pos;
    
	//	debug( 0, L"Insert completion %ls with flags %d", val, flags);

	if( do_replace )
	{
		
		int move_cursor;
		const wchar_t *begin, *end;
		wchar_t *escaped;
		
        const wchar_t *buff = command_line.c_str();
		parse_util_token_extent( buff, cursor_pos, &begin, 0, 0, 0 );
		end = buff + cursor_pos;

		wcstring sb(buff, begin - buff);
		
		if( do_escape )
		{
			escaped = escape( val, ESCAPE_ALL | ESCAPE_NO_QUOTED );		
			sb.append( escaped );
			move_cursor = wcslen(escaped);
			free( escaped );
		}
		else
		{
			sb.append( val );
			move_cursor = wcslen(val);
		}
		

		if( add_space ) 
		{
			sb.append( L" " );
			move_cursor += 1;
		}
		sb.append( end );
        
        size_t new_cursor_pos = (begin - buff) + move_cursor;
        *inout_cursor_pos = new_cursor_pos;
        return sb;
	}
	else
	{
        wchar_t quote = L'\0';
        wcstring replaced;
		if( do_escape )
		{
			get_param(command_line.c_str(), cursor_pos, &quote, 0, 0, 0);
			if( quote == L'\0' )
			{
				replaced = escape_string( val, ESCAPE_ALL | ESCAPE_NO_QUOTED );
			}
			else
			{
				bool unescapable = false;
				for (const wchar_t *pin = val; *pin; pin++)
				{
					switch (*pin )
					{
						case L'\n':
						case L'\t':
						case L'\b':
						case L'\r':
							unescapable = true;
							break;
						default:
							replaced.push_back(*pin);
							break;
					}
				}
                
				if (unescapable)
				{
                    replaced = escape_string(val, ESCAPE_ALL | ESCAPE_NO_QUOTED);
                    replaced.insert(0, &quote, 1);
				}
			}
		}
		else
		{
			replaced = val;
		}
		
        wcstring result = command_line;
        result.insert(cursor_pos, replaced);
        size_t new_cursor_pos = cursor_pos + replaced.size();
        if (add_space)
        {
            if (quote && (command_line.c_str()[cursor_pos] != quote)) 
            {
                /* This is a quoted parameter, first print a quote */
                result.insert(new_cursor_pos++, wcstring(&quote, 1));
            }
            result.insert(new_cursor_pos++, L" ");
        }
        *inout_cursor_pos = new_cursor_pos;
        return result;
	}
}

/**
   Insert the string at the current cursor position. The function
   checks if the string is quoted or not and correctly escapes the
   string.

   \param val the string to insert
   \param flags A union of all flags describing the completion to insert. See the completion_t struct for more information on possible values.

*/
static void completion_insert( const wchar_t *val, int flags )
{
    size_t cursor = data->buff_pos;
    wcstring new_command_line = completion_apply_to_command_line(val, flags, data->command_line, &cursor);
    reader_set_buffer(new_command_line, cursor);
    
    /* Since we just inserted a completion, don't immediately do a new autosuggestion */
    data->suppress_autosuggestion = true;
}

/**
   Run the fish_pager command to display the completion list. If the
   fish_pager outputs any text, it is inserted into the input
   backbuffer.

   \param prefix the string to display before every completion. 
   \param is_quoted should be set if the argument is quoted. This will change the display style.
   \param comp the list of completions to display
*/

static void run_pager( const wcstring &prefix, int is_quoted, const std::vector<completion_t> &comp )
{
    wcstring msg;
	wcstring prefix_esc;
	char *foo;
	io_data_t *in;
	wchar_t *escaped_separator;
	int has_case_sensitive=0;

	if (prefix.empty())
	{
		prefix_esc = L"\"\"";
	}
	else
	{
		prefix_esc = escape_string(prefix, 1);
	}
    	
    wcstring cmd = format_string(L"fish_pager -c 3 -r 4 %ls -p %ls",
                                 // L"valgrind --track-fds=yes --log-file=pager.txt --leak-check=full ./fish_pager %d %ls",
                                is_quoted?L"-q":L"",
                                prefix_esc.c_str() );
    
	in= io_buffer_create( 1 );
	in->fd = 3;

	escaped_separator = escape( COMPLETE_SEP_STR, 1);
	
	for( size_t i=0; i< comp.size(); i++ )
	{
		const completion_t &el = comp.at( i );
		has_case_sensitive |= !(el.flags & COMPLETE_NO_CASE );
	}
	
	for( size_t i=0; i< comp.size(); i++ )
	{

		int base_len=-1;
		const completion_t &el = comp.at( i );

		wchar_t *foo=0;
		wchar_t *baz=0;

		if( has_case_sensitive && (el.flags & COMPLETE_NO_CASE ))
		{
			continue;
		}

		if( el.completion.empty() ){
			continue;
		}
		
		if( el.flags & COMPLETE_NO_CASE )
		  {
		    if( base_len == -1 )
            {
                const wchar_t *begin, *buff = data->command_line.c_str();
                
                parse_util_token_extent( buff, data->buff_pos, &begin, 0, 0, 0 );
                base_len = data->buff_pos - (begin-buff);
            }
								
		    wcstring foo_wstr = escape_string( el.completion.c_str() + base_len, ESCAPE_ALL | ESCAPE_NO_QUOTED );
		    foo = wcsdup(foo_wstr.c_str());
		  }
		else
        {
		    wcstring foo_wstr = escape_string( el.completion, ESCAPE_ALL | ESCAPE_NO_QUOTED );
		    foo = wcsdup(foo_wstr.c_str());
        }
		
	
		if( !el.description.empty() )
		{
			wcstring baz_wstr = escape_string( el.description, 1 );
			baz = wcsdup(baz_wstr.c_str());
		}
		
		if( !foo )
		{
			debug( 0, L"Run pager called with bad argument." );
			bugreport();
			show_stackframe();
		}
		else if( baz )
		{
			append_format(msg, L"%ls%ls%ls\n", foo, escaped_separator, baz);
		}
		else
		{
			append_format(msg, L"%ls\n", foo);
		}

		free( foo );		
		free( baz );		
	}

	free( escaped_separator );		
	
	foo = wcs2str(msg.c_str());
	in->out_buffer_append(foo, strlen(foo) );
	free( foo );
	
	term_donate();
	
	io_data_t *out = io_buffer_create( 0 );
	out->next = in;
	out->fd = 4;
	
    parser_t &parser = parser_t::principal_parser();
	parser.eval( cmd, out, TOP);
	term_steal();

	io_buffer_read( out );

	int nil=0;
    out->out_buffer_append((char *)&nil, 1);

	wchar_t *tmp;
	wchar_t *str = str2wcs(out->out_buffer_ptr());

	if( str )
	{
		for( tmp = str + wcslen(str)-1; tmp >= str; tmp-- )
		{
			input_unreadch( *tmp );
		}
		free( str );
	}


	io_buffer_destroy( out);
	io_buffer_destroy( in);
}

struct autosuggestion_context_t {
    wcstring search_string;
    wcstring autosuggestion;
    size_t cursor_pos;
    history_search_t searcher;
    file_detection_context_t detector;
    const wcstring working_directory;
    const env_vars vars;
    wcstring_list_t commands_to_load;
    const unsigned int generation_count;
    
    // don't reload more than once
    bool has_tried_reloading;
    
    autosuggestion_context_t(history_t *history, const wcstring &term, size_t pos) :
        search_string(term),
        cursor_pos(pos),
        searcher(*history, term, HISTORY_SEARCH_TYPE_PREFIX),
        detector(history, term),
        working_directory(get_working_directory()),
        vars(env_vars::highlighting_keys),
        generation_count(s_generation_count),
        has_tried_reloading(false)
    {
    }
    
    /* The function run in the background thread to determine an autosuggestion */
    int threaded_autosuggest(void) {
        ASSERT_IS_BACKGROUND_THREAD();
        
        /* If the main thread has moved on, skip all the work */
        if (generation_count != s_generation_count) {
            return 0;
        }
        
        /* Let's make sure we aren't using the empty string */
        if (search_string.empty()) {
            return 0;
        }
        
        while (searcher.go_backwards()) {
            history_item_t item = searcher.current_item();
            
            /* Skip items with newlines because they make terrible autosuggestions */
            if (item.str().find('\n') != wcstring::npos)
                continue;
            
            bool item_ok = false;
            if (autosuggest_special_validate_from_history(item.str(), working_directory, &item_ok)) {
                /* The command autosuggestion was handled specially, so we're done */
            } else {
                /* See if the item has any required paths */
                const path_list_t &paths = item.get_required_paths();
                if (paths.empty()) {
                    item_ok = true;
                } else {
                    detector.potential_paths = paths;
                    item_ok = detector.paths_are_valid(paths);
                }
            }
            if (item_ok) {
                this->autosuggestion = searcher.current_string();
                return 1;
            }
        }
        
        /* Try handling a special command like cd */
        wcstring special_suggestion;
        if (autosuggest_suggest_special(search_string, working_directory, special_suggestion)) {
            this->autosuggestion = special_suggestion;
            return 1;
        }
        
        // Here we do something a little funny
        // If the line ends with a space, and the cursor is not at the end,
        // don't use completion autosuggestions. It ends up being pretty weird seeing stuff get spammed on the right
        // while you go back to edit a line
        const bool line_ends_with_space = iswspace(search_string.at(search_string.size() - 1));
        const bool cursor_at_end = (this->cursor_pos == search_string.size());
        if (line_ends_with_space && ! cursor_at_end)
            return 0;

        /* Try normal completions */
        std::vector<completion_t> completions;
        complete(search_string, completions, COMPLETE_AUTOSUGGEST, &this->commands_to_load);
        if (! completions.empty()) {
            const completion_t &comp = completions.at(0);
            size_t cursor = this->cursor_pos;
            this->autosuggestion = completion_apply_to_command_line(comp.completion.c_str(), comp.flags, this->search_string, &cursor);
            return 1;
        }
        
        return 0;
    }
};

static int threaded_autosuggest(autosuggestion_context_t *ctx) {
    return ctx->threaded_autosuggest();
}

static bool can_autosuggest(void) {
    /* We autosuggest if suppress_autosuggestion is not set, if we're not doing a history search, and our command line contains a non-whitespace character. */
    const wchar_t *whitespace = L" \t\r\n\v";
    return ! data->suppress_autosuggestion &&
             data->history_search.is_at_end() &&
             data->command_line.find_first_not_of(whitespace) != wcstring::npos;
}

static void autosuggest_completed(autosuggestion_context_t *ctx, int result) {

    /* Extract the commands to load */
    wcstring_list_t commands_to_load;
    ctx->commands_to_load.swap(commands_to_load);
    
    /* If we have autosuggestions to load, load them and try again */
    if (! result && ! commands_to_load.empty() && ! ctx->has_tried_reloading)
    {
        ctx->has_tried_reloading = true;
        for (wcstring_list_t::const_iterator iter = commands_to_load.begin(); iter != commands_to_load.end(); ++iter)
        {
            complete_load(*iter, false);
        }
        iothread_perform(threaded_autosuggest, autosuggest_completed, ctx);
        return;
    }
    
    if (result &&
        can_autosuggest() &&
        ctx->search_string == data->command_line &&
        string_prefixes_string_case_insensitive(ctx->search_string, ctx->autosuggestion)) {
        /* Autosuggestion is active and the search term has not changed, so we're good to go */
        data->autosuggestion = ctx->autosuggestion;
        sanity_check();
        reader_repaint();
    }
    delete ctx;
}


static void update_autosuggestion(void) {
    /* Updates autosuggestion. We look for an autosuggestion if the command line is non-empty and if we're not doing a history search.  */
#if 0
    /* Old non-threaded mode */
    data->autosuggestion.clear();
    if (can_autosuggest()) {
        history_search_t searcher = history_search_t(*data->history, data->command_line, HISTORY_SEARCH_TYPE_PREFIX);
        if (searcher.go_backwards()) {
            data->autosuggestion = searcher.current_item();
        }
    }
#else
    data->autosuggestion.clear();
    if (! data->suppress_autosuggestion && ! data->command_line.empty() && data->history_search.is_at_end()) {
        autosuggestion_context_t *ctx = new autosuggestion_context_t(data->history, data->command_line, data->buff_pos);
        iothread_perform(threaded_autosuggest, autosuggest_completed, ctx);
    }
#endif
}

/**
  Flash the screen. This function only changed the color of the
  current line, since the flash_screen sequnce is rather painful to
  look at in most terminal emulators.
*/
static void reader_flash()
{
	struct timespec pollint;

	for( size_t i=0; i<data->buff_pos; i++ )
	{
		data->colors.at(i) = HIGHLIGHT_SEARCH_MATCH<<16;
	}
	
	reader_repaint();
	
	pollint.tv_sec = 0;
	pollint.tv_nsec = 100 * 1000000;
	nanosleep( &pollint, NULL );

	reader_super_highlight_me_plenty( data->buff_pos );
    
	reader_repaint();
}

/**
   Characters that may not be part of a token that is to be replaced
   by a case insensitive completion.
 */
#define REPLACE_UNCLEAN L"$*?({})"

/**
   Check if the specified string can be replaced by a case insensitive
   complition with the specified flags.

   Advanced tokens like those containing {}-style expansion can not at
   the moment be replaced, other than if the new token is already an
   exact replacement, e.g. if the COMPLETE_DONT_ESCAPE flag is set.
 */

static int reader_can_replace( const wcstring &in, int flags )
{

	const wchar_t * str = in.c_str();

	if( flags & COMPLETE_DONT_ESCAPE )
	{
		return 1;
	}
	/*
	  Test characters that have a special meaning in any character position
	*/
	while( *str )
	{
		if( wcschr( REPLACE_UNCLEAN, *str ) )
			return 0;
		str++;
	}

	return 1;
}

/**
   Handle the list of completions. This means the following:
   
   - If the list is empty, flash the terminal.
   - If the list contains one element, write the whole element, and if
   the element does not end on a '/', '@', ':', or a '=', also write a trailing
   space.
   - If the list contains multiple elements with a common prefix, write
   the prefix.
   - If the list contains multiple elements without.
   a common prefix, call run_pager to display a list of completions. Depending on terminal size and the length of the list, run_pager may either show less than a screenfull and exit or use an interactive pager to allow the user to scroll through the completions.
   
   \param comp the list of completion strings
*/


static int handle_completions( const std::vector<completion_t> &comp )
{
	wchar_t *base = NULL;
	int len = 0;
	bool done = false;
	int count = 0;
	int flags=0;
	const wchar_t *begin, *end, *buff = data->command_line.c_str();
	
	parse_util_token_extent( buff, data->buff_pos, &begin, 0, 0, 0 );
	end = buff+data->buff_pos;
	
    const wcstring tok(begin, end - begin);
	
	/*
	  Check trivial cases
	 */
	switch(comp.size())
	{
		/* No suitable completions found, flash screen and return */
		case 0:
		{
			reader_flash();
			done = true;
			break;
		}

		/* Exactly one suitable completion found - insert it */
		case 1:
		{
			
			const completion_t &c = comp.at( 0 );
		
			/*
			  If this is a replacement completion, check
			  that we know how to replace it, e.g. that
			  the token doesn't contain evil operators
			  like {}
			 */
			if( !(c.flags & COMPLETE_NO_CASE) || reader_can_replace( tok, c.flags ) )
			{
				completion_insert( c.completion.c_str(), c.flags );			
			}
			done = true;
			len = 1; // I think this just means it's a true return
			break;
		}
	}
	
		
	if( !done )
	{
		/* Try to find something to insert whith the correct case */
		for( size_t i=0; i< comp.size() ; i++ )
		{
			const completion_t &c =  comp.at( i );

			/* Ignore case insensitive completions for now */
			if( c.flags & COMPLETE_NO_CASE )
				continue;
			
			count++;
			
			if( base )
			{
				int new_len = comp_len( base, c.completion.c_str() );
                len = mini(new_len, len);
			}
			else
			{
				base = wcsdup( c.completion.c_str() );
				len = wcslen( base );
				flags = c.flags;
			}
		}

		/* If we found something to insert, do it. */
		if( len > 0 )
		{
			if( count > 1 )
				flags = flags | COMPLETE_NO_SPACE;

			base[len]=L'\0';
			completion_insert(base, flags);
			done = true;
		}
	}
	
	

	if( !done && base == NULL )
	{
		/* Try to find something to insert ignoring case */
		if( begin )
		{

			int offset = tok.size();
			
			count = 0;
			
			for( size_t i=0; i< comp.size(); i++ )
			{
				const completion_t &c = comp.at( i );
				int new_len;


				if( !(c.flags & COMPLETE_NO_CASE) )
					continue;
			
				if( !reader_can_replace( tok, c.flags ) )
				{
					len=0;
					break;
				}

				count++;

				if( base )
				{
					new_len = offset +  comp_ilen( base+offset, c.completion.c_str()+offset );
					len = new_len < len ? new_len: len;
				}
				else
				{
					base = wcsdup( c.completion.c_str() );
					len = wcslen( base );
					flags = c.flags;
					
				}
			}

			if( len > offset )
			{
				if( count > 1 )
					flags = flags | COMPLETE_NO_SPACE;
				
				base[len]=L'\0';
				completion_insert( base, flags );
				done = 1;
			}
			
		}
	}
		
	free( base );

	if( !done )
	{
		/*
		  There is no common prefix in the completions, and show_list
		  is true, so we print the list
		*/
		int len;
		wcstring prefix;
		const wchar_t * prefix_start;
        const wchar_t *buff = data->command_line.c_str();
		get_param( buff,
				   data->buff_pos,
				   0,
				   &prefix_start,
				   0,
				   0 );

		len = &buff[data->buff_pos]-prefix_start;

		if( len <= PREFIX_MAX_LEN )
		{
            prefix.append(prefix_start, len);
		}
		else
		{
            prefix = wcstring(&ellipsis_char, 1);
            prefix.append(prefix_start + (len - PREFIX_MAX_LEN));
		}

		{
			int is_quoted;

			wchar_t quote;
            const wchar_t *buff = data->command_line.c_str();
			get_param( buff, data->buff_pos, &quote, 0, 0, 0 );
			is_quoted = (quote != L'\0');
			
			write_loop(1, "\n", 1 );
			
			run_pager( prefix, is_quoted, comp );
		}
		s_reset( &data->screen, true);
		reader_repaint();

	}		
	return len;
	

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
	  This should enable job control on fish, even if our parent process did
	  not enable it for us.
	*/

	/* 
	   Check if we are in control of the terminal, so that we don't do
	   semi-expensive things like reset signal handlers unless we
	   really have to, which we often don't.
	 */
	if (tcgetpgrp( 0 ) != shell_pgid)
	{
		int block_count = 0;
		int i;
		
		/*
		  Bummer, we are not in control of the terminal. Stop until
		  parent has given us control of it. Stopping in fish is a bit
		  of a challange, what with all the signal fidgeting, we need
		  to reset a bunch of signal state, making this coda a but
		  unobvious.

		  In theory, reseting signal handlers could cause us to miss
		  signal deliveries. In practice, this code should only be run
		  suring startup, when we're not waiting for any signals.
		*/
		while (signal_is_blocked()) 
		{
			signal_unblock();
			block_count++;
		}
		signal_reset_handlers();
	
		/*
		  Ok, signal handlers are taken out of the picture. Stop ourself in a loop
		  until we are in control of the terminal.
		 */
		while (tcgetpgrp( 0 ) != shell_pgid)
		{
			killpg( shell_pgid, SIGTTIN);
		}
		
		signal_set_handlers();

		for( i=0; i<block_count; i++ ) 
		{
			signal_block();
		}
		
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
		exit_without_destructors(1);
	}

	common_handle_winch(0);

    if( tcsetattr(0,TCSANOW,&shell_modes))      /* set the new modes */
    {
        wperror(L"tcsetattr");
    }

	/* 
	   We need to know our own pid so we'll later know if we are a
	   fork 
	*/
	original_pid = getpid();

	env_set( L"_", L"fish", ENV_GLOBAL );
}

/**
   Destroy data for interactive use
*/
static void reader_interactive_destroy()
{
	kill_destroy();
	writestr( L"\n" );
	set_color( rgb_color_t::reset(), rgb_color_t::reset() );
	input_destroy();
}


void reader_sanity_check()
{
	if( get_is_interactive())
	{
		if( !data )
			sanity_lose();

		if(!( data->buff_pos <= data->command_length() ))
			sanity_lose();
        
        if (data->colors.size() != data->command_length())
            sanity_lose();
            
        if (data->indents.size() != data->command_length())
            sanity_lose();
            
	}
}

/**
   Set the specified string from the history as the current buffer. Do
   not modify prefix_width.
*/
static void set_command_line_and_position( const wcstring &new_str, int pos )
{
    data->command_line = new_str;
    data->command_line_changed();
    data->buff_pos = pos;
    reader_super_highlight_me_plenty( data->buff_pos );
    reader_repaint();
}

void reader_replace_current_token( const wchar_t *new_token )
{

	const wchar_t *begin, *end;
	int new_pos;

	/* Find current token */
    const wchar_t *buff = data->command_line.c_str();
	parse_util_token_extent( (wchar_t *)buff, data->buff_pos, &begin, &end, 0, 0 );

	if( !begin || !end )
		return;

	/* Make new string */
    wcstring new_buff(buff, begin - buff);
    new_buff.append(new_token);
    new_buff.append(end);
	new_pos = (begin-buff) + wcslen(new_token);
    
    set_command_line_and_position(new_buff, new_pos);
}


/**
   Reset the data structures associated with the token search
*/
static void reset_token_history()
{
	const wchar_t *begin, *end;
    const wchar_t *buff = data->command_line.c_str();
	parse_util_token_extent( (wchar_t *)buff, data->buff_pos, &begin, &end, 0, 0 );
	
	data->search_buff.clear();
	if( begin )
	{
        data->search_buff.append(begin, end - begin);
	}

	data->token_history_pos = -1;
	data->search_pos=0;
    data->search_prev.clear();
    data->search_prev.push_back(data->search_buff);
    
    data->history_search = history_search_t(*data->history, data->search_buff, HISTORY_SEARCH_TYPE_CONTAINS);
}


/**
   Handles a token search command.

   \param forward if the search should be forward or reverse
   \param reset whether the current token should be made the new search token
*/
static void handle_token_history( int forward, int reset )
{
    /* Paranoia */
    if (! data)
        return;
        
	const wchar_t *str=0;
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

	if( forward || data->search_pos + 1 < (long)data->search_prev.size() )
	{
		if( forward )
		{
			if( data->search_pos > 0 )
			{
				data->search_pos--;
			}
            str = data->search_prev.at(data->search_pos).c_str();
		}
		else
		{
			data->search_pos++;
            str = data->search_prev.at(data->search_pos).c_str();
		}

		reader_replace_current_token( str );
		reader_super_highlight_me_plenty( data->buff_pos );
		reader_repaint();
	}
	else
	{
		if( current_pos == -1 )
		{
            data->token_history_buff.clear();
            
			/*
			  Search for previous item that contains this substring
			*/
            if (data->history_search.go_backwards()) {
                wcstring item = data->history_search.current_string();
                data->token_history_buff = data->history_search.current_string();
            }
			current_pos = data->token_history_buff.size();

		}

		if( data->token_history_buff.empty() )
		{
			/*
			  We have reached the end of the history - check if the
			  history already contains the search string itself, if so
			  return, otherwise add it.
			*/

			const wcstring &last = data->search_prev.back();
            if (data->search_buff != last) 
			{
				str = wcsdup( data->search_buff.c_str() );
			}
			else
			{
				return;
			}
		}
		else
		{

			//debug( 3, L"new '%ls'", data->token_history_buff.c_str() );

			for( tok_init( &tok, data->token_history_buff.c_str(), TOK_ACCEPT_UNFINISHED );
				 tok_has_next( &tok);
				 tok_next( &tok ))
			{
				switch( tok_last_type( &tok ) )
				{
					case TOK_STRING:
					{
						if( wcsstr( tok_last( &tok ), data->search_buff.c_str() ) )
						{
							//debug( 3, L"Found token at pos %d\n", tok_get_pos( &tok ) );
							if( tok_get_pos( &tok ) >= current_pos )
							{
								break;
							}
							//debug( 3, L"ok pos" );

                            const wcstring last_tok = tok_last( &tok );
                            if (find(data->search_prev.begin(), data->search_prev.end(), last_tok) == data->search_prev.end()) {
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
			reader_super_highlight_me_plenty( data->buff_pos );
			reader_repaint();
            data->search_prev.push_back(str);
			data->search_pos = data->search_prev.size() - 1;
		}
		else if( ! reader_interrupted() )
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
   \param new if the new kill item should be appended to the previous kill item or not.
*/
static void move_word( int dir, int erase, int newv )
{
	size_t end_buff_pos=data->buff_pos;
	int step = dir?1:-1;

	/*
	  Return if we are already at the edge
	*/
	if( !dir && data->buff_pos == 0 )
	{
		return;
	}
	
	if( dir && data->buff_pos == data->command_length() )
	{
		return;
	}
	
	/*
	  If we are beyond the last character and moving left, start by
	  moving one step, since otehrwise we'll start on the \0, which
	  should be ignored.
	*/
	if( !dir && (end_buff_pos == data->command_length()) )
	{
		if( !end_buff_pos )
			return;
		
		end_buff_pos--;
	}
	
	/*
	  When moving left, ignore the character under the cursor
	*/
	if( !dir )
	{
		end_buff_pos+=2*step;
	}
	
	/*
	  Remove all whitespace characters before finding a word
	*/
	while( 1 )
	{
		wchar_t c;

		if( !dir )
		{
			if( end_buff_pos <= 0 )
				break;
		}
		else
		{
			if( end_buff_pos >= data->command_length() )
				break;
		}

		/*
		  Always eat at least one character
		*/
		if( end_buff_pos != data->buff_pos )
		{
			
			c = data->command_line.c_str()[end_buff_pos];
			
			if( !iswspace( c ) )
			{
				break;
			}
		}
		
		end_buff_pos+=step;

	}
	
	/*
	  Remove until we find a character that is not alphanumeric
	*/
	while( 1 )
	{
		wchar_t c;
		
		if( !dir )
		{
			if( end_buff_pos <= 0 )
				break;
		}
		else
		{
			if( end_buff_pos >= data->command_length() )
				break;
		}
		
		c = data->command_line.c_str()[end_buff_pos];
		
		if( !iswalnum( c ) )
		{
			/*
			  Don't gobble the boundary character when moving to the
			  right
			*/
			if( !dir )
				end_buff_pos -= step;
			break;
		}
		end_buff_pos+=step;
	}

	/*
	  Make sure we move at least one character
	*/
	if( end_buff_pos==data->buff_pos )
	{
		end_buff_pos+=step;
	}

	/*
	  Make sure we don't move beyond begining or end of buffer
	*/
	end_buff_pos = maxi( 0, mini( end_buff_pos, data->command_length() ) );
	


	if( erase )
	{
		int remove_count = abs(data->buff_pos - end_buff_pos);
		int first_char = mini( data->buff_pos, end_buff_pos );
//		fwprintf( stderr, L"Remove from %d to %d\n", first_char, first_char+remove_count );
		
		reader_kill( first_char, remove_count, dir?KILL_APPEND:KILL_PREPEND, newv );
		
	}
	else
	{
		data->buff_pos = end_buff_pos;
		reader_repaint();
	}
}


const wchar_t *reader_get_buffer(void)
{
    ASSERT_IS_MAIN_THREAD();
	return data?data->command_line.c_str():NULL;
}

history_t *reader_get_history(void) {
    ASSERT_IS_MAIN_THREAD();
	return data ? data->history : NULL;
}

void reader_set_buffer( const wcstring &b, int p )
{
	if( !data )
		return;

    /* Callers like to pass us pointers into ourselves, so be careful! I don't know if we can use operator= with a pointer to our interior, so use an intermediate. */
	size_t l = b.size();
    data->command_line = b;
    data->command_line_changed();

	if( p>=0 )
	{
		data->buff_pos=mini( p, l );
	}
	else
	{
		data->buff_pos=l;
	}

	data->search_mode = NO_SEARCH;
	data->search_buff.clear();
	data->history_search.go_to_end();

	reader_super_highlight_me_plenty( data->buff_pos );
	reader_repaint_needed();
}


int reader_get_cursor_pos()
{
	if( !data )
		return -1;

	return data->buff_pos;
}

#define ENV_CMD_DURATION L"CMD_DURATION"

void set_env_cmd_duration(struct timeval *after, struct timeval *before)
{
	time_t secs = after->tv_sec - before->tv_sec;
	suseconds_t usecs = after->tv_usec - before->tv_usec;
	wchar_t buf[16];

	if (after->tv_usec < before->tv_usec) {
		usecs += 1000000;
		secs -= 1;
	}

	if (secs < 1) {
		env_remove( ENV_CMD_DURATION, 0 );
	} else {
		if (secs < 10) { // 10 secs
			swprintf(buf, 16, L"%lu.%02us", secs, usecs / 10000);
		} else if (secs < 60) { // 1 min
			swprintf(buf, 16, L"%lu.%01us", secs, usecs / 100000);
		} else if (secs < 600) { // 10 mins
			swprintf(buf, 16, L"%lum %lu.%01us", secs / 60, secs % 60, usecs / 100000);
		} else if (secs < 5400) { // 1.5 hours
			swprintf(buf, 16, L"%lum %lus", secs / 60, secs % 60);
		} else {
			swprintf(buf, 16, L"%.1fh", secs / 3600.0f);
		}
		env_set( ENV_CMD_DURATION, buf, ENV_EXPORT );
	}
}

void reader_run_command( parser_t &parser, const wchar_t *cmd )
{

	wchar_t *ft;
	struct timeval time_before, time_after;

	ft= tok_first( cmd );

	if( ft != 0 )
		env_set( L"_", ft, ENV_GLOBAL );
	free(ft);

	reader_write_title();

	term_donate();

	gettimeofday(&time_before, NULL);

	parser.eval( cmd, 0, TOP );
	job_reap( 1 );

	gettimeofday(&time_after, NULL);
	set_env_cmd_duration(&time_after, &time_before);

	term_steal();

	env_set( L"_", program_name, ENV_GLOBAL );

#ifdef HAVE__PROC_SELF_STAT
	proc_update_jiffies();
#endif


}


int reader_shell_test( const wchar_t *b )
{
	int res = parser_t::principal_parser().test( b, 0, 0, 0 );
	
	if( res & PARSER_TEST_ERROR )
	{
		wcstring sb;

		int tmp[1];
		int tmp2[1];
		
		s_write( &data->screen, L"", L"", tmp, tmp2, 0 );
		
		parser_t::principal_parser().test( b, 0, &sb, L"fish" );
		fwprintf( stderr, L"%ls", sb.c_str() );
	}
	return res;
}

/**
   Test if the given string contains error. Since this is the error
   detection for general purpose, there are no invalid strings, so
   this function always returns false.
*/
static int default_test( const wchar_t *b )
{
	return 0;
}

void reader_push( const wchar_t *name )
{
    // use something nasty which guarantees value initialization (that is, all fields zero)
    reader_data_t zerod = {};
    reader_data_t *n = new reader_data_t(zerod);
    
    n->history = & history_t::history_with_name(name);
	n->app_name = name;
	n->next = data;

	data=n;

	data->command_line_changed();

	if( data->next == 0 )
	{
		reader_interactive_init();
	}

	exec_prompt();
	reader_set_highlight_function( &highlight_universal );
	reader_set_test_function( &default_test );
	reader_set_prompt( L"" );
	//history_set_mode( name );
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
	
    /* Invoke the destructor to balance our new */
    delete n;

	if( data == 0 )
	{
		reader_interactive_destroy();
	}
	else
	{
		end_loop = 0;
		//history_set_mode( data->app_name.c_str() );
		s_reset( &data->screen, true);
	}
}

void reader_set_prompt( const wchar_t *new_prompt )
{
    data->prompt = new_prompt;
}

void reader_set_complete_function( complete_function_t f )
{
	data->complete_func = f;
}

void reader_set_highlight_function( highlight_function_t func )
{
	data->highlight_function = func;
}

void reader_set_test_function( int (*f)( const wchar_t * ) )
{
	data->test_func = f;
}


/** A class as the context pointer for a background (threaded) highlight operation. */
class background_highlight_context_t {
public:
    /** The string to highlight */
	const wcstring string_to_highlight;
	
	/** Color buffer */
	std::vector<color_t> colors;
	
	/** The position to use for bracket matching */
	const int match_highlight_pos;
	
	/** Function for syntax highlighting */
	const highlight_function_t highlight_function;
    
    /** Environment variables */
    const env_vars vars;

    /** When the request was made */
    const double when;
    
    /** Gen count at the time the request was made */
    const unsigned int generation_count;
    
    background_highlight_context_t(const wcstring &pbuff, int phighlight_pos, highlight_function_t phighlight_func) :
        string_to_highlight(pbuff),
        match_highlight_pos(phighlight_pos),
        highlight_function(phighlight_func),
        vars(env_vars::highlighting_keys),
        when(timef()),
        generation_count(s_generation_count)
    {
        colors.resize(string_to_highlight.size(), 0);
    }
    
    int threaded_highlight() {
        if (generation_count != s_generation_count) {
            // The gen count has changed, so don't do anything
            return 0;
        }
        const wchar_t *delayer = vars.get(L"HIGHLIGHT_DELAY");
        double secDelay = 0;
        if (delayer) {
            wcstring tmp = delayer;
            secDelay = from_string<double>(tmp);
        }
        if (secDelay > 0) usleep((useconds_t)(secDelay * 1E6));
        //write(0, "Start", 5);
        if (! string_to_highlight.empty()) {
            highlight_function( string_to_highlight.c_str(), colors, match_highlight_pos, NULL /* error */, vars);
        }
        //write(0, "End", 3);
        return 0;
    }
};

/* Called to set the highlight flag for search results */
static void highlight_search(void) {
	if( ! data->search_buff.empty() && ! data->history_search.is_at_end())
	{
        const wchar_t *buff = data->command_line.c_str();
		const wchar_t *match = wcsstr( buff, data->search_buff.c_str() );
		if( match )
		{
			int start = match-buff;
			int count = data->search_buff.size();
			int i;

			for( i=0; i<count; i++ )
			{
				data->colors.at(start+i) |= HIGHLIGHT_SEARCH_MATCH<<16;
			}
		}
	}
}

static void highlight_complete(background_highlight_context_t *ctx, int result) {
    ASSERT_IS_MAIN_THREAD();
	if (ctx->string_to_highlight == data->command_line) {
		/* The data hasn't changed, so swap in our colors */
        assert(ctx->colors.size() == data->command_length());
        data->colors.swap(ctx->colors);
        
        
		//data->repaint_needed = 1;
        //s_reset( &data->screen, 1 );
        
        sanity_check();
        highlight_search();
        reader_repaint();
	}
	
	/* Free our context */
    delete ctx;
}

static int threaded_highlight(background_highlight_context_t *ctx) {
    return ctx->threaded_highlight();
}


/**
   Call specified external highlighting function and then do search
   highlighting. Lastly, clear the background color under the cursor
   to avoid repaint issues on terminals where e.g. syntax highligthing
   maykes characters under the sursor unreadable.

   \param match_highlight_pos the position to use for bracket matching. This need not be the same as the surrent cursor position
   \param error if non-null, any possible errors in the buffer are further descibed by the strings inserted into the specified arraylist
*/
static void reader_super_highlight_me_plenty( int match_highlight_pos )
{
    reader_sanity_check();
    
	background_highlight_context_t *ctx = new background_highlight_context_t(data->command_line, match_highlight_pos, data->highlight_function);
	iothread_perform(threaded_highlight, highlight_complete, ctx);
    highlight_search();
    
    /* Here's a hack. Check to see if our autosuggestion still applies; if so, don't recompute it. Since the autosuggestion computation is asynchronous, this avoids "flashing" as you type into the autosuggestion. */
    const wcstring &cmd = data->command_line, &suggest = data->autosuggestion;
    if (can_autosuggest() && ! suggest.empty() && string_prefixes_string_case_insensitive(cmd, suggest)) {
        /* The autosuggestion is still reasonable, so do nothing */
    } else {
        update_autosuggestion();
    }
}


int exit_status()
{
	if( get_is_interactive() )
		return job_list_is_empty() && data->end_loop;
	else
		return end_loop;
}

/**
   This function is called when the main loop notices that end_loop
   has been set while in interactive mode. It checks if it is ok to
   exit.
 */

static void handle_end_loop()
{
	job_t *j;
	int job_count=0;
	int is_breakpoint=0;
	block_t *b;
	parser_t &parser = parser_t::principal_parser();
    
	for( b = parser.current_block; 
	     b; 
	     b = b->outer )
	{
		if( b->type == BREAKPOINT )
		{
			is_breakpoint = 1;
			break;
		}
	}
	
    job_iterator_t jobs;
    while ((j = jobs.next()))
	{
		if( !job_is_completed(j) )
		{
			job_count++;
			break;
		}
	}
	
	if( !reader_exit_forced() && !data->prev_end_loop && job_count && !is_breakpoint )
	{
		writestr(_( L"There are stopped jobs. A second attempt to exit will enforce their termination.\n" ));
		
		reader_exit( 0, 0 );
		data->prev_end_loop=1;
	}
	else
	{
		if( !isatty(0) )
		{
			/*
			  We already know that stdin is a tty since we're
			  in interactive mode. If isatty returns false, it
			  means stdin must have been closed. 
			*/
			job_iterator_t jobs;
			while ((j = jobs.next()))
			{
				if( ! job_is_completed( j ) )
				{
					job_signal( j, SIGHUP );						
				}
			}
		}
	}
}



/**
   Read interactively. Read input from stdin while providing editing
   facilities.
*/
static int read_i()
{
	event_fire_generic(L"fish_prompt");
	
	reader_push(L"fish");
	reader_set_complete_function( &complete );
	reader_set_highlight_function( &highlight_shell );
	reader_set_test_function( &reader_shell_test );
	parser_t &parser = parser_t::principal_parser();
    
	data->prev_end_loop=0;

	while( (!data->end_loop) && (!sanity_check()) )
	{
		const wchar_t *tmp;

		if( function_exists( PROMPT_FUNCTION_NAME ) )
			reader_set_prompt( PROMPT_FUNCTION_NAME );
		else
			reader_set_prompt( DEFAULT_PROMPT );

		/*
		  Put buff in temporary string and clear buff, so
		  that we can handle a call to reader_set_buffer
		  during evaluation.
		*/

	
		tmp = reader_readline();
	

		if( data->end_loop)
		{
			handle_end_loop();
		}
		else if( tmp )
		{
			tmp = wcsdup( tmp );
			
			data->buff_pos=0;
            data->command_line.clear();
            data->command_line_changed();
			reader_run_command( parser, tmp );
			free( (void *)tmp );
			if( data->end_loop)
			{
				handle_end_loop();
			}
			else
			{
				data->prev_end_loop=0;
			}
		}
		

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

/**
   Test if the specified character in the specified string is
   backslashed.
*/
static int is_backslashed( const wchar_t *str, int pos )
{
	int count = 0;
	int i;
	
	for( i=pos-1; i>=0; i-- )
	{
		if( str[i] != L'\\' )
			break;
		
		count++;
	}

	return count %2;
}


const wchar_t *reader_readline()
{

	wint_t c;
	int i;
	int last_char=0, yank=0;
	const wchar_t *yank_str;
	std::vector<completion_t> comp;
	int comp_empty=1;
	int finished=0;
	struct termios old_modes;

	data->search_buff.clear();
	data->search_mode = NO_SEARCH;
	
	
	exec_prompt();

	reader_super_highlight_me_plenty( data->buff_pos );
	s_reset( &data->screen, true);
	reader_repaint();

	/* 
	   get the current terminal modes. These will be restored when the
	   function returns.
	*/
	tcgetattr(0,&old_modes);        
	/* set the new modes */
	if( tcsetattr(0,TCSANOW,&shell_modes))
	{
		wperror(L"tcsetattr");
    }

	while( !finished && !data->end_loop)
	{
		/*
		  Sometimes strange input sequences seem to generate a zero
		  byte. I believe these simply mean a character was pressed
		  but it should be ignored. (Example: Trying to add a tilde
		  (~) to digit)
		*/
		while( 1 )
		{
			int was_interactive_read = is_interactive_read;
			is_interactive_read = 1;
			c=input_readch();
			is_interactive_read = was_interactive_read;
						
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

					insert_string( arr );

				}
			}

			if( c != 0 )
				break;
		}
/*
  if( (last_char == R_COMPLETE) && (c != R_COMPLETE) && (!comp_empty) )
  {
  halloc_destroy( comp );
  comp = 0;
  }
*/
		if( last_char != R_YANK && last_char != R_YANK_POP )
			yank=0;
        const wchar_t *buff = data->command_line.c_str();
		switch( c )
		{

			/* go to beginning of line*/
			case R_BEGINNING_OF_LINE:
			{
				while( ( data->buff_pos>0 ) && 
					   ( buff[data->buff_pos-1] != L'\n' ) )
				{
					data->buff_pos--;
				}
				
				reader_repaint();
				break;
			}

			case R_END_OF_LINE:
			{
				while( buff[data->buff_pos] && 
					   buff[data->buff_pos] != L'\n' )
				{
					data->buff_pos++;
				}
				
				reader_repaint();
				break;
			}

			
			case R_BEGINNING_OF_BUFFER:
			{
				data->buff_pos = 0;

				reader_repaint();
				break;
			}

			/* go to EOL*/
			case R_END_OF_BUFFER:
			{
				data->buff_pos = data->command_length();

				reader_repaint();
				break;
			}

			case R_NULL:
			{
				reader_repaint_if_needed();
				break;
			}

			case R_REPAINT:
			{
				exec_prompt();
				write_loop( 1, "\r", 1 );
				s_reset( &data->screen, false);
				reader_repaint();
				break;
			}

			case R_EOF:
			{
				exit_forced = 1;
				data->end_loop=1;
				break;
			}

			/* complete */
			case R_COMPLETE:
			{

				if( !data->complete_func )
					break;

 				if( comp_empty || last_char != R_COMPLETE)
				{
					const wchar_t *begin, *end;
					const wchar_t *token_begin, *token_end;
                    const wchar_t *buff = data->command_line.c_str();
					int len;
					int cursor_steps;

					parse_util_cmdsubst_extent( buff, data->buff_pos, &begin, &end );

					parse_util_token_extent( begin, data->buff_pos - (begin-buff), &token_begin, &token_end, 0, 0 );
					
					cursor_steps = token_end - buff- data->buff_pos;
					data->buff_pos += cursor_steps;
					if( is_backslashed( buff, data->buff_pos ) )
					{
						remove_backward();
					}
					
					reader_repaint();
					
					len = data->buff_pos - (begin-buff);
                    const wcstring buffcpy = wcstring(begin, len);

					data->complete_func( buffcpy, comp, COMPLETE_DEFAULT, NULL);
					
					sort(comp.begin(), comp.end());
					remove_duplicates( comp );
					
					comp_empty = handle_completions( comp );
					comp.clear();
				}

				break;
			}

			/* kill */
			case R_KILL_LINE:
			{
                const wchar_t *buff = data->command_line.c_str();
				const wchar_t *begin = &buff[data->buff_pos];
				const wchar_t *end = begin;
				int len;
								
				while( *end && *end != L'\n' )
					end++;
				
				if( end==begin && *end )
					end++;
				
				len = end-begin;
				
				if( len )
				{
					reader_kill( begin - buff, len, KILL_APPEND, last_char!=R_KILL_LINE );
				}
				
				break;
			}

			case R_BACKWARD_KILL_LINE:
			{
				if( data->buff_pos > 0 )
				{
                    const wchar_t *buff = data->command_line.c_str();
					const wchar_t *end = &buff[data->buff_pos];
					const wchar_t *begin = end;
					int len;
					
					while( begin > buff  && *begin != L'\n' )
						begin--;
					
					if( *begin == L'\n' )
						begin++;
					
					len = maxi( end-begin, 1 );
					begin = end - len;
										
					reader_kill( begin - buff, len, KILL_PREPEND, last_char!=R_BACKWARD_KILL_LINE );					
					
				}
				break;

			}

			case R_KILL_WHOLE_LINE:
			{
                const wchar_t *buff = data->command_line.c_str();
				const wchar_t *end = &buff[data->buff_pos];
				const wchar_t *begin = end;
				int len;
		
				while( begin > buff  && *begin != L'\n' )
					begin--;
				
				if( *begin == L'\n' )
					begin++;
				
				len = maxi( end-begin, 0 );
				begin = end - len;

				while( *end && *end != L'\n' )
					end++;
				
				if( begin == end && *end )
					end++;
				
				len = end-begin;
				
				if( len )
				{
					reader_kill( begin - buff, len, KILL_APPEND, last_char!=R_KILL_WHOLE_LINE );					
				}
				
				break;
			}

			/* yank*/
			case R_YANK:
			{
				yank_str = kill_yank();
				insert_string( yank_str );
				yank = wcslen( yank_str );
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
					insert_string(yank_str);
					yank = wcslen(yank_str);
				}
				break;
			}

			/* Escape was pressed */
			case L'\x1b':
			{
				if( data->search_mode )
				{
					data->search_mode= NO_SEARCH;
										
					if( data->token_history_pos==-1 )
					{
						//history_reset();
                        data->history_search.go_to_end();
						reader_set_buffer( data->search_buff.c_str(), data->search_buff.size() );
					}
					else
					{
						reader_replace_current_token( data->search_buff.c_str() );
					}
					data->search_buff.clear();
					reader_super_highlight_me_plenty( data->buff_pos );
					reader_repaint();
					
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
				/**
				   Remove the current character in the character buffer and on the
				   screen using syntax highlighting, etc.
				*/
				if( data->buff_pos < data->command_length() )
				{
					data->buff_pos++;
					remove_backward();
				}
				break;
			}

			/*
			  Evaluate. If the current command is unfinished, or if
			  the charater is escaped using a backslash, insert a
			  newline
			*/
			case R_EXECUTE:
			{
                /* Delete any autosuggestion */
                data->autosuggestion.clear();
                
				/*
				  Allow backslash-escaped newlines
				*/
				if( is_backslashed( data->command_line.c_str(), data->buff_pos ) )
				{
					insert_char( '\n' );
					break;
				}
				
				switch( data->test_func( data->command_line.c_str() ) )
				{

					case 0:
					{
						/*
						  Finished commend, execute it
						*/
						if( ! data->command_line.empty() )
						{
                            if (data->history) {
                                data->history->add_with_file_detection(data->command_line);
                            }
						}
						finished=1;
						data->buff_pos=data->command_length();
						reader_repaint();
						break;
					}
					
					/*
					  We are incomplete, continue editing
					*/
					case PARSER_TEST_INCOMPLETE:
					{						
						insert_char( '\n' );
						break;
					}

					/*
					  Result must be some combination including an
					  error. The error message will already be
					  printed, all we need to do is repaint
					*/
					default:
					{
						s_reset( &data->screen, true);
						reader_repaint();
						break;
					}

				}
				
				break;
			}

			/* History functions */
			case R_HISTORY_SEARCH_BACKWARD:
			case R_HISTORY_TOKEN_SEARCH_BACKWARD:
			case R_HISTORY_SEARCH_FORWARD:
			case R_HISTORY_TOKEN_SEARCH_FORWARD:
			{
				int reset = 0;
				
				if( data->search_mode == NO_SEARCH )
				{
					reset = 1;
					if( ( c == R_HISTORY_SEARCH_BACKWARD ) ||
					    ( c == R_HISTORY_SEARCH_FORWARD ) )
					{
						data->search_mode = LINE_SEARCH;
					}
					else
					{
						data->search_mode = TOKEN_SEARCH;
					}
					
                    data->search_buff.append(data->command_line);
                    data->history_search = history_search_t(*data->history, data->search_buff, HISTORY_SEARCH_TYPE_CONTAINS);
                    
                    /* Skip the autosuggestion as history */
                    const wcstring &suggest = data->autosuggestion;
                    if (! suggest.empty()) {
                        data->history_search.skip_matches(wcstring_list_t(&suggest, 1 + &suggest));
                    }
				}

				switch( data->search_mode )
				{

					case LINE_SEARCH:
					{
						if( ( c == R_HISTORY_SEARCH_BACKWARD ) ||
							( c == R_HISTORY_TOKEN_SEARCH_BACKWARD ) )
						{
							data->history_search.go_backwards();
						}
						else
						{
							if (! data->history_search.go_forwards()) {
                                /* If you try to go forwards past the end, we just go to the end */
                                data->history_search.go_to_end();
                            }
						}
						
                        wcstring new_text;
                        if (data->history_search.is_at_end()) {
                            new_text = data->search_buff;
                        } else {
                            new_text = data->history_search.current_string();
                        }
						set_command_line_and_position(new_text, new_text.size());
						
						break;
					}
					
					case TOKEN_SEARCH:
					{
						if( ( c == R_HISTORY_SEARCH_BACKWARD ) ||
							( c == R_HISTORY_TOKEN_SEARCH_BACKWARD ) )
						{
							handle_token_history( SEARCH_BACKWARD, reset );
						}
						else
						{
							handle_token_history( SEARCH_FORWARD, reset );
						}
						
						break;
					}
						
				}
				break;
			}


			/* Move left*/
			case R_BACKWARD_CHAR:
			{
				if( data->buff_pos > 0 )
				{
					data->buff_pos--;
					reader_repaint();
				}
				break;
			}
			
			/* Move right*/
			case R_FORWARD_CHAR:
			{
				if( data->buff_pos < data->command_length() )
				{
					data->buff_pos++;					
					reader_repaint();
				} else {
                    /* We're at the end of our buffer, and the user hit right. Try autosuggestion. */
                    if (! data->autosuggestion.empty()) {
                        /* Accept the autosuggestion */
                        data->command_line = data->autosuggestion;
                        data->buff_pos = data->command_line.size();
                        data->command_line_changed();
                        reader_super_highlight_me_plenty(data->buff_pos);
                        reader_repaint();
                    }
                }
				break;
			}

			/* kill one word left */
			case R_BACKWARD_KILL_WORD:
			{
				move_word(0,1, last_char!=R_BACKWARD_KILL_WORD);
				break;
			}

			/* kill one word right */
			case R_KILL_WORD:
			{
				move_word(1,1, last_char!=R_KILL_WORD);
				break;
			}

			/* move one word left*/
			case R_BACKWARD_WORD:
			{
				move_word(0,0,0);
				break;
			}

			/* move one word right*/
			case R_FORWARD_WORD:
			{
				move_word( 1,0,0);
				break;
			}

			case R_BEGINNING_OF_HISTORY:
			{
				data->history_search.go_to_beginning();
				break;
			}

			case R_END_OF_HISTORY:
			{
				data->history_search.go_to_end();
				break;
			}

			case R_UP_LINE:
			case R_DOWN_LINE:
			{
				int line_old = parse_util_get_line_from_offset( data->command_line.c_str(), data->buff_pos );
				int line_new;
				
				if( c == R_UP_LINE )
					line_new = line_old-1;
				else
					line_new = line_old+1;
	
				int line_count = parse_util_lineno( data->command_line.c_str(), data->command_length() )-1;
				
				if( line_new >= 0 && line_new <= line_count)
				{
					int base_pos_new;
					int base_pos_old;
					
					int indent_old;
					int indent_new;
					int line_offset_old;
					int total_offset_new;

					base_pos_new = parse_util_get_offset_from_line( data->command_line, line_new );

					base_pos_old = parse_util_get_offset_from_line( data->command_line,  line_old );
					
					
					indent_old = data->indents.at(base_pos_old);
					indent_new = data->indents.at(base_pos_new);
					
					line_offset_old = data->buff_pos - parse_util_get_offset_from_line( data->command_line, line_old );
					total_offset_new = parse_util_get_offset( data->command_line, line_new, line_offset_old - 4*(indent_new-indent_old));
					data->buff_pos = total_offset_new;
					reader_repaint();
				}
								
				break;
			}
			

			/* Other, if a normal character, we add it to the command */
			default:
			{
				
				if( (!wchar_private(c)) && (( (c>31) || (c==L'\n'))&& (c != 127)) )
				{
                    /* Regular character */
					insert_char( c );
				}
				else
				{
					/*
					  Low priority debug message. These can happen if
					  the user presses an unefined control
					  sequnece. No reason to report.
					*/
					debug( 2, _( L"Unknown keybinding %d" ), c );
				}
				break;
			}

		}
		
		if( (c != R_HISTORY_SEARCH_BACKWARD) &&
		    (c != R_HISTORY_SEARCH_FORWARD) &&
		    (c != R_HISTORY_TOKEN_SEARCH_BACKWARD) &&
		    (c != R_HISTORY_TOKEN_SEARCH_FORWARD) &&
		    (c != R_NULL) )
		{
			data->search_mode = NO_SEARCH;
			data->search_buff.clear();
			data->history_search.go_to_end();
			data->token_history_pos=-1;
		}
		
		last_char = c;
	}

	writestr( L"\n" );
/*
  if( comp )
  halloc_free( comp );
*/
	if( !reader_exit_forced() )
	{
		if( tcsetattr(0,TCSANOW,&old_modes))      /* return to previous mode */
		{
			wperror(L"tcsetattr");
		}
		
		set_color( rgb_color_t::reset(), rgb_color_t::reset() );
	}
	
	return finished ? data->command_line.c_str() : 0;
}

int reader_search_mode()
{
	if( !data )
	{
		return -1;
	}
	
	return !!data->search_mode;	
}


/**
   Read non-interactively.  Read input from stdin without displaying
   the prompt, using syntax highlighting. This is used for reading
   scripts and init files.
*/
static int read_ni( int fd, io_data_t *io )
{
    parser_t &parser = parser_t::principal_parser();
	FILE *in_stream;
	wchar_t *buff=0;
	std::vector<char> acc;

	int des = fd == 0 ? dup(0) : fd;
	int res=0;

	if (des == -1)
	{
		wperror( L"dup" );
		return 1;
	}

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
			
			if( ferror( in_stream ) && ( errno != EINTR ) )
			{
				debug( 1,
					   _( L"Error while reading from file descriptor" ) );
				
				/*
				  Reset buffer on error. We won't evaluate incomplete files.
				*/
				acc.clear();
				break;
				
			}

			acc.insert(acc.end(), buff, buff + c);
		}
        acc.push_back(0);
		acc_used = acc.size();
		str = str2wcs(&acc.at(0));
        acc.clear();

		if(	fclose( in_stream ))
		{
			debug( 1,
				   _( L"Error while closing input stream" ) );
			wperror( L"fclose" );
			res = 1;
		}

		if( str )
		{
			wcstring sb;
			if( ! parser.test( str, 0, &sb, L"fish" ) )
			{
				parser.eval( str, io, TOP );
			}
			else
			{
				fwprintf( stderr, L"%ls", sb.c_str() );
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
	return res;
}

int reader_read( int fd, io_data_t *io )
{
	int res;

	/*
	  If reader_read is called recursively through the '.' builtin, we
	  need to preserve is_interactive. This, and signal handler setup
	  is handled by proc_push_interactive/proc_pop_interactive.
	*/

	int inter = ((fd == STDIN_FILENO) && isatty(STDIN_FILENO));
	proc_push_interactive( inter );
	
	res= get_is_interactive() ? read_i():read_ni( fd, io );

	/*
	  If the exit command was called in a script, only exit the
	  script, not the program.
	*/
	if( data )
		data->end_loop = 0;
	end_loop = 0;
	
	proc_pop_interactive();
	return res;
}
