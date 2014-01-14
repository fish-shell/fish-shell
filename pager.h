/** \file pager.h
  Pager support
*/

#include "complete.h"
#include "screen.h"

/* Represents rendering from the pager */
struct page_rendering_t
{
    screen_data_t screen_data;
};

typedef std::vector<completion_t> completion_list_t;
page_rendering_t render_completions(const completion_list_t &raw_completions, const wcstring &prefix);

class pager_t
{
    int term_width;
    int term_height;
    
    completion_list_t completions;
    
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
    
    int completion_try_print(int cols, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const;
    
    void recalc_min_widths(comp_info_list_t * lst) const;
    void measure_completion_infos(std::vector<comp_t> *infos, const wcstring &prefix) const;
    
    void completion_print(int cols, int *width_per_column, int row_start, int row_stop, const wcstring &prefix, const comp_info_list_t &lst, page_rendering_t *rendering) const;
    line_t completion_print_item(const wcstring &prefix, const comp_t *c, size_t row, size_t column, int width, bool secondary, page_rendering_t *rendering) const;

    
    public:
    void set_completions(const completion_list_t &comp);
    void set_term_size(int w, int h);
    wcstring prefix;
    
    page_rendering_t render() const;
};
