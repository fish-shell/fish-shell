#include "null_terminated_array.h"

std::vector<std::string> wide_string_list_to_narrow(const wcstring_list_t &strs) {
    std::vector<std::string> res;
    res.reserve(strs.size());
    for (const wcstring &s : strs) {
        res.push_back(wcs2string(s));
    }
    return res;
}
