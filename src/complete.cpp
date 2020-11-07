/// Functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
///
#include "config.h"  // IWYU pragma: keep

#include "complete.h"

#include <pthread.h>
#include <pwd.h>
#include <stddef.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cwchar>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "autoload.h"
#include "builtin.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "history.h"
#include "iothread.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "parser_keywords.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "util.h"
#include "wcstringutil.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

// Completion description strings, mostly for different types of files, such as sockets, block
// devices, etc.
//
// There are a few more completion description strings defined in expand.c. Maybe all completion
// description strings should be defined in the same file?

/// Description for ~USER completion.
#define COMPLETE_USER_DESC _(L"Home for %ls")

/// Description for short variables. The value is concatenated to this description.
#define COMPLETE_VAR_DESC_VAL _(L"Variable: %ls")

/// Description for abbreviations.
#define ABBR_DESC _(L"Abbreviation: %ls")

/// The special cased translation macro for completions. The empty string needs to be special cased,
/// since it can occur, and should not be translated. (Gettext returns the version information as
/// the response).
#ifdef HAVE_GETTEXT
static const wchar_t *C_(const wcstring &s) {
    return s.empty() ? L"" : wgettext(s.c_str()).c_str();
}
#else
static const wcstring &C_(const wcstring &s) { return s; }
#endif

/// Struct describing a completion option entry.
///
/// If option is empty, the comp field must not be empty and contains a list of arguments to the
/// command.
///
/// The type field determines how the option is to be interpreted: either empty (args_only) or
/// short, single-long ("old") or double-long ("GNU"). An invariant is that the option is empty if
/// and only if the type is args_only.
///
/// If option is non-empty, it specifies a switch for the command. If \c comp is also not empty, it
/// contains a list of non-switch arguments that may only follow directly after the specified
/// switch.
using complete_entry_opt_t = struct complete_entry_opt {
    // Text of the option (like 'foo').
    wcstring option;
    // Type of the option: args-oly, short, single_long, or double_long.
    complete_option_type_t type;
    // Arguments to the option.
    wcstring comp;
    // Description of the completion.
    wcstring desc;
    // Condition under which to use the option.
    wcstring condition;
    // Determines how completions should be performed on the argument after the switch.
    completion_mode_t result_mode;
    // Completion flags.
    complete_flags_t flags;

    wcstring localized_desc() const { return C_(desc); }

    size_t expected_dash_count() const {
        switch (this->type) {
            case option_type_args_only:
                return 0;
            case option_type_short:
            case option_type_single_long:
                return 1;
            case option_type_double_long:
                return 2;
        }
        DIE("unreachable");
    }
};

/// Last value used in the order field of completion_entry_t.
static std::atomic<unsigned int> k_complete_order{0};

/// Struct describing a command completion.
using option_list_t = std::list<complete_entry_opt_t>;
class completion_entry_t {
   public:
    /// List of all options.
    option_list_t options;

    /// Command string.
    const wcstring cmd;
    /// True if command is a path.
    const bool cmd_is_path;
    /// Order for when this completion was created. This aids in outputting completions sorted by
    /// time.
    const unsigned int order;

    /// Getters for option list.
    const option_list_t &get_options() const;

    /// Adds or removes an option.
    void add_option(const complete_entry_opt_t &opt);
    bool remove_option(const wcstring &option, complete_option_type_t type);

    completion_entry_t(wcstring c, bool type)
        : cmd(std::move(c)), cmd_is_path(type), order(++k_complete_order) {}
};

/// Set of all completion entries.
namespace std {
template <>
struct hash<completion_entry_t> {
    size_t operator()(const completion_entry_t &c) const {
        std::hash<wcstring> hasher;
        return hasher(wcstring(c.cmd));
    }
};
template <>
struct equal_to<completion_entry_t> {
    bool operator()(const completion_entry_t &c1, const completion_entry_t &c2) const {
        return c1.cmd == c2.cmd;
    }
};
}  // namespace std
using completion_entry_set_t = std::unordered_set<completion_entry_t>;
static owning_lock<completion_entry_set_t> s_completion_set;

/// Completion "wrapper" support. The map goes from wrapping-command to wrapped-command-list.
using wrapper_map_t = std::unordered_map<wcstring, wcstring_list_t>;
static owning_lock<wrapper_map_t> wrapper_map;

/// Comparison function to sort completions by their order field.
static bool compare_completions_by_order(const completion_entry_t &p1,
                                         const completion_entry_t &p2) {
    return p1.order < p2.order;
}

void completion_entry_t::add_option(const complete_entry_opt_t &opt) { options.push_front(opt); }

const option_list_t &completion_entry_t::get_options() const { return options; }

description_func_t const_desc(const wcstring &s) {
    return [=](const wcstring &ignored) {
        UNUSED(ignored);
        return s;
    };
}

/// Clear the COMPLETE_AUTO_SPACE flag, and set COMPLETE_NO_SPACE appropriately depending on the
/// suffix of the string.
static complete_flags_t resolve_auto_space(const wcstring &comp, complete_flags_t flags) {
    complete_flags_t new_flags = flags;
    if (flags & COMPLETE_AUTO_SPACE) {
        new_flags &= ~COMPLETE_AUTO_SPACE;
        size_t len = comp.size();
        if (len > 0 && (std::wcschr(L"/=@:.,", comp.at(len - 1)) != nullptr))
            new_flags |= COMPLETE_NO_SPACE;
    }
    return new_flags;
}

/// completion_t functions. Note that the constructor resolves flags!
completion_t::completion_t(wcstring comp, wcstring desc, string_fuzzy_match_t mat,
                           complete_flags_t flags_val)
    : completion(std::move(comp)),
      description(std::move(desc)),
      match(mat),
      flags(resolve_auto_space(completion, flags_val)) {}

completion_t::completion_t(const completion_t &) = default;
completion_t::completion_t(completion_t &&) = default;
completion_t &completion_t::operator=(const completion_t &) = default;
completion_t &completion_t::operator=(completion_t &&) = default;
completion_t::~completion_t() = default;

__attribute__((always_inline)) static inline bool natural_compare_completions(
    const completion_t &a, const completion_t &b) {
    // For this to work, stable_sort must be used because results aren't interchangeable.
    if (a.flags & b.flags & COMPLETE_DONT_SORT) {
        // Both completions are from a source with the --keep-order flag.
        return false;
    }
    return wcsfilecmp(a.completion.c_str(), b.completion.c_str()) < 0;
}

bool completion_t::is_naturally_less_than(const completion_t &a, const completion_t &b) {
    return natural_compare_completions(a, b);
}

void completion_t::prepend_token_prefix(const wcstring &prefix) {
    if (this->flags & COMPLETE_REPLACES_TOKEN) {
        this->completion.insert(0, prefix);
    }
}

// If these functions aren't force inlined, it is actually faster to call
// stable_sort twice rather than to iterate once performing all comparisons in one go!
__attribute__((always_inline)) static inline bool compare_completions_by_duplicate_arguments(
    const completion_t &a, const completion_t &b) {
    bool ad = a.flags & COMPLETE_DUPLICATES_ARGUMENT;
    bool bd = b.flags & COMPLETE_DUPLICATES_ARGUMENT;
    return ad < bd;
}

__attribute__((always_inline)) static inline bool compare_completions_by_tilde(
    const completion_t &a, const completion_t &b) {
    if (a.completion.empty() || b.completion.empty()) {
        return false;
    }
    return ((a.completion.back() == L'~') < (b.completion.back() == L'~'));
}

/// Unique the list of completions, without perturbing their order.
static void unique_completions_retaining_order(completion_list_t *comps) {
    std::unordered_set<wcstring> seen;
    seen.reserve(comps->size());
    auto pred = [&seen](const completion_t &c) {
        // Remove (return true) if insertion fails.
        bool inserted = seen.insert(c.completion).second;
        return !inserted;
    };
    comps->erase(std::remove_if(comps->begin(), comps->end(), pred), comps->end());
}

