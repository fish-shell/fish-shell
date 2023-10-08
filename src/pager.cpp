#include "config.h"  // IWYU pragma: keep

#include "pager.h"

#include <stddef.h>
#include <wctype.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <functional>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "complete.h"
#include "fallback.h"
#include "highlight.h"
#include "maybe.h"
#include "operation_context.h"
#include "reader.h"
#include "screen.h"
#include "termsize.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

using comp_t = pager_t::comp_t;
using comp_info_list_t = std::vector<comp_t>;

comp_t &comp_t::operator=(const comp_t &other) {
    if (this == &other) return *this;
    comp = other.comp;
    desc = other.desc;
    representative = other.representative->clone();
    colors = other.colors;
    comp_width = other.comp_width;
    desc_width = other.desc_width;
    return *this;
}

comp_t::comp_t(const comp_t &other) { *this = other; }

/// The minimum width (in characters) the terminal must to show completions at all.
#define PAGER_MIN_WIDTH 16

/// Minimum height to show completions
#define PAGER_MIN_HEIGHT 4

/// The maximum number of columns of completion to attempt to fit onto the screen.
#define PAGER_MAX_COLS 6

/// Width of the search field.
#define PAGER_SEARCH_FIELD_WIDTH 12

/// Text we use for the search field.
#define SEARCH_FIELD_PROMPT _(L"search: ")

static inline bool selection_direction_is_cardinal(selection_motion_t dir) {
    switch (dir) {
        case selection_motion_t::north:
        case selection_motion_t::east:
        case selection_motion_t::south:
        case selection_motion_t::west:
        case selection_motion_t::page_north:
        case selection_motion_t::page_south: {
            return true;
        }
        case selection_motion_t::next:
        case selection_motion_t::prev:
        case selection_motion_t::deselect: {
            return false;
        }
    }

    DIE("unreachable");
}

/// Returns numer / denom, rounding up. As a "courtesy" 0/0 is 0.
static size_t divide_round_up(size_t numer, size_t denom) {
    if (numer == 0) return 0;
    assert(denom > 0);
    bool has_rem = (numer % denom) != 0;
    return numer / denom + (has_rem ? 1 : 0);
}

/// Print the specified string, but use at most the specified amount of space. If the whole string
/// can't be fitted, ellipsize it.
///
/// \param str the string to print
/// \param color the color to apply to every printed character
/// \param max the maximum space that may be used for printing
/// \param has_more if this flag is true, this is not the entire string, and the string should be
/// ellipsized even if the string fits but takes up the whole space.
template <typename Func>
static typename std::enable_if<
    std::is_convertible<Func, std::function<highlight_spec_t(size_t)>>::value, size_t>::type
print_max(const wcstring &str, Func color, size_t max, bool has_more, line_t *line) {
    size_t remaining = max;
    for (size_t i = 0; i < str.size(); i++) {
        wchar_t c = str.at(i);
        int iwidth_c = fish_wcwidth(c);
        if (iwidth_c < 0) {
            // skip non-printable characters
            continue;
        }
        auto width_c = size_t(iwidth_c);

        if (width_c > remaining) break;

        wchar_t ellipsis = get_ellipsis_char();
        if ((width_c == remaining) && (has_more || i + 1 < str.size())) {
            line->append(ellipsis, color(i));
            int ellipsis_width = fish_wcwidth(ellipsis);
            remaining -= std::min(remaining, size_t(ellipsis_width));
            break;
        }

        line->append(c, color(i));
        assert(remaining >= width_c);
        remaining -= width_c;
    }

    // return how much we consumed
    assert(remaining <= max);
    return max - remaining;
}

static size_t print_max(const wcstring &str, highlight_spec_t color, size_t max, bool has_more,
                        line_t *line) {
    return print_max(
        str, [=](size_t) -> highlight_spec_t { return color; }, max, has_more, line);
}

