#include "config.h"

#include "pager.h"
#include "highlight.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <map>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>

#include <locale.h>

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

#include <signal.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <errno.h>
#include <vector>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "common.h"
#include "complete.h"
#include "output.h"
#include "input_common.h"
#include "env_universal.h"
#include "print_help.h"

typedef pager_t::comp_t comp_t;
typedef std::vector<completion_t> completion_list_t;
typedef std::vector<comp_t> comp_info_list_t;

enum
{
    LINE_UP = R_NULL+1,
    LINE_DOWN,
    PAGE_UP,
    PAGE_DOWN
}
;


enum
{
    HIGHLIGHT_PAGER_PREFIX,
    HIGHLIGHT_PAGER_COMPLETION,
    HIGHLIGHT_PAGER_DESCRIPTION,
    HIGHLIGHT_PAGER_PROGRESS,
    HIGHLIGHT_PAGER_SECONDARY
}
;

enum
{
    /*
      Returnd by the pager if no more displaying is needed
    */
    PAGER_DONE,
    /*
      Returned by the pager if the completions would not fit in the specified number of columns
    */
    PAGER_RETRY,
    /*
      Returned by the pager if the terminal changes size
    */
    PAGER_RESIZE
}
;

/**
   The minimum width (in characters) the terminal may have for fish_pager to not refuse showing the completions
*/
#define PAGER_MIN_WIDTH 16

/**
   The maximum number of columns of completion to attempt to fit onto the screen
*/
#define PAGER_MAX_COLS 6

/**
   The string describing the single-character options accepted by fish_pager
*/
#define GETOPT_STRING "c:hr:qvp:"

/**
   Error to use when given an invalid file descriptor for reading completions or writing output
*/
#define ERR_NOT_FD _( L"%ls: Argument '%s' is not a valid file descriptor\n" )

/**
   This buffer is used to buffer the output of the pager to improve
   screen redraw performance bu cutting down the number of write()
   calls to only one.
*/
static std::vector<char> pager_buffer;

/**
   The environment variables used to specify the color of different
   tokens.
*/
static const wchar_t *hightlight_var[] =
{
    L"fish_pager_color_prefix",
    L"fish_pager_color_completion",
    L"fish_pager_color_description",
    L"fish_pager_color_progress",
    L"fish_pager_color_secondary"
}
;

/**
   This string contains the text that should be sent back to the calling program
*/
static wcstring out_buff;


/**
   This function translates from a highlight code to a specific color
   by check invironement variables
*/
static rgb_color_t get_color(int highlight)
{
    const wchar_t *val;

    if (highlight < 0)
        return rgb_color_t::normal();
    if (highlight >= (5))
        return rgb_color_t::normal();

    val = wgetenv(hightlight_var[highlight]);

    if (!val)
    {
        val = env_universal_get(hightlight_var[highlight]);
    }

    if (!val)
    {
        return rgb_color_t::normal();
    }

    return parse_color(val, false);
}

/**
   This function calculates the minimum width for each completion
   entry in the specified array_list. This width depends on the
   terminal size, so this function should be called when the terminal
   changes size.
*/
void pager_t::recalc_min_widths(comp_info_list_t * lst) const
{
    for (size_t i=0; i<lst->size(); i++)
    {
        comp_t *c = &lst->at(i);

        c->min_width = mini(c->desc_width, maxi(0, term_width/3 - 2)) +
                       mini(c->desc_width, maxi(0, term_width/5 - 4)) +4;
    }

}

/**
   Test if the specified character sequence has been entered on the
   keyboard
*/
static int try_sequence(const char *seq)
{
    int j, k;
    wint_t c=0;

    for (j=0;
            seq[j] != '\0' && seq[j] == (c=input_common_readch(j>0));
            j++)
        ;

    if (seq[j] == '\0')
    {
        return 1;
    }
    else
    {
        input_common_unreadch(c);
        for (k=j-1; k>=0; k--)
            input_common_unreadch(seq[k]);
    }
    return 0;
}