void completions_sort_and_prioritize(completion_list_t *comps, completion_request_flags_t flags) {
    // Find the best match type.
    fuzzy_match_type_t best_type = fuzzy_match_none;
    for (const auto &comp : *comps) {
        best_type = std::min(best_type, comp.match.type);
        if (best_type <= fuzzy_match_prefix) {
            // We can't get better than this (see below)
            break;
        }
    }
    // If the best type is an exact match, reduce it to prefix match. Otherwise a tab completion
    // will only show one match if it matches a file exactly. (see issue #959).
    if (best_type == fuzzy_match_exact) {
        best_type = fuzzy_match_prefix;
    }

    // Throw out completions whose match types are less suitable than the best.
    comps->erase(
        std::remove_if(comps->begin(), comps->end(),
                       [&](const completion_t &comp) { return comp.match.type > best_type; }),
        comps->end());

    // Deduplicate both sorted and unsorted results.
    unique_completions_retaining_order(comps);

    // Sort, provided COMPLETE_DONT_SORT isn't set.
    stable_sort(comps->begin(), comps->end(), [](const completion_t &a, const completion_t &b) {
        return a.match.type < b.match.type || natural_compare_completions(a, b);
    });

    // Lastly, if this is for an autosuggestion, prefer to avoid completions that duplicate
    // arguments, and penalize files that end in tilde - they're frequently autosave files from e.g.
    // emacs.
    if (flags & completion_request_t::autosuggestion) {
        stable_sort(comps->begin(), comps->end(), [](const completion_t &a, const completion_t &b) {
            return compare_completions_by_duplicate_arguments(a, b) ||
                   compare_completions_by_tilde(a, b);
        });
    }
}

/// Class representing an attempt to compute completions.
class completer_t {
    /// The operation context for this completion.
    const operation_context_t &ctx;

    /// Flags associated with the completion request.
    const completion_request_flags_t flags;

    /// The output completions.
    completion_list_t completions;

    /// Table of completions conditions that have already been tested and the corresponding test
    /// results.
    using condition_cache_t = std::unordered_map<wcstring, bool>;
    condition_cache_t condition_cache;

    enum complete_type_t { COMPLETE_DEFAULT, COMPLETE_AUTOSUGGEST };

    complete_type_t type() const {
        return (flags & completion_request_t::autosuggestion) ? COMPLETE_AUTOSUGGEST
                                                              : COMPLETE_DEFAULT;
    }

    bool wants_descriptions() const { return flags & completion_request_t::descriptions; }

    bool fuzzy() const { return flags & completion_request_t::fuzzy_match; }

    fuzzy_match_type_t max_fuzzy_match_type() const {
        // If we are doing fuzzy matching, request all types; if not request only prefix matching.
        if (fuzzy()) return fuzzy_match_none;
        return fuzzy_match_prefix_case_insensitive;
    }

    bool try_complete_variable(const wcstring &str);
    bool try_complete_user(const wcstring &str);

    bool complete_param(const wcstring &cmd_orig, const wcstring &popt, const wcstring &str,
                        bool use_switches);

    void complete_param_expand(const wcstring &str, bool do_file,
                               bool handle_as_special_cd = false);

    void complete_cmd(const wcstring &str);

    /// Attempt to complete an abbreviation for the given string.
    void complete_abbr(const wcstring &cmd);

    void complete_from_args(const wcstring &str, const wcstring &args, const wcstring &desc,
                            complete_flags_t flags);

    void complete_cmd_desc(const wcstring &str);

    bool complete_variable(const wcstring &str, size_t start_offset);

    bool condition_test(const wcstring &condition);

    void complete_strings(const wcstring &wc_escaped, const description_func_t &desc_func,
                          const completion_list_t &possible_comp, complete_flags_t flags);

    expand_flags_t expand_flags() const {
        expand_flags_t result{};
        if (this->type() == COMPLETE_AUTOSUGGEST) result |= expand_flag::skip_cmdsubst;
        if (this->fuzzy()) result |= expand_flag::fuzzy_match;
        if (this->wants_descriptions()) result |= expand_flag::gen_descriptions;
        return result;
    }

    // Bag of data to support expanding a command's arguments using custom completions, including
    // the wrap chain.
    struct custom_arg_data_t {
        explicit custom_arg_data_t(wcstring_list_t *vars) : var_assignments(vars) { assert(vars); }

        // The unescaped argument before the argument which is being completed, or empty if none.
        wcstring previous_argument{};

        // The unescaped argument which is being completed, or empty if none.
        wcstring current_argument{};

        // Whether a -- has been encountered, which suppresses options.
        bool had_ddash{false};

        // Whether to perform file completions.
        // This is an "out" parameter of the wrap chain walk: if any wrapped command suppresses file
        // completions this gets set to false.
        bool do_file{true};

        // Depth in the wrap chain.
        size_t wrap_depth{0};

        // The list of variable assignments: escaped strings of the form VAR=VAL.
        // This may be temporarily appended to as we explore the wrap chain.
        // When completing, variable assignments are really set in a local scope.
        wcstring_list_t *var_assignments;

        // The set of wrapped commands which we have visited, and so should not be explored again.
        std::set<wcstring> visited_wrapped_commands{};
    };

    void complete_custom(const wcstring &cmd, const wcstring &cmdline, custom_arg_data_t *ad);

    void walk_wrap_chain(const wcstring &cmd, const wcstring &cmdline, source_range_t cmdrange,
                         custom_arg_data_t *ad);

    cleanup_t apply_var_assignments(const wcstring_list_t &var_assignments);

    bool empty() const { return completions.empty(); }

    void escape_opening_brackets(const wcstring &argument);

    void mark_completions_duplicating_arguments(const wcstring &cmd, const wcstring &prefix,
                                                const std::vector<tok_t> &args);

   public:
    completer_t(const operation_context_t &ctx, completion_request_flags_t f)
        : ctx(ctx), flags(f) {}

    void perform_for_commandline(wcstring cmdline);

    completion_list_t acquire_completions() { return std::move(completions); }
};

// Autoloader for completions.
static owning_lock<autoload_t> completion_autoloader{autoload_t(L"fish_complete_path")};

/// Create a new completion entry.
void append_completion(completion_list_t *completions, wcstring comp, wcstring desc,
                       complete_flags_t flags, string_fuzzy_match_t match) {
    completions->emplace_back(std::move(comp), std::move(desc), match, flags);
}

/// Test if the specified script returns zero. The result is cached, so that if multiple completions
/// use the same condition, it needs only be evaluated once. condition_cache_clear must be called
/// after a completion run to make sure that there are no stale completions.
bool completer_t::condition_test(const wcstring &condition) {
    if (condition.empty()) {
        // std::fwprintf( stderr, L"No condition specified\n" );
        return true;
    }
    if (!ctx.parser) {
        return false;
    }

    ASSERT_IS_MAIN_THREAD();
    bool test_res;
    auto cached_entry = condition_cache.find(condition);
    if (cached_entry == condition_cache.end()) {
        // Compute new value and reinsert it.
        test_res =
            (0 == exec_subshell(condition, *ctx.parser, false /* don't apply exit status */));
        condition_cache[condition] = test_res;
    } else {
        // Use the old value.
        test_res = cached_entry->second;
    }
    return test_res;
}

/// Locate the specified entry. Create it if it doesn't exist. Must be called while locked.
static completion_entry_t &complete_get_exact_entry(completion_entry_set_t &completion_set,
                                                    const wcstring &cmd, bool cmd_is_path) {
    auto ins = completion_set.emplace(cmd, cmd_is_path);

    // NOTE SET_ELEMENTS_ARE_IMMUTABLE: Exposing mutable access here is only okay as long as callers
    // do not change any field that matters to ordering - affecting order without telling std::set
    // invalidates its internal state.
    return const_cast<completion_entry_t &>(*ins.first);
}

void complete_add(const wchar_t *cmd, bool cmd_is_path, const wcstring &option,
                  complete_option_type_t option_type, completion_mode_t result_mode,
                  const wchar_t *condition, const wchar_t *comp, const wchar_t *desc,
                  complete_flags_t flags) {
    assert(cmd && "Null command");
    // option should be empty iff the option type is arguments only.
    assert(option.empty() == (option_type == option_type_args_only));

    // Lock the lock that allows us to edit the completion entry list.
    auto completion_set = s_completion_set.acquire();
    completion_entry_t &c = complete_get_exact_entry(*completion_set, cmd, cmd_is_path);

    // Create our new option.
    complete_entry_opt_t opt;
    opt.option = option;
    opt.type = option_type;
    opt.result_mode = result_mode;

    if (comp) opt.comp = comp;
    if (condition) opt.condition = condition;
    if (desc) opt.desc = desc;
    opt.flags = flags;

    c.add_option(opt);
}

/// Remove all completion options in the specified entry that match the specified short / long
/// option strings. Returns true if it is now empty and should be deleted, false if it's not empty.
/// Must be called while locked.
bool completion_entry_t::remove_option(const wcstring &option, complete_option_type_t type) {
    auto iter = this->options.begin();
    while (iter != this->options.end()) {
        if (iter->option == option && iter->type == type) {
            iter = this->options.erase(iter);
        } else {
            // Just go to the next one.
            ++iter;
        }
    }
    return this->options.empty();
}

