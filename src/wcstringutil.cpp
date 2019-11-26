// Helper functions for working with wcstring.
#include "config.h"  // IWYU pragma: keep

#include "wcstringutil.h"

#include <wctype.h>

#include "common.h"

using size_type = wcstring::size_type;

wcstring_range wcstring_tok(wcstring &str, const wcstring &needle, wcstring_range last) {
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
    if (input.size() <= static_cast<size_t>(max_len)) {
        return input;
    }

    if (etype == ellipsis_type::None) {
        return input.substr(0, max_len);
    }
    if (etype == ellipsis_type::Prettiest) {
        const wchar_t *ellipsis_str = get_ellipsis_str();
        return input.substr(0, max_len - std::wcslen(ellipsis_str)).append(ellipsis_str);
    }
    wcstring output = input.substr(0, max_len - 1);
    output.push_back(get_ellipsis_char());
    return output;
}

wcstring trim(wcstring input) { return trim(std::move(input), L"\t\v \r\n"); }

wcstring trim(wcstring input, const wchar_t *any_of) {
    wcstring result = std::move(input);
    size_t suffix = result.find_last_not_of(any_of);
    if (suffix == wcstring::npos) {
        return wcstring{};
    }
    result.erase(suffix + 1);

    auto prefix = result.find_first_not_of(any_of);
    assert(prefix != wcstring::npos && "Should have one non-trimmed character");
    result.erase(0, prefix);
    return result;
}

wcstring wcstolower(wcstring input) {
    wcstring result = std::move(input);
    std::transform(result.begin(), result.end(), result.begin(), towlower);
    return result;
}
