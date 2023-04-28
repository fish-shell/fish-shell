#if 0
/// Functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
///
#include "config.h"  // IWYU pragma: keep

#include "complete.h"

#include <pwd.h>
#include <wctype.h>

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "abbrs.h"
#include "autoload.h"
#include "builtin.h"
#include "common.h"
#include "enum_set.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "function.h"
#include "global_safety.h"
#include "history.h"
#include "maybe.h"
#include "operation_context.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "parser_keywords.h"
#include "path.h"
#include "tokenizer.h"
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
static const wcstring &C_(const wcstring &s) {
    return s.empty() ? g_empty_string : wgettext(s.c_str());
}
#else
static const wcstring &C_(const wcstring &s) { return s; }
#endif

/// Struct describing a completion rule for options to a command.
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
namespace {
struct complete_entry_opt_t {
    /// Text of the option (like 'foo').
    wcstring option;
    // Arguments to the option; may be a subshell expression expanded at evaluation time.
    wcstring comp;
    /// Description of the completion.
    wcstring desc;
    // Conditions under which to use the option, expanded and evaluated at completion time.
    std::vector<wcstring> conditions;
    /// Type of the option: args_only, short, single_long, or double_long.
    complete_option_type_t type;
    /// Determines how completions should be performed on the argument after the switch.
    completion_mode_t result_mode;
    /// Completion flags.
    complete_flags_t flags;

    const wcstring &localized_desc() const { return C_(desc); }

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
static relaxed_atomic_t<unsigned int> k_complete_order{0};

/// Struct describing a command completion.
using option_list_t = std::vector<complete_entry_opt_t>;
class completion_entry_t {
   public:
    /// List of all options.
    option_list_t options;

    /// Order for when this completion was created. This aids in outputting completions sorted by
    /// time.
    const unsigned int order;

    /// Getters for option list.
    const option_list_t &get_options() const { return options; }

    /// Adds an option.
    void add_option(complete_entry_opt_t &&opt) { options.push_back(std::move(opt)); }

    /// Remove all completion options in the specified entry that match the specified short / long
    /// option strings. Returns true if it is now empty and should be deleted, false if it's not
    /// empty.
    bool remove_option(const wcstring &option, complete_option_type_t type) {
        options.erase(std::remove_if(options.begin(), options.end(),
                                     [&](const complete_entry_opt_t &opt) {
                                         return opt.option == option && opt.type == type;
                                     }),
                      options.end());
        return this->options.empty();
    }