void complete_remove(const wcstring &cmd, bool cmd_is_path, const wcstring &option,
                     complete_option_type_t type) {
    auto completion_set = s_completion_set.acquire();

    completion_entry_t tmp_entry(cmd, cmd_is_path);
    auto iter = completion_set->find(tmp_entry);
    if (iter != completion_set->end()) {
        // const_cast: See SET_ELEMENTS_ARE_IMMUTABLE.
        auto &entry = const_cast<completion_entry_t &>(*iter);

        bool delete_it = entry.remove_option(option, type);
        if (delete_it) {
            completion_set->erase(iter);
        }
    }
}

void complete_remove_all(const wcstring &cmd, bool cmd_is_path) {
    auto completion_set = s_completion_set.acquire();
    completion_entry_t tmp_entry(cmd, cmd_is_path);
    completion_set->erase(tmp_entry);
}

/// Find the full path and commandname from a command string 'str'.
static void parse_cmd_string(const wcstring &str, wcstring *path, wcstring *cmd,
                             const environment_t &vars) {
    if (!path_get_path(str, path, vars)) {
        /// Use the empty string as the 'path' for commands that can not be found.
        path->clear();
    }

    // Make sure the path is not included in the command.
    size_t last_slash = str.find_last_of(L'/');
    if (last_slash != wcstring::npos) {
        *cmd = str.substr(last_slash + 1);
    } else {
        *cmd = str;
    }
}

/// Copy any strings in possible_comp which have the specified prefix to the
/// completer's completion array. The prefix may contain wildcards. The output
/// will consist of completion_t structs.
///
/// There are three ways to specify descriptions for each completion. Firstly,
/// if a description has already been added to the completion, it is _not_
/// replaced. Secondly, if the desc_func function is specified, use it to
/// determine a dynamic completion. Thirdly, if none of the above are available,
/// the desc string is used as a description.
///
/// @param  wc_escaped
///    the prefix, possibly containing wildcards. The wildcard should not have
///    been unescaped, i.e. '*' should be used for any string, not the
///    ANY_STRING character.
/// @param  desc_func
///    the function that generates a description for those completions without an
///    embedded description
/// @param  possible_comp
///    the list of possible completions to iterate over
/// @param  flags
///    The flags
void completer_t::complete_strings(const wcstring &wc_escaped, const description_func_t &desc_func,
                                   const completion_list_t &possible_comp, complete_flags_t flags) {
    wcstring tmp = wc_escaped;
    if (!expand_one(tmp,
                    this->expand_flags() | expand_flag::skip_cmdsubst | expand_flag::skip_wildcards,
                    ctx))
        return;

    const wcstring wc = parse_util_unescape_wildcards(tmp);

    for (const auto &comp : possible_comp) {
        const wcstring &comp_str = comp.completion;
        if (!comp_str.empty()) {
            wildcard_complete(comp_str, wc.c_str(), desc_func, &this->completions,
                              this->expand_flags(), flags);
        }
    }
}

/// If command to complete is short enough, substitute the description with the whatis information
/// for the executable.
void completer_t::complete_cmd_desc(const wcstring &str) {
    ASSERT_IS_MAIN_THREAD();
    if (!ctx.parser) return;

    wcstring cmd;
    size_t pos = str.find_last_of(L'/');
    if (pos != std::string::npos) {
        if (pos + 1 > str.length()) return;
        cmd = wcstring(str, pos + 1);
    } else {
        cmd = str;
    }

    // Using apropos with a single-character search term produces far to many results - require at
    // least two characters if we don't know the location of the whatis-database.
    if (cmd.length() < 2) return;

    if (wildcard_has(cmd, false)) {
        return;
    }

    bool skip = true;
    for (const auto &c : completions) {
        if (c.completion.empty() || (c.completion[c.completion.size() - 1] != L'/')) {
            skip = false;
            break;
        }
    }

    if (skip) {
        return;
    }

    wcstring lookup_cmd(L"__fish_describe_command ");
    lookup_cmd.append(escape_string(cmd, ESCAPE_ALL));

    // First locate a list of possible descriptions using a single call to apropos or a direct
    // search if we know the location of the whatis database. This can take some time on slower
    // systems with a large set of manuals, but it should be ok since apropos is only called once.
    wcstring_list_t list;
    (void)exec_subshell(lookup_cmd, *ctx.parser, list, false /* don't apply exit status */);

    // Then discard anything that is not a possible completion and put the result into a
    // hashtable with the completion as key and the description as value.
    std::unordered_map<wcstring, wcstring> lookup;
    // A typical entry is the command name, followed by a tab, followed by a description.
    for (const wcstring &elstr : list) {
        // Skip keys that are too short.
        if (elstr.size() < cmd.size()) continue;

        // Skip cases without a tab, or without a description, or bizarre cases where the tab is
        // part of the command.
        size_t tab_idx = elstr.find(L'\t');
        if (tab_idx == wcstring::npos || tab_idx + 1 >= elstr.size() || tab_idx < cmd.size())
            continue;

        // Make the key. This is the stuff after the command.
        // For example:
        //  elstr = lsmod
        //  cmd = ls
        //  key = mod
        // Note an empty key is common and natural, if 'cmd' were already valid.
        wcstring key(elstr, cmd.size(), tab_idx - cmd.size());
        wcstring val(elstr, tab_idx + 1);
        assert(!val.empty() && "tab index should not have been at the end.");

        // And once again I make sure the first character is uppercased because I like it that
        // way, and I get to decide these things.
        val.at(0) = towupper(val.at(0));
        lookup.insert(std::make_pair(std::move(key), std::move(val)));
    }

    // Then do a lookup on every completion and if a match is found, change to the new
    // description.
    for (auto &completion : completions) {
        const wcstring &el = completion.completion;
        auto new_desc_iter = lookup.find(el);
        if (new_desc_iter != lookup.end()) completion.description = new_desc_iter->second;
    }
}

/// Returns a description for the specified function, or an empty string if none.
static wcstring complete_function_desc(const wcstring &fn) {
    wcstring result;
    bool has_description = function_get_desc(fn, result);
    if (!has_description) {
        function_get_definition(fn, result);
        // A completion description is a single line.
        for (wchar_t &c : result) {
            if (c == L'\n') c = L' ';
        }
    }
    return result;
}

/// Complete the specified command name. Search for executables in the path, executables defined
/// using an absolute path, functions, builtins and directories for implicit cd commands.
///
/// \param str_cmd the command string to find completions for
void completer_t::complete_cmd(const wcstring &str_cmd) {
    completion_list_t possible_comp;

    // Append all possible executables
    expand_result_t result =
        expand_string(str_cmd, &this->completions,
                      this->expand_flags() | expand_flag::special_for_command |
                          expand_flag::for_completions | expand_flag::executables_only,
                      ctx);
    if (result == expand_result_t::cancel) {
        return;
    }
    if (result == expand_result_t::ok && this->wants_descriptions()) {
        this->complete_cmd_desc(str_cmd);
    }

    // We don't really care if this succeeds or fails. If it succeeds this->completions will be
    // updated with choices for the user.
    expand_result_t ignore =
        // Append all matching directories
        expand_string(
            str_cmd, &this->completions,
            this->expand_flags() | expand_flag::for_completions | expand_flag::directories_only,
            ctx);
    UNUSED(ignore);

    if (str_cmd.empty() || (str_cmd.find(L'/') == wcstring::npos && str_cmd.at(0) != L'~')) {
        bool include_hidden = !str_cmd.empty() && str_cmd.at(0) == L'_';
        wcstring_list_t names = function_get_names(include_hidden);
        for (wcstring &name : names) {
            // Append all known matching functions
            append_completion(&possible_comp, std::move(name));
        }

        this->complete_strings(str_cmd, complete_function_desc, possible_comp, 0);

        possible_comp.clear();

        // Append all matching builtins
        builtin_get_names(&possible_comp);
        this->complete_strings(str_cmd, builtin_get_desc, possible_comp, 0);
    }
}

void completer_t::complete_abbr(const wcstring &cmd) {
    std::map<wcstring, wcstring> abbrs = get_abbreviations(ctx.vars);
    completion_list_t possible_comp;
    possible_comp.reserve(abbrs.size());
    for (const auto &kv : abbrs) {
        possible_comp.emplace_back(kv.first);
    }

    auto desc_func = [&](const wcstring &key) {
        auto iter = abbrs.find(key);
        assert(iter != abbrs.end() && "Abbreviation not found");
        return format_string(ABBR_DESC, iter->second.c_str());
    };
    this->complete_strings(cmd, desc_func, possible_comp, COMPLETE_NO_SPACE);
}