/**
   Read a character from keyboard
*/
static wint_t readch()
{
    struct mapping
    {
        const char *seq;
        wint_t bnd;
    }
    ;

    struct mapping m[]=
    {
        {
            "\x1b[A", LINE_UP
        }
        ,
        {
            key_up, LINE_UP
        }
        ,
        {
            "\x1b[B", LINE_DOWN
        }
        ,
        {
            key_down, LINE_DOWN
        }
        ,
        {
            key_ppage, PAGE_UP
        }
        ,
        {
            key_npage, PAGE_DOWN
        }
        ,
        {
            " ", PAGE_DOWN
        }
        ,
        {
            "\t", PAGE_DOWN
        }
        ,
        {
            0, 0
        }

    }
    ;
    int i;

    for (i=0; m[i].bnd; i++)
    {
        if (!m[i].seq)
        {
            continue;
        }

        if (try_sequence(m[i].seq))
            return m[i].bnd;
    }
    return input_common_readch(0);
}

/**
   Write specified character to the output buffer \c pager_buffer
*/
static int pager_buffered_writer(char c)
{
    pager_buffer.push_back(c);
    return 0;
}

/**
   Print the specified string, but use at most the specified amount of
   space. If the whole string can't be fitted, ellipsize it.

   \param str the string to print
   \param max the maximum space that may be used for printing
   \param has_more if this flag is true, this is not the entire string, and the string should be ellisiszed even if the string fits but takes up the whole space.
*/
static int print_max(const wcstring &str, int max, bool has_more)
{
    int written = 0;
    for (size_t i=0; i < str.size(); i++)
    {
        wchar_t c = str.at(i);

        if (written + wcwidth(c) > max)
            break;
        if ((written + wcwidth(c) == max) && (has_more || i + 1 < str.size()))
        {
            writech(ellipsis_char);
            written += wcwidth(ellipsis_char);
            break;
        }

        writech(c);
        written+= wcwidth(c);
    }
    return written;
}

static int print_max(const wcstring &str, int color, int max, bool has_more, line_t *line)
{
    int written = 0;
    for (size_t i=0; i < str.size(); i++)
    {
        wchar_t c = str.at(i);

        if (written + wcwidth(c) > max)
            break;
        if ((written + wcwidth(c) == max) && (has_more || i + 1 < str.size()))
        {
            line->append(ellipsis_char, color);
            written += wcwidth(ellipsis_char);
            break;
        }
        
        line->append(c, color);
        written += wcwidth(c);
    }
    return written;
}


/**
   Print the specified item using at the specified amount of space
*/
line_t pager_t::completion_print_item(const wcstring &prefix, const comp_t *c, size_t row, size_t column, int width, bool secondary, page_rendering_t *rendering) const
{
    int comp_width=0, desc_width=0;
    int written=0;
    
    line_t line_data;

    if (c->pref_width <= width)
    {
        /*
          The entry fits, we give it as much space as it wants
        */
        comp_width = c->comp_width;
        desc_width = c->desc_width;
    }
    else
    {
        /*
          The completion and description won't fit on the
          allocated space. Give a maximum of 2/3 of the
          space to the completion, and whatever is left to
          the description.
        */
        int desc_all = c->desc_width?c->desc_width+4:0;

        comp_width = maxi(mini(c->comp_width, 2*(width-4)/3), width - desc_all);
        if (c->desc_width)
            desc_width = width-comp_width-4;

    }
    
    int bg_color = secondary ? HIGHLIGHT_PAGER_SECONDARY : HIGHLIGHT_NORMAL;
    
    for (size_t i=0; i<c->comp.size(); i++)
    {
        const wcstring &comp = c->comp.at(i);

        if (i != 0)
            written += print_max(L"  ", 0 /* default color */, comp_width - written, true /* has_more */, &line_data);
        
        int packed_color = HIGHLIGHT_PAGER_PREFIX | (bg_color << 16);
        written += print_max(prefix, packed_color, comp_width - written, ! comp.empty(), &line_data);
        
        packed_color = HIGHLIGHT_PAGER_COMPLETION | (bg_color << 16);
        written += print_max(comp, packed_color, comp_width - written, i + 1 < c->comp.size(), &line_data);
    }

    if (desc_width)
    {
        int packed_color = HIGHLIGHT_PAGER_DESCRIPTION | (bg_color << 16);
        while (written < (width-desc_width-2))
        {
            written += print_max(L" ", packed_color, 1, false, &line_data);
        }
        written += print_max(L"(", packed_color, 1, false, &line_data);
        written += print_max(c->desc, packed_color, desc_width, false, &line_data);
        written += print_max(L")", packed_color, 1, false, &line_data);
    }
    else
    {
        while (written < width)
        {
            written += print_max(L" ", 0, 1, false, &line_data);
        }
    }

    return line_data;
}

