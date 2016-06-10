// Pager support.
#ifndef FISH_PAGER_H
#define FISH_PAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "complete.h"
#include "reader.h"
#include "screen.h"

#define PAGER_SELECTION_NONE ((size_t)(-1))

/// Represents rendering from the pager.
class page_rendering_t {
   public:
    int term_width;
    int term_height;
    size_t rows;
    size_t cols;
    size_t row_start;
    size_t row_end;
    size_t selected_completion_idx;
    screen_data_t screen_data;

    size_t remaining_to_disclose;

    bool search_field_shown;
    editable_line_t search_field_line;

    // Returns a rendering with invalid data, useful to indicate "no rendering".
    page_rendering_t();
};

// The space between adjacent completions.
#define PAGER_SPACER_STRING L"  "
#define PAGER_SPACER_STRING_WIDTH 2

// How many rows we will show in the "initial" pager.
#define PAGER_UNDISCLOSED_MAX_ROWS 4

typedef std::vector<completion_t> completion_list_t;
page_rendering_t render_completions(const completion_list_t &raw_completions,
                                    const wcstring &prefix);

class pager_t {
    int available_term_width;
    int available_term_height;

    size_t selected_completion_idx;
    size_t suggested_row_start;

    // Fully disclosed means that we show all completions.
    bool fully_disclosed;

    // Whether we show the search field.
    bool search_field_shown;

    // Returns the index of the completion that should draw selected, using the given number of
    // columns.
    size_t visual_selected_completion_index(size_t rows, size_t cols) const;

   public:
    /// Data structure describing one or a group of related completions.
    struct comp_t {
        /// The list of all completin strings this entry applies to.
        wcstring_list_t comp;
        /// The description.
        wcstring desc;
        /// The representative completion.
        completion_t representative;
        /// On-screen width of the completion string.
        int comp_width;
        /// On-screen width of the description information.
        int desc_width;
        /// Preferred total width.
        int pref_width;
        /// Minimum acceptable width.
        int min_width;

        comp_t()
            : comp(),
              desc(),
              representative(L""),
              comp_width(0),
              desc_width(0),
              pref_width(0),
              min_width(0) {}
    };

   private:
    typedef std::vector<comp_t> comp_info_list_t;

    // The filtered list of completion infos.
    comp_info_list_t completion_infos;

    // The unfiltered list. Note there's a lot of duplication here.
    comp_info_list_t unfiltered_completion_infos;

    wcstring prefix;

    bool completion_try_print(size_t cols, const wcstring &prefix, const comp_info_list_t &lst,
                              page_rendering_t *rendering, size_t suggested_start_row) const;

    void recalc_min_widths(comp_info_list_t *lst) const;
    void measure_completion_infos(std::vector<comp_t> *infos, const wcstring &prefix) const;

    bool completion_info_passes_filter(const comp_t &info) const;

    void completion_print(size_t cols, int *width_per_column, size_t row_start, size_t row_stop,
                          const wcstring &prefix, const comp_info_list_t &lst,
                          page_rendering_t *rendering) const;
    line_t completion_print_item(const wcstring &prefix, const comp_t *c, size_t row, size_t column,
                                 int width, bool secondary, bool selected,
                                 page_rendering_t *rendering) const;

   public:
    // The text of the search field.
    editable_line_t search_field_line;

    // Sets the set of completions.
    void set_completions(const completion_list_t &comp);

    // Sets the prefix.
    void set_prefix(const wcstring &pref);

    // Sets the terminal width and height.
    void set_term_size(int w, int h);

    // Changes the selected completion in the given direction according to the layout of the given
    // rendering. Returns true if the selection changed.
    bool select_next_completion_in_direction(selection_direction_t direction,
                                             const page_rendering_t &rendering);

    // Returns the currently selected completion for the given rendering.
    const completion_t *selected_completion(const page_rendering_t &rendering) const;

    // Indicates the row and column for the given rendering. Returns -1 if no selection.
    size_t get_selected_row(const page_rendering_t &rendering) const;
    size_t get_selected_column(const page_rendering_t &rendering) const;

    // Produces a rendering of the completions, at the given term size.
    page_rendering_t render() const;

    // Updates the rendering if it's stale.
    void update_rendering(page_rendering_t *rendering) const;

    // Indicates if there are no completions, and therefore nothing to render.
    bool empty() const;

    // Clears all completions and the prefix.
    void clear();

    // Updates the completions list per the filter.
    void refilter_completions();

    // Sets whether the search field is shown.
    void set_search_field_shown(bool flag);

    // Gets whether the search field shown.
    bool is_search_field_shown() const;

    // Indicates if we are navigating our contents.
    bool is_navigating_contents() const;

    // Become fully disclosed.
    void set_fully_disclosed(bool flag);

    // Position of the cursor.
    size_t cursor_position() const;

    // Constructor
    pager_t();
};

#endif