/// Evaluate the argument list (as supplied by complete -a) and insert any
/// return matching completions. Matching is done using @c
/// copy_strings_with_prefix, meaning the completion may contain wildcards.
/// Logically, this is not always the right thing to do, but I have yet to come
/// up with a case where this matters.
///
/// @param  str
///    The string to complete.
/// @param  args
///    The list of option arguments to be evaluated.
/// @param  desc
///    Description of the completion
/// @param  flags
///    The flags
///
void completer_t::complete_from_args(const wcstring &str, const wcstring &args,
                                     const wcstring &desc, complete_flags_t flags) {
    bool is_autosuggest = (this->type() == COMPLETE_AUTOSUGGEST);

    bool saved_interactive = false;
    if (ctx.parser) {
        saved_interactive = ctx.parser->libdata().is_interactive;
        ctx.parser->libdata().is_interactive = false;
    }

    expand_flags_t eflags{};
    if (is_autosuggest) {
        eflags |= expand_flag::skip_cmdsubst;
    }

    completion_list_t possible_comp = parser_t::expand_argument_list(args, eflags, ctx);

    if (ctx.parser) {
        ctx.parser->libdata().is_interactive = saved_interactive;
    }

    this->complete_strings(escape_string(str, ESCAPE_ALL), const_desc(desc), possible_comp, flags);
}

static size_t leading_dash_count(const wchar_t *str) {
    size_t cursor = 0;
    while (str[cursor] == L'-') {
        cursor++;
    }
    return cursor;
}

/// Match a parameter.
static bool param_match(const complete_entry_opt_t *e, const wchar_t *optstr) {
    bool result = false;
    if (e->type != option_type_args_only) {
        size_t dashes = leading_dash_count(optstr);
        result = (dashes == e->expected_dash_count() && e->option == &optstr[dashes]);
    }
    return result;
}

/// Test if a string is an option with an argument, like --color=auto or -I/usr/include.
static const wchar_t *param_match2(const complete_entry_opt_t *e, const wchar_t *optstr) {
    // We may get a complete_entry_opt_t with no options if it's just arguments.
    if (e->option.empty()) {
        return nullptr;
    }

    // Verify leading dashes.
    size_t cursor = leading_dash_count(optstr);
    if (cursor != e->expected_dash_count()) {
        return nullptr;
    }

    // Verify options match.
    if (!string_prefixes_string(e->option, &optstr[cursor])) {
        return nullptr;
    }
    cursor += e->option.length();

    // Short options are like -DNDEBUG. Long options are like --color=auto. So check for an equal
    // sign for long options.
    assert(e->type != option_type_short);
    if (optstr[cursor] != L'=') {
        return nullptr;
    }
    cursor += 1;
    return &optstr[cursor];
}

/// Parses a token of short options plus one optional parameter like
/// '-xzPARAM', where x and z are short options.
///
/// Returns the position of the last option character (e.g. the position of z which is 2).
/// Everything after that is assumed to be part of the parameter.
/// Returns wcstring::npos if there is no valid short option.
static size_t short_option_pos(const wcstring &arg, const option_list_t &options) {
    if (arg.size() <= 1 || leading_dash_count(arg.c_str()) != 1) {
        return wcstring::npos;
    }
    for (size_t pos = 1; pos < arg.size(); pos++) {
        wchar_t arg_char = arg.at(pos);
        const complete_entry_opt_t *match = nullptr;
        for (const complete_entry_opt_t &o : options) {
            if (o.type == option_type_short && o.option.at(0) == arg_char) {
                match = &o;
                break;
            }
        }
        if (match == nullptr) {
            // The first character after the dash is not a valid option.
            if (pos == 1) return wcstring::npos;
            return pos - 1;
        }
        if (match->result_mode.requires_param) {
            return pos;
        }
    }
    return arg.size() - 1;
}

/// Load command-specific completions for the specified command.
static void complete_load(const wcstring &name) {
    // We have to load this as a function, since it may define a --wraps or signature.
    // See issue #2466.
    auto &parser = parser_t::principal_parser();
    function_load(name, parser);

    // It's important to NOT hold the lock around completion loading.
    // We need to take the lock to decide what to load, drop it to perform the load, then reacquire
    // it.
    // Note we only look at the global fish_function_path and fish_complete_path.
    maybe_t<wcstring> path_to_load =
        completion_autoloader.acquire()->resolve_command(name, env_stack_t::globals());
    if (path_to_load) {
        autoload_t::perform_autoload(*path_to_load, parser);
        completion_autoloader.acquire()->mark_autoload_finished(name);
    }
}

