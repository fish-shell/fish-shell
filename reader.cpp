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
backwards searches are also handled by consulting the list up until
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
#include <pthread.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
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
#include "parser_keywords.h"

/**
   Maximum length of prefix string when printing completion
   list. Longer prefixes will be ellipsized.
*/
#define PREFIX_MAX_LEN 9

/**
   A simple prompt for reading shell commands that does not rely on
   fish specific commands, meaning it will work even if fish is not
   installed. This is used by read_i.
*/
#define DEFAULT_PROMPT L"echo -n \"$USER@\"(hostname|cut -d . -f 1)' '(__fish_pwd)'> '"

/**
   The name of the function that prints the fish prompt
 */
#define LEFT_PROMPT_FUNCTION_NAME L"fish_prompt"

/**
   The name of the function that prints the fish right prompt (RPROMPT)
 */
#define RIGHT_PROMPT_FUNCTION_NAME L"fish_right_prompt"


/**
   The default title for the reader. This is used by reader_readline.
*/
#define DEFAULT_TITLE L"echo $_ \" \"; __fish_pwd"

/**
   The maximum number of characters to read from the keyboard without
   repainting. Note that this readahead will only occur if new
   characters are available for reading, fish will never block for
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
   History search mode. This value means that we are performing a line
   history search.
 */
#define LINE_SEARCH 1
/**
   History search mode. This value means that we are performing a token
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

/* Any time the contents of a buffer changes, we update the generation count. This allows for our background highlighting thread to notice it and skip doing work that it would otherwise have to do. This variable should really be of some kind of interlocked or atomic type that guarantees we're not reading stale cache values. With C++11 we should use atomics, but until then volatile should work as well, at least on x86.*/
static volatile unsigned int s_generation_count;

/* This pthreads generation count is set when an autosuggestion background thread starts up, so it can easily check if the work it is doing is no longer useful. */
static pthread_key_t generation_count_key;

/* A color is an int */
typedef int color_t;

static void set_command_line_and_position(const wcstring &new_str, size_t pos);

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

    /** Whether autosuggesting is allowed at all */
    bool allow_autosuggestion;

    /** When backspacing, we temporarily suppress autosuggestions */
    bool suppress_autosuggestion;

    /** Whether abbreviations are expanded */
    bool expand_abbreviations;

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
    size_t search_pos;

    /** Length of the command */
    size_t command_length() const
    {
        return command_line.size();
    }

    /** Do what we need to do whenever our command line changes */
    void command_line_changed(void);

    /** Expand abbreviations at the current cursor position, minus backtrack_amt. */
    bool expand_abbreviation_as_necessary(size_t cursor_backtrack);

    /** The current position of the cursor in buff. */
    size_t buff_pos;

    /** Name of the current application */
    wcstring app_name;

    /** The prompt commands */
    wcstring left_prompt;
    wcstring right_prompt;

    /** The output of the last evaluation of the prompt command */
    wcstring left_prompt_buff;

    /** The output of the last evaluation of the right prompt command */
    wcstring right_prompt_buff;

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
    int (*test_func)(const wchar_t *);

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

    /** Whether a screen reset is needed after a repaint. */
    bool screen_reset_needed;

    /** Whether the reader should exit on ^C. */
    bool exit_on_interrupt;

    /** Constructor */
    reader_data_t() :
        allow_autosuggestion(0),
        suppress_autosuggestion(0),
        expand_abbreviations(0),
        history(0),
        token_history_pos(0),
        search_pos(0),
        buff_pos(0),
        complete_func(0),
        highlight_function(0),
        test_func(0),
        end_loop(0),
        prev_end_loop(0),
        next(0),
        search_mode(0),
        repaint_needed(0),
        screen_reset_needed(0),
        exit_on_interrupt(0)
    {
    }
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
static std::stack<const wchar_t *, std::vector<const wchar_t *> > current_filename;


/**
   Store the pid of the parent process, so the exit function knows whether it should reset the terminal or not.
*/
static pid_t original_pid;

/**
   This variable is set to true by the signal handler when ^C is pressed
*/
static volatile int interrupted=0;


/*
  Prototypes for a bunch of functions defined later on.
*/

static bool is_backslashed(const wcstring &str, size_t pos);
static wchar_t unescaped_quote(const wcstring &str, size_t pos);

/**
   Stores the previous termios mode so we can reset the modes when
   we execute programs and when the shell exits.
*/
static struct termios saved_modes;