/// Print the specified item using at the specified amount of space.
line_t pager_t::completion_print_item(const wcstring &prefix, const comp_t *c, size_t row,
                                      size_t column, size_t width, bool secondary, bool selected,
                                      page_rendering_t *rendering) const {
    UNUSED(column);
    UNUSED(row);
    UNUSED(rendering);
    size_t comp_width;
    line_t line_data;

    if (c->preferred_width() <= width) {
        // The entry fits, we give it as much space as it wants.
        comp_width = c->comp_width;
    } else {
        // The completion and description won't fit on the allocated space. Give a maximum of 2/3 of
        // the space to the completion, and whatever is left to the description
        // This expression is an overflow-safe way of calculating (width-4)*2/3
        size_t width_minus_spacer = width - std::min(width, size_t(4));
        size_t two_thirds_width = (width_minus_spacer / 3) * 2 + ((width_minus_spacer % 3) * 2) / 3;
        comp_width = std::min(c->comp_width, two_thirds_width);

        // If the description is short, give the completion the remaining space
        size_t desc_punct_width = c->description_punctuated_width();
        if (width > desc_punct_width) {
            comp_width = std::max(comp_width, width - desc_punct_width);
        }

        // The description gets what's left
        assert(comp_width <= width);
    }

    auto modify_role = [=](highlight_role_t role) -> highlight_role_t {
        using uint_t = typename std::underlying_type<highlight_role_t>::type;
        auto base = static_cast<uint_t>(role);
        if (selected) {
            base += static_cast<uint_t>(highlight_role_t::pager_selected_background) -
                    static_cast<uint_t>(highlight_role_t::pager_background);
        } else if (secondary) {
            base += static_cast<uint_t>(highlight_role_t::pager_secondary_background) -
                    static_cast<uint_t>(highlight_role_t::pager_background);
        }
        return static_cast<highlight_role_t>(base);
    };

    highlight_role_t bg_role = modify_role(highlight_role_t::pager_background);
    highlight_spec_t bg = {highlight_role_t::normal, bg_role};
    highlight_spec_t prefix_col = {
        modify_role(highlight_prefix ? highlight_role_t::pager_prefix
                                     : highlight_role_t::pager_completion),
        bg_role};
    highlight_spec_t comp_col = {modify_role(highlight_role_t::pager_completion), bg_role};
    highlight_spec_t desc_col = {modify_role(highlight_role_t::pager_description), bg_role};

    // Print the completion part
    size_t comp_remaining = comp_width;
    for (size_t i = 0; i < c->comp.size(); i++) {
        const wcstring &comp = c->comp.at(i);

        if (i > 0) {
            comp_remaining -=
                print_max(PAGER_SPACER_STRING, bg, comp_remaining, true /* has_more */, &line_data);
        }

        comp_remaining -= print_max(prefix, prefix_col, comp_remaining, !comp.empty(), &line_data);
        comp_remaining -= print_max(
            comp,
            [&](size_t i) -> highlight_spec_t {
                if (c->colors.empty()) return comp_col;  // Not a shell command.
                if (selected) return comp_col;  // Rendered in reverse video, so avoid highlighting.
                return i < c->colors.size() ? c->colors[i] : c->colors.back();
            },
            comp_remaining, i + 1 < c->comp.size(), &line_data);
    }

    size_t desc_remaining = width - comp_width + comp_remaining;
    if (c->desc_width > 0 && desc_remaining > 4) {
        // always have at least two spaces to separate completion and description
        desc_remaining -= print_max(L"  ", bg, 2, false, &line_data);

        // right-justify the description by adding spaces
        // the 2 here refers to the parenthesis below
        while (desc_remaining > c->desc_width + 2) {
            desc_remaining -= print_max(L" ", bg, 1, false, &line_data);
        }

        assert(desc_remaining >= 2);
        highlight_spec_t paren_col = {highlight_role_t::pager_completion, bg_role};
        desc_remaining -= print_max(L"(", paren_col, 1, false, &line_data);
        desc_remaining -= print_max(c->desc, desc_col, desc_remaining - 1, false, &line_data);
        desc_remaining -= print_max(L")", paren_col, 1, false, &line_data);
    } else {
        // No description, or it won't fit. Just add spaces.
        print_max(wcstring(desc_remaining, L' '), bg, desc_remaining, false, &line_data);
    }

    return line_data;
}