/// complete_param: Given a command, find completions for the argument str of command cmd_orig with
/// previous option popt.
///
/// Examples in format (cmd, popt, str):
///
///   echo hello world <tab> -> ("echo", "world", "")
///   echo hello world<tab> -> ("echo", "hello", "world")
///
/// Insert results into comp_out. Return true to perform file completion, false to disable it.
bool completer_t::complete_param(const wcstring &cmd_orig, const wcstring &popt,
                                 const wcstring &str, bool use_switches) {
    bool use_common = true, use_files = true, has_force = false;

    wcstring cmd, path;
    parse_cmd_string(cmd_orig, &path, &cmd, ctx.vars);

    // FLOGF(error, L"\nThinking about looking up completions for %ls\n", cmd.c_str());
    bool head_exists = builtin_exists(cmd);
    // Only reload environment variables if builtin_exists returned false, as an optimization
    if (!head_exists) {
        head_exists = function_exists_no_autoload(cmd);
        // While it may seem like first testing `path_get_path` before resorting to an env lookup
        // may be faster, path_get_path can potentially do a lot of FS/IO access, so env.get() +
        // function_exists() should still be faster.
        // Use cmd_orig here as it is potentially pathed.
        head_exists = head_exists || path_get_path(cmd_orig, nullptr, ctx.vars);
    }

    if (!head_exists) {
        // Do not load custom completions if the head does not exist
        // This prevents errors caused during the execution of completion providers for
        // tools that do not exist. Applies to both manual completions ("cm<TAB>", "cmd <TAB>")
        // and automatic completions ("gi" autosuggestion provider -> git)
        FLOG(complete, "Skipping completions for non-existent head");
    } else {
        iothread_perform_on_main([&]() { complete_load(cmd); });
    }

    // Make a list of lists of all options that we care about.
    std::vector<option_list_t> all_options;
    {
        auto completion_set = s_completion_set.acquire();
        for (const completion_entry_t &i : *completion_set) {
            const wcstring &match = i.cmd_is_path ? path : cmd;
            if (wildcard_match(match, i.cmd)) {
                // Copy all of their options into our list.
                all_options.push_back(i.get_options());  // Oof, this is a lot of copying
            }
        }
    }

    // Now release the lock and test each option that we captured above. We have to do this outside
    // the lock because callouts (like the condition) may add or remove completions. See issue 2.
    for (const option_list_t &options : all_options) {
        size_t short_opt_pos = short_option_pos(str, options);
        bool last_option_requires_param = false;
        use_common = true;
        if (use_switches) {
            if (str[0] == L'-') {
                // Check if we are entering a combined option and argument (like --color=auto or
                // -I/usr/include).
                for (const complete_entry_opt_t &o : options) {
                    const wchar_t *arg;
                    if (o.type == option_type_short) {
                        if (short_opt_pos == wcstring::npos) continue;
                        if (o.option.at(0) != str.at(short_opt_pos)) continue;
                        last_option_requires_param = o.result_mode.requires_param;
                        arg = str.c_str() + short_opt_pos + 1;
                    } else {
                        arg = param_match2(&o, str.c_str());
                    }
                    if (arg != nullptr && this->condition_test(o.condition)) {
                        if (o.result_mode.requires_param) use_common = false;
                        if (o.result_mode.no_files) use_files = false;
                        if (o.result_mode.force_files) has_force = true;
                        complete_from_args(arg, o.comp, o.localized_desc(), o.flags);
                    }
                }
            } else if (popt[0] == L'-') {
                // Set to true if we found a matching old-style switch.
                // Here we are testing the previous argument,
                // to see how we should complete the current argument
                bool old_style_match = false;

                // If we are using old style long options, check for them first.
                for (const complete_entry_opt_t &o : options) {
                    if (o.type == option_type_single_long && param_match(&o, popt.c_str()) &&
                        this->condition_test(o.condition)) {
                        old_style_match = true;
                        if (o.result_mode.requires_param) use_common = false;
                        if (o.result_mode.no_files) use_files = false;
                        if (o.result_mode.force_files) has_force = true;
                        complete_from_args(str, o.comp, o.localized_desc(), o.flags);
                    }
                }

                // No old style option matched, or we are not using old style options. We check if
                // any short (or gnu style) options do.
                if (!old_style_match) {
                    size_t prev_short_opt_pos = short_option_pos(popt, options);
                    for (const complete_entry_opt_t &o : options) {
                        // Gnu-style options with _optional_ arguments must be specified as a single
                        // token, so that it can be differed from a regular argument.
                        // Here we are testing the previous argument for a GNU-style match,
                        // to see how we should complete the current argument
                        if (!o.result_mode.requires_param) continue;

                        bool match = false;
                        if (o.type == option_type_short) {
                            match = prev_short_opt_pos != wcstring::npos &&
                                    // Only if the option was the last char in the token,
                                    // i.e. there is no parameter yet.
                                    prev_short_opt_pos + 1 == popt.size() &&
                                    o.option.at(0) == popt.at(prev_short_opt_pos);
                        } else if (o.type == option_type_double_long) {
                            match = param_match(&o, popt.c_str());
                        }
                        if (match && this->condition_test(o.condition)) {
                            if (o.result_mode.requires_param) use_common = false;
                            if (o.result_mode.no_files) use_files = false;
                            if (o.result_mode.force_files) has_force = true;
                            complete_from_args(str, o.comp, o.localized_desc(), o.flags);
                        }
                    }
                }
            }
        }

        if (!use_common) {
            continue;
        }

        // Now we try to complete an option itself
        for (const complete_entry_opt_t &o : options) {
            // If this entry is for the base command, check if any of the arguments match.
            if (!this->condition_test(o.condition)) continue;
            if (o.option.empty()) {
                use_files = use_files && (!(o.result_mode.no_files));
                complete_from_args(str, o.comp, o.localized_desc(), o.flags);
            }

            if (!use_switches || str.empty()) {
                continue;
            }

            // Check if the short style option matches.
            if (o.type == option_type_short) {
                wchar_t optchar = o.option.at(0);
                if (short_opt_pos == wcstring::npos) {
                    // str has no short option at all (but perhaps it is the
                    // prefix of a single long option).
                    // Only complete short options if there is no character after the dash.
                    if (str != L"-") continue;
                } else {
                    // Only complete when the last short option has no parameter yet..
                    if (short_opt_pos + 1 != str.size()) continue;
                    // .. and it does not require one ..
                    if (last_option_requires_param) continue;
                    // .. and the option is not already there.
                    if (str.find(optchar) != wcstring::npos) continue;
                }
                // It's a match.
                wcstring desc = o.localized_desc();
                // Append a short-style option
                append_completion(&this->completions, o.option, std::move(desc), 0);
            }

            // Check if the long style option matches.
            if (o.type != option_type_single_long && o.type != option_type_double_long) {
                continue;
            }

            wcstring whole_opt(o.expected_dash_count(), L'-');
            whole_opt.append(o.option);

            if (whole_opt.length() < str.length()) {
                continue;
            }
            int match = string_prefixes_string(str, whole_opt);
            if (!match) {
                bool match_no_case = wcsncasecmp(str.c_str(), whole_opt.c_str(), str.length()) == 0;
                if (!match_no_case) {
                    continue;
                }
            }

            int has_arg = 0;  // does this switch have any known arguments
            int req_arg = 0;  // does this switch _require_ an argument
            size_t offset = 0;
            complete_flags_t flags = 0;

            if (match) {
                offset = str.length();
            } else {
                flags = COMPLETE_REPLACES_TOKEN;
            }

            has_arg = !o.comp.empty();
            req_arg = o.result_mode.requires_param;

            if (o.type == option_type_double_long && (has_arg && !req_arg)) {
                // Optional arguments to a switch can only be handled using the '=', so we add it as
                // a completion. By default we avoid using '=' and instead rely on '--switch
                // switch-arg', since it is more commonly supported by homebrew getopt-like
                // functions.
                wcstring completion = format_string(L"%ls=", whole_opt.c_str() + offset);
                // Append a long-style option with a mandatory trailing equal sign
                append_completion(&this->completions, std::move(completion), C_(o.desc),
                                  flags | COMPLETE_NO_SPACE);
            }

            // Append a long-style option
            append_completion(&this->completions, whole_opt.substr(offset), C_(o.desc), flags);
        }
    }

    return has_force || use_files;
}

/// Perform generic (not command-specific) expansions on the specified string.
void completer_t::complete_param_expand(const wcstring &str, bool do_file,
                                        bool handle_as_special_cd) {
    if (ctx.check_cancel()) return;
    expand_flags_t flags =
        this->expand_flags() | expand_flag::skip_cmdsubst | expand_flag::for_completions;

    if (!do_file) flags |= expand_flag::skip_wildcards;

    if (handle_as_special_cd && do_file) {
        if (this->type() == COMPLETE_AUTOSUGGEST) {
            flags |= expand_flag::special_for_cd_autosuggestion;
        }
        flags |= expand_flag::directories_only;
        flags |= expand_flag::special_for_cd;
    }

    // Squelch file descriptions per issue #254.
    if (this->type() == COMPLETE_AUTOSUGGEST || do_file) flags.clear(expand_flag::gen_descriptions);

    // We have the following cases:
    //
    // --foo=bar => expand just bar
    // -foo=bar => expand just bar
    // foo=bar => expand the whole thing, and also just bar
    //
    // We also support colon separator (#2178). If there's more than one, prefer the last one.
    size_t sep_index = str.find_last_of(L"=:");
    bool complete_from_separator = (sep_index != wcstring::npos);
    bool complete_from_start = !complete_from_separator || !string_prefixes_string(L"-", str);

    if (complete_from_separator) {
        // FIXME: This just cuts the token,
        // so any quoting or braces gets lost.
        // See #4954.
        const wcstring sep_string = wcstring(str, sep_index + 1);
        completion_list_t local_completions;
        if (expand_string(sep_string, &local_completions, flags, ctx) == expand_result_t::error) {
            FLOGF(complete, L"Error while expanding string '%ls'", sep_string.c_str());
        }

        // Any COMPLETE_REPLACES_TOKEN will also stomp the separator. We need to "repair" them by
        // inserting our separator and prefix.
        const wcstring prefix_with_sep = wcstring(str, 0, sep_index + 1);
        for (completion_t &comp : local_completions) {
            comp.prepend_token_prefix(prefix_with_sep);
        }
        this->completions.insert(this->completions.end(), local_completions.begin(),
                                 local_completions.end());
    }

    if (complete_from_start) {
        // Don't do fuzzy matching for files if the string begins with a dash (issue #568). We could
        // consider relaxing this if there was a preceding double-dash argument.
        if (string_prefixes_string(L"-", str)) flags.clear(expand_flag::fuzzy_match);

        if (expand_string(str, &this->completions, flags, ctx) == expand_result_t::error) {
            FLOGF(complete, L"Error while expanding string '%ls'", str.c_str());
        }
    }
}

/// Complete the specified string as an environment variable.
bool completer_t::complete_variable(const wcstring &str, size_t start_offset) {
    const wchar_t *const whole_var = str.c_str();
    const wchar_t *var = &whole_var[start_offset];
    size_t varlen = str.length() - start_offset;
    bool res = false;

    for (const wcstring &env_name : ctx.vars.get_names(0)) {
        string_fuzzy_match_t match =
            string_fuzzy_match_string(var, env_name, this->max_fuzzy_match_type());
        if (match.type == fuzzy_match_none) {
            continue;  // no match
        }

        wcstring comp;
        int flags = 0;

        if (!match_type_requires_full_replacement(match.type)) {
            // Take only the suffix.
            comp.append(env_name.c_str() + varlen);
        } else {
            comp.append(whole_var, start_offset);
            comp.append(env_name);
            flags = COMPLETE_REPLACES_TOKEN | COMPLETE_DONT_ESCAPE;
        }

        wcstring desc;
        if (this->wants_descriptions()) {
            if (this->type() != COMPLETE_AUTOSUGGEST) {
                // $history can be huge, don't put all of it in the completion description; see
                // #6288.
                if (env_name == L"history") {
                    history_t *history =
                        &history_t::history_with_name(history_session_id(ctx.vars));
                    for (size_t i = 1; i < history->size() && desc.size() < 64; i++) {
                        if (i > 1) desc += L' ';
                        desc += expand_escape_string(history->item_at_index(i).str());
                    }
                } else {
                    // Can't use ctx.vars here, it could be any variable.
                    auto var = ctx.vars.get(env_name);
                    if (!var) continue;

                    wcstring value = expand_escape_variable(*var);
                    desc = format_string(COMPLETE_VAR_DESC_VAL, value.c_str());
                }
            }
        }

        // Append matching environment variables
        append_completion(&this->completions, std::move(comp), std::move(desc), flags, match);

        res = true;
    }

    return res;
}

