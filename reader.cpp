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
#include "parse_tree.h"
#include "pager.h"

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

static void set_command_line_and_position(editable_line_t *el, const wcstring &new_str, size_t pos);

void editable_line_t::insert_string(const wcstring &str)
{
    this->text.insert(this->position, str);
    this->position += str.size();
}

/**
   A struct describing the state of the interactive reader. These
   states can be stacked, in case reader_readline() calls are
   nested. This happens when the 'read' builtin is used.
*/
class reader_data_t
{
public:

    /** String containing the whole current commandline */
    editable_line_t command_line;

    /** String containing the autosuggestion */
    wcstring autosuggestion;

    /** Current pager */
    pager_t pager;

    /** Current page rendering */
    page_rendering_t current_page_rendering;

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

    bool is_navigating_pager_contents() const
    {
        return this->pager.is_navigating_contents();
    }

    /* The line that is currently being edited. Typically the command line, but may be the search field */
    editable_line_t *active_edit_line()
    {
        if (this->is_navigating_pager_contents() && this->pager.is_search_field_shown())
        {
            return &this->pager.search_field_line;
        }
        else
        {
            return &this->command_line;
        }
    }

    /** Do what we need to do whenever our command line changes */
    void command_line_changed(const editable_line_t *el);

    /** Do what we need to do whenever our pager selection */
    void pager_selection_changed();

    /** Expand abbreviations at the current cursor position, minus backtrack_amt. */
    bool expand_abbreviation_as_necessary(size_t cursor_backtrack);

    /** Indicates whether a selection is currently active */
    bool sel_active;

    /** The position of the cursor, when selection was initiated. */
    size_t sel_begin_pos;

    /** The start position of the current selection, if one. */
    size_t sel_start_pos;

    /** The stop position of the current selection, if one. */
    size_t sel_stop_pos;

    /** Name of the current application */
    wcstring app_name;

    /** The prompt commands */
    wcstring left_prompt;
    wcstring right_prompt;

    /** The output of the last evaluation of the prompt command */
    wcstring left_prompt_buff;

    /** The output of the last evaluation of the right prompt command */
    wcstring right_prompt_buff;

    /* Completion support */
    wcstring cycle_command_line;
    size_t cycle_cursor_pos;