/// Print the specified part of the completion list, using the specified column offsets and quoting
/// style.
///
/// \param cols number of columns to print in
/// \param width_by_column An array specifying the width of each column
/// \param row_start The first row to print
/// \param row_stop the row after the last row to print
/// \param prefix The string to print before each completion
/// \param lst The list of completions to print
void pager_t::completion_print(size_t cols, const size_t *width_by_column, size_t row_start,
                               size_t row_stop, const wcstring &prefix, const comp_info_list_t &lst,
                               page_rendering_t *rendering) const {
    // Teach the rendering about the rows it printed.
    assert(row_stop >= row_start);
    rendering->row_start = row_start;
    rendering->row_end = row_stop;

    size_t rows = divide_round_up(lst.size(), cols);

    size_t effective_selected_idx = this->visual_selected_completion_index(rows, cols);

    for (size_t row = row_start; row < row_stop; row++) {
        for (size_t col = 0; col < cols; col++) {
            if (lst.size() <= col * rows + row) continue;

            size_t idx = col * rows + row;
            const comp_t *el = &lst.at(idx);
            bool is_selected = (idx == effective_selected_idx);

            // Print this completion on its own "line".
            line_t line = completion_print_item(prefix, el, row, col, width_by_column[col], row % 2,
                                                is_selected, rendering);

            // If there's more to come, append two spaces.
            if (col + 1 < cols) {
                line.append(PAGER_SPACER_STRING, highlight_spec_t{});
            }

            // Append this to the real line.
            rendering->screen_data.create_line(row - row_start).append_line(line);
        }
    }
}

/// Trim leading and trailing whitespace, and compress other whitespace runs into a single space.
static void mangle_1_completion_description(wcstring *str) {
    size_t leading = 0, trailing = 0, len = str->size();

    // Skip leading spaces.
    for (; leading < len; leading++) {
        if (!iswspace(str->at(leading))) break;
    }

    // Compress runs of spaces to a single space.
    bool was_space = false;
    for (; leading < len; leading++) {
        wchar_t wc = str->at(leading);
        bool is_space = iswspace(wc);
        if (!is_space) {  // normal character
            str->at(trailing++) = wc;
        } else if (!was_space) {  // initial space in a run
            str->at(trailing++) = L' ';
        }  // else non-initial space in a run, do nothing
        was_space = is_space;
    }

    // leading is now at len, trailing is the new length of the string. Delete trailing spaces.
    while (trailing > 0 && iswspace(str->at(trailing - 1))) {
        trailing--;
    }

    str->resize(trailing);
}

static void join_completions(comp_info_list_t *comps) {
    // A map from description to index in the completion list of the element with that description.
    // The indexes are stored +1.
    std::unordered_map<wcstring, size_t> desc_table;

    // Note that we mutate the completion list as we go, so the size changes.
    for (size_t i = 0; i < comps->size(); i++) {
        const comp_t &new_comp = comps->at(i);
        const wcstring &desc = new_comp.desc;
        if (desc.empty()) continue;

        // See if it's in the table.
        size_t prev_idx_plus_one = desc_table[desc];
        if (prev_idx_plus_one == 0) {
            // We're the first with this description.
            desc_table[desc] = i + 1;
        } else {
            // There's a prior completion with this description. Append the new ones to it.
            comp_t *prior_comp = &comps->at(prev_idx_plus_one - 1);
            prior_comp->comp.insert(prior_comp->comp.end(), new_comp.comp.begin(),
                                    new_comp.comp.end());

            // Erase the element at this index, and decrement the index to reflect that fact.
            comps->erase(comps->begin() + i);
            i -= 1;
        }
    }
}

/// Generate a list of comp_t structures from a list of completions.
static comp_info_list_t process_completions_into_infos(const completion_list_t &lst) {
    const size_t lst_size = lst.size();

    // Make the list of the correct size up-front.
    comp_info_list_t result(lst_size);
    for (size_t i = 0; i < lst_size; i++) {
        const completion_t &comp = lst.at(i);
        comp_t *comp_info = &result.at(i);

        // Append the single completion string. We may later merge these into multiple.
        comp_info->comp.push_back(escape_string(
            *comp.completion(), ESCAPE_NO_PRINTABLES | ESCAPE_NO_QUOTED | ESCAPE_SYMBOLIC));
        if (comp.replaces_commandline()
            // HACK We want to render a full shell command, with syntax highlighting.  Above we
            // escape nonprintables, which might make the rendered command longer than the original
            // completion. In that case we get wrong colors.  However this should only happen in
            // contrived cases, since our symbolic escaping uses a single character to represent
            // newline and tab characters; other nonprintables are extremely rare in a command
            // line. It will only be common for single-byte locales where we don't
            // use Unicode characters for escaping, so just disable those here.
            // We should probably fix this by first highlighting the original completion, and
            // then writing a variant of escape_string() that adjusts highlighting according
            // so it matches the escaped string.
            && MB_CUR_MAX > 1) {
            highlight_shell(*comp.completion(), comp_info->colors, *empty_operation_context());
            assert(comp_info->comp.back().size() >= comp_info->colors.size());
        }

        // Append the mangled description.
        comp_info->desc = std::move(*comp.description());
        mangle_1_completion_description(&comp_info->desc);

        // Set the representative completion.
        comp_info->representative = comp.clone();
    }
    return result;
}