bool completer_t::try_complete_variable(const wcstring &str) {
    enum { e_unquoted, e_single_quoted, e_double_quoted } mode = e_unquoted;
    const size_t len = str.size();

    // Get the position of the dollar heading a (possibly empty) run of valid variable characters.
    // npos means none.
    size_t variable_start = wcstring::npos;

    for (size_t in_pos = 0; in_pos < len; in_pos++) {
        wchar_t c = str.at(in_pos);
        if (!valid_var_name_char(c)) {
            // This character cannot be in a variable, reset the dollar.
            variable_start = -1;
        }

        switch (c) {
            case L'\\': {
                in_pos++;
                break;
            }
            case L'$': {
                if (mode == e_unquoted || mode == e_double_quoted) {
                    variable_start = in_pos;
                }
                break;
            }
            case L'\'': {
                if (mode == e_single_quoted) {
                    mode = e_unquoted;
                } else if (mode == e_unquoted) {
                    mode = e_single_quoted;
                }
                break;
            }
            case L'"': {
                if (mode == e_double_quoted) {
                    mode = e_unquoted;
                } else if (mode == e_unquoted) {
                    mode = e_double_quoted;
                }
                break;
            }
            default: {
                break;  // all other chars ignored here
            }
        }
    }

    // Now complete if we have a variable start. Note the variable text may be empty; in that case
    // don't generate an autosuggestion, but do allow tab completion.
    bool allow_empty = !(this->flags & completion_request_t::autosuggestion);
    bool text_is_empty = (variable_start == len);
    bool result = false;
    if (variable_start != wcstring::npos && (allow_empty || !text_is_empty)) {
        result = this->complete_variable(str, variable_start + 1);
    }
    return result;
}

/// Try to complete the specified string as a username. This is used by ~USER type expansion.
///
/// \return false if unable to complete, true otherwise
bool completer_t::try_complete_user(const wcstring &str) {
#ifndef HAVE_GETPWENT
    // The getpwent() function does not exist on Android. A Linux user on Android isn't
    // really a user - each installed app gets an UID assigned. Listing all UID:s is not
    // possible without root access, and doing a ~USER type expansion does not make sense
    // since every app is sandboxed and can't access eachother.
    return false;
#else
    const wchar_t *cmd = str.c_str();
    const wchar_t *first_char = cmd;

    if (*first_char != L'~' || std::wcschr(first_char, L'/')) return false;

    const wchar_t *user_name = first_char + 1;
    const wchar_t *name_end = std::wcschr(user_name, L'~');
    if (name_end) return false;

    double start_time = timef();
    bool result = false;
    size_t name_len = str.length() - 1;

    static std::mutex s_setpwent_lock;
    scoped_lock locker(s_setpwent_lock);
    setpwent();
    // cppcheck-suppress getpwentCalled
    while (struct passwd *pw = getpwent()) {
        if (ctx.check_cancel()) {
            break;
        }
        const wcstring pw_name_str = str2wcstring(pw->pw_name);
        const wchar_t *pw_name = pw_name_str.c_str();
        if (std::wcsncmp(user_name, pw_name, name_len) == 0) {
            wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
            // Append a user name
            append_completion(&this->completions, &pw_name[name_len], std::move(desc),
                              COMPLETE_NO_SPACE);
            result = true;
        } else if (wcsncasecmp(user_name, pw_name, name_len) == 0) {
            wcstring name = format_string(L"~%ls", pw_name);
            wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
            // Append a user name
            append_completion(&this->completions, std::move(name), std::move(desc),
                              COMPLETE_REPLACES_TOKEN | COMPLETE_DONT_ESCAPE | COMPLETE_NO_SPACE);
            result = true;
        }

        // If we've spent too much time (more than 200 ms) doing this give up.
        if (timef() - start_time > 0.2) break;
    }

    endpwent();
    return result;
#endif
}

// If we have variable assignments, attempt to apply them in our parser. As soon as the return
// value goes out of scope, the variables will be removed from the parser.
cleanup_t completer_t::apply_var_assignments(const wcstring_list_t &var_assignments) {
    if (!ctx.parser || var_assignments.empty()) return cleanup_t{[] {}};
    env_stack_t &vars = ctx.parser->vars();
    assert(&vars == &ctx.vars &&
           "Don't know how to tab complete with a parser but a different variable set");

    // clone of parse_execution_context_t::apply_variable_assignments.
    // Crucially do NOT expand subcommands:
    //   VAR=(launch_missiles) cmd<tab>
    // should not launch missiles.
    // Note we also do NOT send --on-variable events.
    const expand_flags_t expand_flags = expand_flag::skip_cmdsubst;
    const block_t *block = ctx.parser->push_block(block_t::variable_assignment_block());
    for (const wcstring &var_assign : var_assignments) {
        maybe_t<size_t> equals_pos = variable_assignment_equals_pos(var_assign);
        assert(equals_pos && "All variable assignments should have equals position");
        const wcstring variable_name = var_assign.substr(0, *equals_pos);
        const wcstring expression = var_assign.substr(*equals_pos + 1);

        completion_list_t expression_expanded;
        auto expand_ret = expand_string(expression, &expression_expanded, expand_flags, ctx);
        // If expansion succeeds, set the value; if it fails (e.g. it has a cmdsub) set an empty
        // value anyways.
        wcstring_list_t vals;
        if (expand_ret == expand_result_t::ok) {
            for (auto &completion : expression_expanded) {
                vals.emplace_back(std::move(completion.completion));
            }
        }
        ctx.parser->vars().set(variable_name, ENV_LOCAL | ENV_EXPORT, std::move(vals));
        if (ctx.check_cancel()) break;
    }
    return cleanup_t([=] { ctx.parser->pop_block(block); });
}

// Complete a command by invoking user-specified completions.
void completer_t::complete_custom(const wcstring &cmd, const wcstring &cmdline,
                                  custom_arg_data_t *ad) {
    if (ctx.check_cancel()) return;

    bool is_autosuggest = this->type() == COMPLETE_AUTOSUGGEST;
    // Perhaps set a transient commandline so that custom completions
    // buitin_commandline will refer to the wrapped command. But not if
    // we're doing autosuggestions.
    maybe_t<cleanup_t> remove_transient{};
    bool wants_transient = ad->wrap_depth > 0 && !is_autosuggest;
    if (wants_transient) {
        ctx.parser->libdata().transient_commandlines.push_back(cmdline);
        remove_transient.emplace([=] { ctx.parser->libdata().transient_commandlines.pop_back(); });
    }

    // Maybe apply variable assignments.
    cleanup_t restore_vars{apply_var_assignments(*ad->var_assignments)};
    if (ctx.check_cancel()) return;

    if (!complete_param(cmd, ad->previous_argument, ad->current_argument,
                        !ad->had_ddash)) {  // Invoke any custom completions for this command.
        ad->do_file = false;
    }
}

// Invoke command-specific completions given by \p arg_data.
// Then, for each target wrapped by the given command, update the command
// line with that target and invoke this recursively.
// The command whose completions to use is given by \p cmd. The full command line is given by \p
// cmdline and the command's range in it is given by \p cmdrange. Note: the command range
// may have a different length than the command itself, because the command is unescaped (i.e.
// quotes removed).
void completer_t::walk_wrap_chain(const wcstring &cmd, const wcstring &cmdline,
                                  source_range_t cmdrange, custom_arg_data_t *ad) {
    // Limit our recursion depth. This prevents cycles in the wrap chain graph from overflowing.
    if (ad->wrap_depth > 24) return;
    if (ctx.cancel_checker()) return;

    // Extract command from the command line and invoke the receiver with it.
    complete_custom(cmd, cmdline, ad);

    wcstring_list_t targets = complete_get_wrap_targets(cmd);
    scoped_push<size_t> saved_depth(&ad->wrap_depth, ad->wrap_depth + 1);

    for (const wcstring &wt : targets) {
        // We may append to the variable assignment list; ensure we restore it.
        const size_t saved_var_count = ad->var_assignments->size();
        cleanup_t restore_vars([=] {
            assert(ad->var_assignments->size() >= saved_var_count &&
                   "Should not delete var assignments");
            ad->var_assignments->resize(saved_var_count);
        });

        // Separate the wrap target into any variable assignments VAR=... and the command itself.
        wcstring wrapped_command;
        tokenizer_t tokenizer(wt.c_str(), 0);
        size_t wrapped_command_offset_in_wt = wcstring::npos;
        while (auto tok = tokenizer.next()) {
            wcstring tok_src = tok->get_source(wt);
            if (variable_assignment_equals_pos(tok_src)) {
                ad->var_assignments->push_back(std::move(tok_src));
            } else {
                wrapped_command_offset_in_wt = tok->offset;
                wrapped_command = std::move(tok_src);
                break;
            }
        }

        // Skip this wrapped command if empty, or if we've seen it before.
        if (wrapped_command.empty() ||
            !ad->visited_wrapped_commands.insert(wrapped_command).second) {
            continue;
        }

        // Construct a fake command line containing the wrap target.
        wcstring faux_commandline = cmdline;
        faux_commandline.replace(cmdrange.start, cmdrange.length, wt);

        // Recurse with our new command and command line.
        source_range_t faux_source_range{uint32_t(cmdrange.start + wrapped_command_offset_in_wt),
                                         uint32_t(wrapped_command.size())};
        walk_wrap_chain(wrapped_command, faux_commandline, faux_source_range, ad);
    }
}