/**
   Print the specified part of the completion list, using the
   specified column offsets and quoting style.

   \param l The list of completions to print
   \param cols number of columns to print in
   \param width An array specifying the width of each column
   \param row_start The first row to print
   \param row_stop the row after the last row to print
   \param prefix The string to print before each completion
*/

void pager_t::completion_print(int cols, int *width_per_column, int row_start, int row_stop, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const
{

    size_t rows = (lst.size()-1)/cols+1;

    fprintf(stderr, "prefix: %ls\n", prefix.c_str());

    for (size_t row = row_start; row < row_stop; row++)
    {
        for (size_t col = 0; col < cols; col++)
        {
            int is_last = (col==(cols-1));

            if (lst.size() <= col * rows + row)
                continue;

            const comp_t *el = &lst.at(col * rows + row);

            /* Print this completion on its own "line" */
            line_t line = completion_print_item(prefix, el, row, col, width_per_column[col] - (is_last?0:2), row%2, rendering);
            
            /* If there's more to come, append two spaces */
            if (col + 1 < cols)
            {
                line.append(L' ', 0);
                line.append(L' ', 0);
            }
            
            /* Append this to the real line */
            rendering->screen_data.create_line(row).append_line(line);
        }
    }
}


/* Trim leading and trailing whitespace, and compress other whitespace runs into a single space. */
static void mangle_1_completion_description(wcstring *str)
{
    size_t leading = 0, trailing = 0, len = str->size();
 
    // Skip leading spaces
    for (; leading < len; leading++)
    {
        if (! iswspace(str->at(leading)))
            break;
    }
    
    // Compress runs of spaces to a single space
    bool was_space = false;
    for (; leading < len; leading++)
    {
        wchar_t wc = str->at(leading);
        bool is_space = iswspace(wc);
        if (! is_space)
        {
            // normal character
            str->at(trailing++) = wc;
        }
        else if (! was_space)
        {
            // initial space in a run
            str->at(trailing++) = L' ';
        }
        else
        {
            // non-initial space in a run, do nothing
        }
        was_space = is_space;
    }
    
    // leading is now at len, trailing is the new length of the string
    // Delete trailing spaces
    while (trailing > 0 && iswspace(str->at(trailing - 1)))
    {
        trailing--;
    }
    
    str->resize(trailing);
}

static void join_completions(comp_info_list_t *comps)
{
    // A map from description to index in the completion list of the element with that description
    // The indexes are stored +1
    std::map<wcstring, size_t> desc_table;

    // note that we mutate the completion list as we go, so the size changes
    for (size_t i=0; i < comps->size(); i++)
    {
        const comp_t &new_comp = comps->at(i);
        const wcstring &desc = new_comp.desc;
        if (desc.empty())
            continue;
        
        // See if it's in the table
        size_t prev_idx_plus_one = desc_table[desc];
        if (prev_idx_plus_one == 0)
        {
            // We're the first with this description
            desc_table[desc] = i+1;
        }
        else
        {
            // There's a prior completion with this description. Append the new ones to it.
            comp_t *prior_comp = &comps->at(prev_idx_plus_one - 1);
            prior_comp->comp.insert(prior_comp->comp.end(), new_comp.comp.begin(), new_comp.comp.end());
            
            // Erase the element at this index, and decrement the index to reflect that fact
            comps->erase(comps->begin() + i);
            i -= 1;
        }
    }
}

/** Generate a list of comp_t structures from a list of completions */
static comp_info_list_t process_completions_into_infos(const completion_list_t &lst, const wcstring &prefix)
{
    const size_t lst_size = lst.size();
    
    // Make the list of the correct size up-front
    comp_info_list_t result(lst_size);
    for (size_t i=0; i<lst_size; i++)
    {
        const completion_t &comp = lst.at(i);
        comp_t *comp_info = &result.at(i);
        
        // Append the single completion string. We may later merge these into multiple.
        comp_info->comp.push_back(escape_string(comp.completion, ESCAPE_ALL | ESCAPE_NO_QUOTED));
        
        // Append the mangled description
        comp_info->desc = comp.description;
        mangle_1_completion_description(&comp_info->desc);
    }
    return result;
}

void pager_t::measure_completion_infos(comp_info_list_t *infos, const wcstring &prefix) const
{
    size_t prefix_len = my_wcswidth(prefix.c_str());
    for (size_t i=0; i < infos->size(); i++)
    {
        comp_t *comp = &infos->at(i);
        
        // Compute comp_width
        const wcstring_list_t &comp_strings = comp->comp;
        for (size_t j=0; j < comp_strings.size(); j++)
        {
            // If there's more than one, append the length of ', '
            if (j >= 1)
                comp->comp_width += 2;
            
            comp->comp_width += prefix_len + my_wcswidth(comp_strings.at(j).c_str());
        }
        
        // Compute desc_width
        comp->desc_width = my_wcswidth(comp->desc.c_str());
        
        // Compute preferred width
        comp->pref_width = comp->comp_width + comp->desc_width + (comp->desc_width?4:0);
    }
    
    recalc_min_widths(infos);
}

/**
   The callback function that the keyboard reading function calls when
   an interrupt occurs. This makes sure that R_NULL is returned at
   once when an interrupt has occured.
*/
static int interrupt_handler()
{
    return R_NULL;
}

#if 0
page_rendering_t render_completions(const completion_list_t &raw_completions, const wcstring &prefix)
{
    // Save old output function so we can restore it
    int (* const saved_writer_func)(char) = output_get_writer();
    output_set_writer(&pager_buffered_writer);

    // Get completion infos out of it
    comp_info_list_t completion_infos = process_completions_into_infos(raw_completions, prefix.c_str());
 
    // Maybe join them
    if (prefix == L"-")
        join_completions(&completion_infos);
    
    // Compute their various widths
    measure_completion_infos(&completion_infos, prefix);
    
    /**
       Try to print the completions. Start by trying to print the
       list in PAGER_MAX_COLS columns, if the completions won't
       fit, reduce the number of columns by one. Printing a single
       column never fails.
    */
    for (int i = PAGER_MAX_COLS; i>0; i--)
    {
        switch (completion_try_print(i, prefix, completion_infos))
        {

            case PAGER_RETRY:
                break;

            case PAGER_DONE:
                i=0;
                break;

            case PAGER_RESIZE:
                /*
                  This means we got a resize event, so we start
                  over from the beginning. Since it the screen got
                  bigger, we might be able to fit all completions
                  on-screen.
                */
                i=PAGER_MAX_COLS+1;
                break;

        }
    }

    fwprintf(out_file, L"%ls", out_buff.c_str());
    
    // Restore saved writer function
    pager_buffer.clear();
    output_set_writer(saved_writer_func);
}
#endif

void pager_t::set_completions(const completion_list_t &raw_completions)
{
    completions = raw_completions;
    
    // Get completion infos out of it
    completion_infos = process_completions_into_infos(raw_completions, prefix.c_str());
 
    // Maybe join them
    if (prefix == L"-")
        join_completions(&completion_infos);
    
    // Compute their various widths
    measure_completion_infos(&completion_infos, prefix);
}

void pager_t::set_term_size(int w, int h)
{
    assert(w > 0);
    assert(h > 0);
    term_width = w;
    term_height = h;
}

/**
   Try to print the list of completions l with the prefix prefix using
   cols as the number of columns. Return 1 if the completion list was
   printed, 0 if the terminal is to narrow for the specified number of
   columns. Always succeeds if cols is 1.

   If all the elements do not fit on the screen at once, make the list
   scrollable using the up, down and space keys to move. The list will
   exit when any other key is pressed.

   \param cols the number of columns to try to fit onto the screen
   \param prefix the character string to prefix each completion with
   \param l the list of completions

   \return one of PAGER_RETRY, PAGER_DONE and PAGER_RESIZE
*/

int pager_t::completion_try_print(int cols, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const
{
    /*
      The calculated preferred width of each column
    */
    int pref_width[PAGER_MAX_COLS] = {0};
    /*
      The calculated minimum width of each column
    */
    int min_width[PAGER_MAX_COLS] = {0};
    /*
      If the list can be printed with this width, width will contain the width of each column
    */
    int *width=pref_width;
    /*
      Set to one if the list should be printed at this width
    */
    int print=0;

    int rows = (int)((lst.size()-1)/cols+1);

    int pref_tot_width=0;
    int min_tot_width = 0;
    int res=PAGER_RETRY;
    
    /* Skip completions on tiny terminals */
    if (term_width < PAGER_MIN_WIDTH)
        return PAGER_DONE;

    /* Calculate how wide the list would be */
    for (long j = 0; j < cols; j++)
    {
        for (long i = 0; i<rows; i++)
        {
            int pref,min;
            const comp_t *c;
            if (lst.size() <= j*rows + i)
                continue;

            c = &lst.at(j*rows + i);
            pref = c->pref_width;
            min = c->min_width;

            if (j != cols-1)
            {
                pref += 2;
                min += 2;
            }
            min_width[j] = maxi(min_width[j],
                                min);
            pref_width[j] = maxi(pref_width[j],
                                 pref);
        }
        min_tot_width += min_width[j];
        pref_tot_width += pref_width[j];
    }
    /*
      Force fit if one column
    */
    if (cols == 1)
    {
        if (pref_tot_width > term_width)
        {
            pref_width[0] = term_width;
        }
        width = pref_width;
        print=1;
    }
    else if (pref_tot_width <= term_width)
    {
        /* Terminal is wide enough. Print the list! */
        width = pref_width;
        print=1;
    }
    else
    {
        long next_rows = (lst.size()-1)/(cols-1)+1;
        /*    fwprintf( stderr,
          L"cols %d, min_tot %d, term %d, rows=%d, nextrows %d, termrows %d, diff %d\n",
          cols,
          min_tot_width, term_width,
          rows, next_rows, term_height,
          pref_tot_width-term_width );
        */
        if (min_tot_width < term_width &&
                (((rows < term_height) && (next_rows >= term_height)) ||
                 (pref_tot_width-term_width< 4 && cols < 3)))
        {
            /*
              Terminal almost wide enough, or squeezing makes the
              whole list fit on-screen.

              This part of the code is really important. People hate
              having to scroll through the completion list. In cases
              where there are a huge number of completions, it can't
              be helped, but it is not uncommon for the completions to
              _almost_ fit on one screen. In those cases, it is almost
              always desirable to 'squeeze' the completions into a
              single page.

              If we are using N columns and can get everything to
              fit using squeezing, but everything would also fit
              using N-1 columns, don't try.
            */

            int tot_width = min_tot_width;
            width = min_width;

            while (tot_width < term_width)
            {
                for (long i=0; (i<cols) && (tot_width < term_width); i++)
                {
                    if (width[i] < pref_width[i])
                    {
                        width[i]++;
                        tot_width++;
                    }
                }
            }
            print=1;
        }
    }

    if (print)
    {
        res=PAGER_DONE;
        if (rows < term_height)
        {
            completion_print(cols, width, 0, rows, prefix, lst, rendering);
        }
        else
        {
            assert(0);
            int npos, pos = 0;
            int do_loop = 1;

            completion_print(cols, width, 0, term_height-1, prefix, lst, rendering);
            
            /* List does not fit on screen. Print one screenful and leave a scrollable interface */
            while (do_loop)
            {
                set_color(rgb_color_t::black(), get_color(HIGHLIGHT_PAGER_PROGRESS));
                wcstring msg = format_string(_(L" %d to %d of %d"), pos, pos+term_height-1, rows);
                msg.append(L"   \r");

                writestr(msg.c_str());
                set_color(rgb_color_t::normal(), rgb_color_t::normal());
                int c = readch();

                switch (c)
                {
                    case LINE_UP:
                    {
                        if (pos > 0)
                        {
                            pos--;
                            writembs(tparm(cursor_address, 0, 0));
                            writembs(scroll_reverse);
                            completion_print(cols, width, pos, pos+1, prefix, lst, rendering);
                            writembs(tparm(cursor_address, term_height-1, 0));
                            writembs(clr_eol);

                        }

                        break;
                    }

                    case LINE_DOWN:
                    {
                        if (pos <= (rows - term_height))
                        {
                            pos++;
                            completion_print(cols, width, pos+term_height-2, pos+term_height-1, prefix, lst, rendering);
                        }
                        break;
                    }

                    case PAGE_DOWN:
                    {

                        npos = mini((int)(rows - term_height+1), (int)(pos + term_height-1));
                        if (npos != pos)
                        {
                            pos = npos;
                            completion_print(cols, width, pos, pos+term_height-1, prefix, lst, rendering);
                        }
                        else
                        {
                            if (flash_screen)
                                writembs(flash_screen);
                        }

                        break;
                    }

                    case PAGE_UP:
                    {
                        npos = maxi(0, pos - term_height+1);

                        if (npos != pos)
                        {
                            pos = npos;
                            completion_print(cols, width, pos, pos+term_height-1, prefix, lst, rendering);
                        }
                        else
                        {
                            if (flash_screen)
                                writembs(flash_screen);
                        }
                        break;
                    }

                    case R_NULL:
                    {
                        do_loop=0;
                        res=PAGER_RESIZE;
                        break;

                    }

                    default:
                    {
                        out_buff.push_back(c);
                        do_loop = 0;
                        break;
                    }
                }
            }
            writembs(clr_eol);
        }
    }
    return res;
}


page_rendering_t pager_t::render() const
{
    /**
       Try to print the completions. Start by trying to print the
       list in PAGER_MAX_COLS columns, if the completions won't
       fit, reduce the number of columns by one. Printing a single
       column never fails.
    */
    page_rendering_t rendering;
    for (int i = PAGER_MAX_COLS; i>0; i--)
    {
        /* Initially empty rendering */
        rendering.screen_data.resize(0);
        
        switch (completion_try_print(i, prefix, completion_infos, &rendering))
        {
            case PAGER_RETRY:
                break;

            case PAGER_DONE:
                i=0;
                break;

            case PAGER_RESIZE:
                /*
                  This means we got a resize event, so we start
                  over from the beginning. Since it the screen got
                  bigger, we might be able to fit all completions
                  on-screen.
                */
                i=PAGER_MAX_COLS+1;
                break;
        }
    }
    return rendering;
}