void pager_t::measure_completion_infos(comp_info_list_t *infos, const wcstring &prefix) const {
    size_t prefix_len = fish_wcswidth(prefix);
    for (auto &info : *infos) {
        comp_t *comp = &info;
        const std::vector<wcstring> &comp_strings = comp->comp;

        for (size_t j = 0; j < comp_strings.size(); j++) {
            // If there's more than one, append the length of ', '.
            if (j >= 1) comp->comp_width += 2;

            // fish_wcswidth() can return -1 if it can't calculate the width. So be cautious.
            int comp_width = fish_wcswidth(comp_strings.at(j));
            if (comp_width >= 0) comp->comp_width += prefix_len + comp_width;
        }

        // fish_wcswidth() can return -1 if it can't calculate the width. So be cautious.
        int desc_width = fish_wcswidth(comp->desc);
        comp->desc_width = desc_width > 0 ? desc_width : 0;
    }
}

// Indicates if the given completion info passes any filtering we have.
bool pager_t::completion_info_passes_filter(const comp_t &info) const {
    // If we have no filter, everything passes.
    if (!search_field_shown || this->search_field_line.empty()) return true;

    const wcstring &needle = this->search_field_line.text();

    // Match against the description.
    if (string_fuzzy_match_string(needle, info.desc)) {
        return true;
    }

    // Match against the completion strings.
    for (const auto &i : info.comp) {
        if (string_fuzzy_match_string(needle, prefix + i)) {
            return true;
        }
    }

    return false;  // no match
}

// Update completion_infos from unfiltered_completion_infos, to reflect the filter.
void pager_t::refilter_completions() {
    this->completion_infos.clear();
    for (const auto &info : this->unfiltered_completion_infos) {
        if (this->completion_info_passes_filter(info)) {
            this->completion_infos.push_back(info);
        }
    }
}

void pager_t::set_completions(const completion_list_t &raw_completions) {
    selected_completion_idx = PAGER_SELECTION_NONE;
    // Get completion infos out of it.
    unfiltered_completion_infos = process_completions_into_infos(raw_completions);

    // Maybe join them.
    if (prefix == L"-") join_completions(&unfiltered_completion_infos);

    // Compute their various widths.
    measure_completion_infos(&unfiltered_completion_infos, prefix);

    // Refilter them.
    this->refilter_completions();
    have_unrendered_completions = true;
}

void pager_t::set_prefix(const wcstring &pref, bool highlight) {
    prefix = pref;
    highlight_prefix = highlight;
}

void pager_t::set_term_size(termsize_t ts) {
    available_term_width = ts.width > 0 ? ts.width : 0;
    available_term_height = ts.height > 0 ? ts.height : 0;
}

