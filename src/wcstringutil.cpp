/** \file wcstringutil.cpp

Helper functions for working with wcstring
*/
#include "common.h"
#include "wcstringutil.h"

typedef wcstring::size_type size_type;

wcstring_range wcstring_tok(wcstring& str, const wcstring &needle, wcstring_range last)
{
    size_type pos = last.second == wcstring::npos ? wcstring::npos : last.first;
    if (pos != wcstring::npos && last.second != wcstring::npos) pos += last.second;
    if (pos != wcstring::npos && pos != 0) ++pos;
    if (pos == wcstring::npos || pos >= str.size())
    {
        return std::make_pair(wcstring::npos, wcstring::npos);
    }

    if (needle.empty())
    {
        return std::make_pair(pos, wcstring::npos);
    }

    pos = str.find_first_not_of(needle, pos);
    if (pos == wcstring::npos) return std::make_pair(wcstring::npos, wcstring::npos);

    size_type next_pos = str.find_first_of(needle, pos);
    if (next_pos == wcstring::npos)
    {
        return std::make_pair(pos, wcstring::npos);
    }
    else
    {
        str[next_pos] = L'\0';
        return std::make_pair(pos, next_pos - pos);
    }
}
