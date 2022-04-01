#include "config.h"  // IWYU pragma: keep

#include "abbrs.h"

#include "env.h"
#include "global_safety.h"
#include "wcstringutil.h"

static relaxed_atomic_t<uint64_t> k_abbrs_next_order{0};

abbreviation_t::abbreviation_t(wcstring replacement, abbrs_position_t position, bool from_universal)
    : replacement(std::move(replacement)),
      position(position),
      from_universal(from_universal),
      order(++k_abbrs_next_order) {}

acquired_lock<abbrs_map_t> abbrs_get_map() {
    static owning_lock<std::unordered_map<wcstring, abbreviation_t>> abbrs;
    return abbrs.acquire();
}

maybe_t<wcstring> abbrs_expand(const wcstring &token, abbrs_position_t position) {
    auto abbrs = abbrs_get_map();
    auto iter = abbrs->find(token);
    maybe_t<wcstring> result{};
    if (iter != abbrs->end()) {
        const abbreviation_t &abbr = iter->second;
        // Expand only if the positions are "compatible."
        if (abbr.position == position || abbr.position == abbrs_position_t::anywhere) {
            result = abbr.replacement;
        }
    }
    return result;
}

wcstring_list_t abbrs_get_keys() {
    auto abbrs = abbrs_get_map();
    wcstring_list_t keys;
    keys.reserve(abbrs->size());
    for (const auto &kv : *abbrs) {
        keys.push_back(kv.first);
    }
    return keys;
}

void abbrs_import_from_uvars(const std::unordered_map<wcstring, env_var_t> &uvars) {
    auto abbrs = abbrs_get_map();
    const wchar_t *const prefix = L"_fish_abbr_";
    size_t prefix_len = wcslen(prefix);
    wcstring name;
    const bool from_universal = true;
    for (const auto &kv : uvars) {
        if (string_prefixes_string(prefix, kv.first)) {
            wcstring escaped_name = kv.first.substr(prefix_len);
            if (unescape_string(escaped_name, &name, unescape_flags_t{}, STRING_STYLE_VAR)) {
                wcstring replacement = join_strings(kv.second.as_list(), L' ');
                abbrs->emplace(name, abbreviation_t{std::move(replacement),
                                                    abbrs_position_t::command, from_universal});
            }
        }
    }
}
