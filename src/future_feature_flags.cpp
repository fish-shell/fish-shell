#include "config.h"  // IWYU pragma: keep

#include "future_feature_flags.h"

#include <cwchar>

#include "wcstringutil.h"

features_t::features_t() = default;

/// The set of features applying to this instance.
features_t features_t::global_features;

const features_t::metadata_t features_t::metadata[features_t::flag_count] = {
    {stderr_nocaret, L"stderr-nocaret", L"3.0", L"^ no longer redirects stderr"},
    {qmark_noglob, L"qmark-noglob", L"3.0", L"? no longer globs"},
    {string_replace_backslash, L"regex-easyesc", L"3.1", L"string replace -r needs fewer \\'s"},
};

const struct features_t::metadata_t *features_t::metadata_for(const wchar_t *name) {
    assert(name && "null flag name");
    for (const auto &md : metadata) {
        if (!std::wcscmp(name, md.name)) return &md;
    }
    return nullptr;
}

void features_t::set_from_string(const wcstring &str) {
    wcstring_list_t entries = split_string(str, L',');
    const wchar_t *whitespace = L"\t\n\v\f\r ";
    for (wcstring entry : entries) {
        if (entry.empty()) continue;

        // Trim leading and trailing whitespace
        entry.erase(0, entry.find_first_not_of(whitespace));
        entry.erase(entry.find_last_not_of(whitespace) + 1);

        const wchar_t *name = entry.c_str();
        bool value = true;
        // A "no-" prefix inverts the sense.
        if (string_prefixes_string(L"no-", name)) {
            value = false;
            name += 3;  // std::wcslen(L"no-")
        }
        // Look for a feature with this name. If we don't find it, assume it's a group name and set
        // all features whose group contain it. Do nothing even if the string is unrecognized; this
        // is to allow uniform invocations of fish (e.g. disable a feature that is only present in
        // future versions).
        // The special name 'all' may be used for those who like to live on the edge.
        if (const metadata_t *md = metadata_for(name)) {
            this->set(md->flag, value);
        } else {
            for (const metadata_t &md : metadata) {
                if (std::wcsstr(md.groups, name) || !std::wcscmp(name, L"all")) {
                    this->set(md.flag, value);
                }
            }
        }
    }
}