/// Try to print the list of completions lst with the prefix prefix using cols as the number of
/// columns. Return true if the completion list was printed, false if the terminal is too narrow for
/// the specified number of columns. Always succeeds if cols is 1.
bool pager_t::completion_try_print(size_t cols, const wcstring &prefix, const comp_info_list_t &lst,
                                   page_rendering_t *rendering, size_t suggested_start_row) const {
    assert(cols > 0);
    // The calculated preferred width of each column.
    size_t width_by_column[PAGER_MAX_COLS] = {0};

    // Skip completions on tiny terminals.
    if (this->available_term_width < PAGER_MIN_WIDTH ||
        this->available_term_height < PAGER_MIN_HEIGHT)
        return true;

    // Compute the effective term width and term height, accounting for disclosure.
    size_t term_width = this->available_term_width;
    size_t term_height =
        this->available_term_height - 1 -
        (search_field_shown ? 1 : 0);  // we always subtract 1 to make room for a comment row
    if (!this->fully_disclosed) {
        // We disclose between half and the entirety of the terminal height,
        // but at least 4 rows.
        //
        // We do this so we show a useful amount but don't force fish to
        // THE VERY TOP, which is jarring.
        term_height =
            std::min(term_height,
                     std::max(term_height / 2, static_cast<size_t>(PAGER_UNDISCLOSED_MAX_ROWS)));
    }

    size_t row_count = divide_round_up(lst.size(), cols);

    // We have more to disclose if we are not fully disclosed and there's more rows than we have in
    // our term height.
    if (!this->fully_disclosed && row_count > term_height) {
        rendering->remaining_to_disclose = row_count - term_height;
    } else {
        rendering->remaining_to_disclose = 0;
    }

    // If we have only one row remaining to disclose, then squelch the comment row. This prevents us
    // from consuming a line to show "...and 1 more row".
    if (rendering->remaining_to_disclose == 1) {
        term_height += 1;
        rendering->remaining_to_disclose = 0;
    }

    // Calculate how wide the list would be.
    for (size_t col = 0; col < cols; col++) {
        for (size_t row = 0; row < row_count; row++) {
            const size_t comp_idx = col * row_count + row;
            if (comp_idx >= lst.size()) continue;
            const comp_t &c = lst.at(comp_idx);
            width_by_column[col] = std::max(width_by_column[col], c.preferred_width());
        }
    }

    bool print;
    // Force fit if one column.
    if (cols == 1) {
        width_by_column[0] = std::min(width_by_column[0], term_width);
        print = true;
    } else {
        // Compute total preferred width, plus spacing
        size_t total_width_needed = std::accumulate(width_by_column, width_by_column + cols, 0);
        total_width_needed += (cols - 1) * PAGER_SPACER_STRING_WIDTH;
        print = (total_width_needed <= term_width);
    }
    if (!print) {
        return false;  // no need to continue
    }

    // Determine the starting and stop row.
    size_t start_row = 0, stop_row = 0;
    if (row_count <= term_height) {
        // Easy, we can show everything.
        start_row = 0;
        stop_row = row_count;
    } else {
        // We can only show part of the full list. Determine which part based on the
        // suggested_start_row.
        assert(row_count > term_height);
        size_t last_starting_row = row_count - term_height;
        start_row = std::min(suggested_start_row, last_starting_row);
        stop_row = start_row + term_height;
        assert(start_row <= last_starting_row);
    }

    assert(stop_row >= start_row);
    assert(stop_row <= row_count);
    assert(stop_row - start_row <= term_height);
    completion_print(cols, width_by_column, start_row, stop_row, prefix, lst, rendering);

    // Add the progress line. It's a "more to disclose" line if necessary, or a row listing if
    // it's scrollable; otherwise ignore it.
    // We should never have one row remaining to disclose (else we would have just disclosed it)
    wcstring progress_text;
    assert(rendering->remaining_to_disclose != 1);
    if (rendering->remaining_to_disclose > 1) {
        progress_text = format_string(_(L"%lsand %lu more rows"), get_ellipsis_str(),
                                      static_cast<unsigned long>(rendering->remaining_to_disclose));
    } else if (start_row > 0 || stop_row < row_count) {
        // We have a scrollable interface. The +1 here is because we are zero indexed, but want
        // to present things as 1-indexed. We do not add 1 to stop_row or row_count because
        // these are the "past the last value".
        progress_text =
            format_string(_(L"rows %lu to %lu of %lu"), start_row + 1, stop_row, row_count);
    } else if (search_field_shown && completion_infos.empty()) {
        // Everything is filtered.
        progress_text = _(L"(no matches)");
    }
    if (!extra_progress_text.empty()) {
        if (!progress_text.empty()) {
            progress_text += L". ";
        }
        progress_text += extra_progress_text;
    }

    if (!progress_text.empty()) {
        line_t &line = rendering->screen_data.add_line();
        highlight_spec_t spec = {highlight_role_t::pager_progress,
                                 highlight_role_t::pager_progress};
        print_max(progress_text, spec, term_width, true /* has_more */, &line);
    }

    if (!search_field_shown) {
        return true;
    }

    // Add the search field.
    wcstring search_field_text = search_field_line.text();
    // Append spaces to make it at least the required width.
    if (search_field_text.size() < PAGER_SEARCH_FIELD_WIDTH) {
        search_field_text.append(PAGER_SEARCH_FIELD_WIDTH - search_field_text.size(), L' ');
    }
    line_t *search_field = &rendering->screen_data.insert_line_at_index(0);

    // We limit the width to term_width - 1.
    highlight_spec_t underline{};
    underline->force_underline = true;

    size_t search_field_remaining = term_width - 1;
    search_field_remaining -= print_max(SEARCH_FIELD_PROMPT, highlight_role_t::normal,
                                        search_field_remaining, false, search_field);
    search_field_remaining -=
        print_max(search_field_text, underline, search_field_remaining, false, search_field);
    return true;
}

