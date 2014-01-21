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
#include "highlight.h"

#define PAGER_SELECTION_NONE ((size_t)(-1))

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
   This string contains the text that should be sent back to the calling program
*/
static wcstring out_buff;

/* Returns numer / denom, rounding up */
static size_t divide_round_up(size_t numer, size_t denom)
{
    return numer / denom + (numer % denom ? 1 : 0);
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
   Print the specified string, but use at most the specified amount of
   space. If the whole string can't be fitted, ellipsize it.

   \param str the string to print
   \param color the color to apply to every printed character
   \param max the maximum space that may be used for printing
   \param has_more if this flag is true, this is not the entire string, and the string should be ellisiszed even if the string fits but takes up the whole space.
*/

static int print_max(const wcstring &str, highlight_spec_t color, int max, bool has_more, line_t *line)
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
line_t pager_t::completion_print_item(const wcstring &prefix, const comp_t *c, size_t row, size_t column, int width, bool secondary, bool selected, page_rendering_t *rendering) const
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
    
    int bg_color = secondary ? highlight_spec_pager_secondary : highlight_spec_normal;
    if (selected)
    {
        bg_color = highlight_spec_search_match;
    }
    
    for (size_t i=0; i<c->comp.size(); i++)
    {
        const wcstring &comp = c->comp.at(i);

        if (i != 0)
            written += print_max(PAGER_SPACER_STRING, highlight_spec_normal, comp_width - written, true /* has_more */, &line_data);
        
        int packed_color = highlight_spec_pager_prefix | highlight_make_background(bg_color);
        written += print_max(prefix, packed_color, comp_width - written, ! comp.empty(), &line_data);
        
        packed_color = highlight_spec_pager_completion | highlight_make_background(bg_color);
        written += print_max(comp, packed_color, comp_width - written, i + 1 < c->comp.size(), &line_data);
    }

    if (desc_width)
    {
        int packed_color = highlight_spec_pager_description | highlight_make_background(bg_color);
        while (written < (width-desc_width-2)) //the 2 here refers to the parenthesis below
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

void pager_t::completion_print(size_t cols, int *width_per_column, size_t row_start, size_t row_stop, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const
{
    /* Teach the rendering about the rows it printed */
    assert(row_start >= 0);
    assert(row_stop >= row_start);
    rendering->row_start = row_start;
    rendering->row_end = row_stop;

    size_t rows = (lst.size()-1)/cols+1;
    
    size_t effective_selected_idx = this->visual_selected_completion_index(rows, cols);
    
    for (size_t row = row_start; row < row_stop; row++)
    {
        for (size_t col = 0; col < cols; col++)
        {
            int is_last = (col==(cols-1));

            if (lst.size() <= col * rows + row)
                continue;

            size_t idx = col * rows + row;
            const comp_t *el = &lst.at(idx);
            bool is_selected = (idx == effective_selected_idx);

            /* Print this completion on its own "line" */
            line_t line = completion_print_item(prefix, el, row, col, width_per_column[col] - (is_last ? 0 : PAGER_SPACER_STRING_WIDTH), row%2, is_selected, rendering);
            
            /* If there's more to come, append two spaces */
            if (col + 1 < cols)
            {
                line.append(PAGER_SPACER_STRING, 0);
            }
            
            /* Append this to the real line */
            rendering->screen_data.create_line(row - row_start).append_line(line);
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

void pager_t::set_prefix(const wcstring &pref)
{
    prefix = pref;
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

int pager_t::completion_try_print(size_t cols, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering, size_t suggested_start_row) const
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

    size_t row_count = divide_round_up(lst.size(), cols);

    int pref_tot_width=0;
    int min_tot_width = 0;
    int res=PAGER_RETRY;
    
    /* Skip completions on tiny terminals */
    if (term_width < PAGER_MIN_WIDTH)
        return PAGER_DONE;

    /* Calculate how wide the list would be */
    for (long col = 0; col < cols; col++)
    {
        for (long row = 0; row<row_count; row++)
        {
            int pref,min;
            const comp_t *c;
            if (lst.size() <= col*row_count + row)
                continue;

            c = &lst.at(col*row_count + row);
            pref = c->pref_width;
            min = c->min_width;

            if (col != cols-1)
            {
                pref += 2;
                min += 2;
            }
            min_width[col] = maxi(min_width[col],
                                min);
            pref_width[col] = maxi(pref_width[col],
                                 pref);
        }
        min_tot_width += min_width[col];
        pref_tot_width += pref_width[col];
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
                (((row_count < term_height) && (next_rows >= term_height)) ||
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
        /* Determine the starting and stop row */
        size_t start_row = 0, stop_row = 0;
        if (row_count <= term_height)
        {
            /* Easy, we can show everything */
            start_row = 0;
            stop_row = row_count;
        }
        else
        {
            /* We can only show part of the full list. Determine which part based on the suggested_start_row */
            assert(row_count > term_height);
            size_t last_starting_row = row_count - term_height;
            start_row = mini(suggested_start_row, last_starting_row);
            stop_row = start_row + term_height;
            assert(start_row >= 0 && start_row <= last_starting_row);
        }
        
        assert(stop_row >= start_row);
        assert(stop_row <= row_count);
        assert(stop_row - start_row <= term_height);
        completion_print(cols, width, start_row, stop_row, prefix, lst, rendering);
        return PAGER_DONE;
        
        res=PAGER_DONE;
        if (row_count < term_height)
        {
            completion_print(cols, width, 0, row_count, prefix, lst, rendering);
        }
        else
        {
            completion_print(cols, width, 0, term_height - 1, prefix, lst, rendering);
            
            assert(0);
            int npos, pos = 0;
            int do_loop = 1;

            completion_print(cols, width, 0, term_height-1, prefix, lst, rendering);
            
            /* List does not fit on screen. Print one screenful and leave a scrollable interface */
            while (do_loop)
            {
                set_color(rgb_color_t::black(), highlight_get_color(highlight_spec_pager_progress, true));
                wcstring msg = format_string(_(L" %d to %d of %d"), pos, pos+term_height-1, row_count);
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
                        if (pos <= (row_count - term_height))
                        {
                            pos++;
                            completion_print(cols, width, pos+term_height-2, pos+term_height-1, prefix, lst, rendering);
                        }
                        break;
                    }

                    case PAGE_DOWN:
                    {

                        npos = mini((int)(row_count - term_height+1), (int)(pos + term_height-1));
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
    rendering.term_width = this->term_width;
    rendering.term_height = this->term_height;
    
    if (! this->empty())
    {
        int cols;
        for (cols = PAGER_MAX_COLS; cols > 0; cols--)
        {
            /* Initially empty rendering */
            rendering.screen_data.resize(0);
            
            /* Determine how many rows we would need if we had 'cols' columns. Then determine how many columns we want from that. For example, say we had 19 completions. We can fit them into 6 columns, 4 rows, with the last row containing only 1 entry. Or we can fit them into 5 columns, 4 rows, the last row containing 4 entries. Since fewer columns with the same number of rows is better, skip cases where we know we can do better. */
            size_t min_rows_required_for_cols = divide_round_up(completion_infos.size(), cols);
            size_t min_cols_required_for_rows = divide_round_up(completion_infos.size(), min_rows_required_for_cols);
            
            assert(min_cols_required_for_rows <= cols);
            if (min_cols_required_for_rows < cols)
            {
                /* Next iteration will be better, so skip this one */
                continue;
            }

            
            rendering.cols = (size_t)cols;
            rendering.rows = divide_round_up(completion_infos.size(), rendering.cols);
            rendering.selected_completion_idx = this->visual_selected_completion_index(rendering.rows, rendering.cols);
            
            bool done = false;
            switch (completion_try_print(cols, prefix, completion_infos, &rendering, suggested_row_start))
            {
                case PAGER_RETRY:
                    break;

                case PAGER_DONE:
                    done = true;
                    break;

                case PAGER_RESIZE:
                    /*
                      This means we got a resize event, so we start
                      over from the beginning. Since it the screen got
                      bigger, we might be able to fit all completions
                      on-screen.
                    */
                    cols=PAGER_MAX_COLS+1;
                    break;
            }
            if (done)
                break;
        }
        assert(cols >= 0);
    }
    return rendering;
}

void pager_t::update_rendering(page_rendering_t *rendering) const
{
    if (rendering->term_width != this->term_width || rendering->term_height != this->term_height || rendering->selected_completion_idx != this->visual_selected_completion_index(rendering->rows, rendering->cols))
    {
        *rendering = this->render();
    }
}

pager_t::pager_t() : term_width(0), term_height(0), selected_completion_idx(PAGER_SELECTION_NONE), suggested_row_start(0)
{
}

bool pager_t::empty() const
{
    return completions.empty();
}

const completion_t *pager_t::select_next_completion_in_direction(selection_direction_t direction, const page_rendering_t &rendering)
{
    /* Must have something to select */
    if (completions.empty())
    {
        return NULL;
    }
    
    /* Handle the case of nothing selected yet */
    if (selected_completion_idx == PAGER_SELECTION_NONE)
    {
        if (selection_direction_is_cardinal(direction))
        {
            /* Cardinal directions do nothing unless something is selected */
            return NULL;
        }
        else
        {
            /* Forward/backward do something sane */
            selected_completion_idx = (direction == direction_next ? 0 : completions.size() - 1);
            return selected_completion(rendering);
        }
    }
    
    /* Ok, we had something selected already. Select something different. */
    size_t new_selected_completion_idx = selected_completion_idx;
    if (! selection_direction_is_cardinal(direction))
    {
        /* Next / previous, easy */
        if (direction == direction_next)
        {
            new_selected_completion_idx = selected_completion_idx + 1;
            if (new_selected_completion_idx >= completion_infos.size())
            {
                new_selected_completion_idx = 0;
            }
        }
        else if (direction == direction_prev)
        {
            if (selected_completion_idx == 0)
            {
                new_selected_completion_idx = completion_infos.size() - 1;
            }
            else
            {
                new_selected_completion_idx = selected_completion_idx - 1;
            }
        }
        else
        {
            assert(0 && "Unknown non-cardinal direction");
        }
    }
    else
    {
        /* Cardinal directions. We have a completion index; we wish to compute its row and column. Completions are rendered column first, i.e. we go south before we go west. So if we have N rows, and our selected index is N + 2, then our row is 2 (mod by N) and our column is 1 (divide by N) */
        size_t current_row = selected_completion_idx % rendering.rows;
        size_t current_col = selected_completion_idx / rendering.rows;
        
        switch (direction)
        {
            case direction_north:
            {
                /* Go up a whole row. If we cycle, go to the previous column. */
                if (current_row > 0)
                {
                    current_row--;
                }
                else
                {
                    current_row = rendering.rows - 1;
                    if (current_col > 0)
                        current_col--;
                }
                break;
            }
                
            case direction_south:
            {
                /* Go down, unless we are in the last row. Note that this means that we may set selected_completion_idx to an out-of-bounds value if the last row is incomplete; this is a feature (it allows "last column memory"). */
                if (current_row + 1 < rendering.rows)
                {
                    current_row++;
                }
                else
                {
                    current_row = 0;
                    if (current_col + 1 < rendering.cols)
                        current_col++;
                    
                }
                break;
            }
            
            case direction_east:
            {
                /* Go east, wrapping to the next row. There is no "row memory," so if we run off the end, wrap. */
                if (current_col + 1 < rendering.cols && (current_col + 1) * rendering.rows + current_row < completion_infos.size())
                {
                    current_col++;
                }
                else
                {
                    current_col = 0;
                    if (current_row + 1 < rendering.rows)
                        current_row++;
                }
                break;
            }
            
            case direction_west:
            {
                /* Go west, wrapping to the previous row */
                if (current_col > 0)
                {
                    current_col--;
                }
                else
                {
                    current_col = rendering.cols - 1;
                    if (current_row > 0)
                        current_row--;
                }
                break;
            }
            
            default:
                assert(0 && "Unknown cardinal direction");
                break;
        }
        
        /* Compute the new index based on the changed row */
        new_selected_completion_idx = current_col * rendering.rows + current_row;
    }
    
    if (new_selected_completion_idx != selected_completion_idx)
    {
        selected_completion_idx = new_selected_completion_idx;
        
        /* Update suggested_row_start to ensure the selection is visible. suggested_row_start * rendering.cols is the first suggested visible completion; add the visible completion count to that to get the last one */
        size_t visible_row_count = rendering.row_end - rendering.row_start;
        
        if (visible_row_count > 0 && selected_completion_idx != PAGER_SELECTION_NONE) //paranoia
        {
            size_t row_containing_selection = selected_completion_idx % rendering.rows;
            
            /* Ensure our suggested row start is not past the selected row */
            if (suggested_row_start > row_containing_selection)
            {
                suggested_row_start = row_containing_selection;
            }
            
            /* Ensure our suggested row start is not too early before it */
            if (suggested_row_start + visible_row_count <= row_containing_selection)
            {
                suggested_row_start = row_containing_selection - visible_row_count + 1;
            }
        }
        
        
        return selected_completion(rendering);
    }
    else
    {
        return NULL;
    }
}

size_t pager_t::visual_selected_completion_index(size_t rows, size_t cols) const
{
    size_t result = selected_completion_idx;
    if (result != PAGER_SELECTION_NONE)
    {
        /* If the selected completion is beyond the last selection, go left by columns until it's within it. This is how we implement "column memory." */
        while (result >= completions.size() && result >= rows)
        {
            result -= rows;
        }
    }
    assert(result == PAGER_SELECTION_NONE || result < completions.size());
    return result;
}

void pager_t::set_selected_completion_index(size_t completion_idx)
{
    selected_completion_idx = completion_idx;
}

const completion_t *pager_t::selected_completion(const page_rendering_t &rendering) const
{
    const completion_t * result = NULL;
    size_t idx = visual_selected_completion_index(rendering.rows, rendering.cols);
    if (idx != PAGER_SELECTION_NONE)
    {
        result = &completions.at(idx);
    }
    return result;
}

void pager_t::clear()
{
    completions.clear();
    completion_infos.clear();
    prefix.clear();
    selected_completion_idx = PAGER_SELECTION_NONE;
}

page_rendering_t::page_rendering_t() : term_width(-1), term_height(-1), rows(0), cols(0), row_start(0), row_end(0), selected_completion_idx(-1)
{
}
