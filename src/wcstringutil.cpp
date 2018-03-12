// Helper functions for working with wcstring.
#include "config.h"  // IWYU pragma: keep

#include "common.h"
#include "wcstringutil.h"

typedef wcstring::size_type size_type;

wcstring_range wcstring_tok(wcstring& str, const wcstring& needle, wcstring_range last) {
    size_type pos = last.second == wcstring::npos ? wcstring::npos : last.first;
    if (pos != wcstring::npos && last.second != wcstring::npos) pos += last.second;
    if (pos != wcstring::npos && pos != 0) ++pos;
    if (pos == wcstring::npos || pos >= str.size()) {
        return std::make_pair(wcstring::npos, wcstring::npos);
    }

    if (needle.empty()) {
        return std::make_pair(pos, wcstring::npos);
    }

    pos = str.find_first_not_of(needle, pos);
    if (pos == wcstring::npos) return std::make_pair(wcstring::npos, wcstring::npos);

    size_type next_pos = str.find_first_of(needle, pos);
    if (next_pos == wcstring::npos) {
        return std::make_pair(pos, wcstring::npos);
    }

    str[next_pos] = L'\0';
    return std::make_pair(pos, next_pos - pos);
}

wcstring truncate(const wcstring &input, int max_len, ellipsis_type etype) {
    if (input.size() <= max_len) {
        return input;
    }

    if (etype == ellipsis_type::None) {
        return input.substr(0, max_len);
    }
    if (etype == ellipsis_type::Prettiest) {
        return input.substr(0, max_len - wcslen(ellipsis_str)).append(ellipsis_str);
    }
    wcstring output = input.substr(0, max_len - 1);
    output.push_back(ellipsis_char);
    return output;
}

wcstring trim(const wcstring &input) {
    debug(0, "trimming '%ls'", input.c_str());

    // auto begin = input.cbegin();
    // for (begin; *begin == L' '; ++begin);
    // auto end = input.cbegin() + input.size();
    // for (end; end > begin && *end == L' '; ++end);

    auto begin_offset = input.find_first_not_of(whitespace);
    if (begin_offset == wcstring::npos) {
        return wcstring{};
    }
    auto end = input.cbegin() + input.find_last_not_of(whitespace);

    wcstring result(input.begin() + begin_offset, end + 1);
    return result;
}
