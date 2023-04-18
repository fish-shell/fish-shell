#include "null_terminated_array.h"

std::vector<std::string> wide_string_list_to_narrow(const std::vector<wcstring> &strs) {
    std::vector<std::string> res;
    res.reserve(strs.size());
    for (const wcstring &s : strs) {
        res.push_back(wcs2zstring(s));
    }
    return res;
}
