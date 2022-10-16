#include "config.h"  // IWYU pragma: keep

#include "abbrs.h"

#include "env.h"
#include "global_safety.h"
#include "wcstringutil.h"

abbreviation_t::abbreviation_t(wcstring name, wcstring key, wcstring replacement,
                               abbrs_position_t position, bool from_universal)
    : name(std::move(name)),
      key(std::move(key)),
      replacement(std::move(replacement)),
      position(position),
      from_universal(from_universal) {}

bool abbreviation_t::matches_position(abbrs_position_t position) const {
    return this->position == abbrs_position_t::anywhere || this->position == position;
}

bool abbreviation_t::matches_phases(abbrs_phases_t p) const { return bool(this->phases & p); }

bool abbreviation_t::matches(const wcstring &token, abbrs_position_t position,
                             abbrs_phases_t phases) const {
    if (!this->matches_position(position) || !this->matches_phases(phases)) {
        return false;
    }
    if (this->is_regex()) {
        return this->regex->match(token).has_value();
    } else {
        return this->key == token;
    }
}

acquired_lock<abbrs_set_t> abbrs_get_set() {
    static owning_lock<abbrs_set_t> abbrs;
    return abbrs.acquire();
}

abbrs_replacer_list_t abbrs_set_t::match(const wcstring &token, abbrs_position_t position,
                                         abbrs_phases_t phases) const {
    abbrs_replacer_list_t result{};
    // Later abbreviations take precedence so walk backwards.
    for (auto it = abbrs_.rbegin(); it != abbrs_.rend(); ++it) {
        const abbreviation_t &abbr = *it;
        if (abbr.matches(token, position, phases)) {
            result.push_back(abbrs_replacer_t{abbr.replacement, abbr.replacement_is_function,
                                              abbr.set_cursor_indicator});
        }
    }
    return result;
}

bool abbrs_set_t::has_match(const wcstring &token, abbrs_position_t position,
                            abbrs_phases_t phases) const {
    for (const auto &abbr : abbrs_) {
        if (abbr.matches(token, position, phases)) {
            return true;
        }
    }
    return false;
}

void abbrs_set_t::add(abbreviation_t &&abbr) {
    assert(!abbr.name.empty() && "Invalid name");
    bool inserted = used_names_.insert(abbr.name).second;
    if (!inserted) {
        // Name was already used, do a linear scan to find it.
        auto where = std::find_if(abbrs_.begin(), abbrs_.end(), [&](const abbreviation_t &other) {
            return other.name == abbr.name;
        });
        assert(where != abbrs_.end() && "Abbreviation not found though its name was present");
        abbrs_.erase(where);
    }
    abbrs_.push_back(std::move(abbr));
}

void abbrs_set_t::rename(const wcstring &old_name, const wcstring &new_name) {
    bool erased = this->used_names_.erase(old_name) > 0;
    bool inserted = this->used_names_.insert(new_name).second;
    assert(erased && inserted && "Old name not found or new name already present");
    (void)erased;
    (void)inserted;
    for (auto &abbr : abbrs_) {
        if (abbr.name == old_name) {
            abbr.name = new_name;
            break;
        }
    }
}

bool abbrs_set_t::erase(const wcstring &name) {
    bool erased = this->used_names_.erase(name) > 0;
    if (!erased) {
        return false;
    }
    for (auto it = abbrs_.begin(); it != abbrs_.end(); ++it) {
        if (it->name == name) {
            abbrs_.erase(it);
            return true;
        }
    }
    assert(false && "Unable to find named abbreviation");
    return false;
}

void abbrs_set_t::import_from_uvars(const std::unordered_map<wcstring, env_var_t> &uvars) {
    const wchar_t *const prefix = L"_fish_abbr_";
    size_t prefix_len = wcslen(prefix);
    const bool from_universal = true;
    for (const auto &kv : uvars) {
        if (string_prefixes_string(prefix, kv.first)) {
            wcstring escaped_name = kv.first.substr(prefix_len);
            wcstring name;
            if (unescape_string(escaped_name, &name, unescape_flags_t{}, STRING_STYLE_VAR)) {
                wcstring key = name;
                wcstring replacement = join_strings(kv.second.as_list(), L' ');
                this->add(abbreviation_t{std::move(name), std::move(key), std::move(replacement),
                                         abbrs_position_t::command, from_universal});
            }
        }
    }
}

// static
abbrs_replacement_t abbrs_replacement_t::from(source_range_t range, wcstring text,
                                              const abbrs_replacer_t &replacer) {
    abbrs_replacement_t result{};
    result.range = range;
    result.text = std::move(text);
    if (replacer.set_cursor_indicator.has_value()) {
        size_t pos = result.text.find(*replacer.set_cursor_indicator);
        if (pos != wcstring::npos) {
            result.text.erase(pos, replacer.set_cursor_indicator->size());
            result.cursor = pos + range.start;
        }
    }
    return result;
}