/// If the argument contains a '[' typed by the user, completion by appending to the argument might
/// produce an invalid token (#5831).
///
/// Check if there is any unescaped, unquoted '['; if yes, make the completions replace the entire
/// argument instead of appending, so '[' will be escaped.
void completer_t::escape_opening_brackets(const wcstring &argument) {
    bool have_unquoted_unescaped_bracket = false;
    wchar_t quote = L'\0';
    bool escaped = false;
    for (wchar_t c : argument) {
        have_unquoted_unescaped_bracket |= (c == L'[') && !quote && !escaped;
        if (escaped) {
            escaped = false;
        } else if (c == L'\\') {
            escaped = true;
        } else if (c == L'\'' || c == L'"') {
            if (quote == c) {
                // Closing a quote.
                quote = L'\0';
            } else if (quote == L'\0') {
                // Opening a quote.
                quote = c;
            }
        }
    }
    if (!have_unquoted_unescaped_bracket) return;
    // Since completion_apply_to_command_line will escape the completion, we need to provide an
    // unescaped version.
    wcstring unescaped_argument;
    if (!unescape_string(argument, &unescaped_argument, UNESCAPE_INCOMPLETE)) return;
    for (completion_t &comp : completions) {
        if (comp.flags & COMPLETE_REPLACES_TOKEN) continue;
        comp.flags |= COMPLETE_REPLACES_TOKEN;
        if (comp.flags & COMPLETE_DONT_ESCAPE) {
            // If the completion won't be escaped, we need to do it here.
            // Currently, this will probably never happen since COMPLETE_DONT_ESCAPE
            // is only set for user or variable names which cannot contain '['.
            unescaped_argument = escape_string(unescaped_argument, ESCAPE_ALL);
        }
        comp.completion = unescaped_argument + comp.completion;
    }
}

/// Set the DUPLICATES_ARG flag in any completion that duplicates an argument.
void completer_t::mark_completions_duplicating_arguments(const wcstring &cmd,
                                                         const wcstring &prefix,
                                                         const std::vector<tok_t> &args) {
    // Get all the arguments, unescaped, into an array that we're going to bsearch.
    wcstring_list_t arg_strs;
    for (const auto &arg : args) {
        wcstring argstr = arg.get_source(cmd);
        wcstring argstr_unesc;
        if (unescape_string(argstr, &argstr_unesc, UNESCAPE_DEFAULT)) {
            arg_strs.push_back(std::move(argstr_unesc));
        }
    }
    std::sort(arg_strs.begin(), arg_strs.end());

    wcstring comp_str;
    for (completion_t &comp : completions) {
        comp_str = comp.completion;
        if (!(comp.flags & COMPLETE_REPLACES_TOKEN)) {
            comp_str.insert(0, prefix);
        }
        if (std::binary_search(arg_strs.begin(), arg_strs.end(), comp_str)) {
            comp.flags |= COMPLETE_DUPLICATES_ARGUMENT;
        }
    }
}

void completer_t::perform_for_commandline(wcstring cmdline) {
    // Limit recursion, in case a user-defined completion has cycles, or the completion for "x"
    // wraps "A=B x" (#3474, #7344).  No need to do that when there is no parser: this happens only
    // for autosuggestions where we don't evaluate command substitutions or variable assignments.
    if (ctx.parser) {
        if (ctx.parser->libdata().complete_recursion_level >= 24) {
            FLOGF(error, _(L"completion reached maximum recursion depth, possible cycle?"),
                  cmdline.c_str());
            return;
        }
        ++ctx.parser->libdata().complete_recursion_level;
    };
    cleanup_t decrement{[this]() {
        if (ctx.parser) --ctx.parser->libdata().complete_recursion_level;
    }};

    const size_t cursor_pos = cmdline.size();
    const bool is_autosuggest = (flags & completion_request_t::autosuggestion);

    // Find the process to operate on. The cursor may be past it (#1261), so backtrack
    // until we know we're no longer in a space. But the space may actually be part of the
    // argument (#2477).
    size_t position_in_statement = cursor_pos;
    while (position_in_statement > 0 && cmdline.at(position_in_statement - 1) == L' ') {
        position_in_statement--;
    }

    // Get all the arguments.
    std::vector<tok_t> tokens;
    parse_util_process_extent(cmdline.c_str(), position_in_statement, nullptr, nullptr, &tokens);

    // Hack: fix autosuggestion by removing prefixing "and"s #6249.
    if (is_autosuggest) {
        while (!tokens.empty() && parser_keywords_is_subcommand(tokens.front().get_source(cmdline)))
            tokens.erase(tokens.begin());
    }

    // Consume variable assignments in tokens strictly before the cursor.
    // This is a list of (escaped) strings of the form VAR=VAL.
    wcstring_list_t var_assignments;
    for (const tok_t &tok : tokens) {
        if (tok.location_in_or_at_end_of_source_range(cursor_pos)) break;
        wcstring tok_src = tok.get_source(cmdline);
        if (!variable_assignment_equals_pos(tok_src)) break;
        var_assignments.push_back(std::move(tok_src));
    }
    tokens.erase(tokens.begin(), tokens.begin() + var_assignments.size());

    // Empty process (cursor is after one of ;, &, |, \n, &&, || modulo whitespace).
    if (tokens.empty()) {
        // Don't autosuggest anything based on the empty string (generalizes #1631).
        if (is_autosuggest) return;

        complete_cmd(L"");
        complete_abbr(L"");
        return;
    }

    const tok_t &cmd_tok = tokens.front();
    const tok_t &cur_tok = tokens.back();

    // Since fish does not currently support redirect in command position, we return here.
    if (cmd_tok.type != token_type_t::string) return;
    if (cur_tok.type == token_type_t::error) return;
    for (const auto &tok : tokens) {  // If there was an error, it was in the last token.
        assert(tok.type == token_type_t::string || tok.type == token_type_t::redirect);
    }
    // If we are completing a variable name or a tilde expansion user name, we do that and
    // return. No need for any other completions.
    const wcstring current_token = cur_tok.get_source(cmdline);
    if (cur_tok.location_in_or_at_end_of_source_range(cursor_pos)) {
        if (try_complete_variable(current_token) || try_complete_user(current_token)) {
            return;
        }
    }

    if (cmd_tok.location_in_or_at_end_of_source_range(cursor_pos)) {
        maybe_t<size_t> equal_sign_pos = variable_assignment_equals_pos(current_token);
        if (equal_sign_pos) {
            complete_param_expand(current_token, true /* do_file */);
            return;
        }
        // Complete command filename.
        complete_cmd(current_token);
        complete_abbr(current_token);
        return;
    }
    // See whether we are in an argument, in a redirection or in the whitespace in between.
    bool in_redirection = cur_tok.type == token_type_t::redirect;

    bool had_ddash = false;
    wcstring current_argument, previous_argument;
    if (cur_tok.type == token_type_t::string &&
        cur_tok.location_in_or_at_end_of_source_range(position_in_statement)) {
        // If the cursor is in whitespace, then the "current" argument is empty and the
        // previous argument is the matching one. But if the cursor was in or at the end
        // of the argument, then the current argument is the matching one, and the
        // previous argument is the one before it.
        bool cursor_in_whitespace = !cur_tok.location_in_or_at_end_of_source_range(cursor_pos);
        if (cursor_in_whitespace) {
            current_argument.clear();
            previous_argument = current_token;
        } else {
            current_argument = current_token;
            if (tokens.size() >= 2) {
                tok_t prev_tok = tokens.at(tokens.size() - 2);
                if (prev_tok.type == token_type_t::string)
                    previous_argument = prev_tok.get_source(cmdline);
                in_redirection = prev_tok.type == token_type_t::redirect;
            }
        }

        // Check to see if we have a preceding double-dash.
        for (size_t i = 0; i < tokens.size() - 1; i++) {
            if (tokens.at(i).get_source(cmdline) == L"--") {
                had_ddash = true;
                break;
            }
        }
    }

    bool do_file = false, handle_as_special_cd = false;
    if (in_redirection) {
        do_file = true;
    } else {
        // Try completing as an argument.
        custom_arg_data_t arg_data{&var_assignments};
        arg_data.had_ddash = had_ddash;

        assert(cmd_tok.offset < std::numeric_limits<uint32_t>::max());
        assert(cmd_tok.length < std::numeric_limits<uint32_t>::max());
        source_range_t command_range = {static_cast<uint32_t>(cmd_tok.offset),
                                        static_cast<uint32_t>(cmd_tok.length)};

        wcstring unesc_command;
        bool unescaped =
            unescape_string(cmd_tok.get_source(cmdline), &unesc_command, UNESCAPE_DEFAULT) &&
            unescape_string(previous_argument, &arg_data.previous_argument, UNESCAPE_DEFAULT) &&
            unescape_string(current_argument, &arg_data.current_argument, UNESCAPE_INCOMPLETE);
        if (unescaped) {
            // Have to walk over the command and its entire wrap chain. If any command
            // disables do_file, then they all do.
            walk_wrap_chain(unesc_command, cmdline, command_range, &arg_data);
            do_file = arg_data.do_file;

            // If we're autosuggesting, and the token is empty, don't do file suggestions.
            if (is_autosuggest && arg_data.current_argument.empty()) {
                do_file = false;
            }
        }

        // Hack. If we're cd, handle it specially (issue #1059, others).
        handle_as_special_cd = (unesc_command == L"cd");
    }

    // Maybe apply variable assignments.
    cleanup_t restore_vars{apply_var_assignments(var_assignments)};
    if (ctx.check_cancel()) return;

    // This function wants the unescaped string.
    complete_param_expand(current_argument, do_file, handle_as_special_cd);

    // Escape '[' in the argument before completing it.
    escape_opening_brackets(current_argument);

    // Lastly mark any completions that appear to already be present in arguments.
    mark_completions_duplicating_arguments(cmdline, current_token, tokens);
}

