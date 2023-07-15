// Pager support.
#ifndef FISH_PAGER_H
#define FISH_PAGER_H

#include <stddef.h>

#include <string>
#include <vector>

#include "common.h"
#include "complete.h"
#include "cxx.h"
#include "highlight.h"
#include "reader.h"
#include "screen.h"

struct termsize_t;

#define PAGER_SELECTION_NONE static_cast<size_t>(-1)

/// Represents rendering from the pager.
class page_rendering_t {
   public:
    size_t term_width{size_t(-1)};
    size_t term_height{size_t(-1)};
    size_t rows{0};
    size_t cols{0};
    size_t row_start{0};
    size_t row_end{0};
    size_t selected_completion_idx{size_t(-1)};
    screen_data_t screen_data{};

    size_t remaining_to_disclose{0};

    bool search_field_shown{false};
    editable_line_t search_field_line{};

    // Returns a rendering with invalid data, useful to indicate "no rendering".
    page_rendering_t();
};

enum class selection_motion_t {
    // Visual directions.
    north,
    east,
    south,
    west,
    page_north,
    page_south,

    // Logical directions.
    next,
    prev,

    // Special value that means deselect.
    deselect
};

// The space between adjacent completions.
#define PAGER_SPACER_STRING L"  "
#define PAGER_SPACER_STRING_WIDTH 2

// How many rows we will show in the "initial" pager.
#define PAGER_UNDISCLOSED_MAX_ROWS 4

class pager_t {
    size_t available_term_width{0};
    size_t available_term_height{0};

    size_t selected_completion_idx{PAGER_SELECTION_NONE};
    size_t suggested_row_start{0};

    // Fully disclosed means that we show all completions.
    bool fully_disclosed{false};

    // Whether we show the search field.
    bool search_field_shown{false};

    // Returns the index of the completion that should draw selected, using the given number of
    // columns.
    size_t visual_selected_completion_index(size_t rows, size_t cols) const;

   public:
    /// Data structure describing one or a group of related completions.
    struct comp_t {
        /// The list of all completion strings this entry applies to.
        std::vector<wcstring> comp{};
        /// The description.
        wcstring desc{};
        /// The representative completion.
        rust::Box<completion_t> representative = new_completion();
        /// The per-character highlighting, used when this is a full shell command.
        std::vector<highlight_spec_t> colors{};
        /// On-screen width of the completion string.
        size_t comp_width{0};
        /// On-screen width of the description information.
        size_t desc_width{0};

        comp_t() = default;
        comp_t(const comp_t &other);
        comp_t &operator=(const comp_t &other);
        comp_t(comp_t &&) = default;
        comp_t &operator=(comp_t &&) = default;

        // Our text looks like this:
        // completion  (description)
        // Two spaces separating, plus parens, yields 4 total extra space
        // but only if we have a description of course
        size_t description_punctuated_width() const {
            return this->desc_width + (this->desc_width ? 4 : 0);
        }

        // Returns the preferred width, containing the sum of the
        // width of the completion, separator, description
        size_t preferred_width() const {
            return this->comp_width + this->description_punctuated_width();
        }
    };

   private:
    using comp_info_list_t = std::vector<comp_t>;

    // The filtered list of completion infos.
    comp_info_list_t completion_infos;

    // The unfiltered list. Note there's a lot of duplication here.
    comp_info_list_t unfiltered_completion_infos;

    // This tracks if the completion list has been changed since we last rendered. If yes,
    // then we definitely need to re-render.
    bool have_unrendered_completions = false;

    wcstring prefix;
    bool highlight_prefix = false;

    bool completion_try_print(size_t cols, const wcstring &prefix, const comp_info_list_t &lst,
                              page_rendering_t *rendering, size_t suggested_start_row) const;

    void recalc_min_widths(comp_info_list_t *lst) const;
    void measure_completion_infos(std::vector<comp_t> *infos, const wcstring &prefix) const;

    bool completion_info_passes_filter(const comp_t &info) const;

    void completion_print(size_t cols, const size_t *width_by_column, size_t row_start,
                          size_t row_stop, const wcstring &prefix, const comp_info_list_t &lst,
                          page_rendering_t *rendering) const;
    line_t completion_print_item(const wcstring &prefix, const comp_t *c, size_t row, size_t column,
                                 size_t width, bool secondary, bool selected,
                                 page_rendering_t *rendering) const;

   public:
    // The text of the search field.
    editable_line_t search_field_line;

    // Extra text to display at the bottom of the pager.
    wcstring extra_progress_text{};

    // Sets the set of completions.
    void set_completions(const completion_list_t &raw_completions);

    // Sets the prefix.
    void set_prefix(const wcstring &pref, bool highlight = true);

    // Sets the terminal size.
    void set_term_size(termsize_t ts);

    // Changes the selected completion in the given direction according to the layout of the given
    // rendering. Returns true if the selection changed.
    bool select_next_completion_in_direction(selection_motion_t direction,
                                             const page_rendering_t &rendering);

    // Returns the currently selected completion for the given rendering.
    const completion_t *selected_completion(const page_rendering_t &rendering) const;

    size_t selected_completion_index() const;
    void set_selected_completion_index(size_t new_index);

    // Indicates the row and column for the given rendering. Returns -1 if no selection.
    size_t get_selected_row(const page_rendering_t &rendering) const;
    size_t get_selected_column(const page_rendering_t &rendering) const;
    // Indicates the row assuming we render this many rows. Returns -1 if no selection.
    size_t get_selected_row(size_t rows) const;

    // Produces a rendering of the completions, at the given term size.
    page_rendering_t render() const;

    // \return true if the given rendering needs to be updated.
    bool rendering_needs_update(const page_rendering_t &rendering) const;

    // Updates the rendering.
    void update_rendering(page_rendering_t *rendering);

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
    void set_fully_disclosed();

    // Position of the cursor.
    size_t cursor_position() const;

    pager_t();
    ~pager_t();
};

#endif
