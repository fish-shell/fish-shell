/** \file pager.h
  Pager support
*/

#include "complete.h"
#include "screen.h"

/* Represents rendering from the pager */
class page_rendering_t
{
    public:
    int term_width;
    int term_height;
    size_t rows;
    size_t cols;
    size_t selected_completion_idx;
    screen_data_t screen_data;
    
    /* Returns a rendering with invalid data, useful to indicate "no rendering" */
    page_rendering_t();
};

typedef std::vector<completion_t> completion_list_t;
page_rendering_t render_completions(const completion_list_t &raw_completions, const wcstring &prefix);

class pager_t
{
    int term_width;
    int term_height;
    
    completion_list_t completions;
    
    size_t selected_completion_idx;
    
    /** Data structure describing one or a group of related completions */
    public:
    struct comp_t
    {
        /** The list of all completin strings this entry applies to */
        wcstring_list_t comp;
        
        /** The description */
        wcstring desc;
        
        /** On-screen width of the completion string */
        int comp_width;
        
        /** On-screen width of the description information */
        int desc_width;
        
        /** Preffered total width */
        int pref_width;
        
        /** Minimum acceptable width */
        int min_width;
        
        comp_t() : comp(), desc(), comp_width(0), desc_width(0), pref_width(0), min_width(0)
        {
        }
    };
    
    private:
    typedef std::vector<comp_t> comp_info_list_t;
    comp_info_list_t completion_infos;
    
    wcstring prefix;
    
    int completion_try_print(int cols, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const;
    
    void recalc_min_widths(comp_info_list_t * lst) const;
    void measure_completion_infos(std::vector<comp_t> *infos, const wcstring &prefix) const;
    
    void completion_print(int cols, int *width_per_column, int row_start, int row_stop, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const;
    line_t completion_print_item(const wcstring &prefix, const comp_t *c, size_t row, size_t column, int width, bool secondary, bool selected, page_rendering_t *rendering) const;

    
    public:
    
    /* Sets the set of completions */
    void set_completions(const completion_list_t &comp);
    
    /* Sets the prefix */
    void set_prefix(const wcstring &pref);
    
    /* Sets the terminal width and height */
    void set_term_size(int w, int h);
    
    /* Sets the index of the selected completion */
    void set_selected_completion(size_t completion_idx);
    
    /* Changes the selected completion in the given direction according to the layout of the given rendering. Returns true if the values changed. */
    bool select_next_completion_in_direction(cardinal_direction_t direction, const page_rendering_t &rendering);
    
    /* Produces a rendering of the completions, at the given term size */
    page_rendering_t render() const;
    
    /* Updates the rendering if it's stale */
    void update_rendering(page_rendering_t *rendering) const;
    
    /* Indicates if there are no completions, and therefore nothing to render */
    bool empty() const;
    
    /* Clears all completions and the prefix */
    void clear();
    
    /* Constructor */
    pager_t();
};
