/** \file pager.h
  Pager support
*/

#include "complete.h"
#include "screen.h"

/* Represents rendering from the pager */
class page_rendering_t
{
    screen_data_t screen_data;
};

typedef std::vector<completion_t> completion_list_t;
page_rendering_t render_completions(const completion_list_t &raw_completions, const wcstring &prefix);