    completion_entry_t() : order(++k_complete_order) {}
};
}  // namespace

/// Set of all completion entries. Keyed by the command name, and whether it is a path.
using completion_key_t = std::pair<wcstring, bool>;
using completion_entry_map_t = std::map<completion_key_t, completion_entry_t>;
static owning_lock<completion_entry_map_t> s_completion_map;

/// Completion "wrapper" support. The map goes from wrapping-command to wrapped-command-list.
using wrapper_map_t = std::unordered_map<wcstring, std::vector<wcstring>>;
static owning_lock<wrapper_map_t> wrapper_map;

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
        if (len > 0 && (std::wcschr(L"/=@:.,-", comp.at(len - 1)) != nullptr))
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

void completion_t::prepend_token_prefix(const wcstring &prefix) {
    if (this->flags & COMPLETE_REPLACES_TOKEN) {
        this->completion.insert(0, prefix);
    }
}

bool completion_receiver_t::add(completion_t &&comp) {
    if (this->completions_.size() >= limit_) {
        return false;
    }
    this->completions_.push_back(std::move(comp));
    return true;
}

bool completion_receiver_t::add(wcstring &&comp) { return this->add(std::move(comp), wcstring{}); }

bool completion_receiver_t::add(wcstring &&comp, wcstring desc, complete_flags_t flags,
                                string_fuzzy_match_t match) {
    return this->add(completion_t(std::move(comp), std::move(desc), match, flags));
}

bool completion_receiver_t::add_list(completion_list_t &&lst) {
    size_t total_size = lst.size() + this->size();
    if (total_size < this->size() || total_size > limit_) {
        return false;
    }

    if (completions_.empty()) {
        completions_ = std::move(lst);
    } else {
        completions_.reserve(completions_.size() + lst.size());
        std::move(lst.begin(), lst.end(), std::back_inserter(completions_));
    }
    return true;
}

completion_list_t completion_receiver_t::take() {
    completion_list_t res{};
    std::swap(res, this->completions_);
    return res;
}

completion_receiver_t completion_receiver_t::subreceiver() const {
    size_t remaining_capacity = limit_ < size() ? 0 : limit_ - size();
    return completion_receiver_t(remaining_capacity);
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

void completions_sort_and_prioritize(completion_list_t *comps, completion_request_options_t flags) {
    if (comps->empty()) return;

    // Find the best rank.
    uint32_t best_rank = UINT32_MAX;
    for (const auto &comp : *comps) {
        best_rank = std::min(best_rank, comp.rank());
    }

    // Throw out completions of worse ranks.
    comps->erase(std::remove_if(comps->begin(), comps->end(),
                                [=](const completion_t &comp) { return comp.rank() > best_rank; }),
                 comps->end());

    // Deduplicate both sorted and unsorted results.
    unique_completions_retaining_order(comps);

    // Sort, provided COMPLETE_DONT_SORT isn't set.
    // Here we do not pass suppress_exact, so that exact matches appear first.
    stable_sort(comps->begin(), comps->end(), natural_compare_completions);

    // Lastly, if this is for an autosuggestion, prefer to avoid completions that duplicate
    // arguments, and penalize files that end in tilde - they're frequently autosave files from e.g.
    // emacs. Also prefer samecase to smartcase.
    if (flags.autosuggestion) {
        stable_sort(comps->begin(), comps->end(), [](const completion_t &a, const completion_t &b) {
            if (a.match.case_fold != b.match.case_fold) {
                return a.match.case_fold < b.match.case_fold;
            }
            return compare_completions_by_duplicate_arguments(a, b) ||
                   compare_completions_by_tilde(a, b);
        });
    }
}

namespace {
/// Class representing an attempt to compute completions.
class completer_t {
    /// The operation context for this completion.
    const operation_context_t &ctx;

    /// Flags associated with the completion request.
    const completion_request_options_t flags;

    /// The output completions.
    completion_receiver_t completions;

    /// Commands which we would have tried to load, if we had a parser.
    std::vector<wcstring> needs_load;

    /// Table of completions conditions that have already been tested and the corresponding test
    /// results.
    using condition_cache_t = std::unordered_map<wcstring, bool>;
    condition_cache_t condition_cache;

    bool try_complete_variable(const wcstring &str);
    bool try_complete_user(const wcstring &str);

    bool complete_param_for_command(const wcstring &cmd_orig, const wcstring &popt,
                                    const wcstring &str, bool use_switches, bool *out_do_file);

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
    bool conditions_test(const std::vector<wcstring> &conditions);

    void complete_strings(const wcstring &wc_escaped, const description_func_t &desc_func,
                          const completion_list_t &possible_comp, complete_flags_t flags,
                          expand_flags_t extra_expand_flags = {});

    expand_flags_t expand_flags() const {
        expand_flags_t result{};
        if (flags.autosuggestion) result |= expand_flag::skip_cmdsubst;
        if (flags.fuzzy_match) result |= expand_flag::fuzzy_match;
        if (flags.descriptions) result |= expand_flag::gen_descriptions;
        return result;
    }

    // Bag of data to support expanding a command's arguments using custom completions, including
    // the wrap chain.
    struct custom_arg_data_t {
        explicit custom_arg_data_t(std::vector<wcstring> *vars) : var_assignments(vars) {
            assert(vars);
        }

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
        std::vector<wcstring> *var_assignments;

        // The set of wrapped commands which we have visited, and so should not be explored again.
        std::set<wcstring> visited_wrapped_commands{};
    };

    void complete_custom(const wcstring &cmd, const wcstring &cmdline, custom_arg_data_t *ad);

    void walk_wrap_chain(const wcstring &cmd, const wcstring &cmdline, source_range_t cmdrange,
                         custom_arg_data_t *ad);

    cleanup_t apply_var_assignments(const std::vector<wcstring> &var_assignments);

    bool empty() const { return completions.empty(); }

    void escape_opening_brackets(const wcstring &argument);

    void mark_completions_duplicating_arguments(const wcstring &cmd, const wcstring &prefix,
                                                const std::vector<tok_t> &args);

   public:
    completer_t(const operation_context_t &ctx, completion_request_options_t f)
        : ctx(ctx), flags(f), completions(ctx.expansion_limit) {}

    void perform_for_commandline(wcstring cmdline);

    completion_list_t acquire_completions() { return completions.take(); }

    std::vector<wcstring> acquire_needs_load() { return std::move(needs_load); }
};

// Autoloader for completions.
static owning_lock<autoload_t> completion_autoloader{autoload_t(L"fish_complete_path")};

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

bool completer_t::conditions_test(const std::vector<wcstring> &conditions) {
    for (const auto &c : conditions) {
        if (!condition_test(c)) return false;
    }
    return true;
}

/// Find the full path and commandname from a command string 'str'.
static void parse_cmd_string(const wcstring &str, wcstring *path, wcstring *cmd,
                             const environment_t &vars) {
    auto path_result = path_try_get_path(str, vars);
    bool found = (path_result.err == 0);
    *path = std::move(path_result.path);
    // Resolve commands that use relative paths because we compare full paths with "complete -p".
    if (found && !str.empty() && str.at(0) != L'/') {
        if (auto full_path = wrealpath(*path)) {
            path->assign(full_path.acquire());
        }
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
///    The flags controlling completion
/// @param extra_expand_flags
///    Additional flags controlling expansion.
void completer_t::complete_strings(const wcstring &wc_escaped, const description_func_t &desc_func,
                                   const completion_list_t &possible_comp, complete_flags_t flags,
                                   expand_flags_t extra_expand_flags) {
    wcstring tmp = wc_escaped;
    if (!expand_one(tmp,
                    this->expand_flags() | extra_expand_flags | expand_flag::skip_cmdsubst |
                        expand_flag::skip_wildcards,
                    ctx))
        return;

    const wcstring wc = parse_util_unescape_wildcards(tmp);

    for (const auto &comp : possible_comp) {
        const wcstring &comp_str = comp.completion;
        if (!comp_str.empty()) {
            wildcard_complete(comp_str, wc.c_str(), desc_func, &this->completions,
                              this->expand_flags() | extra_expand_flags, flags);
        }
    }
}

/// If command to complete is short enough, substitute the description with the whatis information
/// for the executable.
void completer_t::complete_cmd_desc(const wcstring &str) {
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

    if (wildcard_has(cmd)) {
        return;
    }

    bool skip = true;
    for (const auto &c : completions.get_list()) {
        if (c.completion.empty() || (c.completion.back() != L'/')) {
            skip = false;
            break;
        }
    }

    if (skip) {
        return;
    }

    wcstring lookup_cmd(L"__fish_describe_command ");
    lookup_cmd.append(escape_string(cmd));

    // First locate a list of possible descriptions using a single call to apropos or a direct
    // search if we know the location of the whatis database. This can take some time on slower
    // systems with a large set of manuals, but it should be ok since apropos is only called once.
    std::vector<wcstring> list;
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
        lookup.emplace(std::move(key), std::move(val));
    }

    // Then do a lookup on every completion and if a match is found, change to the new
    // description.
    for (auto &completion : completions.get_list()) {
        const wcstring &el = completion.completion;
        auto new_desc_iter = lookup.find(el);
        if (new_desc_iter != lookup.end()) completion.description = new_desc_iter->second;
    }
}

/// Returns a description for the specified function, or an empty string if none.
static wcstring complete_function_desc(const wcstring &fn) {
    if (auto props = function_get_props(fn)) {
        return props->description;
    }
    return wcstring{};
}

/// Complete the specified command name. Search for executables in the path, executables defined
/// using an absolute path, functions, builtins and directories for implicit cd commands.
///
/// \param str_cmd the command string to find completions for
void completer_t::complete_cmd(const wcstring &str_cmd) {
    completion_list_t possible_comp;

    // Append all possible executables
    expand_result_t result = expand_string(
        str_cmd, &this->completions,
        this->expand_flags() | expand_flag::special_for_command | expand_flag::for_completions |
            expand_flag::preserve_home_tildes | expand_flag::executables_only,
        ctx);
    if (result == expand_result_t::cancel) {
        return;
    }
    if (result == expand_result_t::ok && this->flags.descriptions) {
        this->complete_cmd_desc(str_cmd);
    }

    // We don't really care if this succeeds or fails. If it succeeds this->completions will be
    // updated with choices for the user.
    expand_result_t ignore =
        // Append all matching directories
        expand_string(str_cmd, &this->completions,
                      this->expand_flags() | expand_flag::for_completions |
                          expand_flag::preserve_home_tildes | expand_flag::directories_only,
                      ctx);
    UNUSED(ignore);

    if (str_cmd.empty() || (str_cmd.find(L'/') == wcstring::npos && str_cmd.at(0) != L'~')) {
        bool include_hidden = !str_cmd.empty() && str_cmd.at(0) == L'_';
        std::vector<wcstring> names = function_get_names(include_hidden);
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
    // Copy the list of names and descriptions so as not to hold the lock across the call to
    // complete_strings.
    completion_list_t possible_comp;
    std::unordered_map<wcstring, wcstring> descs;
    {
        auto abbrs = abbrs_list();
        for (const auto &abbr : abbrs) {
            if (!abbr.is_regex) {
                possible_comp.emplace_back(*abbr.key);
                descs[*abbr.key] = *abbr.replacement;
            }
        }
    }

    auto desc_func = [&](const wcstring &key) {
        auto iter = descs.find(key);
        assert(iter != descs.end() && "Abbreviation not found");
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
    const bool is_autosuggest = this->flags.autosuggestion;

    bool saved_interactive = false;
    statuses_t status;
    if (ctx.parser) {
        saved_interactive = ctx.parser->libdata().is_interactive;
        ctx.parser->libdata().is_interactive = false;
        status = ctx.parser->get_last_statuses();
    }

    expand_flags_t eflags{};
    if (is_autosuggest) {
        eflags |= expand_flag::skip_cmdsubst;
    }

    completion_list_t possible_comp = parser_t::expand_argument_list(args, eflags, ctx);

    if (ctx.parser) {
        ctx.parser->libdata().is_interactive = saved_interactive;
        ctx.parser->set_last_statuses(status);
    }

    // Allow leading dots - see #3707.
    this->complete_strings(escape_string(str), const_desc(desc), possible_comp, flags,
                           expand_flag::allow_nonliteral_leading_dot);
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

/// complete_param: Given a command, find completions for the argument str of command cmd_orig with
/// previous option popt. If file completions should be disabled, then mark *out_do_file as false.
///
/// \return true if successful, false if there's an error.
///
/// Examples in format (cmd, popt, str):
///
///   echo hello world <tab> -> ("echo", "world", "")
///   echo hello world<tab> -> ("echo", "hello", "world")
///
bool completer_t::complete_param_for_command(const wcstring &cmd_orig, const wcstring &popt,
                                             const wcstring &str, bool use_switches,
                                             bool *out_do_file) {
    bool use_files = true, has_force = false;

    wcstring cmd, path;
    parse_cmd_string(cmd_orig, &path, &cmd, ctx.vars);

    // Don't use cmd_orig here for paths. It's potentially pathed,
    // so that command might exist, but the completion script
    // won't be using it.
    bool cmd_exists = builtin_exists(cmd) || function_exists_no_autoload(cmd) ||
                      path_get_path(cmd, ctx.vars).has_value();
    if (!cmd_exists) {
        // Do not load custom completions if the command does not exist
        // This prevents errors caused during the execution of completion providers for
        // tools that do not exist. Applies to both manual completions ("cm<TAB>", "cmd <TAB>")
        // and automatic completions ("gi" autosuggestion provider -> git)
        FLOG(complete, "Skipping completions for non-existent command");
    } else if (ctx.parser) {
        complete_load(cmd, *ctx.parser);
    } else if (!completion_autoloader.acquire()->has_attempted_autoload(cmd)) {
        needs_load.push_back(cmd);
    }

    // Make a list of lists of all options that we care about.
    std::vector<option_list_t> all_options;
    {
        auto completion_map = s_completion_map.acquire();
        for (const auto &kv : *completion_map) {
            const completion_key_t &key = kv.first;
            bool cmd_is_path = key.second;
            const wcstring &match = cmd_is_path ? path : cmd;
            if (wildcard_match(match, key.first)) {
                // Copy all of their options into our list. Oof, this is a lot of copying.
                // We have to copy them in reverse order to preserve legacy behavior (#9221).
                const auto &options = kv.second.get_options();
                all_options.emplace_back(options.rbegin(), options.rend());
            }
        }
    }

    // Now release the lock and test each option that we captured above. We have to do this outside
    // the lock because callouts (like the condition) may add or remove completions. See issue 2.
    for (const auto &options : all_options) {
        size_t short_opt_pos = short_option_pos(str, options);
        // We want last_option_requires_param to default to false but distinguish between when
        // a previous completion has set it to false and when it has its default value.
        maybe_t<bool> last_option_requires_param{};
        bool use_common = true;
        if (use_switches) {
            if (str[0] == L'-') {
                // Check if we are entering a combined option and argument (like --color=auto or
                // -I/usr/include).
                for (const complete_entry_opt_t &o : options) {
                    const wchar_t *arg;
                    if (o.type == option_type_short) {
                        if (short_opt_pos == wcstring::npos) continue;
                        if (o.option.at(0) != str.at(short_opt_pos)) continue;
                        arg = str.c_str() + short_opt_pos + 1;
                    } else {
                        arg = param_match2(&o, str.c_str());
                    }

                    if (this->conditions_test(o.conditions)) {
                        if (o.type == option_type_short) {
                            // Only override a true last_option_requires_param value with a false
                            // one
                            if (last_option_requires_param.has_value()) {
                                last_option_requires_param =
                                    *last_option_requires_param && o.result_mode.requires_param;
                            } else {
                                last_option_requires_param = o.result_mode.requires_param;
                            }
                        }
                        if (arg != nullptr) {
                            if (o.result_mode.requires_param) use_common = false;
                            if (o.result_mode.no_files) use_files = false;
                            if (o.result_mode.force_files) has_force = true;
                            complete_from_args(arg, o.comp, o.localized_desc(), o.flags);
                        }
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
                        this->conditions_test(o.conditions)) {
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
                        if (match && this->conditions_test(o.conditions)) {
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

        // Set a default value for last_option_requires_param only if one hasn't been set
        if (!last_option_requires_param.has_value()) {
            last_option_requires_param = false;
        }

        // Now we try to complete an option itself
        for (const complete_entry_opt_t &o : options) {
            // If this entry is for the base command, check if any of the arguments match.
            if (!this->conditions_test(o.conditions)) continue;
            if (o.option.empty()) {
                use_files = use_files && (!(o.result_mode.no_files));
                has_force = has_force || o.result_mode.force_files;
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
                    if (*last_option_requires_param) continue;
                    // .. and the option is not already there.
                    if (str.find(optchar) != wcstring::npos) continue;
                }
                // It's a match.
                wcstring desc = o.localized_desc();
                // Append a short-style option
                if (!this->completions.add(wcstring{o.option}, std::move(desc), 0)) {
                    return false;
                }
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
                if (!this->completions.add(std::move(completion), C_(o.desc),
                                           flags | COMPLETE_NO_SPACE)) {
                    return false;
                }
            }

            // Append a long-style option
            if (!this->completions.add(whole_opt.substr(offset), C_(o.desc), flags)) {
                return false;
            }
        }
    }

    if (has_force) {
        *out_do_file = true;
    } else if (!use_files) {
        *out_do_file = false;
    }
    return true;
}

/// Perform generic (not command-specific) expansions on the specified string.
void completer_t::complete_param_expand(const wcstring &str, bool do_file,
                                        bool handle_as_special_cd) {
    if (ctx.check_cancel()) return;
    expand_flags_t flags = this->expand_flags() | expand_flag::skip_cmdsubst |
                           expand_flag::for_completions | expand_flag::preserve_home_tildes;

    if (!do_file) flags |= expand_flag::skip_wildcards;

    if (handle_as_special_cd && do_file) {
        if (this->flags.autosuggestion) {
            flags |= expand_flag::special_for_cd_autosuggestion;
        }
        flags |= expand_flag::directories_only;
        flags |= expand_flag::special_for_cd;
    }

    // Squelch file descriptions per issue #254.
    if (this->flags.autosuggestion || do_file) flags.clear(expand_flag::gen_descriptions);

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
        if (!this->completions.add_list(std::move(local_completions))) {
            return;
        }
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
/// \return true if this was a variable, so we should stop completion.
bool completer_t::complete_variable(const wcstring &str, size_t start_offset) {
    const wchar_t *const whole_var = str.c_str();
    const wchar_t *var = &whole_var[start_offset];
    size_t varlen = str.length() - start_offset;
    bool res = false;

    for (const wcstring &env_name : ctx.vars.get_names(0)) {
        bool anchor_start = !this->flags.fuzzy_match;
        maybe_t<string_fuzzy_match_t> match =
            string_fuzzy_match_string(var, env_name, anchor_start);
        if (!match) continue;

        wcstring comp;
        complete_flags_t flags = 0;

        if (!match->requires_full_replacement()) {
            // Take only the suffix.
            comp.append(env_name.c_str() + varlen);
        } else {
            comp.append(whole_var, start_offset);
            comp.append(env_name);
            flags = COMPLETE_REPLACES_TOKEN | COMPLETE_DONT_ESCAPE;
        }

        wcstring desc;
        if (this->flags.descriptions) {
            if (this->flags.autosuggestion) {
                // $history can be huge, don't put all of it in the completion description; see
                // #6288.
                if (env_name == L"history") {
                    HistorySharedPtr &history =
                        *history_with_name(history_session_id(ctx.vars));
                    for (size_t i = 1; i < history.size() && desc.size() < 64; i++) {
                        if (i > 1) desc += L' ';
                        desc += expand_escape_string(*(*history.item_at_index(i)).str());
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
        // TODO: need to propagate overflow here.
        ignore_result(this->completions.add(std::move(comp), std::move(desc), flags, *match));

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
            variable_start = wcstring::npos;
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
    bool allow_empty = !this->flags.autosuggestion;
    bool text_is_empty = (variable_start == len - 1);
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
            // Append a user name.
            // TODO: propagate overflow?
            ignore_result(
                this->completions.add(&pw_name[name_len], std::move(desc), COMPLETE_NO_SPACE));
            result = true;
        } else if (wcsncasecmp(user_name, pw_name, name_len) == 0) {
            wcstring name = format_string(L"~%ls", pw_name);
            wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
            // Append a user name
            ignore_result(this->completions.add(
                std::move(name), std::move(desc),
                COMPLETE_REPLACES_TOKEN | COMPLETE_DONT_ESCAPE | COMPLETE_NO_SPACE));
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
cleanup_t completer_t::apply_var_assignments(const std::vector<wcstring> &var_assignments) {
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
        auto equals_pos = variable_assignment_equals_pos(var_assign);
        assert(equals_pos && "All variable assignments should have equals position");
        const wcstring variable_name = var_assign.substr(0, *equals_pos);
        const wcstring expression = var_assign.substr(*equals_pos + 1);

        completion_list_t expression_expanded;
        auto expand_ret = expand_string(expression, &expression_expanded, expand_flags, ctx);
        // If expansion succeeds, set the value; if it fails (e.g. it has a cmdsub) set an empty
        // value anyways.
        std::vector<wcstring> vals;
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

    bool is_autosuggest = this->flags.autosuggestion;
    // Perhaps set a transient commandline so that custom completions
    // builtin_commandline will refer to the wrapped command. But not if
    // we're doing autosuggestions.
    maybe_t<cleanup_t> remove_transient{};
    bool wants_transient = (ad->wrap_depth > 0 || ad->var_assignments) && !is_autosuggest;
    if (wants_transient) {
        ctx.parser->libdata().transient_commandlines.push_back(cmdline);
        remove_transient.emplace([=] { ctx.parser->libdata().transient_commandlines.pop_back(); });
    }

    // Maybe apply variable assignments.
    cleanup_t restore_vars{apply_var_assignments(*ad->var_assignments)};
    if (ctx.check_cancel()) return;

    // Invoke any custom completions for this command.
    (void)complete_param_for_command(cmd, ad->previous_argument, ad->current_argument,
                                     !ad->had_ddash, &ad->do_file);
}

static bool expand_command_token(const operation_context_t &ctx, wcstring &cmd_tok) {
    // TODO: we give up if the first token expands to more than one argument. We could handle
    // that case by propagating arguments.
    // Also we could expand wildcards.
    return expand_one(cmd_tok, {expand_flag::skip_cmdsubst, expand_flag::skip_wildcards}, ctx,
                      nullptr);
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

    std::vector<wcstring> targets = complete_get_wrap_targets(cmd);
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
        auto tokenizer = new_tokenizer(wt.c_str(), 0);
        size_t wrapped_command_offset_in_wt = wcstring::npos;
        while (auto tok = tokenizer->next()) {
            wcstring tok_src = *tok->get_source(wt);
            if (variable_assignment_equals_pos(tok_src)) {
                ad->var_assignments->push_back(std::move(tok_src));
            } else {
                wrapped_command_offset_in_wt = tok->offset;
                wrapped_command = std::move(tok_src);
                expand_command_token(ctx, wrapped_command);
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
    auto unescaped_argument = unescape_string(argument, UNESCAPE_INCOMPLETE);
    if (!unescaped_argument) return;
    for (completion_t &comp : completions.get_list()) {
        if (comp.flags & COMPLETE_REPLACES_TOKEN) continue;
        comp.flags |= COMPLETE_REPLACES_TOKEN;
        comp.flags |= COMPLETE_DONT_ESCAPE_TILDES;  // See #9073.
        // We are grafting a completion that is expected to be escaped later. This will break
        // if the original completion doesn't want escaping.  Happily, this is only the case
        // for username completion and variable name completion. They shouldn't end up here
        // anyway because they won't contain '['.
        if (comp.flags & COMPLETE_DONT_ESCAPE) {
            FLOG(warning, L"unexpected completion flag");
        }
        comp.completion = *unescaped_argument + comp.completion;
    }
}

/// Set the DUPLICATES_ARG flag in any completion that duplicates an argument.
void completer_t::mark_completions_duplicating_arguments(const wcstring &cmd,
                                                         const wcstring &prefix,
                                                         const std::vector<tok_t> &args) {
    // Get all the arguments, unescaped, into an array that we're going to bsearch.
    std::vector<wcstring> arg_strs;
    for (const auto &arg : args) {
        wcstring argstr = *arg.get_source(cmd);
        if (auto argstr_unesc = unescape_string(argstr, UNESCAPE_DEFAULT)) {
            arg_strs.push_back(std::move(*argstr_unesc));
        }
    }
    std::sort(arg_strs.begin(), arg_strs.end());

    wcstring comp_str;
    for (completion_t &comp : completions.get_list()) {
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
    const bool is_autosuggest = flags.autosuggestion;

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
    size_t actual_token_count = tokens.size();

    // Hack: fix autosuggestion by removing prefixing "and"s #6249.
    if (is_autosuggest) {
        tokens.erase(
            std::remove_if(tokens.begin(), tokens.end(),
                           [&cmdline](const tok_t &token) {
                               return parser_keywords_is_subcommand(*token.get_source(cmdline));
                           }),
            tokens.end());
    }

    // Consume variable assignments in tokens strictly before the cursor.
    // This is a list of (escaped) strings of the form VAR=VAL.
    std::vector<wcstring> var_assignments;
    for (const tok_t &tok : tokens) {
        if (tok.location_in_or_at_end_of_source_range(cursor_pos)) break;
        wcstring tok_src = *tok.get_source(cmdline);
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

    wcstring *effective_cmdline, effective_cmdline_buf;
    if (tokens.size() == actual_token_count) {
        effective_cmdline = &cmdline;
    } else {
        effective_cmdline_buf.assign(cmdline, tokens.front().offset, wcstring::npos);
        effective_cmdline = &effective_cmdline_buf;
    }

    if (tokens.back().type_ == token_type_t::comment) {
        return;
    }
    tokens.erase(
        std::remove_if(tokens.begin(), tokens.end(),
                       [](const tok_t &tok) { return tok.type_ == token_type_t::comment; }),
        tokens.end());
    assert(!tokens.empty());

    const tok_t &cmd_tok = tokens.front();
    const tok_t &cur_tok = tokens.back();

    // Since fish does not currently support redirect in command position, we return here.
    if (cmd_tok.type_ != token_type_t::string) return;
    if (cur_tok.type_ == token_type_t::error) return;
    for (const auto &tok : tokens) {  // If there was an error, it was in the last token.
        assert(tok.type_ == token_type_t::string || tok.type_ == token_type_t::redirect);
    }
    // If we are completing a variable name or a tilde expansion user name, we do that and
    // return. No need for any other completions.
    const wcstring current_token = *cur_tok.get_source(cmdline);
    if (cur_tok.location_in_or_at_end_of_source_range(cursor_pos)) {
        if (try_complete_variable(current_token) || try_complete_user(current_token)) {
            return;
        }
    }

    if (cmd_tok.location_in_or_at_end_of_source_range(cursor_pos)) {
        auto equal_sign_pos = variable_assignment_equals_pos(current_token);
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
    bool in_redirection = cur_tok.type_ == token_type_t::redirect;

    bool had_ddash = false;
    wcstring current_argument, previous_argument;
    if (cur_tok.type_ == token_type_t::string &&
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
                if (prev_tok.type_ == token_type_t::string)
                    previous_argument = *prev_tok.get_source(cmdline);
                in_redirection = prev_tok.type_ == token_type_t::redirect;
            }
        }

        // Check to see if we have a preceding double-dash.
        for (size_t i = 0; i < tokens.size() - 1; i++) {
            if (*tokens.at(i).get_source(cmdline) == L"--") {
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

        source_offset_t bias = cmdline.size() - effective_cmdline->size();
        source_range_t command_range = {cmd_tok.offset - bias, cmd_tok.length};

        wcstring exp_command = *cmd_tok.get_source(cmdline);
        std::unique_ptr<wcstring> prev;
        std::unique_ptr<wcstring> cur;
        bool unescaped = expand_command_token(ctx, exp_command) &&
                         (prev = unescape_string(previous_argument, UNESCAPE_DEFAULT)) &&
                         (cur = unescape_string(current_argument, UNESCAPE_INCOMPLETE));
        if (unescaped) {
            arg_data.previous_argument = *prev;
            arg_data.current_argument = *cur;
            // Have to walk over the command and its entire wrap chain. If any command
            // disables do_file, then they all do.
            walk_wrap_chain(exp_command, *effective_cmdline, command_range, &arg_data);
            do_file = arg_data.do_file;

            // If we're autosuggesting, and the token is empty, don't do file suggestions.
            if (is_autosuggest && arg_data.current_argument.empty()) {
                do_file = false;
            }
        }

        // Hack. If we're cd, handle it specially (issue #1059, others).
        handle_as_special_cd =
            (exp_command == L"cd") || arg_data.visited_wrapped_commands.count(L"cd");
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

}  // namespace

/// Create a new completion entry.
void append_completion(completion_list_t *completions, wcstring comp, wcstring desc,
                       complete_flags_t flags, string_fuzzy_match_t match) {
    completions->emplace_back(std::move(comp), std::move(desc), match, flags);
}

void complete_add(const wcstring &cmd, bool cmd_is_path, const wcstring &option,
                  complete_option_type_t option_type, completion_mode_t result_mode,
                  std::vector<wcstring> condition, const wchar_t *comp, const wchar_t *desc,
                  complete_flags_t flags) {
    // option should be empty iff the option type is arguments only.
    assert(option.empty() == (option_type == option_type_args_only));

    // Lock the lock that allows us to edit the completion entry list.
    auto completion_map = s_completion_map.acquire();
    completion_entry_t &c = (*completion_map)[std::make_pair(cmd, cmd_is_path)];

    // Create our new option.
    complete_entry_opt_t opt;
    opt.option = option;
    opt.type = option_type;
    opt.result_mode = result_mode;

    if (comp) opt.comp = comp;
    opt.conditions = std::move(condition);
    if (desc) opt.desc = desc;
    opt.flags = flags;

    c.add_option(std::move(opt));
}

void complete_remove(const wcstring &cmd, bool cmd_is_path, const wcstring &option,
                     complete_option_type_t type) {
    auto completion_map = s_completion_map.acquire();
    auto iter = completion_map->find(std::make_pair(cmd, cmd_is_path));
    if (iter != completion_map->end()) {
        bool delete_it = iter->second.remove_option(option, type);
        if (delete_it) {
            completion_map->erase(iter);
        }
    }
}

void complete_remove_all(const wcstring &cmd, bool cmd_is_path) {
    auto completion_map = s_completion_map.acquire();
    completion_map->erase(std::make_pair(cmd, cmd_is_path));
}

completion_list_t complete(const wcstring &cmd_with_subcmds, completion_request_options_t flags,
                           const operation_context_t &ctx, std::vector<wcstring> *out_needs_loads) {
    // Determine the innermost subcommand.
    const wchar_t *cmdsubst_begin, *cmdsubst_end;
    parse_util_cmdsubst_extent(cmd_with_subcmds.c_str(), cmd_with_subcmds.size(), &cmdsubst_begin,
                               &cmdsubst_end);
    assert(cmdsubst_begin != nullptr && cmdsubst_end != nullptr && cmdsubst_end >= cmdsubst_begin);
    wcstring cmd = wcstring(cmdsubst_begin, cmdsubst_end - cmdsubst_begin);
    completer_t completer(ctx, flags);
    completer.perform_for_commandline(std::move(cmd));
    if (out_needs_loads) {
        *out_needs_loads = completer.acquire_needs_load();
    }
    return completer.acquire_completions();
}

/// Print the short switch \c opt, and the argument \c arg to the specified
/// wcstring, but only if \c argument isn't an empty string.
static void append_switch(wcstring &out, wchar_t opt, const wcstring &arg) {
    if (arg.empty()) return;
    append_format(out, L" -%lc %ls", opt, escape_string(arg).c_str());
}
static void append_switch(wcstring &out, const wcstring &opt, const wcstring &arg) {
    if (arg.empty()) return;
    append_format(out, L" --%ls %ls", opt.c_str(), escape_string(arg).c_str());
}
static void append_switch(wcstring &out, wchar_t opt) { append_format(out, L" -%lc", opt); }
static void append_switch(wcstring &out, const wcstring &opt) {
    append_format(out, L" --%ls", opt.c_str());
}

static wcstring completion2string(const completion_key_t &key, const complete_entry_opt_t &o) {
    const wcstring &cmd = key.first;
    bool is_path = key.second;
    wcstring out = L"complete";

    if (o.flags & COMPLETE_DONT_SORT) append_switch(out, L'k');

    if (o.result_mode.no_files && o.result_mode.requires_param) {
        append_switch(out, L"exclusive");
    } else if (o.result_mode.no_files) {
        append_switch(out, L"no-files");
    } else if (o.result_mode.force_files) {
        append_switch(out, L"force-files");
    } else if (o.result_mode.requires_param) {
        append_switch(out, L"require-parameter");
    }

    if (is_path)
        append_switch(out, L'p', cmd);
    else {
        out.append(L" ");
        out.append(escape_string(cmd));
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
    for (const auto &c : o.conditions) {
        append_switch(out, L'n', c);
    }
    out.append(L"\n");
    return out;
}

bool complete_load(const wcstring &cmd, parser_t &parser) {
    bool loaded_new = false;

    // We have to load this as a function, since it may define a --wraps or signature.
    // See issue #2466.
    if (function_load(cmd, parser)) {
        // We autoloaded something; check if we have a --wraps.
        loaded_new |= complete_get_wrap_targets(cmd).size() > 0;
    }

    // It's important to NOT hold the lock around completion loading.
    // We need to take the lock to decide what to load, drop it to perform the load, then reacquire
    // it.
    // Note we only look at the global fish_function_path and fish_complete_path.
    maybe_t<wcstring> path_to_load =
        completion_autoloader.acquire()->resolve_command(cmd, env_stack_t::globals());
    if (path_to_load) {
        autoload_t::perform_autoload(*path_to_load, parser);
        completion_autoloader.acquire()->mark_autoload_finished(cmd);
        loaded_new = true;
    }
    return loaded_new;
}

/// Use by the bare `complete`, loaded completions are printed out as commands
wcstring complete_print(const wcstring &cmd) {
    wcstring out;

    // Get references to our completions and sort them by order.
    auto completions = s_completion_map.acquire();
    using comp_ref_t = std::reference_wrapper<const completion_entry_map_t::value_type>;
    std::vector<comp_ref_t> completion_refs(completions->begin(), completions->end());
    std::sort(completion_refs.begin(), completion_refs.end(), [](comp_ref_t a, comp_ref_t b) {
        return a.get().second.order < b.get().second.order;
    });

    for (const comp_ref_t &cr : completion_refs) {
        const completion_key_t &key = cr.get().first;
        const completion_entry_t &entry = cr.get().second;
        if (!cmd.empty() && key.first != cmd) continue;
        const option_list_t &options = entry.get_options();
        // Output in reverse order to preserve legacy behavior (see #9221).
        for (auto o = options.rbegin(); o != options.rend(); ++o) {
            out.append(completion2string(key, *o));
        }
    }

    // Append wraps.
    auto locked_wrappers = wrapper_map.acquire();
    for (const auto &entry : *locked_wrappers) {
        const wcstring &src = entry.first;
        if (!cmd.empty() && src != cmd) continue;
        for (const wcstring &target : entry.second) {
            out.append(L"complete ");
            out.append(escape_string(src));
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
    std::vector<wcstring> cmds = completion_autoloader.acquire()->get_autoloaded_commands();
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
    std::vector<wcstring> *targets = &wraps[command];
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
        std::vector<wcstring> *targets = &current_targets_iter->second;
        auto where = std::find(targets->begin(), targets->end(), target_to_remove);
        if (where != targets->end()) {
            targets->erase(where);
            result = true;
        }
    }
    return result;
}

std::vector<wcstring> complete_get_wrap_targets(const wcstring &command) {
    if (command.empty()) {
        return {};
    }
    auto locked_map = wrapper_map.acquire();
    wrapper_map_t &wraps = *locked_map;
    auto iter = wraps.find(command);
    if (iter == wraps.end()) return {};
    return iter->second;
}
#endif