page_rendering_t pager_t::render() const {
    /// Try to print the completions. Start by trying to print the list in PAGER_MAX_COLS columns,
    /// if the completions won't fit, reduce the number of columns by one. Printing a single column
    /// never fails.
    page_rendering_t rendering;
    rendering.term_width = this->available_term_width;
    rendering.term_height = this->available_term_height;
    rendering.search_field_shown = this->search_field_shown;
    rendering.search_field_line = this->search_field_line;

    for (size_t cols = PAGER_MAX_COLS; cols > 0; cols--) {
        // Initially empty rendering.
        rendering.screen_data.resize(0);

        // Determine how many rows we would need if we had 'cols' columns. Then determine how many
        // columns we want from that. For example, say we had 19 completions. We can fit them into 6
        // columns, 4 rows, with the last row containing only 1 entry. Or we can fit them into 5
        // columns, 4 rows, the last row containing 4 entries. Since fewer columns with the same
        // number of rows is better, skip cases where we know we can do better.
        size_t min_rows_required_for_cols = divide_round_up(completion_infos.size(), cols);
        size_t min_cols_required_for_rows =
            divide_round_up(completion_infos.size(), min_rows_required_for_cols);

        assert(min_cols_required_for_rows <= cols);
        if (cols > 1 && min_cols_required_for_rows < cols) {
            // Next iteration will be better, so skip this one.
            continue;
        }

        rendering.cols = cols;
        rendering.rows = min_rows_required_for_cols;
        rendering.selected_completion_idx =
            this->visual_selected_completion_index(rendering.rows, rendering.cols);

        if (completion_try_print(cols, prefix, completion_infos, &rendering, suggested_row_start)) {
            break;
        }
    }
    return rendering;
}

bool pager_t::rendering_needs_update(const page_rendering_t &rendering) const {
    if (have_unrendered_completions) return true;
    // Common case is no pager.
    if (this->empty() && rendering.screen_data.empty()) return false;

    return (this->empty() && !rendering.screen_data.empty()) ||     // Do update after clear().
           rendering.term_width != this->available_term_width ||    //
           rendering.term_height != this->available_term_height ||  //
           rendering.selected_completion_idx !=
               this->visual_selected_completion_index(rendering.rows, rendering.cols) ||    //
           rendering.search_field_shown != this->search_field_shown ||                      //
           rendering.search_field_line.text() != this->search_field_line.text() ||          //
           rendering.search_field_line.position() != this->search_field_line.position() ||  //
           (rendering.remaining_to_disclose > 0 && this->fully_disclosed);
}

void pager_t::update_rendering(page_rendering_t *rendering) {
    if (rendering_needs_update(*rendering)) {
        *rendering = this->render();
        have_unrendered_completions = false;
    }
}

pager_t::pager_t() = default;
pager_t::~pager_t() = default;

bool pager_t::empty() const { return unfiltered_completion_infos.empty(); }