    /**
       Color is the syntax highlighting for buff.  The format is that
       color[i] is the classification (according to the enum in
       highlight.h) of buff[i].
    */
    std::vector<highlight_spec_t> colors;

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
        sel_active(0),
        sel_begin_pos(0),
        sel_start_pos(0),
        sel_stop_pos(0),
        cycle_cursor_pos(0),
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

/* Sets the command line contents, without clearing the pager */
static void reader_set_buffer_maintaining_pager(const wcstring &b, size_t pos);

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

/** Mode on startup, which we restore on exit */
static struct termios terminal_mode_on_startup;

/** Mode we use to execute programs */
static struct termios terminal_mode_for_executing_programs;


static void reader_super_highlight_me_plenty(int highlight_pos_adjust = 0, bool no_io = false);

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
        if (tcsetattr(0, TCSANOW, &terminal_mode_for_executing_programs))
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
   Update the cursor position
*/
static void update_buff_pos(editable_line_t *el, size_t buff_pos)
{
    el->position = buff_pos;
    if (el == &data->command_line && data->sel_active)
    {
        if (data->sel_begin_pos <= buff_pos)
        {
            data->sel_start_pos = data->sel_begin_pos;
            data->sel_stop_pos = buff_pos;
        }
        else
        {
            data->sel_start_pos = buff_pos;
            data->sel_stop_pos = data->sel_begin_pos;
        }
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
    editable_line_t *cmd_line = &data->command_line;
    // Update the indentation
    data->indents = parse_util_compute_indents(cmd_line->text);

    // Combine the command and autosuggestion into one string
    wcstring full_line = combine_command_and_autosuggestion(cmd_line->text, data->autosuggestion);

    size_t len = full_line.size();
    if (len < 1)
        len = 1;

    std::vector<highlight_spec_t> colors = data->colors;
    colors.resize(len, highlight_spec_autosuggestion);

    if (data->sel_active)
    {
        highlight_spec_t selection_color = highlight_make_background(highlight_spec_selection);
        for (size_t i = data->sel_start_pos; i <= std::min(len - 1, data->sel_stop_pos); i++)
        {
            colors[i] = selection_color;
        }
    }

    std::vector<int> indents = data->indents;
    indents.resize(len);

    // Re-render our completions page if necessary
    // We set the term size to 1 less than the true term height. This means we will always show the (bottom) line of the prompt.
    data->pager.set_term_size(maxi(1, common_get_width()), maxi(1, common_get_height() - 1));
    data->pager.update_rendering(&data->current_page_rendering);

    bool focused_on_pager = data->active_edit_line() == &data->pager.search_field_line;
    size_t cursor_position = focused_on_pager ? data->pager.cursor_position() : cmd_line->position;

    s_write(&data->screen,
            data->left_prompt_buff,
            data->right_prompt_buff,
            full_line,
            cmd_line->size(),
            &colors[0],
            &indents[0],
            cursor_position,
            data->sel_start_pos,
            data->sel_stop_pos,
            data->current_page_rendering,
            focused_on_pager);

    data->repaint_needed = false;
}

/** Internal helper function for handling killing parts of text. */
static void reader_kill(editable_line_t *el, size_t begin_idx, size_t length, int mode, int newv)
{
    const wchar_t *begin = el->text.c_str() + begin_idx;
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

    if (el->position > begin_idx)
    {
        /* Move the buff position back by the number of characters we deleted, but don't go past buff_pos */
        size_t backtrack = mini(el->position - begin_idx, length);
        update_buff_pos(el, el->position - backtrack);
    }

    el->text.erase(begin_idx, length);
    data->command_line_changed(el);

    reader_super_highlight_me_plenty();
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
void reader_data_t::command_line_changed(const editable_line_t *el)
{
    ASSERT_IS_MAIN_THREAD();
    if (el == &this->command_line)
    {
        size_t len = this->command_line.size();

        /* When we grow colors, propagate the last color (if any), under the assumption that usually it will be correct. If it is, it avoids a repaint. */
        highlight_spec_t last_color = colors.empty() ? highlight_spec_t() : colors.back();
        colors.resize(len, last_color);

        indents.resize(len);

        /* Update the gen count */
        s_generation_count++;
    }
    else if (el == &this->pager.search_field_line)
    {
        this->pager.refilter_completions();
        this->pager_selection_changed();
    }
}

void reader_data_t::pager_selection_changed()
{
    ASSERT_IS_MAIN_THREAD();

    const completion_t *completion = this->pager.selected_completion(this->current_page_rendering);

    /* Update the cursor and command line */
    size_t cursor_pos = this->cycle_cursor_pos;
    wcstring new_cmd_line;

    if (completion == NULL)
    {
        new_cmd_line = this->cycle_command_line;
    }
    else
    {
        new_cmd_line = completion_apply_to_command_line(completion->completion, completion->flags, this->cycle_command_line, &cursor_pos, false);
    }
    reader_set_buffer_maintaining_pager(new_cmd_line, cursor_pos);

    /* Since we just inserted a completion, don't immediately do a new autosuggestion */
    this->suppress_autosuggestion = true;

    /* Trigger repaint (see #765) */
    reader_repaint_needed();
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
    const size_t subcmd_cursor_pos = cursor_pos - subcmd_offset;

    /* Parse this subcmd */
    parse_node_tree_t parse_tree;
    parse_tree_from_string(subcmd, parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens, &parse_tree, NULL);

    /* Look for plain statements where the cursor is at the end of the command */
    const parse_node_t *matching_cmd_node = NULL;
    const size_t len = parse_tree.size();
    for (size_t i=0; i < len; i++)
    {
        const parse_node_t &node = parse_tree.at(i);

        /* Only interested in plain statements with source */
        if (node.type != symbol_plain_statement || ! node.has_source())
            continue;

        /* Skip decorated statements */
        if (parse_tree.decoration_for_plain_statement(node) != parse_statement_decoration_none)
            continue;

        /* Get the command node. Skip it if we can't or it has no source */
        const parse_node_t *cmd_node = parse_tree.get_child(node, 0, parse_token_type_string);
        if (cmd_node == NULL || ! cmd_node->has_source())
            continue;

        /* Now see if its source range contains our cursor, including at the end */
        if (subcmd_cursor_pos >= cmd_node->source_start && subcmd_cursor_pos <= cmd_node->source_start + cmd_node->source_length)
        {
            /* Success! */
            matching_cmd_node = cmd_node;
            break;
        }
    }

    /* Now if we found a command node, expand it */
    bool result = false;
    if (matching_cmd_node != NULL)
    {
        assert(matching_cmd_node->type == parse_token_type_string);
        const wcstring token = matching_cmd_node->get_source(subcmd);
        wcstring abbreviation;
        if (expand_abbreviation(token, &abbreviation))
        {
            /* There was an abbreviation! Replace the token in the full command. Maintain the relative position of the cursor. */
            if (output != NULL)
            {
                output->assign(cmdline);
                output->replace(subcmd_offset + matching_cmd_node->source_start, matching_cmd_node->source_length, abbreviation);
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
    editable_line_t *el = data->active_edit_line();
    if (this->expand_abbreviations && el == &data->command_line)
    {
        /* Try expanding abbreviations */
        wcstring new_cmdline;
        size_t cursor_pos = el->position - mini(el->position, cursor_backtrack);
        if (reader_expand_abbreviation_in_command(el->text, cursor_pos, &new_cmdline))
        {
            /* We expanded an abbreviation! The cursor moves by the difference in the command line lengths. */
            size_t new_buff_pos = el->position + new_cmdline.size() - el->text.size();


            el->text.swap(new_cmdline);
            update_buff_pos(el, new_buff_pos);
            data->command_line_changed(el);
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

    /* Save the initial terminal mode */
    tcgetattr(STDIN_FILENO, &terminal_mode_on_startup);

    /* Set the mode used for program execution, initialized to the current mode */
    memcpy(&terminal_mode_for_executing_programs, &terminal_mode_on_startup, sizeof terminal_mode_for_executing_programs);
    terminal_mode_for_executing_programs.c_iflag &= ~IXON;     /* disable flow control */
    terminal_mode_for_executing_programs.c_iflag &= ~IXOFF;    /* disable flow control */

    /* Set the mode used for the terminal, initialized to the current mode */
    memcpy(&shell_modes, &terminal_mode_on_startup, sizeof shell_modes);
    shell_modes.c_lflag &= ~ICANON;   /* turn off canonical mode */
    shell_modes.c_lflag &= ~ECHO;     /* turn off echo mode */
    shell_modes.c_iflag &= ~IXON;     /* disable flow control */
    shell_modes.c_iflag &= ~IXOFF;    /* disable flow control */
    shell_modes.c_cc[VMIN]=1;
    shell_modes.c_cc[VTIME]=0;

#if defined(_POSIX_VDISABLE)
    // PCA disable VDSUSP (typically control-Y), which is a funny job control
    // function available only on OS X and BSD systems
    // This lets us use control-Y for yank instead
#ifdef VDSUSP
    shell_modes.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
#endif
}


void reader_destroy()
{
    pthread_key_delete(generation_count_key);
}

void restore_term_mode()
{
    // Restore the term mode if we own the terminal
    // It's important we do this before restore_foreground_process_group, otherwise we won't think we own the terminal
    if (getpid() == tcgetpgrp(STDIN_FILENO))
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &terminal_mode_on_startup);
    }
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


/* Indicates if the given command char ends paging */
static bool command_ends_paging(wchar_t c, bool focused_on_search_field)
{
    switch (c)
    {
            /* These commands always end paging */
        case R_HISTORY_SEARCH_BACKWARD:
        case R_HISTORY_SEARCH_FORWARD:
        case R_BEGINNING_OF_HISTORY:
        case R_END_OF_HISTORY:
        case R_HISTORY_TOKEN_SEARCH_BACKWARD:
        case R_HISTORY_TOKEN_SEARCH_FORWARD:
        case R_ACCEPT_AUTOSUGGESTION:
        case R_CANCEL:
            return true;

            /* These commands never do */
        case R_COMPLETE:
        case R_COMPLETE_AND_SEARCH:
        case R_BACKWARD_CHAR:
        case R_FORWARD_CHAR:
        case R_UP_LINE:
        case R_DOWN_LINE:
        case R_NULL:
        case R_REPAINT:
        case R_SUPPRESS_AUTOSUGGESTION:
        default:
            return false;

            /* R_EXECUTE does end paging, but only executes if it was not paging. So it's handled specially */
        case R_EXECUTE:
            return false;

            /* These commands operate on the search field if that's where the focus is */
        case R_BEGINNING_OF_LINE:
        case R_END_OF_LINE:
        case R_FORWARD_WORD:
        case R_BACKWARD_WORD:
        case R_DELETE_CHAR:
        case R_BACKWARD_DELETE_CHAR:
        case R_KILL_LINE:
        case R_YANK:
        case R_YANK_POP:
        case R_BACKWARD_KILL_LINE:
        case R_KILL_WHOLE_LINE:
        case R_KILL_WORD:
        case R_BACKWARD_KILL_WORD:
        case R_BACKWARD_KILL_PATH_COMPONENT:
        case R_SELF_INSERT:
        case R_TRANSPOSE_CHARS:
        case R_TRANSPOSE_WORDS:
        case R_UPCASE_WORD:
        case R_DOWNCASE_WORD:
        case R_CAPITALIZE_WORD:
        case R_VI_ARG_DIGIT:
        case R_VI_DELETE_TO:
        case R_BEGINNING_OF_BUFFER:
        case R_END_OF_BUFFER:
            return ! focused_on_search_field;
    }
}

/**
   Remove the previous character in the character buffer and on the
   screen using syntax highlighting, etc.
*/
static void remove_backward()
{
    editable_line_t *el = data->active_edit_line();

    if (el->position <= 0)
        return;

    /* Fake composed character sequences by continuing to delete until we delete a character of width at least 1. */
    int width;
    do
    {
        update_buff_pos(el, el->position - 1);
        width = fish_wcwidth(el->text.at(el->position));
        el->text.erase(el->position, 1);
    }
    while (width == 0 && el->position > 0);
    data->command_line_changed(el);
    data->suppress_autosuggestion = true;

    reader_super_highlight_me_plenty();

    reader_repaint();

}


/**
   Insert the characters of the string into the command line buffer
   and print them to the screen using syntax highlighting, etc.
   Optionally also expand abbreviations.
   Returns true if the string changed.
*/
static bool insert_string(editable_line_t *el, const wcstring &str, bool should_expand_abbreviations = false)
{
    size_t len = str.size();
    if (len == 0)
        return false;

    el->insert_string(str);
    update_buff_pos(el, el->position);
    data->command_line_changed(el);

    if (el == &data->command_line)
    {
        data->suppress_autosuggestion = false;

        if (should_expand_abbreviations)
            data->expand_abbreviation_as_necessary(1);

        /* Syntax highlight. Note we must have that buff_pos > 0 because we just added something nonzero to its length  */
        assert(el->position > 0);
        reader_super_highlight_me_plenty(-1);
    }
    reader_repaint();

    return true;
}

/**
   Insert the character into the command line buffer and print it to
   the screen using syntax highlighting, etc.
*/
static bool insert_char(editable_line_t *el, wchar_t c, bool should_expand_abbreviations = false)
{
    return insert_string(el, wcstring(1, c), should_expand_abbreviations);
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
    editable_line_t *el = data->active_edit_line();
    size_t cursor = el->position;
    wcstring new_command_line = completion_apply_to_command_line(val, flags, el->text, &cursor, false /* not append only */);
    reader_set_buffer_maintaining_pager(new_command_line, cursor);

    /* Since we just inserted a completion, don't immediately do a new autosuggestion */
    data->suppress_autosuggestion = true;
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
    const unsigned int generation_count;

    autosuggestion_context_t(history_t *history, const wcstring &term, size_t pos) :
        search_string(term),
        cursor_pos(pos),
        searcher(*history, term, HISTORY_SEARCH_TYPE_PREFIX),
        detector(history),
        working_directory(env_get_pwd_slash()),
        vars(env_vars_snapshot_t::highlighting_keys),
        generation_count(s_generation_count)
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
        complete(search_string, completions, COMPLETION_REQUEST_AUTOSUGGESTION);
        if (! completions.empty())
        {
            const completion_t &comp = completions.at(0);
            size_t cursor = this->cursor_pos;
            this->autosuggestion = completion_apply_to_command_line(comp.completion, comp.flags, this->search_string, &cursor, true /* append only */);
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
    const editable_line_t *el = data->active_edit_line();
    const wchar_t *whitespace = L" \t\r\n\v";
    return ! data->suppress_autosuggestion &&
           data->history_search.is_at_end() &&
           el == &data->command_line &&
           el->text.find_first_not_of(whitespace) != wcstring::npos;
}

static void autosuggest_completed(autosuggestion_context_t *ctx, int result)
{
    if (result &&
            can_autosuggest() &&
            ctx->search_string == data->command_line.text &&
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
        const editable_line_t *el = data->active_edit_line();
        autosuggestion_context_t *ctx = new autosuggestion_context_t(data->history, el->text, el->position);
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
            data->command_line.text = data->autosuggestion;
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
                data->command_line.text.push_back(wc);
            }
        }
        update_buff_pos(&data->command_line, data->command_line.size());
        data->command_line_changed(&data->command_line);
        reader_super_highlight_me_plenty();
        reader_repaint();
    }
}

/* Ensure we have no pager contents */
static void clear_pager()
{
    if (data)
    {
        data->pager.clear();
        data->current_page_rendering = page_rendering_t();
        reader_repaint_needed();
    }
}

static void select_completion_in_direction(enum selection_direction_t dir)
{
    assert(data != NULL);
    /* Note: this will probably trigger reader_selected_completion_changed, which will cause us to update stuff */
    bool selection_changed = data->pager.select_next_completion_in_direction(dir, data->current_page_rendering);
    if (selection_changed)
    {
        data->pager_selection_changed();
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

    editable_line_t *el = &data->command_line;
    for (size_t i=0; i<el->position; i++)
    {
        data->colors.at(i) = highlight_spec_search_match<<16;
    }

    reader_repaint();

    pollint.tv_sec = 0;
    pollint.tv_nsec = 100 * 1000000;
    nanosleep(&pollint, NULL);

    reader_super_highlight_me_plenty();

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
   \param continue_after_prefix_insertion If we have a shared prefix, whether to print the list of completions after inserting it.

   Return true if we inserted text into the command line, false if we did not.
*/

static bool handle_completions(const std::vector<completion_t> &comp, bool continue_after_prefix_insertion)
{
    bool done = false;
    bool success = false;
    const editable_line_t *el = &data->command_line;
    const wchar_t *begin, *end, *buff = el->text.c_str();

    parse_util_token_extent(buff, el->position, &begin, 0, 0, 0);
    end = buff+el->position;

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

        if (continue_after_prefix_insertion || common_prefix.empty())
        {
            /* We didn't get a common prefix, or we want to print the list anyways. */
            size_t len, prefix_start = 0;
            wcstring prefix;
            parse_util_get_parameter_info(el->text, el->position, NULL, &prefix_start, NULL);

            assert(el->position >= prefix_start);
            len = el->position - prefix_start;

            if (match_type_requires_full_replacement(best_match_type))
            {
                // No prefix
                prefix.clear();
            }
            else if (len <= PREFIX_MAX_LEN)
            {
                prefix.append(el->text, prefix_start, len);
            }
            else
            {
                // append just the end of the string
                prefix = wcstring(&ellipsis_char, 1);
                prefix.append(el->text, prefix_start + len - PREFIX_MAX_LEN, PREFIX_MAX_LEN);
            }

            wchar_t quote;
            parse_util_get_parameter_info(el->text, el->position, &quote, NULL, NULL);

            /* Update the pager data */
            data->pager.set_prefix(prefix);
            data->pager.set_completions(surviving_completions);

            /* Invalidate our rendering */
            data->current_page_rendering = page_rendering_t();

            /* Modify the command line to reflect the new pager */
            data->pager_selection_changed();

            reader_repaint_needed();

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
    /* Note: 'data' is non-null if we are interactive, except in the testing environment */
    if (get_is_interactive() && data != NULL)
    {
        if (data->command_line.position > data->command_line.size())
            sanity_lose();

        if (data->colors.size() != data->command_line.size())
            sanity_lose();

        if (data->indents.size() != data->command_line.size())
            sanity_lose();

    }
}

/**
   Set the specified string as the current buffer.
*/
static void set_command_line_and_position(editable_line_t *el, const wcstring &new_str, size_t pos)
{
    el->text = new_str;
    update_buff_pos(el, pos);
    data->command_line_changed(el);
    reader_super_highlight_me_plenty();
    reader_repaint_needed();
}

static void reader_replace_current_token(const wchar_t *new_token)
{

    const wchar_t *begin, *end;
    size_t new_pos;

    /* Find current token */
    editable_line_t *el = data->active_edit_line();
    const wchar_t *buff = el->text.c_str();
    parse_util_token_extent(buff, el->position, &begin, &end, 0, 0);

    if (!begin || !end)
        return;

    /* Make new string */
    wcstring new_buff(buff, begin - buff);
    new_buff.append(new_token);
    new_buff.append(end);
    new_pos = (begin-buff) + wcslen(new_token);

    set_command_line_and_position(el, new_buff, new_pos);
}


/**
   Reset the data structures associated with the token search
*/
static void reset_token_history()
{
    const editable_line_t *el = data->active_edit_line();
    const wchar_t *begin, *end;
    const wchar_t *buff = el->text.c_str();
    parse_util_token_extent((wchar_t *)buff, el->position, &begin, &end, 0, 0);

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
        reader_super_highlight_me_plenty();
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
                    break;

                    default:
                    {
                        break;
                    }

                }
            }
        }

        if (str)
        {
            reader_replace_current_token(str);
            reader_super_highlight_me_plenty();
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

static void move_word(editable_line_t *el, bool move_right, bool erase, enum move_word_style_t style, bool newv)
{
    /* Return if we are already at the edge */
    const size_t boundary = move_right ? el->size() : 0;
    if (el->position == boundary)
        return;

    /* When moving left, a value of 1 means the character at index 0. */
    move_word_state_machine_t state(style);
    const wchar_t * const command_line = el->text.c_str();
    const size_t start_buff_pos = el->position;

    size_t buff_pos = el->position;
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

    /* If we are moving left, buff_pos-1 is the index of the first character we do not delete (possibly -1). If we are moving right, then buff_pos is that index - possibly el->size(). */
    if (erase)
    {
        /* Don't autosuggest after a kill */
        if (el == &data->command_line)
        {
            data->suppress_autosuggestion = true;
        }

        if (move_right)
        {
            reader_kill(el, start_buff_pos, buff_pos - start_buff_pos, KILL_APPEND, newv);
        }
        else
        {
            reader_kill(el, buff_pos, start_buff_pos - buff_pos, KILL_PREPEND, newv);
        }
    }
    else
    {
        update_buff_pos(el, buff_pos);
        reader_repaint();
    }

}

const wchar_t *reader_get_buffer(void)
{
    ASSERT_IS_MAIN_THREAD();
    return data ? data->command_line.text.c_str() : NULL;
}

history_t *reader_get_history(void)
{
    ASSERT_IS_MAIN_THREAD();
    return data ? data->history : NULL;
}

/* Sets the command line contents, without clearing the pager */
static void reader_set_buffer_maintaining_pager(const wcstring &b, size_t pos)
{
    /* Callers like to pass us pointers into ourselves, so be careful! I don't know if we can use operator= with a pointer to our interior, so use an intermediate. */
    size_t command_line_len = b.size();
    data->command_line.text = b;
    data->command_line_changed(&data->command_line);

    /* Don't set a position past the command line length */
    if (pos > command_line_len)
        pos = command_line_len;

    update_buff_pos(&data->command_line, pos);

    /* Clear history search and pager contents */
    data->search_mode = NO_SEARCH;
    data->search_buff.clear();
    data->history_search.go_to_end();

    reader_super_highlight_me_plenty();
    reader_repaint_needed();
}

/* Sets the command line contents, clearing the pager */
void reader_set_buffer(const wcstring &b, size_t pos)
{
    if (!data)
        return;

    clear_pager();
    reader_set_buffer_maintaining_pager(b, pos);
}


size_t reader_get_cursor_pos()
{
    if (!data)
        return (size_t)(-1);

    return data->command_line.position;
}

bool reader_get_selection(size_t *start, size_t *len)
{
    bool result = false;
    if (data != NULL && data->sel_active)
    {
        *start = data->sel_start_pos;
        *len = std::min(data->sel_stop_pos - data->sel_start_pos + 1, data->command_line.size());
        result = true;
    }
    return result;
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
    assert(b != NULL);
    wcstring bstr = b;

    /* Append a newline, to act as a statement terminator */
    bstr.push_back(L'\n');

    parse_error_list_t errors;
    int res = parse_util_detect_errors(bstr, &errors);

    if (res & PARSER_TEST_ERROR)
    {
        wcstring error_desc;
        parser_t::principal_parser().get_backtrace(bstr, errors, &error_desc);

        // ensure we end with a newline. Also add an initial newline, because it's likely the user just hit enter and so there's junk on the current line
        if (! string_suffixes_string(L"\n", error_desc))
        {
            error_desc.push_back(L'\n');
        }
        fwprintf(stderr, L"\n%ls", error_desc.c_str());
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

    data->command_line_changed(&data->command_line);

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
    std::vector<highlight_spec_t> colors;

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

    int perform_highlight()
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
        const editable_line_t *el = &data->command_line;
        const wcstring &needle = data->search_buff;
        size_t match_pos = el->text.find(needle);
        if (match_pos != wcstring::npos)
        {
            size_t end = match_pos + needle.size();
            for (size_t i=match_pos; i < end; i++)
            {
                data->colors.at(i) |= (highlight_spec_search_match<<16);
            }
        }
    }
}

static void highlight_complete(background_highlight_context_t *ctx, int result)
{
    ASSERT_IS_MAIN_THREAD();
    if (ctx->string_to_highlight == data->command_line.text)
    {
        /* The data hasn't changed, so swap in our colors. The colors may not have changed, so do nothing if they have not. */
        assert(ctx->colors.size() == data->command_line.size());
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
    return ctx->perform_highlight();
}


/**
   Call specified external highlighting function and then do search
   highlighting. Lastly, clear the background color under the cursor
   to avoid repaint issues on terminals where e.g. syntax highlighting
   maykes characters under the sursor unreadable.

   \param match_highlight_pos_adjust the adjustment to the position to use for bracket matching. This is added to the current cursor position and may be negative.
   \param error if non-null, any possible errors in the buffer are further descibed by the strings inserted into the specified arraylist
    \param no_io if true, do a highlight that does not perform I/O, synchronously. If false, perform an asynchronous highlight in the background, which may perform disk I/O.
*/
static void reader_super_highlight_me_plenty(int match_highlight_pos_adjust, bool no_io)
{
    const editable_line_t *el = &data->command_line;
    long match_highlight_pos = (long)el->position + match_highlight_pos_adjust;
    assert(match_highlight_pos >= 0);

    reader_sanity_check();

    highlight_function_t highlight_func = no_io ? highlight_shell_no_io : data->highlight_function;
    background_highlight_context_t *ctx = new background_highlight_context_t(el->text, match_highlight_pos, highlight_func);
    if (no_io)
    {
        // Highlighting without IO, we just do it
        // Note that highlight_complete deletes ctx.
        int result = ctx->perform_highlight();
        highlight_complete(ctx, result);
    }
    else
    {
        // Highlighting including I/O proceeds in the background
        iothread_perform(threaded_highlight, highlight_complete, ctx);
    }
    highlight_search();

    /* Here's a hack. Check to see if our autosuggestion still applies; if so, don't recompute it. Since the autosuggestion computation is asynchronous, this avoids "flashing" as you type into the autosuggestion. */
    const wcstring &cmd = el->text, &suggest = data->autosuggestion;
    if (can_autosuggest() && ! suggest.empty() && string_prefixes_string_case_insensitive(cmd, suggest))
    {
        /* The autosuggestion is still reasonable, so do nothing */
    }
    else
    {
        update_autosuggestion();
    }
}


bool shell_is_exiting()
{
    if (get_is_interactive())
        return job_list_is_empty() && data != NULL && data->end_loop;
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
    int stopped_jobs_count=0;
    int is_breakpoint=0;
    const parser_t &parser = parser_t::principal_parser();

    for (size_t i = 0; i < parser.block_count(); i++)
    {
        if (parser.block_at_index(i)->type() == BREAKPOINT)
        {
            is_breakpoint = 1;
            break;
        }
    }

    job_iterator_t jobs;
    while ((j = jobs.next()))
    {
        if (!job_is_completed(j) && (job_is_stopped(j)))
        {
            stopped_jobs_count++;
            break;
        }
    }

    if (!reader_exit_forced() && !data->prev_end_loop && stopped_jobs_count && !is_breakpoint)
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

static bool selection_is_at_top()
{
    const pager_t *pager = &data->pager;
    size_t row = pager->get_selected_row(data->current_page_rendering);
    if (row != 0 && row != PAGER_SELECTION_NONE)
        return false;

    size_t col = pager->get_selected_column(data->current_page_rendering);
    if (col != 0 && col != PAGER_SELECTION_NONE)
        return false;

    return true;
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
            const wcstring command = tmp;
            update_buff_pos(&data->command_line, 0);
            data->command_line.text.clear();
            data->command_line_changed(&data->command_line);
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

    /* The command line before completion */
    data->cycle_command_line.clear();
    data->cycle_cursor_pos = 0;

    data->search_buff.clear();
    data->search_mode = NO_SEARCH;

    exec_prompt();

    reader_super_highlight_me_plenty();
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
            //fprintf(stderr, "C: %lx\n", (long)c);

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

                    insert_string(&data->command_line, arr);

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

        /* Restore the text */
        if (c == R_CANCEL && data->is_navigating_pager_contents())
        {
            set_command_line_and_position(&data->command_line, data->cycle_command_line, data->cycle_cursor_pos);
        }


        /* Clear the pager if necessary */
        bool focused_on_search_field = (data->active_edit_line() == &data->pager.search_field_line);
        if (command_ends_paging(c, focused_on_search_field))
        {
            clear_pager();
        }

        //fprintf(stderr, "\n\nchar: %ls\n\n", describe_char(c).c_str());

        switch (c)
        {
                /* go to beginning of line*/
            case R_BEGINNING_OF_LINE:
            {
                editable_line_t *el = data->active_edit_line();
                while (el->position > 0 && el->text.at(el->position - 1) != L'\n')
                {
                    update_buff_pos(el, el->position - 1);
                }

                reader_repaint_needed();
                break;
            }

            case R_END_OF_LINE:
            {
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size())
                {
                    const wchar_t *buff = el->text.c_str();
                    while (buff[el->position] &&
                            buff[el->position] != L'\n')
                    {
                        update_buff_pos(el, el->position + 1);
                    }
                }
                else
                {
                    accept_autosuggestion(true);
                }

                reader_repaint_needed();
                break;
            }


            case R_BEGINNING_OF_BUFFER:
            {
                update_buff_pos(&data->command_line, 0);
                reader_repaint_needed();
                break;
            }

            /* go to EOL*/
            case R_END_OF_BUFFER:
            {
                update_buff_pos(&data->command_line, data->command_line.size());

                reader_repaint_needed();
                break;
            }

            case R_NULL:
            {
                break;
            }

            case R_CANCEL:
            {
                // The only thing we can cancel right now is paging, which we handled up above
                break;
            }

            case R_FORCE_REPAINT:
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
            case R_COMPLETE_AND_SEARCH:
            {

                if (!data->complete_func)
                    break;

                /* Use the command line only; it doesn't make sense to complete in any other line */
                editable_line_t *el = &data->command_line;
                if (data->is_navigating_pager_contents() || (! comp_empty && last_char == R_COMPLETE))
                {
                    /* The user typed R_COMPLETE more than once in a row. If we are not yet fully disclosed, then become so; otherwise cycle through our available completions. */
                    if (data->current_page_rendering.remaining_to_disclose > 0)
                    {
                        data->pager.set_fully_disclosed(true);
                        reader_repaint_needed();
                    }
                    else
                    {
                        select_completion_in_direction(c == R_COMPLETE ? direction_next : direction_prev);
                    }
                }
                else
                {
                    /* Either the user hit tab only once, or we had no visible completion list. */

                    /* Remove a trailing backslash. This may trigger an extra repaint, but this is rare. */
                    if (is_backslashed(el->text, el->position))
                    {
                        remove_backward();
                    }

                    /* Get the string; we have to do this after removing any trailing backslash */
                    const wchar_t * const buff = el->text.c_str();

                    /* Clear the completion list */
                    comp.clear();

                    /* Figure out the extent of the command substitution surrounding the cursor. This is because we only look at the current command substitution to form completions - stuff happening outside of it is not interesting. */
                    const wchar_t *cmdsub_begin, *cmdsub_end;
                    parse_util_cmdsubst_extent(buff, el->position, &cmdsub_begin, &cmdsub_end);

                    /* Figure out the extent of the token within the command substitution. Note we pass cmdsub_begin here, not buff */
                    const wchar_t *token_begin, *token_end;
                    parse_util_token_extent(cmdsub_begin, el->position - (cmdsub_begin-buff), &token_begin, &token_end, 0, 0);

                    /* Hack: the token may extend past the end of the command substitution, e.g. in (echo foo) the last token is 'foo)'. Don't let that happen. */
                    if (token_end > cmdsub_end) token_end = cmdsub_end;

                    /* Figure out how many steps to get from the current position to the end of the current token. */
                    size_t end_of_token_offset = token_end - buff;

                    /* Move the cursor to the end */
                    if (el->position != end_of_token_offset)
                    {
                        update_buff_pos(el, end_of_token_offset);
                        reader_repaint();
                    }

                    /* Construct a copy of the string from the beginning of the command substitution up to the end of the token we're completing */
                    const wcstring buffcpy = wcstring(cmdsub_begin, token_end);

                    //fprintf(stderr, "Complete (%ls)\n", buffcpy.c_str());
                    data->complete_func(buffcpy, comp, COMPLETION_REQUEST_DEFAULT | COMPLETION_REQUEST_DESCRIPTIONS | COMPLETION_REQUEST_FUZZY_MATCH);

                    /* Munge our completions */
                    sort_and_make_unique(comp);
                    prioritize_completions(comp);

                    /* Record our cycle_command_line */
                    data->cycle_command_line = el->text;
                    data->cycle_cursor_pos = el->position;

                    bool continue_after_prefix_insertion = (c == R_COMPLETE_AND_SEARCH);
                    comp_empty = handle_completions(comp, continue_after_prefix_insertion);

                    /* Show the search field if requested and if we printed a list of completions */
                    if (c == R_COMPLETE_AND_SEARCH && ! comp_empty && ! data->pager.empty())
                    {
                        data->pager.set_search_field_shown(true);
                        select_completion_in_direction(direction_next);
                        reader_repaint_needed();
                    }

                }

                break;
            }

            /* kill */
            case R_KILL_LINE:
            {
                editable_line_t *el = data->active_edit_line();
                const wchar_t *buff = el->text.c_str();
                const wchar_t *begin = &buff[el->position];
                const wchar_t *end = begin;

                while (*end && *end != L'\n')
                    end++;

                if (end==begin && *end)
                    end++;

                size_t len = end-begin;
                if (len)
                {
                    reader_kill(el, begin - buff, len, KILL_APPEND, last_char!=R_KILL_LINE);
                }

                break;
            }

            case R_BACKWARD_KILL_LINE:
            {
                editable_line_t *el = data->active_edit_line();
                if (el->position > 0)
                {
                    const wchar_t *buff = el->text.c_str();
                    const wchar_t *end = &buff[el->position];
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

                    reader_kill(el, begin - buff, len, KILL_PREPEND, last_char!=R_BACKWARD_KILL_LINE);
                }
                break;

            }

            case R_KILL_WHOLE_LINE:
            {
                editable_line_t *el = data->active_edit_line();
                const wchar_t *buff = el->text.c_str();
                const wchar_t *end = &buff[el->position];
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
                    reader_kill(el, begin - buff, len, KILL_APPEND, last_char!=R_KILL_WHOLE_LINE);
                }

                break;
            }

            /* yank*/
            case R_YANK:
            {
                yank_str = kill_yank();
                insert_string(data->active_edit_line(), yank_str);
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
                    insert_string(data->active_edit_line(), yank_str);
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
                        reader_set_buffer(data->search_buff, data->search_buff.size());
                    }
                    else
                    {
                        reader_replace_current_token(data->search_buff.c_str());
                    }
                    data->search_buff.clear();
                    reader_super_highlight_me_plenty();
                    reader_repaint_needed();
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
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size())
                {
                    update_buff_pos(el, el->position + 1);
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

                /* If the user hits return while navigating the pager, it only clears the pager */
                if (data->is_navigating_pager_contents())
                {
                    clear_pager();
                    break;
                }

                /* The user may have hit return with pager contents, but while not navigating them. Clear the pager in that event. */
                clear_pager();

                /* We only execute the command line */
                editable_line_t *el = &data->command_line;

                /* Allow backslash-escaped newlines, but only if the following character is whitespace, or we're at the end of the text (see issue #163) */
                if (is_backslashed(el->text, el->position))
                {
                    if (el->position >= el->size() || iswspace(el->text.at(el->position)))
                    {
                        insert_char(el, '\n');
                        break;
                    }
                }

                /* See if this command is valid */
                int command_test_result = data->test_func(el->text.c_str());
                if (command_test_result == 0 || command_test_result == PARSER_TEST_INCOMPLETE)
                {
                    /* This command is valid, but an abbreviation may make it invalid. If so, we will have to test again. */
                    bool abbreviation_expanded = data->expand_abbreviation_as_necessary(1);
                    if (abbreviation_expanded)
                    {
                        /* It's our reponsibility to rehighlight and repaint. But everything we do below triggers a repaint. */
                        command_test_result = data->test_func(el->text.c_str());

                        /* If the command is OK, then we're going to execute it. We still want to do syntax highlighting, but a synchronous variant that performs no I/O, so as not to block the user */
                        bool skip_io = (command_test_result == 0);
                        reader_super_highlight_me_plenty(0, skip_io);
                    }
                }

                switch (command_test_result)
                {

                    case 0:
                    {
                        /* Finished command, execute it. Don't add items that start with a leading space. */
                        const editable_line_t *el = &data->command_line;
                        if (data->history != NULL && ! el->empty() && el->text.at(0) != L' ')
                        {
                            data->history->add_with_file_detection(el->text);
                        }
                        finished=1;
                        update_buff_pos(&data->command_line, data->command_line.size());
                        reader_repaint();
                        break;
                    }

                    /*
                     We are incomplete, continue editing
                     */
                    case PARSER_TEST_INCOMPLETE:
                    {
                        insert_char(el, '\n');
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
                        reader_repaint_needed();
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

                    const editable_line_t *el = &data->command_line;
                    data->search_buff.append(el->text);
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
                        set_command_line_and_position(&data->command_line, new_text, new_text.size());

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
                editable_line_t *el = data->active_edit_line();
                if (data->is_navigating_pager_contents())
                {
                    select_completion_in_direction(direction_west);
                }
                else if (el->position > 0)
                {
                    update_buff_pos(el, el->position-1);
                    reader_repaint_needed();
                }
                break;
            }

            /* Move right*/
            case R_FORWARD_CHAR:
            {
                editable_line_t *el = data->active_edit_line();
                if (data->is_navigating_pager_contents())
                {
                    select_completion_in_direction(direction_east);
                }
                else if (el->position < el->size())
                {
                    update_buff_pos(el, el->position + 1);
                    reader_repaint_needed();
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
                move_word(data->active_edit_line(), MOVE_DIR_LEFT, true /* erase */, style, newv);
                break;
            }

            /* kill one word right */
            case R_KILL_WORD:
            {
                move_word(data->active_edit_line(), MOVE_DIR_RIGHT, true /* erase */, move_word_style_punctuation, last_char!=R_KILL_WORD);
                break;
            }

            /* move one word left*/
            case R_BACKWARD_WORD:
            {
                move_word(data->active_edit_line(), MOVE_DIR_LEFT, false /* do not erase */, move_word_style_punctuation, false);
                break;
            }

            /* move one word right*/
            case R_FORWARD_WORD:
            {
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size())
                {
                    move_word(el, MOVE_DIR_RIGHT, false /* do not erase */, move_word_style_punctuation, false);
                }
                else
                {
                    accept_autosuggestion(false /* accept only one word */);
                }
                break;
            }

            case R_BEGINNING_OF_HISTORY:
            {
                const editable_line_t *el = &data->command_line;
                data->history_search = history_search_t(*data->history, el->text, HISTORY_SEARCH_TYPE_PREFIX);
                data->history_search.go_to_beginning();
                if (! data->history_search.is_at_end())
                {
                    wcstring new_text = data->history_search.current_string();
                    set_command_line_and_position(&data->command_line, new_text, new_text.size());
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
                if (data->is_navigating_pager_contents())
                {
                    /* We are already navigating pager contents. */
                    selection_direction_t direction;
                    if (c == R_DOWN_LINE)
                    {
                        /* Down arrow is always south */
                        direction = direction_south;
                    }
                    else if (selection_is_at_top())
                    {
                        /* Up arrow, but we are in the first column and first row. End navigation */
                        direction = direction_deselect;
                    }
                    else
                    {
                        /* Up arrow, go north */
                        direction = direction_north;
                    }

                    /* Now do the selection */
                    select_completion_in_direction(direction);
                }
                else if (c == R_DOWN_LINE && ! data->pager.empty())
                {
                    /* We pressed down with a non-empty pager contents, begin navigation */
                    select_completion_in_direction(direction_south);
                }
                else
                {
                    /* Not navigating the pager contents */
                    editable_line_t *el = data->active_edit_line();
                    int line_old = parse_util_get_line_from_offset(el->text, el->position);
                    int line_new;

                    if (c == R_UP_LINE)
                        line_new = line_old-1;
                    else
                        line_new = line_old+1;

                    int line_count = parse_util_lineno(el->text.c_str(), el->size())-1;

                    if (line_new >= 0 && line_new <= line_count)
                    {
                        size_t base_pos_new;
                        size_t base_pos_old;

                        int indent_old;
                        int indent_new;
                        size_t line_offset_old;
                        size_t total_offset_new;

                        base_pos_new = parse_util_get_offset_from_line(el->text, line_new);

                        base_pos_old = parse_util_get_offset_from_line(el->text,  line_old);

                        assert(base_pos_new != (size_t)(-1) && base_pos_old != (size_t)(-1));
                        indent_old = data->indents.at(base_pos_old);
                        indent_new = data->indents.at(base_pos_new);

                        line_offset_old = el->position - parse_util_get_offset_from_line(el->text, line_old);
                        total_offset_new = parse_util_get_offset(el->text, line_new, line_offset_old - 4*(indent_new-indent_old));
                        update_buff_pos(el, total_offset_new);
                        reader_repaint_needed();
                    }
                }

                break;
            }

            case R_SUPPRESS_AUTOSUGGESTION:
            {
                data->suppress_autosuggestion = true;
                data->autosuggestion.clear();
                reader_repaint_needed();
                break;
            }

            case R_ACCEPT_AUTOSUGGESTION:
            {
                accept_autosuggestion(true);
                break;
            }

            case R_TRANSPOSE_CHARS:
            {
                editable_line_t *el = data->active_edit_line();
                if (el->size() < 2)
                {
                    break;
                }

                /* If the cursor is at the end, transpose the last two characters of the line */
                if (el->position == el->size())
                {
                    update_buff_pos(el, el->position - 1);
                }

                /*
                 Drag the character before the cursor forward over the character at the cursor, moving
                 the cursor forward as well.
                 */
                if (el->position > 0)
                {
                    wcstring local_cmd = el->text;
                    std::swap(local_cmd.at(el->position), local_cmd.at(el->position-1));
                    set_command_line_and_position(el, local_cmd, el->position + 1);
                }
                break;
            }

            case R_TRANSPOSE_WORDS:
            {
                editable_line_t *el = data->active_edit_line();
                size_t len = el->size();
                const wchar_t *buff = el->text.c_str();
                const wchar_t *tok_begin, *tok_end, *prev_begin, *prev_end;

                /* If we are not in a token, look for one ahead */
                size_t buff_pos = el->position;
                while (buff_pos != len && !iswalnum(buff[buff_pos]))
                    buff_pos++;

                update_buff_pos(el, buff_pos);

                parse_util_token_extent(buff, el->position, &tok_begin, &tok_end, &prev_begin, &prev_end);

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
                    set_command_line_and_position(el, new_buff, tok_end - buff);
                }
                break;
            }

            case R_UPCASE_WORD:
            case R_DOWNCASE_WORD:
            case R_CAPITALIZE_WORD:
            {
                editable_line_t *el = data->active_edit_line();
                // For capitalize_word, whether we've capitalized a character so far
                bool capitalized_first = false;

                // We apply the operation from the current location to the end of the word
                size_t pos = el->position;
                move_word(el, MOVE_DIR_RIGHT, false, move_word_style_punctuation, false);
                for (; pos < el->position; pos++)
                {
                    wchar_t chr = el->text.at(pos);

                    // We always change the case; this decides whether we go uppercase (true) or lowercase (false)
                    bool make_uppercase;
                    if (c == R_CAPITALIZE_WORD)
                        make_uppercase = ! capitalized_first && iswalnum(chr);
                    else
                        make_uppercase = (c == R_UPCASE_WORD);

                    // Apply the operation and then record what we did
                    if (make_uppercase)
                        chr = towupper(chr);
                    else
                        chr = towlower(chr);

                    data->command_line.text.at(pos) = chr;
                    capitalized_first = capitalized_first || make_uppercase;
                }
                data->command_line_changed(el);
                reader_super_highlight_me_plenty();
                reader_repaint_needed();
                break;
            }

            case R_BEGIN_SELECTION:
            {
                data->sel_active = true;
                data->sel_begin_pos = data->command_line.position;
                data->sel_start_pos = data->command_line.position;
                data->sel_stop_pos = data->command_line.position;
                break;
            }

            case R_END_SELECTION:
            {
                data->sel_active = false;
                data->sel_start_pos = data->command_line.position;
                data->sel_stop_pos = data->command_line.position;
                break;
            }

            case R_KILL_SELECTION:
            {
                bool newv = (last_char != R_KILL_SELECTION);
                size_t start, len;
                if (reader_get_selection(&start, &len))
                {
                    reader_kill(&data->command_line, start, len, KILL_APPEND, newv);
                }
                break;
            }

            case R_FORWARD_JUMP:
            {
                editable_line_t *el = data->active_edit_line();
                wchar_t target = input_function_pop_arg();
                bool status = false;

                for (size_t i = el->position + 1; i < el->size(); i++)
                {
                    if (el->at(i) == target)
                    {
                        update_buff_pos(el, i);
                        status = true;
                        break;
                    }
                }
                input_function_set_status(status);
                reader_repaint_needed();
                break;
            }

            case R_BACKWARD_JUMP:
            {
                editable_line_t *el = data->active_edit_line();
                wchar_t target = input_function_pop_arg();
                bool status = false;

                size_t tmp_pos = el->position;
                while (tmp_pos--)
                {
                    if (el->at(tmp_pos) == target)
                    {
                        update_buff_pos(el, tmp_pos);
                        status = true;
                        break;
                    }
                }
                input_function_set_status(status);
                reader_repaint_needed();
                break;
            }

            /* Other, if a normal character, we add it to the command */
            default:
            {
                if ((!wchar_private(c)) && (((c>31) || (c==L'\n'))&& (c != 127)))
                {
                    bool should_expand_abbreviations = false;
                    if (data->is_navigating_pager_contents())
                    {
                        data->pager.set_search_field_shown(true);
                    }
                    else
                    {
                        /* Expand abbreviations after space */
                        should_expand_abbreviations = (c == L' ');
                    }

                    /* Regular character */
                    editable_line_t *el = data->active_edit_line();
                    insert_char(data->active_edit_line(), c, should_expand_abbreviations);

                    /* End paging upon inserting into the normal command line */
                    if (el == &data->command_line)
                    {
                        clear_pager();
                    }


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

        reader_repaint_if_needed();
    }

    writestr(L"\n");

    /* Ensure we have no pager contents when we exit */
    if (! data->pager.empty())
    {
        /* Clear to end of screen to erase the pager contents. TODO: this may fail if eos doesn't exist, in which case we should emit newlines */
        screen_force_clear_to_end();
        data->pager.clear();
    }

    if (!reader_exit_forced())
    {
        if (tcsetattr(0,TCSANOW,&old_modes))      /* return to previous mode */
        {
            wperror(L"tcsetattr");
        }

        set_color(rgb_color_t::reset(), rgb_color_t::reset());
    }

    return finished ? data->command_line.text.c_str() : NULL;
}

int reader_search_mode()
{
    if (!data)
    {
        return -1;
    }

    return !!data->search_mode;
}

int reader_has_pager_contents()
{
    if (!data)
    {
        return -1;
    }

    return ! data->current_page_rendering.screen_data.empty();
}

void reader_selected_completion_changed(pager_t *pager)
{
    /* Only interested in the top level pager */
    if (data == NULL || pager != &data->pager)
        return;

    const completion_t *completion = pager->selected_completion(data->current_page_rendering);

    /* Update the cursor and command line */
    size_t cursor_pos = data->cycle_cursor_pos;
    wcstring new_cmd_line;

    if (completion == NULL)
    {
        new_cmd_line = data->cycle_command_line;
    }
    else
    {
        new_cmd_line = completion_apply_to_command_line(completion->completion, completion->flags, data->cycle_command_line, &cursor_pos, false);
    }
    reader_set_buffer_maintaining_pager(new_cmd_line, cursor_pos);

    /* Since we just inserted a completion, don't immediately do a new autosuggestion */
    data->suppress_autosuggestion = true;

    /* Trigger repaint (see #765) */
    reader_repaint_needed();
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

        parse_error_list_t errors;
        if (! parse_util_detect_errors(str, &errors))
        {
            parser.eval(str, io, TOP);
        }
        else
        {
            wcstring sb;
            parser.get_backtrace(str, errors, &sb);
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
