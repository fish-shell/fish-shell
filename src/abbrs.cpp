#include "config.h"  // IWYU pragma: keep

#include "abbrs.h"

// void abbrs_set_t::import_from_uvars(const std::unordered_map<wcstring, env_var_t> &uvars) {
//     const wchar_t *const prefix = L"_fish_abbr_";
//     size_t prefix_len = wcslen(prefix);
//     const bool from_universal = true;
//     for (const auto &kv : uvars) {
//         if (string_prefixes_string(prefix, kv.first)) {
//             wcstring escaped_name = kv.first.substr(prefix_len);
//             wcstring name;
//             if (unescape_string(escaped_name, &name, unescape_flags_t{}, STRING_STYLE_VAR)) {
//                 wcstring key = name;
//                 wcstring replacement = join_strings(kv.second.as_list(), L' ');
//                 this->add(abbreviation_t{std::move(name), std::move(key), std::move(replacement),
//                                          abbrs_position_t::command, from_universal});
//             }
//         }
//     }
// }