bool pager_t::select_next_completion_in_direction(selection_motion_t direction,
                                                  const page_rendering_t &rendering) {
    // Must have something to select.
    if (this->completion_infos.empty()) {
        return false;
    }

    if (selected_completion_idx == PAGER_SELECTION_NONE) {
        // Handle the case of nothing selected yet.
        switch (direction) {
            case selection_motion_t::south:
            case selection_motion_t::page_south:
            case selection_motion_t::next:
            case selection_motion_t::north:
            case selection_motion_t::prev: {
                // These directions do something sane.
                if (direction == selection_motion_t::prev ||
                    direction == selection_motion_t::north) {
                    selected_completion_idx = completion_infos.size() - 1;
                } else {
                    selected_completion_idx = 0;
                }
                break;
            }
            case selection_motion_t::page_north:
            case selection_motion_t::east:
            case selection_motion_t::west:
            case selection_motion_t::deselect: {
                // These do nothing.
                return false;
            }
        }
    } else {
        // Ok, we had something selected already. Select something different.
        size_t new_selected_completion_idx;
        if (!selection_direction_is_cardinal(direction)) {
            // Next, previous, or deselect, all easy.
            if (direction == selection_motion_t::deselect) {
                new_selected_completion_idx = PAGER_SELECTION_NONE;
            } else if (direction == selection_motion_t::next) {
                new_selected_completion_idx = selected_completion_idx + 1;
                if (new_selected_completion_idx >= completion_infos.size()) {
                    new_selected_completion_idx = 0;
                }
            } else if (direction == selection_motion_t::prev) {
                if (selected_completion_idx == 0) {
                    new_selected_completion_idx = completion_infos.size() - 1;
                } else {
                    new_selected_completion_idx = selected_completion_idx - 1;
                }
            } else {
                DIE("unknown non-cardinal direction");
            }
        } else {
            // Cardinal directions. We have a completion index; we wish to compute its row and
            // column.
            size_t current_row = this->get_selected_row(rendering);
            size_t current_col = this->get_selected_column(rendering);
            size_t page_height = std::max(rendering.term_height - 1, static_cast<size_t>(1));

            switch (direction) {
                case selection_motion_t::page_north: {
                    if (current_row > page_height) {
                        current_row = current_row - page_height;
                    } else {
                        current_row = 0;
                    }
                    break;
                }
                case selection_motion_t::north: {
                    // Go up a whole row. If we cycle, go to the previous column.
                    if (current_row > 0) {
                        current_row--;
                    } else {
                        current_row = rendering.rows - 1;
                        if (current_col > 0) {
                            current_col--;
                        } else {
                            current_col = rendering.cols - 1;
                        }
                    }
                    break;
                }
                case selection_motion_t::page_south: {
                    if (current_row + page_height < rendering.rows) {
                        current_row += page_height;
                    } else {
                        current_row = rendering.rows - 1;
                        if (current_col * rendering.rows + current_row >= completion_infos.size()) {
                            current_row = (completion_infos.size() - 1) % rendering.rows;
                        }
                    }
                    break;
                }
                case selection_motion_t::south: {
                    // Go down, unless we are in the last row.
                    // If we go over the last element, wrap to the first.
                    if (current_row + 1 < rendering.rows &&
                        current_col * rendering.rows + current_row + 1 < completion_infos.size()) {
                        current_row++;
                    } else {
                        current_row = 0;
                        current_col = (current_col + 1) % rendering.cols;
                    }
                    break;
                }
                case selection_motion_t::east: {
                    // Go east, wrapping to the next row. There is no "row memory," so if we run off
                    // the end, wrap.
                    if (current_col + 1 < rendering.cols &&
                        (current_col + 1) * rendering.rows + current_row <
                            completion_infos.size()) {
                        current_col++;
                    } else {
                        current_col = 0;
                        current_row = (current_row + 1) % rendering.rows;
                    }
                    break;
                }
                case selection_motion_t::west: {
                    // Go west, wrapping to the previous row.
                    if (current_col > 0) {
                        current_col--;
                    } else {
                        current_col = rendering.cols - 1;
                        if (current_row > 0) {
                            current_row--;
                        } else {
                            current_row = rendering.rows - 1;
                        }
                    }
                    break;
                }
                default: {
                    DIE("unknown cardinal direction");
                }
            }

            // Compute the new index based on the changed row.
            new_selected_completion_idx = current_col * rendering.rows + current_row;
        }

        if (selected_completion_idx == new_selected_completion_idx) {
            return false;
        }
        selected_completion_idx = new_selected_completion_idx;
    }

    // Update suggested_row_start to ensure the selection is visible. suggested_row_start *
    // rendering.cols is the first suggested visible completion; add the visible completion
    // count to that to get the last one.
    size_t visible_row_count = rendering.row_end - rendering.row_start;
    if (visible_row_count == 0) {
        return true;  // this happens if there was no room to draw the pager
    }
    if (selected_completion_idx == PAGER_SELECTION_NONE) {
        return true;  // this should never happen but be paranoid
    }

    // Ensure our suggested row start is not past the selected row.
    size_t row_containing_selection = this->get_selected_row(rendering.rows);
    if (suggested_row_start > row_containing_selection) {
        suggested_row_start = row_containing_selection;
    }

    // Ensure our suggested row start is not too early before it.
    if (suggested_row_start + visible_row_count <= row_containing_selection) {
        // The user moved south past the bottom completion.
        if (!fully_disclosed && rendering.remaining_to_disclose > 0) {
            fully_disclosed = true;  // perform disclosure
        } else {
            // Scroll
            suggested_row_start = row_containing_selection - visible_row_count + 1;
            // Ensure fully_disclosed is set. I think we can hit this case if the user
            // resizes the window - we don't want to drop back to the disclosed style.
            fully_disclosed = true;
        }
    }

    return true;
}