static void reader_super_highlight_me_plenty(size_t pos);

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

    while (1)
    {
        if (tcsetattr(0,TCSANOW,&saved_modes))
        {
            if (errno != EINTR)
            {
                debug(1, _(L"Could not set terminal mode for new job"));
                wperror(L"tcsetattr");
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

    while (1)
    {
        if (tcsetattr(0,TCSANOW,&shell_modes))
        {
            if (errno != EINTR)
            {
                debug(1, _(L"Could not set terminal mode for shell"));
                wperror(L"tcsetattr");
                break;
            }
        }
        else
            break;
    }

    common_handle_winch(0);

}

int reader_exit_forced()
{
    return exit_forced;
}

/* Given a command line and an autosuggestion, return the string that gets shown to the user */
wcstring combine_command_and_autosuggestion(const wcstring &cmdline, const wcstring &autosuggestion)
{
    // We want to compute the full line, containing the command line and the autosuggestion
    // They may disagree on whether characters are uppercase or lowercase
    // Here we do something funny: if the last token of the command line contains any uppercase characters, we use its case
    // Otherwise we use the case of the autosuggestion
    // This is an idea from https://github.com/fish-shell/fish-shell/issues/335
    wcstring full_line;
    if (autosuggestion.size() <= cmdline.size() || cmdline.empty())
    {
        // No or useless autosuggestion, or no command line
        full_line = cmdline;
    }
    else if (string_prefixes_string(cmdline, autosuggestion))
    {
        // No case disagreements, or no extra characters in the autosuggestion
        full_line = autosuggestion;
    }
    else
    {
        // We have an autosuggestion which is not a prefix of the command line, i.e. a case disagreement
        // Decide whose case we want to use
        const wchar_t *begin = NULL, *cmd = cmdline.c_str();
        parse_util_token_extent(cmd, cmdline.size() - 1, &begin, NULL, NULL, NULL);
        bool last_token_contains_uppercase = false;
        if (begin)
        {
            const wchar_t *end = begin + wcslen(begin);
            last_token_contains_uppercase = (std::find_if(begin, end, iswupper) != end);
        }
        if (! last_token_contains_uppercase)
        {
            // Use the autosuggestion's case
            full_line = autosuggestion;
        }
        else
        {
            // Use the command line case for its characters, then append the remaining characters in the autosuggestion
            // Note that we know that autosuggestion.size() > cmdline.size() due to the first test above
            full_line = cmdline;
            full_line.append(autosuggestion, cmdline.size(), autosuggestion.size() - cmdline.size());
        }
    }
    return full_line;
}

/**
   Repaint the entire commandline. This means reset and clear the
   commandline, write the prompt, perform syntax highlighting, write
   the commandline and move the cursor.
*/

static void reader_repaint()
{
    // Update the indentation
    parser_t::principal_parser().test(data->command_line.c_str(), &data->indents[0]);

    // Combine the command and autosuggestion into one string
    wcstring full_line = combine_command_and_autosuggestion(data->command_line, data->autosuggestion);

    size_t len = full_line.size();
    if (len < 1)
        len = 1;

    std::vector<color_t> colors = data->colors;
    colors.resize(len, HIGHLIGHT_AUTOSUGGESTION);

    std::vector<int> indents = data->indents;
    indents.resize(len);

    s_write(&data->screen,
            data->left_prompt_buff,
            data->right_prompt_buff,
            full_line,
            data->command_length(),
            &colors[0],
            &indents[0],
            data->buff_pos);

    data->repaint_needed = false;
}

static void reader_repaint_without_autosuggestion()
{
    // Swap in an empty autosuggestion, repaint, then swap it out
    wcstring saved_autosuggestion;
    data->autosuggestion.swap(saved_autosuggestion);
    reader_repaint();
    data->autosuggestion.swap(saved_autosuggestion);
}

/** Internal helper function for handling killing parts of text. */
static void reader_kill(size_t begin_idx, size_t length, int mode, int newv)
{
    const wchar_t *begin = data->command_line.c_str() + begin_idx;
    if (newv)
    {
        data->kill_item = wcstring(begin, length);
        kill_add(data->kill_item);
    }
    else
    {
        wcstring old = data->kill_item;
        if (mode == KILL_APPEND)
        {
            data->kill_item.append(begin, length);
        }
        else
        {
            data->kill_item = wcstring(begin, length);
            data->kill_item.append(old);
        }


        kill_replace(old, data->kill_item);
    }

    if (data->buff_pos > begin_idx)
    {
        /* Move the buff position back by the number of characters we deleted, but don't go past buff_pos */
        size_t backtrack = mini(data->buff_pos - begin_idx, length);
        data->buff_pos -= backtrack;
    }

    data->command_line.erase(begin_idx, length);
    data->command_line_changed();

    reader_super_highlight_me_plenty(data->buff_pos);
    reader_repaint();

}


/* This is called from a signal handler! */
void reader_handle_int(int sig)
{
    if (!is_interactive_read)
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


void reader_push_current_filename(const wchar_t *fn)
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
void reader_data_t::command_line_changed()
{
    ASSERT_IS_MAIN_THREAD();
    size_t len = command_length();

    /* When we grow colors, propagate the last color (if any), under the assumption that usually it will be correct. If it is, it avoids a repaint. */
    color_t last_color = colors.empty() ? color_t() : colors.back();
    colors.resize(len, last_color);

    indents.resize(len);

    /* Update the gen count */
    s_generation_count++;
}

/* Expand abbreviations at the given cursor position. Does NOT inspect 'data'. */
bool reader_expand_abbreviation_in_command(const wcstring &cmdline, size_t cursor_pos, wcstring *output)
{
    /* See if we are at "command position". Get the surrounding command substitution, and get the extent of the first token. */
    const wchar_t * const buff = cmdline.c_str();
    const wchar_t *cmdsub_begin = NULL, *cmdsub_end = NULL;
    parse_util_cmdsubst_extent(buff, cursor_pos, &cmdsub_begin, &cmdsub_end);
    assert(cmdsub_begin != NULL && cmdsub_begin >= buff);
    assert(cmdsub_end != NULL && cmdsub_end >= cmdsub_begin);

    /* Determine the offset of this command substitution */
    const size_t subcmd_offset = cmdsub_begin - buff;

    const wcstring subcmd = wcstring(cmdsub_begin, cmdsub_end - cmdsub_begin);
    const wchar_t *subcmd_cstr = subcmd.c_str();

    /* Get the token containing the cursor */
    const wchar_t *subcmd_tok_begin = NULL, *subcmd_tok_end = NULL;
    assert(cursor_pos >= subcmd_offset);
    size_t subcmd_cursor_pos = cursor_pos - subcmd_offset;
    parse_util_token_extent(subcmd_cstr, subcmd_cursor_pos, &subcmd_tok_begin, &subcmd_tok_end, NULL, NULL);

    /* Compute the offset of the token before the cursor within the subcmd */
    assert(subcmd_tok_begin >= subcmd_cstr);
    assert(subcmd_tok_end >= subcmd_tok_begin);
    const size_t subcmd_tok_begin_offset = subcmd_tok_begin - subcmd_cstr;
    const size_t subcmd_tok_length = subcmd_tok_end - subcmd_tok_begin;

    /* Now parse the subcmd, looking for commands */
    bool had_cmd = false, previous_token_is_cmd = false;
    tokenizer_t tok(subcmd_cstr, TOK_ACCEPT_UNFINISHED | TOK_SQUASH_ERRORS);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        size_t tok_pos = static_cast<size_t>(tok_get_pos(&tok));
        if (tok_pos > subcmd_tok_begin_offset)
        {
            /* We've passed the token we're interested in */
            break;
        }

        int last_type = tok_last_type(&tok);

        switch (last_type)
        {
            case TOK_STRING:
            {
                if (had_cmd)
                {
                    /* Parameter to the command. */
                }
                else
                {
                    const wcstring potential_cmd = tok_last(&tok);
                    if (parser_keywords_is_subcommand(potential_cmd))
                    {
                        if (potential_cmd == L"command" || potential_cmd == L"builtin")
                        {
                            /* 'command' and 'builtin' defeat abbreviation expansion. Skip this command. */
                            had_cmd = true;
                        }
                        else
                        {
                            /* Other subcommand. Pretend it doesn't exist so that we can expand the following command */
                            had_cmd = false;
                        }
                    }
                    else
                    {
                        /* It's a normal command */
                        had_cmd = true;
                        if (tok_pos == subcmd_tok_begin_offset)
                        {
                            /* This is the token we care about! */
                            previous_token_is_cmd = true;
                        }
                    }
                }
                break;
            }

            case TOK_REDIRECT_NOCLOB:
            case TOK_REDIRECT_OUT:
            case TOK_REDIRECT_IN:
            case TOK_REDIRECT_APPEND:
            case TOK_REDIRECT_FD:
            {
                if (!had_cmd)
                {
                    break;
                }
                tok_next(&tok);
                break;
            }

            case TOK_PIPE:
            case TOK_BACKGROUND:
            case TOK_END:
            {
                had_cmd = false;
                break;
            }

            case TOK_COMMENT:
            case TOK_ERROR:
            default:
            {
                break;
            }
        }
    }

    bool result = false;
    if (previous_token_is_cmd)
    {
        /* The token is a command. Try expanding it as an abbreviation. */
        const wcstring token = wcstring(subcmd, subcmd_tok_begin_offset, subcmd_tok_length);
        wcstring abbreviation;
        if (expand_abbreviation(token, &abbreviation))
        {
            /* There was an abbreviation! Replace the token in the full command. Maintain the relative position of the cursor. */
            if (output != NULL)
            {
                size_t cmd_tok_begin_offset = subcmd_tok_begin_offset + subcmd_offset;
                output->assign(cmdline);
                output->replace(cmd_tok_begin_offset, subcmd_tok_length, abbreviation);
            }
            result = true;
        }
    }
    return result;
}

/* Expand abbreviations at the current cursor position, minus the given  cursor backtrack. This may change the command line but does NOT repaint it. This is to allow the caller to coalesce repaints. */
bool reader_data_t::expand_abbreviation_as_necessary(size_t cursor_backtrack)
{
    bool result = false;
    if (this->expand_abbreviations)
    {
        /* Try expanding abbreviations */
        wcstring new_cmdline;
        size_t cursor_pos = this->buff_pos - mini(this->buff_pos, cursor_backtrack);
        if (reader_expand_abbreviation_in_command(this->command_line, cursor_pos, &new_cmdline))
        {
            /* We expanded an abbreviation! The cursor moves by the difference in the command line lengths. */
            size_t new_buff_pos = this->buff_pos + new_cmdline.size() - this->command_line.size();

            this->command_line.swap(new_cmdline);
            data->buff_pos = new_buff_pos;
            data->command_line_changed();
            result = true;
        }
    }
    return result;
}

/** Sorts and remove any duplicate completions in the list. */
static void sort_and_make_unique(std::vector<completion_t> &l)
{
    sort(l.begin(), l.end(), completion_t::is_alphabetically_less_than);
    l.erase(std::unique(l.begin(), l.end(), completion_t::is_alphabetically_equal_to), l.end());
}


void reader_reset_interrupted()
{
    interrupted = 0;
}

int reader_interrupted()
{
    int res = interrupted;
    if (res)
    {
        interrupted=0;
    }
    return res;
}

int reader_reading_interrupted()
{
    int res = reader_interrupted();
    if (res && data && data->exit_on_interrupt)
    {
        reader_exit(1, 0);
        parser_t::skip_all_blocks();
        // We handled the interrupt ourselves, our caller doesn't need to
        // handle it.
        return 0;
    }
    return res;
}

bool reader_thread_job_is_stale()
{
    ASSERT_IS_BACKGROUND_THREAD();
    return (void*)(uintptr_t) s_generation_count != pthread_getspecific(generation_count_key);
}

void reader_write_title()
{
    const wchar_t *title;
    const env_var_t term_str = env_get_string(L"TERM");

    /*
      This is a pretty lame heuristic for detecting terminals that do
      not support setting the title. If we recognise the terminal name
      as that of a virtual terminal, we assume it supports setting the
      title. If we recognise it as that of a console, we assume it
      does not support setting the title. Otherwise we check the
      ttyname and see if we believe it is a virtual terminal.

      One situation in which this breaks down is with screen, since
      screen supports setting the terminal title if the underlying
      terminal does so, but will print garbage on terminals that
      don't. Since we can't see the underlying terminal below screen
      there is no way to fix this.
    */
    if (term_str.missing())
        return;

    const wchar_t *term = term_str.c_str();
    bool recognized = false;
    recognized = recognized || contains(term, L"xterm", L"screen", L"nxterm", L"rxvt");
    recognized = recognized || ! wcsncmp(term, L"xterm-", wcslen(L"xterm-"));
    recognized = recognized || ! wcsncmp(term, L"screen-", wcslen(L"screen-"));

    if (! recognized)
    {
        char *n = ttyname(STDIN_FILENO);


        if (contains(term, L"linux"))
        {
            return;
        }

        if (strstr(n, "tty") || strstr(n, "/vc/"))
            return;


    }

    title = function_exists(L"fish_title")?L"fish_title":DEFAULT_TITLE;

    if (wcslen(title) ==0)
        return;

    wcstring_list_t lst;

    proc_push_interactive(0);
    if (exec_subshell(title, lst, false /* do not apply exit status */) != -1)
    {
        if (! lst.empty())
        {
            writestr(L"\x1b]0;");
            for (size_t i=0; i<lst.size(); i++)
            {
                writestr(lst.at(i).c_str());
            }
            writestr(L"\7");
        }
    }
    proc_pop_interactive();
    set_color(rgb_color_t::reset(), rgb_color_t::reset());
}

/**
   Reexecute the prompt command. The output is inserted into data->prompt_buff.
*/
static void exec_prompt()
{
    /* Clear existing prompts */
    data->left_prompt_buff.clear();
    data->right_prompt_buff.clear();

    /* Do not allow the exit status of the prompts to leak through */
    const bool apply_exit_status = false;

    /* If we have any prompts, they must be run non-interactively */
    if (data->left_prompt.size() || data->right_prompt.size())
    {
        proc_push_interactive(0);

        if (! data->left_prompt.empty())
        {
            wcstring_list_t prompt_list;
            // ignore return status
            exec_subshell(data->left_prompt, prompt_list, apply_exit_status);
            for (size_t i = 0; i < prompt_list.size(); i++)
            {
                if (i > 0) data->left_prompt_buff += L'\n';
                data->left_prompt_buff += prompt_list.at(i);
            }
        }

        if (! data->right_prompt.empty())
        {
            wcstring_list_t prompt_list;
            // status is ignored
            exec_subshell(data->right_prompt, prompt_list, apply_exit_status);
            for (size_t i = 0; i < prompt_list.size(); i++)
            {
                // Right prompt does not support multiple lines, so just concatenate all of them
                data->right_prompt_buff += prompt_list.at(i);
            }
        }

        proc_pop_interactive();
    }

    /* Write the screen title */
    reader_write_title();
}

void reader_init()
{
    VOMIT_ON_FAILURE(pthread_key_create(&generation_count_key, NULL));

    tcgetattr(0,&shell_modes);        /* get the current terminal modes */
    memcpy(&saved_modes,
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
}


void reader_destroy()
{
    tcsetattr(0, TCSANOW, &saved_modes);
    pthread_key_delete(generation_count_key);
}


void reader_exit(int do_exit, int forced)
{
    if (data)
        data->end_loop=do_exit;
    end_loop=do_exit;
    if (forced)
        exit_forced = 1;

}

void reader_repaint_needed()
{
    if (data)
    {
        data->repaint_needed = true;
    }
}

void reader_repaint_if_needed()
{
    if (data == NULL)
        return;

    bool needs_reset = data->screen_reset_needed;
    bool needs_repaint = needs_reset || data->repaint_needed;

    if (needs_reset)
    {
        exec_prompt();
        s_reset(&data->screen, screen_reset_current_line_and_prompt);
        data->screen_reset_needed = false;
    }

    if (needs_repaint)
    {
        reader_repaint();
        /* reader_repaint clears repaint_needed */
    }
}

static void reader_repaint_if_needed_one_arg(void * unused)
{
    reader_repaint_if_needed();
}

void reader_react_to_color_change()
{
    if (! data)
        return;

    if (! data->repaint_needed || ! data->screen_reset_needed)
    {
        data->repaint_needed = true;
        data->screen_reset_needed = true;
        input_common_add_callback(reader_repaint_if_needed_one_arg, NULL);
    }
}


/**
   Remove the previous character in the character buffer and on the
   screen using syntax highlighting, etc.
*/
static void remove_backward()
{

    if (data->buff_pos <= 0)
        return;

    /* Fake composed character sequences by continuing to delete until we delete a character of width at least 1. */
    int width;
    do
    {
        data->buff_pos -= 1;
        width = fish_wcwidth(data->command_line.at(data->buff_pos));
        data->command_line.erase(data->buff_pos, 1);
    }
    while (width == 0 && data->buff_pos > 0);
    data->command_line_changed();
    data->suppress_autosuggestion = true;

    reader_super_highlight_me_plenty(data->buff_pos);

    reader_repaint();

}


/**
   Insert the characters of the string into the command line buffer
   and print them to the screen using syntax highlighting, etc.
   Optionally also expand abbreviations.
   Returns true if the string changed.
*/
static bool insert_string(const wcstring &str, bool should_expand_abbreviations = false)
{
    size_t len = str.size();
    if (len == 0)
        return false;

    data->command_line.insert(data->buff_pos, str);
    data->buff_pos += len;
    data->command_line_changed();
    data->suppress_autosuggestion = false;

    if (should_expand_abbreviations)
        data->expand_abbreviation_as_necessary(1);

    /* Syntax highlight. Note we must have that buff_pos > 0 because we just added something nonzero to its length  */
    assert(data->buff_pos > 0);
    reader_super_highlight_me_plenty(data->buff_pos-1);
    reader_repaint();

    return true;
}

/**
   Insert the character into the command line buffer and print it to
   the screen using syntax highlighting, etc.
*/
static bool insert_char(wchar_t c, bool should_expand_abbreviations = false)
{
    return insert_string(wcstring(1, c), should_expand_abbreviations);
}


/**
   Insert the string in the given command line at the given cursor
   position. The function checks if the string is quoted or not and
   correctly escapes the string.
   \param val the string to insert
   \param flags A union of all flags describing the completion to insert. See the completion_t struct for more information on possible values.
   \param command_line The command line into which we will insert
   \param inout_cursor_pos On input, the location of the cursor within the command line. On output, the new desired position.
   \param append_only Whether we can only append to the command line, or also modify previous characters. This is used to determine whether we go inside a trailing quote.
   \return The completed string
*/
wcstring completion_apply_to_command_line(const wcstring &val_str, complete_flags_t flags, const wcstring &command_line, size_t *inout_cursor_pos, bool append_only)
{
    const wchar_t *val = val_str.c_str();
    bool add_space = !(flags & COMPLETE_NO_SPACE);
    bool do_replace = !!(flags & COMPLETE_REPLACES_TOKEN);
    bool do_escape = !(flags & COMPLETE_DONT_ESCAPE);

    const size_t cursor_pos = *inout_cursor_pos;
    bool back_into_trailing_quote = false;

    if (do_replace)
    {
        size_t move_cursor;
        const wchar_t *begin, *end;
        wchar_t *escaped;

        const wchar_t *buff = command_line.c_str();
        parse_util_token_extent(buff, cursor_pos, &begin, 0, 0, 0);
        end = buff + cursor_pos;

        wcstring sb(buff, begin - buff);

        if (do_escape)
        {
            /* Respect COMPLETE_DONT_ESCAPE_TILDES */
            bool no_tilde = !!(flags & COMPLETE_DONT_ESCAPE_TILDES);
            escaped = escape(val, ESCAPE_ALL | ESCAPE_NO_QUOTED | (no_tilde ? ESCAPE_NO_TILDE : 0));
            sb.append(escaped);
            move_cursor = wcslen(escaped);
            free(escaped);
        }
        else
        {
            sb.append(val);
            move_cursor = wcslen(val);
        }


        if (add_space)
        {
            sb.append(L" ");
            move_cursor += 1;
        }
        sb.append(end);

        size_t new_cursor_pos = (begin - buff) + move_cursor;
        *inout_cursor_pos = new_cursor_pos;
        return sb;
    }
    else
    {
        wchar_t quote = L'\0';
        wcstring replaced;
        if (do_escape)
        {
            /* Note that we ignore COMPLETE_DONT_ESCAPE_TILDES here. We get away with this because unexpand_tildes only operates on completions that have COMPLETE_REPLACES_TOKEN set, but we ought to respect them */
            parse_util_get_parameter_info(command_line, cursor_pos, &quote, NULL, NULL);

            /* If the token is reported as unquoted, but ends with a (unescaped) quote, and we can modify the command line, then delete the trailing quote so that we can insert within the quotes instead of after them. See https://github.com/fish-shell/fish-shell/issues/552 */
            if (quote == L'\0' && ! append_only && cursor_pos > 0)
            {
                /* The entire token is reported as unquoted...see if the last character is an unescaped quote */
                wchar_t trailing_quote = unescaped_quote(command_line, cursor_pos - 1);
                if (trailing_quote != L'\0')
                {
                    quote = trailing_quote;
                    back_into_trailing_quote = true;
                }
            }

            replaced = parse_util_escape_string_with_quote(val_str, quote);
        }
        else
        {
            replaced = val;
        }

        size_t insertion_point = cursor_pos;
        if (back_into_trailing_quote)
        {
            /* Move the character back one so we enter the terminal quote */
            assert(insertion_point > 0);
            insertion_point--;
        }

        /* Perform the insertion and compute the new location */
        wcstring result = command_line;
        result.insert(insertion_point, replaced);
        size_t new_cursor_pos = insertion_point + replaced.size() + (back_into_trailing_quote ? 1 : 0);
        if (add_space)
        {
            if (quote != L'\0' && unescaped_quote(command_line, insertion_point) != quote)
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
static void completion_insert(const wchar_t *val, complete_flags_t flags)
{
    size_t cursor = data->buff_pos;
    wcstring new_command_line = completion_apply_to_command_line(val, flags, data->command_line, &cursor, false /* not append only */);
    reader_set_buffer(new_command_line, cursor);

    /* Since we just inserted a completion, don't immediately do a new autosuggestion */
    data->suppress_autosuggestion = true;
}

/* Return an escaped path to fish_pager */
static wcstring escaped_fish_pager_path(void)
{
    wcstring result;
    const env_var_t bin_dir = env_get_string(L"__fish_bin_dir");
    if (bin_dir.missing_or_empty())
    {
        /* This isn't good, hope our normal command stuff can find it */
        result = L"fish_pager";
    }
    else
    {
        result = escape_string(bin_dir + L"/fish_pager", ESCAPE_ALL);
    }
    return result;
}

/**
   Run the fish_pager command to display the completion list. If the
   fish_pager outputs any text, it is inserted into the input
   backbuffer.

   \param prefix the string to display before every completion.
   \param is_quoted should be set if the argument is quoted. This will change the display style.
   \param comp the list of completions to display
*/
static void run_pager(const wcstring &prefix, int is_quoted, const std::vector<completion_t> &comp)
{
    wcstring msg;
    wcstring prefix_esc;
    char *foo;

    shared_ptr<io_buffer_t> in_buff(io_buffer_t::create(true, 3));
    shared_ptr<io_buffer_t> out_buff(io_buffer_t::create(false, 4));

    // The above may fail e.g. if we have too many open fds
    if (in_buff.get() == NULL || out_buff.get() == NULL)
        return;

    wchar_t *escaped_separator;

    if (prefix.empty())
    {
        prefix_esc = L"\"\"";
    }
    else
    {
        prefix_esc = escape_string(prefix, 1);
    }


    const wcstring pager_path = escaped_fish_pager_path();
    const wcstring cmd = format_string(L"%ls -c 3 -r 4 %ls -p %ls",
                                       // L"valgrind --track-fds=yes --log-file=pager.txt --leak-check=full ./%ls %d %ls",
                                       pager_path.c_str(),
                                       is_quoted?L"-q":L"",
                                       prefix_esc.c_str());

    escaped_separator = escape(COMPLETE_SEP_STR, 1);

    for (size_t i=0; i< comp.size(); i++)
    {
        long base_len=-1;
        const completion_t &el = comp.at(i);

        wcstring completion_text;
        wcstring description_text;

        // Note that an empty completion is perfectly sensible here, e.g. tab-completing 'foo' with a file called 'foo' and another called 'foobar'
        if ((el.flags & COMPLETE_REPLACES_TOKEN) && match_type_shares_prefix(el.match.type))
        {
            // Compute base_len if we have not yet
            if (base_len == -1)
            {
                const wchar_t *begin, *buff = data->command_line.c_str();

                parse_util_token_extent(buff, data->buff_pos, &begin, 0, 0, 0);
                base_len = data->buff_pos - (begin-buff);
            }

            completion_text = escape_string(el.completion.c_str() + base_len, ESCAPE_ALL | ESCAPE_NO_QUOTED);
        }
        else
        {
            completion_text = escape_string(el.completion, ESCAPE_ALL | ESCAPE_NO_QUOTED);
        }


        if (! el.description.empty())
        {
            description_text = escape_string(el.description, true);
        }

        /* It's possible (even common) to have an empty completion with no description. An example would be completing 'foo' with extant files 'foo' and 'foobar'. But fish_pager ignores blank lines. So if our completion text is empty, always include a description, even if it's empty.
        */
        msg.reserve(msg.size() + completion_text.size() + description_text.size() + 2);
        msg.append(completion_text);
        if (! description_text.empty() || completion_text.empty())
        {
            msg.append(escaped_separator);
            msg.append(description_text);
        }
        msg.push_back(L'\n');
    }

    free(escaped_separator);

    foo = wcs2str(msg.c_str());
    in_buff->out_buffer_append(foo, strlen(foo));
    free(foo);

    term_donate();
    parser_t &parser = parser_t::principal_parser();
    io_chain_t io_chain;
    io_chain.push_back(out_buff);
    io_chain.push_back(in_buff);
    parser.eval(cmd, io_chain, TOP);
    term_steal();

    out_buff->read();

    const char zero = 0;
    out_buff->out_buffer_append(&zero, 1);

    const char *out_data = out_buff->out_buffer_ptr();
    if (out_data)
    {
        const wcstring str = str2wcstring(out_data);
        size_t idx = str.size();
        while (idx--)
        {
            input_unreadch(str.at(idx));
        }
    }
}

struct autosuggestion_context_t
{
    wcstring search_string;
    wcstring autosuggestion;
    size_t cursor_pos;
    history_search_t searcher;
    file_detection_context_t detector;
    const wcstring working_directory;
    const env_vars_snapshot_t vars;
    wcstring_list_t commands_to_load;
    const unsigned int generation_count;

    // don't reload more than once
    bool has_tried_reloading;

    autosuggestion_context_t(history_t *history, const wcstring &term, size_t pos) :
        search_string(term),
        cursor_pos(pos),
        searcher(*history, term, HISTORY_SEARCH_TYPE_PREFIX),
        detector(history, term),
        working_directory(env_get_pwd_slash()),
        vars(env_vars_snapshot_t::highlighting_keys),
        generation_count(s_generation_count),
        has_tried_reloading(false)
    {
    }

    /* The function run in the background thread to determine an autosuggestion */
    int threaded_autosuggest(void)
    {
        ASSERT_IS_BACKGROUND_THREAD();

        /* If the main thread has moved on, skip all the work */
        if (generation_count != s_generation_count)
        {
            return 0;
        }

        VOMIT_ON_FAILURE(pthread_setspecific(generation_count_key, (void*)(uintptr_t) generation_count));

        /* Let's make sure we aren't using the empty string */
        if (search_string.empty())
        {
            return 0;
        }

        while (! reader_thread_job_is_stale() && searcher.go_backwards())
        {
            history_item_t item = searcher.current_item();

            /* Skip items with newlines because they make terrible autosuggestions */
            if (item.str().find('\n') != wcstring::npos)
                continue;

            if (autosuggest_validate_from_history(item, detector, working_directory, vars))
            {
                /* The command autosuggestion was handled specially, so we're done */
                this->autosuggestion = searcher.current_string();
                return 1;
            }
        }

        /* Maybe cancel here */
        if (reader_thread_job_is_stale())
            return 0;

        /* Try handling a special command like cd */
        wcstring special_suggestion;
        if (autosuggest_suggest_special(search_string, working_directory, special_suggestion))
        {
            this->autosuggestion = special_suggestion;
            return 1;
        }

        /* Maybe cancel here */
        if (reader_thread_job_is_stale())
            return 0;

        // Here we do something a little funny
        // If the line ends with a space, and the cursor is not at the end,
        // don't use completion autosuggestions. It ends up being pretty weird seeing stuff get spammed on the right
        // while you go back to edit a line
        const wchar_t last_char = search_string.at(search_string.size() - 1);
        const bool cursor_at_end = (this->cursor_pos == search_string.size());
        if (! cursor_at_end && iswspace(last_char))
            return 0;

        /* On the other hand, if the line ends with a quote, don't go dumping stuff after the quote */
        if (wcschr(L"'\"", last_char) && cursor_at_end)
            return 0;

        /* Try normal completions */
        std::vector<completion_t> completions;
        complete(search_string, completions, COMPLETION_REQUEST_AUTOSUGGESTION, &this->commands_to_load);
        if (! completions.empty())
        {
            const completion_t &comp = completions.at(0);
            size_t cursor = this->cursor_pos;
            this->autosuggestion = completion_apply_to_command_line(comp.completion.c_str(), comp.flags, this->search_string, &cursor, true /* append only */);
            return 1;
        }

        return 0;
    }
};

static int threaded_autosuggest(autosuggestion_context_t *ctx)
{
    return ctx->threaded_autosuggest();
}

static bool can_autosuggest(void)
{
    /* We autosuggest if suppress_autosuggestion is not set, if we're not doing a history search, and our command line contains a non-whitespace character. */
    const wchar_t *whitespace = L" \t\r\n\v";
    return ! data->suppress_autosuggestion &&
           data->history_search.is_at_end() &&
           data->command_line.find_first_not_of(whitespace) != wcstring::npos;
}

static void autosuggest_completed(autosuggestion_context_t *ctx, int result)
{

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
            string_prefixes_string_case_insensitive(ctx->search_string, ctx->autosuggestion))
    {
        /* Autosuggestion is active and the search term has not changed, so we're good to go */
        data->autosuggestion = ctx->autosuggestion;
        sanity_check();
        reader_repaint();
    }
    delete ctx;
}


static void update_autosuggestion(void)
{
    /* Updates autosuggestion. We look for an autosuggestion if the command line is non-empty and if we're not doing a history search.  */
    data->autosuggestion.clear();
    if (data->allow_autosuggestion && ! data->suppress_autosuggestion && ! data->command_line.empty() && data->history_search.is_at_end())
    {
        autosuggestion_context_t *ctx = new autosuggestion_context_t(data->history, data->command_line, data->buff_pos);
        iothread_perform(threaded_autosuggest, autosuggest_completed, ctx);
    }
}

/* Accept any autosuggestion by replacing the command line with it. If full is true, take the whole thing; if it's false, then take only the first "word" */
static void accept_autosuggestion(bool full)
{
    if (! data->autosuggestion.empty())
    {
        /* Accept the autosuggestion */
        if (full)
        {
            /* Just take the whole thing */
            data->command_line = data->autosuggestion;
        }
        else
        {
            /* Accept characters up to a word separator */
            move_word_state_machine_t state(move_word_style_punctuation);
            for (size_t idx = data->command_line.size(); idx < data->autosuggestion.size(); idx++)
            {
                wchar_t wc = data->autosuggestion.at(idx);
                if (! state.consume_char(wc))
                    break;
                data->command_line.push_back(wc);
            }
        }
        data->buff_pos = data->command_line.size();
        data->command_line_changed();
        reader_super_highlight_me_plenty(data->buff_pos);
        reader_repaint();
    }
}

/**
  Flash the screen. This function only changed the color of the
  current line, since the flash_screen sequnce is rather painful to
  look at in most terminal emulators.
*/
static void reader_flash()
{
    struct timespec pollint;

    for (size_t i=0; i<data->buff_pos; i++)
    {
        data->colors.at(i) = HIGHLIGHT_SEARCH_MATCH<<16;
    }

    reader_repaint();

    pollint.tv_sec = 0;
    pollint.tv_nsec = 100 * 1000000;
    nanosleep(&pollint, NULL);

    reader_super_highlight_me_plenty(data->buff_pos);

    reader_repaint();
}

/**
   Characters that may not be part of a token that is to be replaced
   by a case insensitive completion.
 */
#define REPLACE_UNCLEAN L"$*?({})"

/**
   Check if the specified string can be replaced by a case insensitive
   completion with the specified flags.

   Advanced tokens like those containing {}-style expansion can not at
   the moment be replaced, other than if the new token is already an
   exact replacement, e.g. if the COMPLETE_DONT_ESCAPE flag is set.
 */

static bool reader_can_replace(const wcstring &in, int flags)
{

    const wchar_t * str = in.c_str();

    if (flags & COMPLETE_DONT_ESCAPE)
    {
        return true;
    }
    /*
      Test characters that have a special meaning in any character position
    */
    while (*str)
    {
        if (wcschr(REPLACE_UNCLEAN, *str))
            return false;
        str++;
    }

    return true;
}

/* Compare two completions, ordering completions with better match types first */
bool compare_completions_by_match_type(const completion_t &a, const completion_t &b)
{
    /* Compare match types, unless both completions are prefix (#923) in which case we always want to compare them alphabetically */
    if (a.match.type != fuzzy_match_prefix || b.match.type != fuzzy_match_prefix)
    {
        int match_compare = a.match.compare(b.match);
        if (match_compare != 0)
        {
            return match_compare < 0;
        }
    }

    /* Compare using file comparison */
    return wcsfilecmp(a.completion.c_str(), b.completion.c_str()) < 0;
}

/* Determine the best match type for a set of completions */
static fuzzy_match_type_t get_best_match_type(const std::vector<completion_t> &comp)
{
    fuzzy_match_type_t best_type = fuzzy_match_none;
    for (size_t i=0; i < comp.size(); i++)
    {
        const completion_t &el = comp.at(i);
        if (el.match.type < best_type)
        {
            best_type = el.match.type;
        }
    }
    /* If the best type is an exact match, reduce it to prefix match. Otherwise a tab completion will only show one match if it matches a file exactly. (see issue #959) */
    if (best_type == fuzzy_match_exact)
    {
        best_type = fuzzy_match_prefix;
    }
    return best_type;
}

/* Order completions such that case insensitive completions come first. */
static void prioritize_completions(std::vector<completion_t> &comp)
{
    fuzzy_match_type_t best_type = get_best_match_type(comp);

    /* Throw out completions whose match types are less suitable than the best. */
    size_t i = comp.size();
    while (i--)
    {
        if (comp.at(i).match.type > best_type)
        {
            comp.erase(comp.begin() + i);
        }
    }

    /* Sort the remainder */
    sort(comp.begin(), comp.end(), compare_completions_by_match_type);
}

/* Given a list of completions, get the completion at an index past *inout_idx, and then increment it. inout_idx should be initialized to (size_t)(-1) for the first call. */
static const completion_t *cycle_competions(const std::vector<completion_t> &comp, const wcstring &command_line, size_t *inout_idx)
{
    const size_t size = comp.size();
    if (size == 0)
        return NULL;

    const size_t start_idx = *inout_idx;
    size_t idx = start_idx;
    const completion_t *result = NULL;
    for (;;)
    {
        /* Bump the index */
        idx = (idx + 1) % size;

        /* Bail if we've looped */
        if (idx == start_idx)
            break;

        /* Get the completion */
        const completion_t &c = comp.at(idx);

        /* Try this completion */
        if (!(c.flags & COMPLETE_REPLACES_TOKEN) || reader_can_replace(command_line, c.flags))
        {
            /* Success */
            result = &c;
            break;
        }
    }

    *inout_idx = idx;
    return result;
}

/**
   Handle the list of completions. This means the following:

   - If the list is empty, flash the terminal.
   - If the list contains one element, write the whole element, and if
   the element does not end on a '/', '@', ':', or a '=', also write a trailing
   space.
   - If the list contains multiple elements with a common prefix, write
   the prefix.
   - If the list contains multiple elements without a common prefix, call
   run_pager to display a list of completions. Depending on terminal size and
   the length of the list, run_pager may either show less than a screenfull and
   exit or use an interactive pager to allow the user to scroll through the
   completions.

   \param comp the list of completion strings

   Return true if we inserted text into the command line, false if we did not.
*/

static bool handle_completions(const std::vector<completion_t> &comp)
{
    bool done = false;
    bool success = false;
    const wchar_t *begin, *end, *buff = data->command_line.c_str();

    parse_util_token_extent(buff, data->buff_pos, &begin, 0, 0, 0);
    end = buff+data->buff_pos;

    const wcstring tok(begin, end - begin);

    /*
      Check trivial cases
     */
    switch (comp.size())
    {
            /* No suitable completions found, flash screen and return */
        case 0:
        {
            reader_flash();
            done = true;
            success = false;
            break;
        }

        /* Exactly one suitable completion found - insert it */
        case 1:
        {

            const completion_t &c = comp.at(0);

            /*
              If this is a replacement completion, check
              that we know how to replace it, e.g. that
              the token doesn't contain evil operators
              like {}
             */
            if (!(c.flags & COMPLETE_REPLACES_TOKEN) || reader_can_replace(tok, c.flags))
            {
                completion_insert(c.completion.c_str(), c.flags);
            }
            done = true;
            success = true;
            break;
        }
    }


    if (!done)
    {
        fuzzy_match_type_t best_match_type = get_best_match_type(comp);

        /* Determine whether we are going to replace the token or not. If any commands of the best type do not require replacement, then ignore all those that want to use replacement */
        bool will_replace_token = true;
        for (size_t i=0; i< comp.size(); i++)
        {
            const completion_t &el = comp.at(i);
            if (el.match.type <= best_match_type && !(el.flags & COMPLETE_REPLACES_TOKEN))
            {
                will_replace_token = false;
                break;
            }
        }

        /* Decide which completions survived. There may be a lot of them; it would be nice if we could figure out how to avoid copying them here */
        std::vector<completion_t> surviving_completions;
        for (size_t i=0; i < comp.size(); i++)
        {
            const completion_t &el = comp.at(i);
            /* Ignore completions with a less suitable match type than the best. */
            if (el.match.type > best_match_type)
                continue;

            /* Only use completions that match replace_token */
            bool completion_replace_token = !!(el.flags & COMPLETE_REPLACES_TOKEN);
            if (completion_replace_token != will_replace_token)
                continue;

            /* Don't use completions that want to replace, if we cannot replace them */
            if (completion_replace_token && ! reader_can_replace(tok, el.flags))
                continue;

            /* This completion survived */
            surviving_completions.push_back(el);
        }


        /* Try to find a common prefix to insert among the surviving completions */
        wcstring common_prefix;
        complete_flags_t flags = 0;
        bool prefix_is_partial_completion = false;
        for (size_t i=0; i < surviving_completions.size(); i++)
        {
            const completion_t &el = surviving_completions.at(i);
            if (i == 0)
            {
                /* First entry, use the whole string */
                common_prefix = el.completion;
                flags = el.flags;
            }
            else
            {
                /* Determine the shared prefix length. */
                size_t idx, max = mini(common_prefix.size(), el.completion.size());
                for (idx=0; idx < max; idx++)
                {
                    wchar_t ac = common_prefix.at(idx), bc = el.completion.at(idx);
                    bool matches = (ac == bc);
                    /* If we are replacing the token, allow case to vary */
                    if (will_replace_token && ! matches)
                    {
                        /* Hackish way to compare two strings in a case insensitive way, hopefully better than towlower(). */
                        matches = (wcsncasecmp(&ac, &bc, 1) == 0);
                    }
                    if (! matches)
                        break;
                }

                /* idx is now the length of the new common prefix */
                common_prefix.resize(idx);
                prefix_is_partial_completion = true;

                /* Early out if we decide there's no common prefix */
                if (idx == 0)
                    break;
            }
        }

        if (! common_prefix.empty())
        {
            /* We got something. If more than one completion contributed, then it means we have a prefix; don't insert a space after it */
            if (prefix_is_partial_completion)
                flags |= COMPLETE_NO_SPACE;
            completion_insert(common_prefix.c_str(), flags);
            success = true;
        }
        else
        {
            /* We didn't get a common prefix. Print the list. */
            size_t len, prefix_start = 0;
            wcstring prefix;
            parse_util_get_parameter_info(data->command_line, data->buff_pos, NULL, &prefix_start, NULL);

            assert(data->buff_pos >= prefix_start);
            len = data->buff_pos - prefix_start;

            if (match_type_requires_full_replacement(best_match_type))
            {
                // No prefix
                prefix.clear();
            }
            else if (len <= PREFIX_MAX_LEN)
            {
                prefix.append(data->command_line, prefix_start, len);
            }
            else
            {
                // append just the end of the string
                prefix = wcstring(&ellipsis_char, 1);
                prefix.append(data->command_line, prefix_start + len - PREFIX_MAX_LEN, PREFIX_MAX_LEN);
            }

            {
                int is_quoted;

                wchar_t quote;
                parse_util_get_parameter_info(data->command_line, data->buff_pos, &quote, NULL, NULL);
                is_quoted = (quote != L'\0');

                /* Clear the autosuggestion from the old commandline before abandoning it (see #561) */
                if (! data->autosuggestion.empty())
                    reader_repaint_without_autosuggestion();

                write_loop(1, "\n", 1);

                run_pager(prefix, is_quoted, surviving_completions);
            }
            s_reset(&data->screen, screen_reset_abandon_line);
            reader_repaint();
            success = false;
        }
    }
    return success;
}

/* Return true if we believe ourselves to be orphaned. loop_count is how many times we've tried to stop ourselves via SIGGTIN */
static bool check_for_orphaned_process(unsigned long loop_count, pid_t shell_pgid)
{
    bool we_think_we_are_orphaned = false;
    /* Try kill-0'ing the process whose pid corresponds to our process group ID. It's possible this will fail because we don't have permission to signal it. But more likely it will fail because it no longer exists, and we are orphaned. */
    if (loop_count % 64 == 0)
    {
        if (kill(shell_pgid, 0) < 0 && errno == ESRCH)
        {
            we_think_we_are_orphaned = true;
        }
    }

    if (! we_think_we_are_orphaned && loop_count % 128 == 0)
    {
        /* Try reading from the tty; if we get EIO we are orphaned. This is sort of bad because it may block. */

        char *tty = ctermid(NULL);
        if (! tty)
        {
            wperror(L"ctermid");
            exit_without_destructors(1);
        }

        /* Open the tty. Presumably this is stdin, but maybe not? */
        int tty_fd = open(tty, O_RDONLY | O_NONBLOCK);
        if (tty_fd < 0)
        {
            wperror(L"open");
            exit_without_destructors(1);
        }

        char tmp;
        if (read(tty_fd, &tmp, 1) < 0 && errno == EIO)
        {
            we_think_we_are_orphaned = true;
        }

        close(tty_fd);
    }

    /* Just give up if we've done it a lot times */
    if (loop_count > 4096)
    {
        we_think_we_are_orphaned = true;
    }

    return we_think_we_are_orphaned;
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
    shell_pgid = getpgrp();

    /*
      This should enable job control on fish, even if our parent process did
      not enable it for us.
    */

    /*
       Check if we are in control of the terminal, so that we don't do
       semi-expensive things like reset signal handlers unless we
       really have to, which we often don't.
     */
    if (tcgetpgrp(STDIN_FILENO) != shell_pgid)
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

        /* Ok, signal handlers are taken out of the picture. Stop ourself in a loop until we are in control of the terminal. However, the call to signal(SIGTTIN) may silently not do anything if we are orphaned.

            As far as I can tell there's no really good way to detect that we are orphaned. One way is to just detect if the group leader exited, via kill(shell_pgid, 0). Another possibility is that read() from the tty fails with EIO - this is more reliable but it's harder, because it may succeed or block. So we loop for a while, trying those strategies. Eventually we just give up and assume we're orphaend.
         */
        for (unsigned long loop_count = 0;; loop_count++)
        {
            pid_t owner = tcgetpgrp(STDIN_FILENO);
            shell_pgid = getpgrp();
            if (owner < 0 && errno == ENOTTY)
            {
                // No TTY, cannot be interactive?
                debug(1,
                      _(L"No TTY for interactive shell (tcgetpgrp failed)"));
                wperror(L"setpgid");
                exit_without_destructors(1);
            }
            if (owner == shell_pgid)
            {
                /* Success */
                break;
            }
            else
            {
                if (check_for_orphaned_process(loop_count, shell_pgid))
                {
                    /* We're orphaned, so we just die. Another sad statistic. */
                    debug(1,
                          _(L"I appear to be an orphaned process, so I am quitting politely. My pid is %d."), (int)getpid());
                    exit_without_destructors(1);
                }

                /* Try stopping us */
                int ret = killpg(shell_pgid, SIGTTIN);
                if (ret < 0)
                {
                    wperror(L"killpg");
                    exit_without_destructors(1);
                }
            }
        }

        signal_set_handlers();

        for (i=0; i<block_count; i++)
        {
            signal_block();
        }

    }


    /* Put ourselves in our own process group.  */
    shell_pgid = getpid();
    if (getpgrp() != shell_pgid)
    {
        if (setpgid(shell_pgid, shell_pgid) < 0)
        {
            debug(1,
                  _(L"Couldn't put the shell in its own process group"));
            wperror(L"setpgid");
            exit_without_destructors(1);
        }
    }

    /* Grab control of the terminal.  */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid))
    {
        debug(1,
              _(L"Couldn't grab control of terminal"));
        wperror(L"tcsetpgrp");
        exit_without_destructors(1);
    }

    common_handle_winch(0);


    if (tcsetattr(0,TCSANOW,&shell_modes))      /* set the new modes */
    {
        wperror(L"tcsetattr");
    }

    /*
       We need to know our own pid so we'll later know if we are a
       fork
    */
    original_pid = getpid();

    env_set(L"_", L"fish", ENV_GLOBAL);
}

/**
   Destroy data for interactive use
*/
static void reader_interactive_destroy()
{
    kill_destroy();
    writestr(L"\n");
    set_color(rgb_color_t::reset(), rgb_color_t::reset());
    input_destroy();
}


void reader_sanity_check()
{
    if (get_is_interactive())
    {
        if (!data)
            sanity_lose();

        if (!(data->buff_pos <= data->command_length()))
            sanity_lose();

        if (data->colors.size() != data->command_length())
            sanity_lose();

        if (data->indents.size() != data->command_length())
            sanity_lose();

    }
}

/**
   Set the specified string as the current buffer.
*/
static void set_command_line_and_position(const wcstring &new_str, size_t pos)
{
    data->command_line = new_str;
    data->command_line_changed();
    data->buff_pos = pos;
    reader_super_highlight_me_plenty(data->buff_pos);
    reader_repaint();
}

void reader_replace_current_token(const wchar_t *new_token)
{

    const wchar_t *begin, *end;
    size_t new_pos;

    /* Find current token */
    const wchar_t *buff = data->command_line.c_str();
    parse_util_token_extent((wchar_t *)buff, data->buff_pos, &begin, &end, 0, 0);

    if (!begin || !end)
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
    parse_util_token_extent((wchar_t *)buff, data->buff_pos, &begin, &end, 0, 0);

    data->search_buff.clear();
    if (begin)
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
static void handle_token_history(int forward, int reset)
{
    /* Paranoia */
    if (! data)
        return;

    const wchar_t *str=0;
    long current_pos;

    if (reset)
    {
        /*
          Start a new token search using the current token
        */
        reset_token_history();

    }


    current_pos  = data->token_history_pos;

    if (forward || data->search_pos + 1 < data->search_prev.size())
    {
        if (forward)
        {
            if (data->search_pos > 0)
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

        reader_replace_current_token(str);
        reader_super_highlight_me_plenty(data->buff_pos);
        reader_repaint();
    }
    else
    {
        if (current_pos == -1)
        {
            data->token_history_buff.clear();

            /*
              Search for previous item that contains this substring
            */
            if (data->history_search.go_backwards())
            {
                wcstring item = data->history_search.current_string();
                data->token_history_buff = data->history_search.current_string();
            }
            current_pos = data->token_history_buff.size();

        }

        if (data->token_history_buff.empty())
        {
            /*
              We have reached the end of the history - check if the
              history already contains the search string itself, if so
              return, otherwise add it.
            */

            const wcstring &last = data->search_prev.back();
            if (data->search_buff != last)
            {
                str = wcsdup(data->search_buff.c_str());
            }
            else
            {
                return;
            }
        }
        else
        {

            //debug( 3, L"new '%ls'", data->token_history_buff.c_str() );
            tokenizer_t tok(data->token_history_buff.c_str(), TOK_ACCEPT_UNFINISHED);
            for (; tok_has_next(&tok); tok_next(&tok))
            {
                switch (tok_last_type(&tok))
                {
                    case TOK_STRING:
                    {
                        if (wcsstr(tok_last(&tok), data->search_buff.c_str()))
                        {
                            //debug( 3, L"Found token at pos %d\n", tok_get_pos( &tok ) );
                            if (tok_get_pos(&tok) >= current_pos)
                            {
                                break;
                            }
                            //debug( 3, L"ok pos" );

                            const wcstring last_tok = tok_last(&tok);
                            if (find(data->search_prev.begin(), data->search_prev.end(), last_tok) == data->search_prev.end())
                            {
                                data->token_history_pos = tok_get_pos(&tok);
                                str = wcsdup(tok_last(&tok));
                            }

                        }
                    }
                }
            }
        }

        if (str)
        {
            reader_replace_current_token(str);
            reader_super_highlight_me_plenty(data->buff_pos);
            reader_repaint();
            data->search_pos = data->search_prev.size();
            data->search_prev.push_back(str);
        }
        else if (! reader_interrupted())
        {
            data->token_history_pos=-1;
            handle_token_history(0, 0);
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
enum move_word_dir_t
{
    MOVE_DIR_LEFT,
    MOVE_DIR_RIGHT
};

static void move_word(bool move_right, bool erase, enum move_word_style_t style, bool newv)
{
    /* Return if we are already at the edge */
    const size_t boundary = move_right ? data->command_length() : 0;
    if (data->buff_pos == boundary)
        return;

    /* When moving left, a value of 1 means the character at index 0. */
    move_word_state_machine_t state(style);
    const wchar_t * const command_line = data->command_line.c_str();
    const size_t start_buff_pos = data->buff_pos;

    size_t buff_pos = data->buff_pos;
    while (buff_pos != boundary)
    {
        size_t idx = (move_right ? buff_pos : buff_pos - 1);
        wchar_t c = command_line[idx];
        if (! state.consume_char(c))
            break;
        buff_pos = (move_right ? buff_pos + 1 : buff_pos - 1);
    }

    /* Always consume at least one character */
    if (buff_pos == start_buff_pos)
        buff_pos = (move_right ? buff_pos + 1 : buff_pos - 1);

    /* If we are moving left, buff_pos-1 is the index of the first character we do not delete (possibly -1). If we are moving right, then buff_pos is that index - possibly data->command_length(). */
    if (erase)
    {
        /* Don't autosuggest after a kill */
        data->suppress_autosuggestion = true;

        if (move_right)
        {
            reader_kill(start_buff_pos, buff_pos - start_buff_pos, KILL_APPEND, newv);
        }
        else
        {
            reader_kill(buff_pos, start_buff_pos - buff_pos, KILL_PREPEND, newv);
        }
    }
    else
    {
        data->buff_pos = buff_pos;
        reader_repaint();
    }

}

const wchar_t *reader_get_buffer(void)
{
    ASSERT_IS_MAIN_THREAD();
    return data?data->command_line.c_str():NULL;
}

history_t *reader_get_history(void)
{
    ASSERT_IS_MAIN_THREAD();
    return data ? data->history : NULL;
}

void reader_set_buffer(const wcstring &b, size_t pos)
{
    if (!data)
        return;

    /* Callers like to pass us pointers into ourselves, so be careful! I don't know if we can use operator= with a pointer to our interior, so use an intermediate. */
    size_t command_line_len = b.size();
    data->command_line = b;
    data->command_line_changed();

    /* Don't set a position past the command line length */
    if (pos > command_line_len)
        pos = command_line_len;

    data->buff_pos = pos;

    data->search_mode = NO_SEARCH;
    data->search_buff.clear();
    data->history_search.go_to_end();

    reader_super_highlight_me_plenty(data->buff_pos);
    reader_repaint_needed();
}


size_t reader_get_cursor_pos()
{
    if (!data)
        return (size_t)(-1);

    return data->buff_pos;
}

#define ENV_CMD_DURATION L"CMD_DURATION"

void set_env_cmd_duration(struct timeval *after, struct timeval *before)
{
    time_t secs = after->tv_sec - before->tv_sec;
    suseconds_t usecs = after->tv_usec - before->tv_usec;
    wchar_t buf[16];

    if (after->tv_usec < before->tv_usec)
    {
        usecs += 1000000;
        secs -= 1;
    }

    if (secs < 1)
    {
        env_remove(ENV_CMD_DURATION, 0);
    }
    else
    {
        if (secs < 10)   // 10 secs
        {
            swprintf(buf, 16, L"%lu.%02us", secs, usecs / 10000);
        }
        else if (secs < 60)     // 1 min
        {
            swprintf(buf, 16, L"%lu.%01us", secs, usecs / 100000);
        }
        else if (secs < 600)     // 10 mins
        {
            swprintf(buf, 16, L"%lum %lu.%01us", secs / 60, secs % 60, usecs / 100000);
        }
        else if (secs < 5400)     // 1.5 hours
        {
            swprintf(buf, 16, L"%lum %lus", secs / 60, secs % 60);
        }
        else
        {
            swprintf(buf, 16, L"%.1fh", secs / 3600.0);
        }
        env_set(ENV_CMD_DURATION, buf, ENV_EXPORT);
    }
}

void reader_run_command(parser_t &parser, const wcstring &cmd)
{

    struct timeval time_before, time_after;

    wcstring ft = tok_first(cmd.c_str());

    if (! ft.empty())
        env_set(L"_", ft.c_str(), ENV_GLOBAL);

    reader_write_title();

    term_donate();

    gettimeofday(&time_before, NULL);

    parser.eval(cmd, io_chain_t(), TOP);
    job_reap(1);

    gettimeofday(&time_after, NULL);
    set_env_cmd_duration(&time_after, &time_before);

    term_steal();

    env_set(L"_", program_name, ENV_GLOBAL);

#ifdef HAVE__PROC_SELF_STAT
    proc_update_jiffies();
#endif


}


int reader_shell_test(const wchar_t *b)
{
    int res = parser_t::principal_parser().test(b);

    if (res & PARSER_TEST_ERROR)
    {
        wcstring sb;

        const int tmp[1] = {0};
        const int tmp2[1] = {0};
        const wcstring empty;

        s_write(&data->screen,
                empty,
                empty,
                empty,
                0,
                tmp,
                tmp2,
                0);


        parser_t::principal_parser().test(b, NULL, &sb, L"fish");
        fwprintf(stderr, L"%ls", sb.c_str());
    }
    return res;
}

/**
   Test if the given string contains error. Since this is the error
   detection for general purpose, there are no invalid strings, so
   this function always returns false.
*/
static int default_test(const wchar_t *b)
{
    return 0;
}

void reader_push(const wchar_t *name)
{
    reader_data_t *n = new reader_data_t();

    n->history = & history_t::history_with_name(name);
    n->app_name = name;
    n->next = data;

    data=n;

    data->command_line_changed();

    if (data->next == 0)
    {
        reader_interactive_init();
    }

    exec_prompt();
    reader_set_highlight_function(&highlight_universal);
    reader_set_test_function(&default_test);
    reader_set_left_prompt(L"");
}

void reader_pop()
{
    reader_data_t *n = data;

    if (data == 0)
    {
        debug(0, _(L"Pop null reader block"));
        sanity_lose();
        return;
    }

    data=data->next;

    /* Invoke the destructor to balance our new */
    delete n;

    if (data == 0)
    {
        reader_interactive_destroy();
    }
    else
    {
        end_loop = 0;
        //history_set_mode( data->app_name.c_str() );
        s_reset(&data->screen, screen_reset_abandon_line);
    }
}

void reader_set_left_prompt(const wcstring &new_prompt)
{
    data->left_prompt = new_prompt;
}

void reader_set_right_prompt(const wcstring &new_prompt)
{
    data->right_prompt = new_prompt;
}

void reader_set_allow_autosuggesting(bool flag)
{
    data->allow_autosuggestion = flag;
}

void reader_set_expand_abbreviations(bool flag)
{
    data->expand_abbreviations = flag;
}

void reader_set_complete_function(complete_function_t f)
{
    data->complete_func = f;
}

void reader_set_highlight_function(highlight_function_t func)
{
    data->highlight_function = func;
}

void reader_set_test_function(int (*f)(const wchar_t *))
{
    data->test_func = f;
}

void reader_set_exit_on_interrupt(bool i)
{
    data->exit_on_interrupt = i;
}

void reader_import_history_if_necessary(void)
{
    /* Import history from bash, etc. if our current history is empty */
    if (data->history && data->history->is_empty())
    {
        /* Try opening a bash file. We make an effort to respect $HISTFILE; this isn't very complete (AFAIK it doesn't have to be exported), and to really get this right we ought to ask bash itself. But this is better than nothing.
        */
        const env_var_t var = env_get_string(L"HISTFILE");
        wcstring path = (var.missing() ? L"~/.bash_history" : var);
        expand_tilde(path);
        FILE *f = wfopen(path, "r");
        if (f)
        {
            data->history->populate_from_bash(f);
            fclose(f);
        }
    }
}

/** A class as the context pointer for a background (threaded) highlight operation. */
class background_highlight_context_t
{
public:
    /** The string to highlight */
    const wcstring string_to_highlight;

    /** Color buffer */
    std::vector<color_t> colors;

    /** The position to use for bracket matching */
    const size_t match_highlight_pos;

    /** Function for syntax highlighting */
    const highlight_function_t highlight_function;

    /** Environment variables */
    const env_vars_snapshot_t vars;

    /** When the request was made */
    const double when;

    /** Gen count at the time the request was made */
    const unsigned int generation_count;

    background_highlight_context_t(const wcstring &pbuff, size_t phighlight_pos, highlight_function_t phighlight_func) :
        string_to_highlight(pbuff),
        colors(pbuff.size(), 0),
        match_highlight_pos(phighlight_pos),
        highlight_function(phighlight_func),
        vars(env_vars_snapshot_t::highlighting_keys),
        when(timef()),
        generation_count(s_generation_count)
    {
    }

    int threaded_highlight()
    {
        if (generation_count != s_generation_count)
        {
            // The gen count has changed, so don't do anything
            return 0;
        }
        if (! string_to_highlight.empty())
        {
            highlight_function(string_to_highlight, colors, match_highlight_pos, NULL /* error */, vars);
        }
        return 0;
    }
};

/* Called to set the highlight flag for search results */
static void highlight_search(void)
{
    if (! data->search_buff.empty() && ! data->history_search.is_at_end())
    {
        const wcstring &needle = data->search_buff;
        size_t match_pos = data->command_line.find(needle);
        if (match_pos != wcstring::npos)
        {
            size_t end = match_pos + needle.size();
            for (size_t i=match_pos; i < end; i++)
            {
                data->colors.at(i) |= (HIGHLIGHT_SEARCH_MATCH<<16);
            }
        }
    }
}

static void highlight_complete(background_highlight_context_t *ctx, int result)
{
    ASSERT_IS_MAIN_THREAD();
    if (ctx->string_to_highlight == data->command_line)
    {
        /* The data hasn't changed, so swap in our colors. The colors may not have changed, so do nothing if they have not. */
        assert(ctx->colors.size() == data->command_length());
        if (data->colors != ctx->colors)
        {
            data->colors.swap(ctx->colors);
            sanity_check();
            highlight_search();
            reader_repaint();
        }
    }

    /* Free our context */
    delete ctx;
}

static int threaded_highlight(background_highlight_context_t *ctx)
{
    return ctx->threaded_highlight();
}


/**
   Call specified external highlighting function and then do search
   highlighting. Lastly, clear the background color under the cursor
   to avoid repaint issues on terminals where e.g. syntax highlighting
   maykes characters under the sursor unreadable.

   \param match_highlight_pos the position to use for bracket matching. This need not be the same as the surrent cursor position
   \param error if non-null, any possible errors in the buffer are further descibed by the strings inserted into the specified arraylist
*/
static void reader_super_highlight_me_plenty(size_t match_highlight_pos)
{
    reader_sanity_check();

    background_highlight_context_t *ctx = new background_highlight_context_t(data->command_line, match_highlight_pos, data->highlight_function);
    iothread_perform(threaded_highlight, highlight_complete, ctx);
    highlight_search();

    /* Here's a hack. Check to see if our autosuggestion still applies; if so, don't recompute it. Since the autosuggestion computation is asynchronous, this avoids "flashing" as you type into the autosuggestion. */
    const wcstring &cmd = data->command_line, &suggest = data->autosuggestion;
    if (can_autosuggest() && ! suggest.empty() && string_prefixes_string_case_insensitive(cmd, suggest))
    {
        /* The autosuggestion is still reasonable, so do nothing */
    }
    else
    {
        update_autosuggestion();
    }
}


int exit_status()
{
    if (get_is_interactive())
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

    for (b = parser.current_block;
            b;
            b = b->outer)
    {
        if (b->type() == BREAKPOINT)
        {
            is_breakpoint = 1;
            break;
        }
    }

    job_iterator_t jobs;
    while ((j = jobs.next()))
    {
        if (!job_is_completed(j))
        {
            job_count++;
            break;
        }
    }

    if (!reader_exit_forced() && !data->prev_end_loop && job_count && !is_breakpoint)
    {
        writestr(_(L"There are stopped jobs. A second attempt to exit will enforce their termination.\n"));

        reader_exit(0, 0);
        data->prev_end_loop=1;
    }
    else
    {
        /* PCA: we used to only hangup jobs if stdin was closed. This prevented child processes from exiting. It's unclear to my why it matters if stdin is closed, but it seems to me if we're forcing an exit, we definitely want to hang up our processes.

            See https://github.com/fish-shell/fish-shell/issues/138
        */
        if (reader_exit_forced() || !isatty(0))
        {
            /*
              We already know that stdin is a tty since we're
              in interactive mode. If isatty returns false, it
              means stdin must have been closed.
            */
            job_iterator_t jobs;
            while ((j = jobs.next()))
            {
                if (! job_is_completed(j))
                {
                    job_signal(j, SIGHUP);
                }
            }
        }
    }
}

/**
   Read interactively. Read input from stdin while providing editing
   facilities.
*/
static int read_i(void)
{
    reader_push(L"fish");
    reader_set_complete_function(&complete);
    reader_set_highlight_function(&highlight_shell);
    reader_set_test_function(&reader_shell_test);
    reader_set_allow_autosuggesting(true);
    reader_set_expand_abbreviations(true);
    reader_import_history_if_necessary();

    parser_t &parser = parser_t::principal_parser();

    data->prev_end_loop=0;

    while ((!data->end_loop) && (!sanity_check()))
    {
        event_fire_generic(L"fish_prompt");
        if (function_exists(LEFT_PROMPT_FUNCTION_NAME))
            reader_set_left_prompt(LEFT_PROMPT_FUNCTION_NAME);
        else
            reader_set_left_prompt(DEFAULT_PROMPT);

        if (function_exists(RIGHT_PROMPT_FUNCTION_NAME))
            reader_set_right_prompt(RIGHT_PROMPT_FUNCTION_NAME);
        else
            reader_set_right_prompt(L"");


        /*
          Put buff in temporary string and clear buff, so
          that we can handle a call to reader_set_buffer
          during evaluation.
        */

        const wchar_t *tmp = reader_readline();

        if (data->end_loop)
        {
            handle_end_loop();
        }
        else if (tmp)
        {
            wcstring command = tmp;
            data->buff_pos=0;
            data->command_line.clear();
            data->command_line_changed();
            reader_run_command(parser, command);
            if (data->end_loop)
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
static int can_read(int fd)
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
static int wchar_private(wchar_t c)
{
    return ((c >= 0xe000) && (c <= 0xf8ff));
}

/**
   Test if the specified character in the specified string is
   backslashed. pos may be at the end of the string, which indicates
   if there is a trailing backslash.
*/
static bool is_backslashed(const wcstring &str, size_t pos)
{
    /* note pos == str.size() is OK */
    if (pos > str.size())
        return false;

    size_t count = 0, idx = pos;
    while (idx--)
    {
        if (str.at(idx) != L'\\')
            break;
        count++;
    }

    return (count % 2) == 1;
}

static wchar_t unescaped_quote(const wcstring &str, size_t pos)
{
    wchar_t result = L'\0';
    if (pos < str.size())
    {
        wchar_t c = str.at(pos);
        if ((c == L'\'' || c == L'"') && ! is_backslashed(str, pos))
        {
            result = c;
        }
    }
    return result;
}


const wchar_t *reader_readline(void)
{
    wint_t c;
    int last_char=0;
    size_t yank_len=0;
    const wchar_t *yank_str;
    bool comp_empty = true;
    std::vector<completion_t> comp;
    int finished=0;
    struct termios old_modes;

    /* Coalesce redundant repaints. When we get a repaint, we set this to true, and skip repaints until we get something else. */
    bool coalescing_repaints = false;

    /* The cycle index in our completion list */
    size_t completion_cycle_idx = (size_t)(-1);

    /* The command line before completion */
    wcstring cycle_command_line;
    size_t cycle_cursor_pos = 0;

    data->search_buff.clear();
    data->search_mode = NO_SEARCH;

    exec_prompt();

    reader_super_highlight_me_plenty(data->buff_pos);
    s_reset(&data->screen, screen_reset_abandon_line);
    reader_repaint();

    /*
     get the current terminal modes. These will be restored when the
     function returns.
     */
    tcgetattr(0,&old_modes);
    /* set the new modes */
    if (tcsetattr(0,TCSANOW,&shell_modes))
    {
        wperror(L"tcsetattr");
    }

    while (!finished && !data->end_loop)
    {
        /*
         Sometimes strange input sequences seem to generate a zero
         byte. I believe these simply mean a character was pressed
         but it should be ignored. (Example: Trying to add a tilde
         (~) to digit)
         */
        while (1)
        {
            int was_interactive_read = is_interactive_read;
            is_interactive_read = 1;
            c=input_readch();
            is_interactive_read = was_interactive_read;

            if (((!wchar_private(c))) && (c>31) && (c != 127))
            {
                if (can_read(0))
                {

                    wchar_t arr[READAHEAD_MAX+1];
                    int i;

                    memset(arr, 0, sizeof(arr));
                    arr[0] = c;

                    for (i=1; i<READAHEAD_MAX; i++)
                    {

                        if (!can_read(0))
                        {
                            c = 0;
                            break;
                        }
                        c = input_readch();
                        if ((!wchar_private(c)) && (c>31) && (c != 127))
                        {
                            arr[i]=c;
                            c=0;
                        }
                        else
                            break;
                    }

                    insert_string(arr);

                }
            }

            if (c != 0)
                break;
        }

        /* If we get something other than a repaint, then stop coalescing them */
        if (c != R_REPAINT)
            coalescing_repaints = false;

        if (last_char != R_YANK && last_char != R_YANK_POP)
            yank_len=0;

        const wchar_t *buff = data->command_line.c_str();
        switch (c)
        {

                /* go to beginning of line*/
            case R_BEGINNING_OF_LINE:
            {
                while ((data->buff_pos>0) &&
                        (buff[data->buff_pos-1] != L'\n'))
                {
                    data->buff_pos--;
                }

                reader_repaint();
                break;
            }

            case R_END_OF_LINE:
            {
                if (data->buff_pos < data->command_length())
                {
                    while (buff[data->buff_pos] &&
                            buff[data->buff_pos] != L'\n')
                    {
                        data->buff_pos++;
                    }
                }
                else
                {
                    accept_autosuggestion(true);
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
                if (! coalescing_repaints)
                {
                    coalescing_repaints = true;
                    exec_prompt();
                    s_reset(&data->screen, screen_reset_current_line_and_prompt);
                    data->screen_reset_needed = false;
                    reader_repaint();
                }
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

                if (!data->complete_func)
                    break;

                if (! comp_empty && last_char == R_COMPLETE)
                {
                    /* The user typed R_COMPLETE more than once in a row. Cycle through our available completions */
                    const completion_t *next_comp = cycle_competions(comp, cycle_command_line, &completion_cycle_idx);
                    if (next_comp != NULL)
                    {
                        size_t cursor_pos = cycle_cursor_pos;
                        const wcstring new_cmd_line = completion_apply_to_command_line(next_comp->completion, next_comp->flags, cycle_command_line, &cursor_pos, false);
                        reader_set_buffer(new_cmd_line, cursor_pos);

                        /* Since we just inserted a completion, don't immediately do a new autosuggestion */
                        data->suppress_autosuggestion = true;

                        /* Trigger repaint (see #765) */
                        reader_repaint_if_needed();
                    }
                }
                else
                {
                    /* Either the user hit tab only once, or we had no visible completion list. */

                    /* Remove a trailing backslash. This may trigger an extra repaint, but this is rare. */
                    if (is_backslashed(data->command_line, data->buff_pos))
                    {
                        remove_backward();
                    }

                    /* Get the string; we have to do this after removing any trailing backslash */
                    const wchar_t * const buff = data->command_line.c_str();

                    /* Clear the completion list */
                    comp.clear();

                    /* Figure out the extent of the command substitution surrounding the cursor. This is because we only look at the current command substitution to form completions - stuff happening outside of it is not interesting. */
                    const wchar_t *cmdsub_begin, *cmdsub_end;
                    parse_util_cmdsubst_extent(buff, data->buff_pos, &cmdsub_begin, &cmdsub_end);

                    /* Figure out the extent of the token within the command substitution. Note we pass cmdsub_begin here, not buff */
                    const wchar_t *token_begin, *token_end;
                    parse_util_token_extent(cmdsub_begin, data->buff_pos - (cmdsub_begin-buff), &token_begin, &token_end, 0, 0);

                    /* Figure out how many steps to get from the current position to the end of the current token. */
                    size_t end_of_token_offset = token_end - buff;

                    /* Move the cursor to the end */
                    if (data->buff_pos != end_of_token_offset)
                    {
                        data->buff_pos = end_of_token_offset;
                        reader_repaint();
                    }

                    /* Construct a copy of the string from the beginning of the command substitution up to the end of the token we're completing */
                    const wcstring buffcpy = wcstring(cmdsub_begin, token_end);

                    //fprintf(stderr, "Complete (%ls)\n", buffcpy.c_str());
                    data->complete_func(buffcpy, comp, COMPLETION_REQUEST_DEFAULT | COMPLETION_REQUEST_DESCRIPTIONS | COMPLETION_REQUEST_FUZZY_MATCH, NULL);

                    /* Munge our completions */
                    sort_and_make_unique(comp);
                    prioritize_completions(comp);

                    /* Record our cycle_command_line */
                    cycle_command_line = data->command_line;
                    cycle_cursor_pos = data->buff_pos;

                    comp_empty = handle_completions(comp);

                    /* Start the cycle at the beginning */
                    completion_cycle_idx = (size_t)(-1);

                    /* Repaint */
                    reader_repaint_if_needed();
                }

                break;
            }

            /* kill */
            case R_KILL_LINE:
            {
                const wchar_t *buff = data->command_line.c_str();
                const wchar_t *begin = &buff[data->buff_pos];
                const wchar_t *end = begin;

                while (*end && *end != L'\n')
                    end++;

                if (end==begin && *end)
                    end++;

                size_t len = end-begin;
                if (len)
                {
                    reader_kill(begin - buff, len, KILL_APPEND, last_char!=R_KILL_LINE);
                }

                break;
            }

            case R_BACKWARD_KILL_LINE:
            {
                if (data->buff_pos > 0)
                {
                    const wchar_t *buff = data->command_line.c_str();
                    const wchar_t *end = &buff[data->buff_pos];
                    const wchar_t *begin = end;

                    /* Make sure we delete at least one character (see #580) */
                    begin--;

                    /* Delete until we hit a newline, or the beginning of the string */
                    while (begin > buff  && *begin != L'\n')
                        begin--;

                    /* If we landed on a newline, don't delete it */
                    if (*begin == L'\n')
                        begin++;

                    assert(end >= begin);
                    size_t len = maxi<size_t>(end-begin, 1);
                    begin = end - len;

                    reader_kill(begin - buff, len, KILL_PREPEND, last_char!=R_BACKWARD_KILL_LINE);
                }
                break;

            }

            case R_KILL_WHOLE_LINE:
            {
                const wchar_t *buff = data->command_line.c_str();
                const wchar_t *end = &buff[data->buff_pos];
                const wchar_t *begin = end;
                size_t len;

                while (begin > buff  && *begin != L'\n')
                    begin--;

                if (*begin == L'\n')
                    begin++;

                len = maxi<size_t>(end-begin, 0);
                begin = end - len;

                while (*end && *end != L'\n')
                    end++;

                if (begin == end && *end)
                    end++;

                len = end-begin;

                if (len)
                {
                    reader_kill(begin - buff, len, KILL_APPEND, last_char!=R_KILL_WHOLE_LINE);
                }

                break;
            }

            /* yank*/
            case R_YANK:
            {
                yank_str = kill_yank();
                insert_string(yank_str);
                yank_len = wcslen(yank_str);
                break;
            }

            /* rotate killring*/
            case R_YANK_POP:
            {
                if (yank_len)
                {
                    for (size_t i=0; i<yank_len; i++)
                        remove_backward();

                    yank_str = kill_yank_rotate();
                    insert_string(yank_str);
                    yank_len = wcslen(yank_str);
                }
                break;
            }

            /* Escape was pressed */
            case L'\x1b':
            {
                if (data->search_mode)
                {
                    data->search_mode= NO_SEARCH;

                    if (data->token_history_pos==-1)
                    {
                        //history_reset();
                        data->history_search.go_to_end();
                        reader_set_buffer(data->search_buff.c_str(), data->search_buff.size());
                    }
                    else
                    {
                        reader_replace_current_token(data->search_buff.c_str());
                    }
                    data->search_buff.clear();
                    reader_super_highlight_me_plenty(data->buff_pos);
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
                if (data->buff_pos < data->command_length())
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

                /* Allow backslash-escaped newlines, but only if the following character is whitespace, or we're at the end of the text (see issue #163) */
                if (is_backslashed(data->command_line, data->buff_pos))
                {
                    if (data->buff_pos >= data->command_length() || iswspace(data->command_line.at(data->buff_pos)))
                    {
                        insert_char('\n');
                        break;
                    }
                }

                /* See if this command is valid */
                int command_test_result = data->test_func(data->command_line.c_str());
                if (command_test_result == 0 || command_test_result == PARSER_TEST_INCOMPLETE)
                {
                    /* This command is valid, but an abbreviation may make it invalid. If so, we will have to test again. */
                    bool abbreviation_expanded = data->expand_abbreviation_as_necessary(1);
                    if (abbreviation_expanded)
                    {
                        /* It's our reponsibility to rehighlight and repaint. But everything we do below triggers a repaint. */
                        reader_super_highlight_me_plenty(data->buff_pos);
                        command_test_result = data->test_func(data->command_line.c_str());
                    }
                }

                switch (command_test_result)
                {

                    case 0:
                    {
                        /* Finished command, execute it. Don't add items that start with a leading space. */
                        if (! data->command_line.empty() && data->command_line.at(0) != L' ')
                        {
                            if (data->history != NULL)
                            {
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
                        insert_char('\n');
                        break;
                    }

                    /*
                     Result must be some combination including an
                     error. The error message will already be
                     printed, all we need to do is repaint
                     */
                    default:
                    {
                        s_reset(&data->screen, screen_reset_abandon_line);
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

                if (data->search_mode == NO_SEARCH)
                {
                    reset = 1;
                    if ((c == R_HISTORY_SEARCH_BACKWARD) ||
                            (c == R_HISTORY_SEARCH_FORWARD))
                    {
                        data->search_mode = LINE_SEARCH;
                    }
                    else
                    {
                        data->search_mode = TOKEN_SEARCH;
                    }

                    data->search_buff.append(data->command_line);
                    data->history_search = history_search_t(*data->history, data->search_buff, HISTORY_SEARCH_TYPE_CONTAINS);

                    /* Skip the autosuggestion as history unless it was truncated */
                    const wcstring &suggest = data->autosuggestion;
                    if (! suggest.empty() && ! data->screen.autosuggestion_is_truncated)
                    {
                        data->history_search.skip_matches(wcstring_list_t(&suggest, 1 + &suggest));
                    }
                }

                switch (data->search_mode)
                {

                    case LINE_SEARCH:
                    {
                        if ((c == R_HISTORY_SEARCH_BACKWARD) ||
                                (c == R_HISTORY_TOKEN_SEARCH_BACKWARD))
                        {
                            data->history_search.go_backwards();
                        }
                        else
                        {
                            if (! data->history_search.go_forwards())
                            {
                                /* If you try to go forwards past the end, we just go to the end */
                                data->history_search.go_to_end();
                            }
                        }

                        wcstring new_text;
                        if (data->history_search.is_at_end())
                        {
                            new_text = data->search_buff;
                        }
                        else
                        {
                            new_text = data->history_search.current_string();
                        }
                        set_command_line_and_position(new_text, new_text.size());

                        break;
                    }

                    case TOKEN_SEARCH:
                    {
                        if ((c == R_HISTORY_SEARCH_BACKWARD) ||
                                (c == R_HISTORY_TOKEN_SEARCH_BACKWARD))
                        {
                            handle_token_history(SEARCH_BACKWARD, reset);
                        }
                        else
                        {
                            handle_token_history(SEARCH_FORWARD, reset);
                        }

                        break;
                    }

                }
                break;
            }


            /* Move left*/
            case R_BACKWARD_CHAR:
            {
                if (data->buff_pos > 0)
                {
                    data->buff_pos--;
                    reader_repaint();
                }
                break;
            }

            /* Move right*/
            case R_FORWARD_CHAR:
            {
                if (data->buff_pos < data->command_length())
                {
                    data->buff_pos++;
                    reader_repaint();
                }
                else
                {
                    accept_autosuggestion(true);
                }
                break;
            }

            /* kill one word left */
            case R_BACKWARD_KILL_WORD:
            case R_BACKWARD_KILL_PATH_COMPONENT:
            {
                move_word_style_t style = (c == R_BACKWARD_KILL_PATH_COMPONENT ? move_word_style_path_components : move_word_style_punctuation);
                bool newv = (last_char != R_BACKWARD_KILL_WORD && last_char != R_BACKWARD_KILL_PATH_COMPONENT);
                move_word(MOVE_DIR_LEFT, true /* erase */, style, newv);
                break;
            }

            /* kill one word right */
            case R_KILL_WORD:
            {
                move_word(MOVE_DIR_RIGHT, true /* erase */, move_word_style_punctuation, last_char!=R_KILL_WORD);
                break;
            }

            /* move one word left*/
            case R_BACKWARD_WORD:
            {
                move_word(MOVE_DIR_LEFT, false /* do not erase */, move_word_style_punctuation, false);
                break;
            }

            /* move one word right*/
            case R_FORWARD_WORD:
            {
                if (data->buff_pos < data->command_length())
                {
                    move_word(MOVE_DIR_RIGHT, false /* do not erase */, move_word_style_punctuation, false);
                }
                else
                {
                    accept_autosuggestion(false /* accept only one word */);
                }
                break;
            }

            case R_BEGINNING_OF_HISTORY:
            {
                data->history_search = history_search_t(*data->history, data->command_line, HISTORY_SEARCH_TYPE_PREFIX);
                data->history_search.go_to_beginning();
                if (! data->history_search.is_at_end())
                {
                    wcstring new_text = data->history_search.current_string();
                    set_command_line_and_position(new_text, new_text.size());
                }

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
                int line_old = parse_util_get_line_from_offset(data->command_line, data->buff_pos);
                int line_new;

                if (c == R_UP_LINE)
                    line_new = line_old-1;
                else
                    line_new = line_old+1;

                int line_count = parse_util_lineno(data->command_line.c_str(), data->command_length())-1;

                if (line_new >= 0 && line_new <= line_count)
                {
                    size_t base_pos_new;
                    size_t base_pos_old;

                    int indent_old;
                    int indent_new;
                    size_t line_offset_old;
                    size_t total_offset_new;

                    base_pos_new = parse_util_get_offset_from_line(data->command_line, line_new);

                    base_pos_old = parse_util_get_offset_from_line(data->command_line,  line_old);

                    assert(base_pos_new != (size_t)(-1) && base_pos_old != (size_t)(-1));
                    indent_old = data->indents.at(base_pos_old);
                    indent_new = data->indents.at(base_pos_new);

                    line_offset_old = data->buff_pos - parse_util_get_offset_from_line(data->command_line, line_old);
                    total_offset_new = parse_util_get_offset(data->command_line, line_new, line_offset_old - 4*(indent_new-indent_old));
                    data->buff_pos = total_offset_new;
                    reader_repaint();
                }

                break;
            }

            case R_SUPPRESS_AUTOSUGGESTION:
            {
                data->suppress_autosuggestion = true;
                data->autosuggestion.clear();
                reader_repaint();
                break;
            }

            case R_ACCEPT_AUTOSUGGESTION:
            {
                accept_autosuggestion(true);
                break;
            }

            case R_TRANSPOSE_CHARS:
            {
                if (data->command_length() < 2)
                {
                    break;
                }

                /* If the cursor is at the end, transpose the last two characters of the line */
                if (data->buff_pos == data->command_length())
                {
                    data->buff_pos--;
                }

                /*
                 Drag the character before the cursor forward over the character at the cursor, moving
                 the cursor forward as well.
                 */
                if (data->buff_pos > 0)
                {
                    wcstring local_cmd = data->command_line;
                    std::swap(local_cmd.at(data->buff_pos), local_cmd.at(data->buff_pos-1));
                    set_command_line_and_position(local_cmd, data->buff_pos + 1);
                }
                break;
            }

            case R_TRANSPOSE_WORDS:
            {
                size_t len = data->command_length();
                const wchar_t *buff = data->command_line.c_str();
                const wchar_t *tok_begin, *tok_end, *prev_begin, *prev_end;

                /* If we are not in a token, look for one ahead */
                while (data->buff_pos != len && !iswalnum(buff[data->buff_pos]))
                    data->buff_pos++;

                parse_util_token_extent(buff, data->buff_pos, &tok_begin, &tok_end, &prev_begin, &prev_end);

                /* In case we didn't find a token at or after the cursor... */
                if (tok_begin == &buff[len])
                {
                    /* ...retry beginning from the previous token */
                    size_t pos = prev_end - &buff[0];
                    parse_util_token_extent(buff, pos, &tok_begin, &tok_end, &prev_begin, &prev_end);
                }

                /* Make sure we have two tokens */
                if (prev_begin < prev_end && tok_begin < tok_end && tok_begin > prev_begin)
                {
                    const wcstring prev(prev_begin, prev_end - prev_begin);
                    const wcstring sep(prev_end, tok_begin - prev_end);
                    const wcstring tok(tok_begin, tok_end - tok_begin);
                    const wcstring trail(tok_end, &buff[len] - tok_end);

                    /* Compose new command line with swapped tokens */
                    wcstring new_buff(buff, prev_begin - buff);
                    new_buff.append(tok);
                    new_buff.append(sep);
                    new_buff.append(prev);
                    new_buff.append(trail);
                    /* Put cursor right after the second token */
                    set_command_line_and_position(new_buff, tok_end - buff);
                }
                break;
            }

            /* Other, if a normal character, we add it to the command */
            default:
            {
                if ((!wchar_private(c)) && (((c>31) || (c==L'\n'))&& (c != 127)))
                {
                    /* Expand abbreviations after space */
                    bool should_expand_abbreviations = (c == L' ');

                    /* Regular character */
                    insert_char(c, should_expand_abbreviations);
                }
                else
                {
                    /*
                     Low priority debug message. These can happen if
                     the user presses an unefined control
                     sequnece. No reason to report.
                     */
                    debug(2, _(L"Unknown keybinding %d"), c);
                }
                break;
            }

        }

        if ((c != R_HISTORY_SEARCH_BACKWARD) &&
                (c != R_HISTORY_SEARCH_FORWARD) &&
                (c != R_HISTORY_TOKEN_SEARCH_BACKWARD) &&
                (c != R_HISTORY_TOKEN_SEARCH_FORWARD) &&
                (c != R_NULL))
        {
            data->search_mode = NO_SEARCH;
            data->search_buff.clear();
            data->history_search.go_to_end();
            data->token_history_pos=-1;
        }

        last_char = c;
    }

    writestr(L"\n");

    if (!reader_exit_forced())
    {
        if (tcsetattr(0,TCSANOW,&old_modes))      /* return to previous mode */
        {
            wperror(L"tcsetattr");
        }

        set_color(rgb_color_t::reset(), rgb_color_t::reset());
    }

    return finished ? data->command_line.c_str() : 0;
}

int reader_search_mode()
{
    if (!data)
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
static int read_ni(int fd, const io_chain_t &io)
{
    parser_t &parser = parser_t::principal_parser();
    FILE *in_stream;
    wchar_t *buff=0;
    std::vector<char> acc;

    int des = (fd == STDIN_FILENO ? dup(STDIN_FILENO) : fd);
    int res=0;

    if (des == -1)
    {
        wperror(L"dup");
        return 1;
    }

    in_stream = fdopen(des, "r");
    if (in_stream != 0)
    {
        while (!feof(in_stream))
        {
            char buff[4096];
            size_t c = fread(buff, 1, 4096, in_stream);

            if (ferror(in_stream))
            {
                if (errno == EINTR)
                {
                    /* We got a signal, just keep going. Be sure that we call insert() below because we may get data as well as EINTR. */
                    clearerr(in_stream);
                }
                else if ((errno == EAGAIN || errno == EWOULDBLOCK) && make_fd_blocking(des) == 0)
                {
                    /* We succeeded in making the fd blocking, keep going */
                    clearerr(in_stream);
                }
                else
                {
                    /* Fatal error */
                    debug(1, _(L"Error while reading from file descriptor"));

                    /* Reset buffer on error. We won't evaluate incomplete files. */
                    acc.clear();
                    break;
                }
            }

            acc.insert(acc.end(), buff, buff + c);
        }

        const wcstring str = acc.empty() ? wcstring() : str2wcstring(&acc.at(0), acc.size());
        acc.clear();

        if (fclose(in_stream))
        {
            debug(1,
                  _(L"Error while closing input stream"));
            wperror(L"fclose");
            res = 1;
        }

        wcstring sb;
        if (! parser.test(str.c_str(), 0, &sb, L"fish"))
        {
            parser.eval(str, io, TOP);
        }
        else
        {
            fwprintf(stderr, L"%ls", sb.c_str());
            res = 1;
        }
    }
    else
    {
        debug(1,
              _(L"Error while opening input stream"));
        wperror(L"fdopen");
        free(buff);
        res=1;
    }
    return res;
}

int reader_read(int fd, const io_chain_t &io)
{
    int res;

    /*
      If reader_read is called recursively through the '.' builtin, we
      need to preserve is_interactive. This, and signal handler setup
      is handled by proc_push_interactive/proc_pop_interactive.
    */

    int inter = ((fd == STDIN_FILENO) && isatty(STDIN_FILENO));
    proc_push_interactive(inter);

    res= get_is_interactive() ? read_i():read_ni(fd, io);

    /*
      If the exit command was called in a script, only exit the
      script, not the program.
    */
    if (data)
        data->end_loop = 0;
    end_loop = 0;

    proc_pop_interactive();
    return res;
}