completion_list_t complete(const wcstring &cmd_with_subcmds, completion_request_flags_t flags,
                           const operation_context_t &ctx) {
    // Determine the innermost subcommand.
    const wchar_t *cmdsubst_begin, *cmdsubst_end;
    parse_util_cmdsubst_extent(cmd_with_subcmds.c_str(), cmd_with_subcmds.size(), &cmdsubst_begin,
                               &cmdsubst_end);
    assert(cmdsubst_begin != nullptr && cmdsubst_end != nullptr && cmdsubst_end >= cmdsubst_begin);
    wcstring cmd = wcstring(cmdsubst_begin, cmdsubst_end - cmdsubst_begin);
    completer_t completer(ctx, flags);
    completer.perform_for_commandline(std::move(cmd));
    return completer.acquire_completions();
}

/// Print the short switch \c opt, and the argument \c arg to the specified
/// wcstring, but only if \c argument isn't an empty string.
static void append_switch(wcstring &out, wchar_t opt, const wcstring &arg) {
    if (arg.empty()) return;
    append_format(out, L" -%lc %ls", opt, escape_string(arg, ESCAPE_ALL).c_str());
}
static void append_switch(wcstring &out, const wcstring &opt, const wcstring &arg) {
    if (arg.empty()) return;
    append_format(out, L" --%ls %ls", opt.c_str(), escape_string(arg, ESCAPE_ALL).c_str());
}
static void append_switch(wcstring &out, wchar_t opt) { append_format(out, L" -%lc", opt); }
static void append_switch(wcstring &out, const wcstring &opt) {
    append_format(out, L" --%ls", opt.c_str());
}

static wcstring completion2string(const complete_entry_opt_t &o, const wcstring &cmd,
                                  bool is_path) {
    wcstring out;
    out.append(L"complete");

    if (o.flags & COMPLETE_DONT_SORT) append_switch(out, L'k');

    if (o.result_mode.no_files && o.result_mode.requires_param) {
        append_switch(out, L"exclusive");
    } else if (o.result_mode.no_files) {
        append_switch(out, L"no-files");
    } else if (o.result_mode.force_files) {
        append_switch(out, L"force-files");
    } else if (o.result_mode.requires_param) {
        append_switch(out, L"requires-param");
    }

    if (is_path)
        append_switch(out, L'p', cmd);
    else {
        out.append(L" ");
        out.append(escape_string(cmd, ESCAPE_ALL));
    }

    switch (o.type) {
        case option_type_args_only: {
            break;
        }
        case option_type_short: {
            append_switch(out, L's', wcstring(1, o.option.at(0)));
            break;
        }
        case option_type_single_long:
        case option_type_double_long: {
            append_switch(out, o.type == option_type_single_long ? L'o' : L'l', o.option);
            break;
        }
    }

    append_switch(out, L'd', C_(o.desc));
    append_switch(out, L'a', o.comp);
    append_switch(out, L'n', o.condition);
    out.append(L"\n");
    return out;
}

/// Use by the bare `complete`, loaded completions are printed out as commands
wcstring complete_print(const wcstring &cmd) {
    wcstring out;
    out.reserve(40);  // just a guess
    auto completion_set = s_completion_set.acquire();

    // Get a list of all completions in a vector, then sort it by order.
    std::vector<std::reference_wrapper<const completion_entry_t>> all_completions;
    // These should be "c"begin/end, but then gcc from ~~the dark ages~~ RHEL 7 would complain.
    all_completions.insert(all_completions.begin(), completion_set->begin(), completion_set->end());
    sort(all_completions.begin(), all_completions.end(), compare_completions_by_order);

    for (const completion_entry_t &e : all_completions) {
        if (!cmd.empty() && e.cmd != cmd) continue;
        const option_list_t &options = e.get_options();
        for (const complete_entry_opt_t &o : options) {
            out.append(completion2string(o, e.cmd, e.cmd_is_path));
        }
    }

    // Append wraps.
    auto locked_wrappers = wrapper_map.acquire();
    for (const auto &entry : *locked_wrappers) {
        const wcstring &src = entry.first;
        if (!cmd.empty() && src != cmd) continue;
        for (const wcstring &target : entry.second) {
            out.append(L"complete ");
            out.append(escape_string(src, ESCAPE_ALL));
            append_switch(out, L"wraps", target);
            out.append(L"\n");
        }
    }
    return out;
}

void complete_invalidate_path() {
    // TODO: here we unload all completions for commands that are loaded by the autoloader. We also
    // unload any completions that the user may specified on the command line. We should in
    // principle track those completions loaded by the autoloader alone.
    wcstring_list_t cmds = completion_autoloader.acquire()->get_autoloaded_commands();
    for (const wcstring &cmd : cmds) {
        complete_remove_all(cmd, false /* not a path */);
    }
}

/// Add a new target that wraps a command. Example: __fish_XYZ (function) wraps XYZ (target).
bool complete_add_wrapper(const wcstring &command, const wcstring &new_target) {
    if (command.empty() || new_target.empty()) {
        return false;
    }

    // If the command and the target are the same,
    // there's no point in following the wrap-chain because we'd only complete the same thing.
    // TODO: This should maybe include full cycle detection.
    if (command == new_target) return false;

    auto locked_map = wrapper_map.acquire();
    wrapper_map_t &wraps = *locked_map;
    wcstring_list_t *targets = &wraps[command];
    // If it's already present, we do nothing.
    if (!contains(*targets, new_target)) {
        targets->push_back(new_target);
    }
    return true;
}

bool complete_remove_wrapper(const wcstring &command, const wcstring &target_to_remove) {
    if (command.empty() || target_to_remove.empty()) {
        return false;
    }

    auto locked_map = wrapper_map.acquire();
    wrapper_map_t &wraps = *locked_map;
    bool result = false;
    auto current_targets_iter = wraps.find(command);
    if (current_targets_iter != wraps.end()) {
        wcstring_list_t *targets = &current_targets_iter->second;
        auto where = std::find(targets->begin(), targets->end(), target_to_remove);
        if (where != targets->end()) {
            targets->erase(where);
            result = true;
        }
    }
    return result;
}

wcstring_list_t complete_get_wrap_targets(const wcstring &command) {
    if (command.empty()) {
        return {};
    }
    auto locked_map = wrapper_map.acquire();
    wrapper_map_t &wraps = *locked_map;
    auto iter = wraps.find(command);
    if (iter == wraps.end()) return {};
    return iter->second;
}