size_t pager_t::visual_selected_completion_index(size_t rows, size_t cols) const {
    // No completions -> no selection.
    if (completion_infos.empty()) {
        return PAGER_SELECTION_NONE;
    }

    size_t result = selected_completion_idx;
    if (result == 0) {
        return result;
    }
    if (rows == 0 || cols == 0) {
        return PAGER_SELECTION_NONE;
    }
    if (result != PAGER_SELECTION_NONE) {
        // If the selected completion is beyond the last selection, go left by columns until it's
        // within it. This is how we implement "column memory".
        while (result >= completion_infos.size() && result >= rows) {
            result -= rows;
        }

        // If we are still beyond the last selection, clamp it.
        if (result >= completion_infos.size()) result = completion_infos.size() - 1;
    }
    assert(result == PAGER_SELECTION_NONE || result < completion_infos.size());
    return result;
}

// It's possible we have no visual selection but are still navigating the contents, e.g. every
// completion is filtered.
bool pager_t::is_navigating_contents() const {
    return selected_completion_idx != PAGER_SELECTION_NONE;
}

void pager_t::set_fully_disclosed() { fully_disclosed = true; }

const completion_t *pager_t::selected_completion(const page_rendering_t &rendering) const {
    const completion_t *result = nullptr;
    size_t idx = visual_selected_completion_index(rendering.rows, rendering.cols);
    if (idx != PAGER_SELECTION_NONE) {
        result = &*completion_infos.at(idx).representative;
    }
    return result;
}

size_t pager_t::selected_completion_index() const { return selected_completion_idx; }

void pager_t::set_selected_completion_index(size_t new_index) {
    // Current users are off by one at most.
    assert(new_index == PAGER_SELECTION_NONE || new_index <= completion_infos.size());
    if (new_index == completion_infos.size()) --new_index;
    selected_completion_idx = new_index;
}

/// Get the selected row and column. Completions are rendered column first, i.e. we go south before
/// we go west. So if we have N rows, and our selected index is N + 2, then our row is 2 (mod by N)
/// and our column is 1 (divide by N).
size_t pager_t::get_selected_row(const page_rendering_t &rendering) const {
    if (rendering.rows == 0) return PAGER_SELECTION_NONE;

    return rendering.selected_completion_idx == PAGER_SELECTION_NONE
               ? PAGER_SELECTION_NONE
               : rendering.selected_completion_idx % rendering.rows;
}

size_t pager_t::get_selected_row(size_t rows) const {
    if (rows == 0) return PAGER_SELECTION_NONE;

    return selected_completion_idx == PAGER_SELECTION_NONE ? PAGER_SELECTION_NONE
                                                           : selected_completion_idx % rows;
}

size_t pager_t::get_selected_column(const page_rendering_t &rendering) const {
    if (rendering.rows == 0) return PAGER_SELECTION_NONE;

    return rendering.selected_completion_idx == PAGER_SELECTION_NONE
               ? PAGER_SELECTION_NONE
               : rendering.selected_completion_idx / rendering.rows;
}

void pager_t::clear() {
    unfiltered_completion_infos.clear();
    completion_infos.clear();
    prefix.clear();
    highlight_prefix = false;
    selected_completion_idx = PAGER_SELECTION_NONE;
    fully_disclosed = false;
    search_field_shown = false;
    search_field_line.clear();
    extra_progress_text.clear();
}

void pager_t::set_search_field_shown(bool flag) { this->search_field_shown = flag; }

bool pager_t::is_search_field_shown() const { return this->search_field_shown; }

size_t pager_t::cursor_position() const {
    size_t result = std::wcslen(SEARCH_FIELD_PROMPT) + this->search_field_line.position();
    // Clamp it to the right edge.
    if (available_term_width > 0 && result + 1 > available_term_width) {
        result = available_term_width - 1;
    }
    return result;
}

page_rendering_t::page_rendering_t() = default;
