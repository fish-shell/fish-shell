/// Functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
///
#include "config.h"  // IWYU pragma: keep

#include <pthread.h>
#include <pwd.h>
#include <stddef.h>
#include <cwchar>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "autoload.h"
#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "iothread.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "tnode.h"
#include "util.h"
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

static void complete_load(const wcstring &name, bool reload);

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
typedef struct complete_entry_opt {
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
    // Must be one of the values SHARED, NO_FILES, NO_COMMON, EXCLUSIVE, and determines how
    // completions should be performed on the argument after the switch.
    int result_mode;
    // Completion flags.
    complete_flags_t flags;

    const wcstring localized_desc() const { return C_(desc); }

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

} complete_entry_opt_t;

using arg_list_t = std::vector<tnode_t<grammar::argument>>;

/// Last value used in the order field of completion_entry_t.
static std::atomic<unsigned int> k_complete_order{0};

/// Struct describing a command completion.
typedef std::list<complete_entry_opt_t> option_list_t;
class completion_entry_t {
   public:
    /// List of all options.
    option_list_t options;

   public:
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
        return hasher((wcstring)c.cmd);
    }
};
template <>
struct equal_to<completion_entry_t> {
    bool operator()(const completion_entry_t &c1, const completion_entry_t &c2) const {
        return c1.cmd == c2.cmd;
    }
};
}  // namespace std
typedef std::unordered_set<completion_entry_t> completion_entry_set_t;
static owning_lock<completion_entry_set_t> s_completion_set;

/// Completion "wrapper" support. The map goes from wrapping-command to wrapped-command-list.
using wrapper_map_t = std::unordered_map<wcstring, wcstring_list_t>;
static owning_lock<wrapper_map_t> wrapper_map;

/// Comparison function to sort completions by their order field.
static bool compare_completions_by_order(const completion_entry_t &p1,
                                         const completion_entry_t &p2) {
    return p1.order < p2.order;
}

void completion_entry_t::add_option(const complete_entry_opt_t &opt) {
    options.push_front(opt);
}

const option_list_t &completion_entry_t::get_options() const {
    return options;
}

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
        if (len > 0 && (std::wcschr(L"/=@:", comp.at(len - 1)) != 0)) new_flags |= COMPLETE_NO_SPACE;
    }
    return new_flags;
}

/// completion_t functions. Note that the constructor resolves flags!
completion_t::completion_t(wcstring comp, wcstring desc, string_fuzzy_match_t mat,
                           complete_flags_t flags_val)
    : completion(std::move(comp)),
      description(std::move(desc)),
      match(std::move(mat)),
      flags(resolve_auto_space(completion, flags_val)) {}

completion_t::completion_t(const completion_t &him) = default;
completion_t::completion_t(completion_t &&him) = default;
completion_t &completion_t::operator=(const completion_t &him) = default;
completion_t &completion_t::operator=(completion_t &&him) = default;
completion_t::~completion_t() = default;

bool completion_t::is_naturally_less_than(const completion_t &a, const completion_t &b) {
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

static bool compare_completions_by_match_type(const completion_t &a, const completion_t &b) {
    return a.match.type < b.match.type;
}

static bool compare_completions_by_duplicate_arguments(const completion_t &a,
                                                       const completion_t &b) {
    bool ad = a.flags & COMPLETE_DUPLICATES_ARGUMENT;
    bool bd = b.flags & COMPLETE_DUPLICATES_ARGUMENT;
    return ad < bd;
}

template <class Iterator, class HashFunction>
static Iterator unique_unsorted(Iterator begin, Iterator end, HashFunction hash) {
    typedef typename std::iterator_traits<Iterator>::value_type T;

    std::unordered_set<size_t> temp;
    return std::remove_if(begin, end, [&](const T &val) { return !temp.insert(hash(val)).second; });
}

void completions_sort_and_prioritize(std::vector<completion_t> *comps,
                                     completion_request_flags_t flags) {
    // Find the best match type.
    fuzzy_match_type_t best_type = fuzzy_match_none;
    for (size_t i = 0; i < comps->size(); i++) {
        best_type = std::min(best_type, comps->at(i).match.type);
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

    // Sort, provided COMPLETION_DONT_SORT isn't set
    stable_sort(comps->begin(), comps->end(), completion_t::is_naturally_less_than);
    // Deduplicate both sorted and unsorted results
    comps->erase(
        unique_unsorted(comps->begin(), comps->end(),
                        [](const completion_t &c) { return std::hash<wcstring>{}(c.completion); }),
        comps->end());

    // Sort the remainder by match type. They're already sorted alphabetically.
    stable_sort(comps->begin(), comps->end(), compare_completions_by_match_type);

    // Lastly, if this is for an autosuggestion, prefer to avoid completions that duplicate
    // arguments.
    if (flags & COMPLETION_REQUEST_AUTOSUGGESTION)
        stable_sort(comps->begin(), comps->end(), compare_completions_by_duplicate_arguments);
}

/// Class representing an attempt to compute completions.
class completer_t {
    /// Environment inside which we are completing.
    const environment_t &vars;

    /// The command to complete.
    const wcstring cmd;

    /// Flags associated with the completion request.
    const completion_request_flags_t flags;

    /// The output cmopletions.
    std::vector<completion_t> completions;

    /// Table of completions conditions that have already been tested and the corresponding test
    /// results.
    typedef std::unordered_map<wcstring, bool> condition_cache_t;
    condition_cache_t condition_cache;

    enum complete_type_t { COMPLETE_DEFAULT, COMPLETE_AUTOSUGGEST };

    complete_type_t type() const {
        return flags & COMPLETION_REQUEST_AUTOSUGGESTION ? COMPLETE_AUTOSUGGEST : COMPLETE_DEFAULT;
    }

    bool wants_descriptions() const {
        return static_cast<bool>(flags & COMPLETION_REQUEST_DESCRIPTIONS);
    }

    bool fuzzy() const { return static_cast<bool>(flags & COMPLETION_REQUEST_FUZZY_MATCH); }

    fuzzy_match_type_t max_fuzzy_match_type() const {
        // If we are doing fuzzy matching, request all types; if not request only prefix matching.
        if (flags & COMPLETION_REQUEST_FUZZY_MATCH) return fuzzy_match_none;
        return fuzzy_match_prefix_case_insensitive;
    }

    bool try_complete_variable(const wcstring &str);
    bool try_complete_user(const wcstring &str);

    bool complete_param(const wcstring &cmd_orig, const wcstring &popt, const wcstring &str,
                        bool use_switches);

    void complete_param_expand(const wcstring &str, bool do_file,
                               bool handle_as_special_cd = false);

    void complete_cmd(const wcstring &str, bool use_function, bool use_builtin, bool use_command,
                      bool use_implicit_cd);

    /// Attempt to complete an abbreviation for the given string.
    void complete_abbr(const wcstring &str);

    void complete_from_args(const wcstring &str, const wcstring &args, const wcstring &desc,
                            complete_flags_t flags);

    void complete_cmd_desc(const wcstring &str);

    bool complete_variable(const wcstring &str, size_t start_offset);

    bool condition_test(const wcstring &condition);

    void complete_strings(const wcstring &wc_escaped, const description_func_t &desc_func,
                          const std::vector<completion_t> &possible_comp, complete_flags_t flags);

    expand_flags_t expand_flags() const {
        // Never do command substitution in autosuggestions. Sadly, we also can't yet do job
        // expansion because it's not thread safe.
        expand_flags_t result = 0;
        if (this->type() == COMPLETE_AUTOSUGGEST) result |= EXPAND_SKIP_CMDSUBST;

        // Allow fuzzy matching.
        if (this->fuzzy()) result |= EXPAND_FUZZY_MATCH;

        return result;
    }

    bool empty() const { return completions.empty(); }

    void mark_completions_duplicating_arguments(const wcstring &prefix, const arg_list_t &args);

   public:
    completer_t(const environment_t &vars, wcstring c, completion_request_flags_t f)
        : vars(vars), cmd(std::move(c)), flags(f) {}

    void perform();

    std::vector<completion_t> acquire_completions() { return std::move(completions); }
};

// Callback when an autoloaded completion is removed.
static void autoloaded_completion_removed(const wcstring &cmd) {
    complete_remove_all(cmd, false /* not a path */);
}

// Autoloader for completions
static autoload_t completion_autoloader(L"fish_complete_path", autoloaded_completion_removed);

/// Create a new completion entry.
void append_completion(std::vector<completion_t> *completions, wcstring comp, wcstring desc,
                       complete_flags_t flags, string_fuzzy_match_t match) {
    complete_flags_t resolved_flags = resolve_auto_space(comp, flags);
    completion_t completion{std::move(comp), std::move(desc), match, resolved_flags};
    completions->push_back(std::move(completion));
}

/// Test if the specified script returns zero. The result is cached, so that if multiple completions
/// use the same condition, it needs only be evaluated once. condition_cache_clear must be called
/// after a completion run to make sure that there are no stale completions.
bool completer_t::condition_test(const wcstring &condition) {
    if (condition.empty()) {
        // std::fwprintf( stderr, L"No condition specified\n" );
        return true;
    }

    if (this->type() == COMPLETE_AUTOSUGGEST) {
        // Autosuggestion can't support conditions.
        return false;
    }

    ASSERT_IS_MAIN_THREAD();

    bool test_res;
    condition_cache_t::iterator cached_entry = condition_cache.find(condition);
    if (cached_entry == condition_cache.end()) {
        // Compute new value and reinsert it.
        // TODO: rationalize this principal_parser.
        test_res = (0 == exec_subshell(condition, parser_t::principal_parser(),
                                       false /* don't apply exit status */));
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
    auto ins = completion_set.emplace(completion_entry_t(cmd, cmd_is_path));

    // NOTE SET_ELEMENTS_ARE_IMMUTABLE: Exposing mutable access here is only okay as long as callers
    // do not change any field that matters to ordering - affecting order without telling std::set
    // invalidates its internal state.
    return const_cast<completion_entry_t &>(*ins.first);
}

void complete_add(const wchar_t *cmd, bool cmd_is_path, const wcstring &option,
                  complete_option_type_t option_type, int result_mode, const wchar_t *condition,
                  const wchar_t *comp, const wchar_t *desc, complete_flags_t flags) {
    CHECK(cmd, );
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
    option_list_t::iterator iter = this->options.begin();
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
    completion_entry_set_t::iterator iter = completion_set->find(tmp_entry);
    if (iter != completion_set->end()) {
        // const_cast: See SET_ELEMENTS_ARE_IMMUTABLE.
        completion_entry_t &entry = const_cast<completion_entry_t &>(*iter);

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
                                   const std::vector<completion_t> &possible_comp,
                                   complete_flags_t flags) {
    wcstring tmp = wc_escaped;
    if (!expand_one(tmp, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_WILDCARDS | this->expand_flags(), vars))
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

    if (wildcard_has(cmd, 0)) {
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
    lookup_cmd.append(escape_string(cmd, 1));

    // First locate a list of possible descriptions using a single call to apropos or a direct
    // search if we know the location of the whatis database. This can take some time on slower
    // systems with a large set of manuals, but it should be ok since apropos is only called once.
    wcstring_list_t list;
    // TODO: justify this use of parser_t::principal_parser.
    if (exec_subshell(lookup_cmd, parser_t::principal_parser(), list,
                      false /* don't apply exit status */) != -1) {
        std::unordered_map<wcstring, wcstring> lookup;
        lookup.reserve(list.size());

        // Then discard anything that is not a possible completion and put the result into a
        // hashtable with the completion as key and the description as value.
        //
        // Should be reasonably fast, since no memory allocations are needed.
        // mqudsi: I don't know if the above were ever true, but it's certainly not any more.
        // Plenty of allocations below.
        for (const wcstring &elstr : list) {
            if (elstr.length() < cmd.length()) continue;
            const wcstring fullkey(elstr, cmd.length());

            size_t tab_idx = fullkey.find(L'\t');
            if (tab_idx == wcstring::npos) continue;

            const wcstring key(fullkey, 0, tab_idx);
            wcstring val(fullkey, tab_idx + 1);

            // And once again I make sure the first character is uppercased because I like it that
            // way, and I get to decide these things.
            if (!val.empty()) val[0] = towupper(val[0]);
            lookup[key] = val;
        }

        // Then do a lookup on every completion and if a match is found, change to the new
        // description.
        //
        // This needs to do a reallocation for every description added, but there shouldn't be that
        // many completions, so it should be ok.
        for (auto &completion : completions) {
            const wcstring &el = completion.completion;
            if (el.empty()) continue;

            auto new_desc_iter = lookup.find(el);
            if (new_desc_iter != lookup.end()) completion.description = new_desc_iter->second;
        }
    }
}

/// Returns a description for the specified function, or an empty string if none.
static wcstring complete_function_desc(const wcstring &fn) {
    wcstring result;
    bool has_description = function_get_desc(fn, result);
    if (!has_description) {
        function_get_definition(fn, result);
    }
    return result;
}

/// Complete the specified command name. Search for executables in the path, executables defined
/// using an absolute path, functions, builtins and directories for implicit cd commands.
///
/// \param str_cmd the command string to find completions for
void completer_t::complete_cmd(const wcstring &str_cmd, bool use_function, bool use_builtin,
                               bool use_command, bool use_implicit_cd) {
    if (str_cmd.empty()) return;

    std::vector<completion_t> possible_comp;

    if (use_command) {
        // Append all possible executables
        expand_error_t result = expand_string(str_cmd, &this->completions,
                                              EXPAND_SPECIAL_FOR_COMMAND | EXPAND_FOR_COMPLETIONS |
                                                  EXECUTABLES_ONLY | this->expand_flags(),
                                              vars, NULL);
        if (result != EXPAND_ERROR && this->wants_descriptions()) {
            this->complete_cmd_desc(str_cmd);
        }
    }

    if (use_implicit_cd) {
        // We don't really care if this succeeds or fails. If it succeeds this->completions will be
        // updated with choices for the user.
        expand_error_t ignore =
            // Append all matching directories
            expand_string(str_cmd, &this->completions,
                          EXPAND_FOR_COMPLETIONS | DIRECTORIES_ONLY | this->expand_flags(), vars,
                          NULL);
        UNUSED(ignore);
    }

    if (str_cmd.find(L'/') == wcstring::npos && str_cmd.at(0) != L'~') {
        if (use_function) {
            wcstring_list_t names = function_get_names(str_cmd.at(0) == L'_');
            for (wcstring &name : names) {
                // Append all known matching functions
                append_completion(&possible_comp, std::move(name));
            }

            this->complete_strings(str_cmd, complete_function_desc, possible_comp, 0);
        }

        possible_comp.clear();

        if (use_builtin) {
            // Append all matching builtins
            builtin_get_names(&possible_comp);
            this->complete_strings(str_cmd, builtin_get_desc, possible_comp, 0);
        }
    }
}

void completer_t::complete_abbr(const wcstring &cmd) {
    std::map<wcstring, wcstring> abbrs = get_abbreviations();
    std::vector<completion_t> possible_comp;
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
///    The list into which the results will be inserted
///
void completer_t::complete_from_args(const wcstring &str, const wcstring &args,
                                     const wcstring &desc, complete_flags_t flags) {
    bool is_autosuggest = (this->type() == COMPLETE_AUTOSUGGEST);

    // If type is COMPLETE_AUTOSUGGEST, it means we're on a background thread, so don't call
    // proc_push_interactive.
    if (!is_autosuggest) {
        proc_push_interactive(0);
    }

    expand_flags_t eflags = 0;
    if (is_autosuggest) {
        eflags |= EXPAND_NO_DESCRIPTIONS | EXPAND_SKIP_CMDSUBST;
    }

    std::vector<completion_t> possible_comp;
    parser_t::expand_argument_list(args, eflags, vars, &possible_comp);

    if (!is_autosuggest) {
        proc_pop_interactive();
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
        return NULL;
    }

    // Verify leading dashes.
    size_t cursor = leading_dash_count(optstr);
    if (cursor != e->expected_dash_count()) {
        return NULL;
    }

    // Verify options match.
    if (!string_prefixes_string(e->option, &optstr[cursor])) {
        return NULL;
    }
    cursor += e->option.length();

    // Short options are like -DNDEBUG. Long options are like --color=auto. So check for an equal
    // sign for long options.
    if (e->type != option_type_short) {
        if (optstr[cursor] != L'=') {
            return NULL;
        }
        cursor += 1;
    }
    return &optstr[cursor];
}

/// Tests whether a short option is a viable completion. arg_str will be like '-xzv', nextopt will
/// be a character like 'f' options will be the list of all options, used to validate the argument.
static bool short_ok(const wcstring &arg, const complete_entry_opt_t *entry,
                     const option_list_t &options) {
    // Ensure it's a short option.
    if (entry->type != option_type_short || entry->option.empty()) {
        return false;
    }
    const wchar_t nextopt = entry->option.at(0);

    // Empty strings are always 'OK'.
    if (arg.empty()) {
        return true;
    }

    // The argument must start with exactly one dash.
    if (leading_dash_count(arg.c_str()) != 1) {
        return false;
    }

    // Short option must not be already present.
    if (arg.find(nextopt) != wcstring::npos) {
        return false;
    }

    // Verify that all characters in our combined short option list are present as short options in
    // the options list. If we get a short option that can't be combined (NO_COMMON), then we stop.
    bool result = true;
    for (size_t i = 1; i < arg.size(); i++) {
        wchar_t arg_char = arg.at(i);
        const complete_entry_opt_t *match = NULL;
        for (option_list_t::const_iterator iter = options.begin(); iter != options.end(); ++iter) {
            if (iter->type == option_type_short && iter->option.at(0) == arg_char) {
                match = &*iter;
                break;
            }
        }
        if (match == NULL || (match->result_mode & NO_COMMON)) {
            result = false;
            break;
        }
    }
    return result;
}

/// Load command-specific completions for the specified command.
static void complete_load(const wcstring &name, bool reload) {
    // We have to load this as a function, since it may define a --wraps or signature.
    // See issue #2466.
    function_load(name);
    completion_autoloader.load(name, reload);
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
    bool use_common = 1, use_files = 1;

    wcstring cmd, path;
    parse_cmd_string(cmd_orig, &path, &cmd, vars);

    // mqudsi: run_on_main_thread() already just runs `func` if we're on the main thread,
    // but it makes a kcall to get the current thread id to ascertain that. Perhaps even
    // that single kcall proved to be a source of slowdown so this test on a local variable
    // is used to make that determination instead? I don't know.
    auto run_on_main_thread = [&](std::function<void(void)> &&func) {
        if (this->type() == COMPLETE_DEFAULT) {
            ASSERT_IS_MAIN_THREAD();
            func();
        } else if (this->type() == COMPLETE_AUTOSUGGEST) {
            iothread_perform_on_main([&]() { func(); });
        } else {
            assert(false && "this->type() is unknown!");
        }
    };

    // debug(0, L"\nThinking about looking up completions for %ls\n", cmd.c_str());
    bool head_exists = builtin_exists(cmd);
    // Only reload environment variables if builtin_exists returned false, as an optimization
    if (head_exists == false) {
        head_exists = function_exists_no_autoload(cmd, vars);
        // While it may seem like first testing `path_get_path` before resorting to an env lookup
        // may be faster, path_get_path can potentially do a lot of FS/IO access, so env.get() +
        // function_exists() should still be faster.
        head_exists =
            head_exists || path_get_path(cmd_orig, nullptr,
                                         vars);  // use cmd_orig here as it is potentially pathed
    }

    if (!head_exists) {
        // Do not load custom completions if the head does not exist
        // This prevents errors caused during the execution of completion providers for
        // tools that do not exist. Applies to both manual completions ("cm<TAB>", "cmd <TAB>")
        // and automatic completions ("gi" autosuggestion provider -> git)
        debug(4, "Skipping completions for non-existent head\n");
    } else {
        run_on_main_thread([&]() { complete_load(cmd, true); });
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
        use_common = 1;
        if (use_switches) {
            if (str[0] == L'-') {
                // Check if we are entering a combined option and argument (like --color=auto or
                // -I/usr/include).
                for (const complete_entry_opt_t &o : options) {
                    const wchar_t *arg = param_match2(&o, str.c_str());
                    if (arg != NULL && this->condition_test(o.condition)) {
                        if (o.result_mode & NO_COMMON) use_common = false;
                        if (o.result_mode & NO_FILES) use_files = false;
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
                        if (o.result_mode & NO_COMMON) use_common = false;
                        if (o.result_mode & NO_FILES) use_files = false;
                        complete_from_args(str, o.comp, o.localized_desc(), o.flags);
                    }
                }

                // No old style option matched, or we are not using old style options. We check if
                // any short (or gnu style options do.
                if (!old_style_match) {
                    for (const complete_entry_opt_t &o : options) {
                        // Gnu-style options with _optional_ arguments must be specified as a single
                        // token, so that it can be differed from a regular argument.
                        // Here we are testing the previous argument for a GNU-style match,
                        // to see how we should complete the current argument
                        if (o.type == option_type_double_long && !(o.result_mode & NO_COMMON))
                            continue;

                        if (param_match(&o, popt.c_str()) && this->condition_test(o.condition)) {
                            if (o.result_mode & NO_COMMON) use_common = false;
                            if (o.result_mode & NO_FILES) use_files = false;
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
                use_files = use_files && ((o.result_mode & NO_FILES) == 0);
                complete_from_args(str, o.comp, o.localized_desc(), o.flags);
            }

            if (!use_switches || str.empty()) {
                continue;
            }

            // Check if the short style option matches.
            if (short_ok(str, &o, options)) {
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
            req_arg = (o.result_mode & NO_COMMON);

            if (o.type == option_type_double_long && (has_arg && !req_arg)) {
                // Optional arguments to a switch can only be handled using the '=', so we add it as
                // a completion. By default we avoid using '=' and instead rely on '--switch
                // switch-arg', since it is more commonly supported by homebrew getopt-like
                // functions.
                wcstring completion = format_string(L"%ls=", whole_opt.c_str() + offset);
                // Append a long-style option with a mandatory trailing equal sign
                append_completion(&this->completions, std::move(completion), C_(o.desc), flags);
            }

            // Append a long-style option
            append_completion(&this->completions, whole_opt.substr(offset), C_(o.desc), flags);
        }
    }

    return use_files;
}

/// Perform generic (not command-specific) expansions on the specified string.
void completer_t::complete_param_expand(const wcstring &str, bool do_file,
                                        bool handle_as_special_cd) {
    expand_flags_t flags = EXPAND_SKIP_CMDSUBST | EXPAND_FOR_COMPLETIONS | this->expand_flags();

    if (!do_file) flags |= EXPAND_SKIP_WILDCARDS;

    if (handle_as_special_cd && do_file) {
        if (this->type() == COMPLETE_AUTOSUGGEST) {
            flags |= EXPAND_SPECIAL_FOR_CD_AUTOSUGGEST;
        }
        flags |= DIRECTORIES_ONLY | EXPAND_SPECIAL_FOR_CD | EXPAND_NO_DESCRIPTIONS;
    }

    // Squelch file descriptions per issue #254.
    if (this->type() == COMPLETE_AUTOSUGGEST || do_file) flags |= EXPAND_NO_DESCRIPTIONS;

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
        std::vector<completion_t> local_completions;
        if (expand_string(sep_string, &local_completions, flags, vars, NULL) == EXPAND_ERROR) {
            debug(3, L"Error while expanding string '%ls'", sep_string.c_str());
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
        if (string_prefixes_string(L"-", str)) flags &= ~EXPAND_FUZZY_MATCH;

        if (expand_string(str, &this->completions, flags, vars, NULL) == EXPAND_ERROR) {
            debug(3, L"Error while expanding string '%ls'", str.c_str());
        }
    }
}

/// Complete the specified string as an environment variable.
bool completer_t::complete_variable(const wcstring &str, size_t start_offset) {
    const wchar_t *const whole_var = str.c_str();
    const wchar_t *var = &whole_var[start_offset];
    size_t varlen = str.length() - start_offset;
    bool res = false;

    for (const wcstring &env_name : vars.get_names(0)) {
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
            // Can't use this->vars here, it could be any variable.
            auto var = vars.get(env_name);
            if (!var) continue;

            if (this->type() != COMPLETE_AUTOSUGGEST) {
                wcstring value = expand_escape_variable(*var);
                desc = format_string(COMPLETE_VAR_DESC_VAL, value.c_str());
            }
        }

        // Append matching environment variables
        append_completion(&this->completions, std::move(comp), desc, flags, match);

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
    bool allow_empty = !(this->flags & COMPLETION_REQUEST_AUTOSUGGESTION);
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

    static std::mutex lock;
    scoped_lock locker(lock);
    setpwent();
    // cppcheck-suppress getpwentCalled
    while (struct passwd *pw = getpwent()) {
        bool interrupted =
            is_main_thread() ? reader_test_and_clear_interrupted() : reader_thread_job_is_stale();
        if (interrupted) {
            break;
        }
        const wcstring pw_name_str = str2wcstring(pw->pw_name);
        const wchar_t *pw_name = pw_name_str.c_str();
        if (std::wcsncmp(user_name, pw_name, name_len) == 0) {
            wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
            // Append a user name
            append_completion(&this->completions, &pw_name[name_len], desc, COMPLETE_NO_SPACE);
            result = true;
        } else if (wcsncasecmp(user_name, pw_name, name_len) == 0) {
            wcstring name = format_string(L"~%ls", pw_name);
            wcstring desc = format_string(COMPLETE_USER_DESC, pw_name);
            // Append a user name
            append_completion(&this->completions, name, desc,
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

// The callback type for walk_wrap_chain
using wrap_chain_visitor_t = std::function<void(const wcstring &, const wcstring &, size_t depth)>;

// Helper to complete a parameter for a command and its transitive wrap chain.
// Given a command line \p command_line and the range of the command itself within the command line
// as \p command_range, invoke the \p receiver with the command and the command line. Then, for each
// target wrapped by the given command, update the command line with that target and invoke this
// recursively.
static void walk_wrap_chain(const wcstring &command_line, source_range_t command_range,
                            const wrap_chain_visitor_t &visitor, size_t depth = 0) {
    // Limit our recursion depth. This prevents cycles in the wrap chain graph from overflowing.
    if (depth > 24) return;

    // Extract command from the command line and invoke the receiver with it.
    wcstring command(command_line, command_range.start, command_range.length);
    visitor(command, command_line, depth);

    wcstring_list_t targets = complete_get_wrap_targets(command);
    for (const wcstring &wt : targets) {
        // Construct a fake command line containing the wrap target.
        wcstring faux_commandline = command_line;
        faux_commandline.replace(command_range.start, command_range.length, wt);

        // Try to extract the command from the faux commandline.
        // We do this by simply getting the first token. This is a hack; for example one might
        // imagine the first token being 'builtin' or similar. Nevertheless that is simpler than
        // re-parsing everything.
        wcstring wrapped_command = tok_first(wt);
        if (!wrapped_command.empty()) {
            size_t where = faux_commandline.find(wrapped_command, command_range.start);
            if (where != wcstring::npos) {
                // Recurse with our new command and command line.
                source_range_t faux_source_range{uint32_t(where), uint32_t(wrapped_command.size())};
                walk_wrap_chain(faux_commandline, faux_source_range, visitor, depth + 1);
            }
        }
    }
}

/// Set the DUPLICATES_ARG flag in any completion that duplicates an argument.
void completer_t::mark_completions_duplicating_arguments(const wcstring &prefix,
                                                         const arg_list_t &args) {
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

/// Return the index of an argument from \p args containing the position \p pos, or none if none.
static maybe_t<size_t> find_argument_containing_position(const arg_list_t &args, size_t pos) {
    size_t idx = 0;
    for (const auto &arg : args) {
        if (arg.location_in_or_at_end_of_source_range(pos)) {
            return idx;
        }
        idx++;
    }
    return none();
}

void completer_t::perform() {
    wcstring current_command;
    const size_t pos = cmd.size();
    // debug( 1, L"Complete '%ls'", cmd.c_str() );

    const wchar_t *tok_begin = nullptr;
    parse_util_token_extent(cmd.c_str(), cmd.size(), &tok_begin, nullptr, nullptr, nullptr);
    assert(tok_begin != nullptr);

    // If we are completing a variable name or a tilde expansion user name, we do that and return.
    // No need for any other completions.
    // Unconditionally complete variables and processes. This is a little weird since we will
    // happily complete variables even in e.g. command position, despite the fact that they are
    // invalid there. */
    const wcstring current_token = tok_begin;
    if (try_complete_variable(current_token) || try_complete_user(current_token)) {
        return;
    }

    parse_node_tree_t tree;
    parse_tree_from_string(cmd,
                           parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens |
                               parse_flag_include_comments,
                           &tree, NULL);

    // Find the plain statement to operate on. The cursor may be past it (#1261), so backtrack
    // until we know we're no longer in a space. But the space may actually be part of the
    // argument (#2477).
    size_t position_in_statement = pos;
    while (position_in_statement > 0 && cmd.at(position_in_statement - 1) == L' ') {
        position_in_statement--;
    }
    auto plain_statement = tnode_t<grammar::plain_statement>::find_node_matching_source_location(
        &tree, position_in_statement, nullptr);
    if (!plain_statement) {
        // Not part of a plain statement. This could be e.g. a for loop header, case expression,
        // etc. Do generic file completions (issue #1309). If we had to backtrack, it means
        // there was whitespace; don't do an autosuggestion in that case. Also don't do it if we
        // are just after a pipe, semicolon, or & (issue #1631), or in a comment.
        //
        // Overall this logic is a total mess. A better approach would be to return the
        // "possible next token" from the parse tree directly (this data is available as the
        // first of the sequence of nodes without source locations at the very end of the parse
        // tree).
        bool do_file = true;
        if (flags & COMPLETION_REQUEST_AUTOSUGGESTION) {
            if (position_in_statement < pos) {
                do_file = false;
            } else if (pos > 0) {
                // If the previous character is in one of these types, we don't do file
                // suggestions.
                const parse_token_type_t bad_types[] = {parse_token_type_pipe, parse_token_type_end,
                                                        parse_token_type_background,
                                                        parse_special_type_comment};
                for (parse_token_type_t type : bad_types) {
                    if (tree.find_node_matching_source_location(type, pos - 1, NULL)) {
                        do_file = false;
                        break;
                    }
                }
            }
        }
        complete_param_expand(current_token, do_file);
    } else {
        assert(plain_statement && plain_statement.has_source());

        bool use_command = true;
        bool use_function = true;
        bool use_builtin = true;
        bool use_implicit_cd = true;
        bool use_abbr = true;

        // Get the command node.
        tnode_t<grammar::tok_string> cmd_node = plain_statement.child<0>();
        assert(cmd_node && cmd_node.has_source() && "Expected command node to be valid");

        // Get the actual command string.
        current_command = cmd_node.get_source(cmd);

        // Check the decoration.
        switch (get_decoration(plain_statement)) {
            case parse_statement_decoration_none: {
                use_command = true;
                use_function = true;
                use_builtin = true;
                use_implicit_cd = true;
                use_abbr = true;
                break;
            }
            case parse_statement_decoration_command:
            case parse_statement_decoration_exec: {
                use_command = true;
                use_function = false;
                use_builtin = false;
                use_implicit_cd = false;
                use_abbr = false;
                break;
            }
            case parse_statement_decoration_builtin: {
                use_command = false;
                use_function = false;
                use_builtin = true;
                use_implicit_cd = false;
                use_abbr = false;
                break;
            }
            case parse_statement_decoration_eval: {
                use_command = true;
                use_function = true;
                use_builtin = true;
                use_implicit_cd = true;
                use_abbr = true;
                break;
            }
        }

        if (cmd_node.location_in_or_at_end_of_source_range(pos)) {
            // Complete command filename.
            complete_cmd(current_token, use_function, use_builtin, use_command, use_implicit_cd);
            if (use_abbr) complete_abbr(current_token);
        } else {
            // Get all the arguments.
            arg_list_t all_arguments = plain_statement.descendants<grammar::argument>();

            // See whether we are in an argument. We may also be in a redirection, or nothing at
            // all.
            maybe_t<size_t> matching_arg_index =
                find_argument_containing_position(all_arguments, position_in_statement);

            bool had_ddash = false;
            wcstring current_argument, previous_argument;
            if (matching_arg_index) {
                const wcstring matching_arg = all_arguments.at(*matching_arg_index).get_source(cmd);

                // If the cursor is in whitespace, then the "current" argument is empty and the
                // previous argument is the matching one. But if the cursor was in or at the end
                // of the argument, then the current argument is the matching one, and the
                // previous argument is the one before it.
                bool cursor_in_whitespace =
                    !plain_statement.location_in_or_at_end_of_source_range(pos);
                if (cursor_in_whitespace) {
                    current_argument.clear();
                    previous_argument = matching_arg;
                } else {
                    current_argument = matching_arg;
                    if (*matching_arg_index > 0) {
                        previous_argument =
                            all_arguments.at(*matching_arg_index - 1).get_source(cmd);
                    }
                }

                // Check to see if we have a preceding double-dash.
                for (size_t i = 0; i < *matching_arg_index; i++) {
                    if (all_arguments.at(i).get_source(cmd) == L"--") {
                        had_ddash = true;
                        break;
                    }
                }
            }

            // If we are not in an argument, we may be in a redirection.
            bool in_redirection = false;
            if (!matching_arg_index) {
                if (tnode_t<grammar::redirection>::find_node_matching_source_location(
                        &tree, position_in_statement, plain_statement)) {
                    in_redirection = true;
                }
            }

            bool do_file = false, handle_as_special_cd = false;
            if (in_redirection) {
                do_file = true;
            } else {
                // Try completing as an argument.
                wcstring current_command_unescape, previous_argument_unescape,
                    current_argument_unescape;
                if (unescape_string(current_command, &current_command_unescape, UNESCAPE_DEFAULT) &&
                    unescape_string(previous_argument, &previous_argument_unescape,
                                    UNESCAPE_DEFAULT) &&
                    unescape_string(current_argument, &current_argument_unescape,
                                    UNESCAPE_INCOMPLETE)) {
                    // Have to walk over the command and its entire wrap chain. If any command
                    // disables do_file, then they all do.
                    do_file = true;
                    auto receiver = [&](const wcstring &cmd, const wcstring &cmdline,
                                        size_t depth) {
                        // Perhaps set a transient commandline so that custom completions
                        // buitin_commandline will refer to the wrapped command. But not if
                        // we're doing autosuggestions.
                        std::unique_ptr<builtin_commandline_scoped_transient_t> bcst;
                        if (depth > 0 && !(flags & COMPLETION_REQUEST_AUTOSUGGESTION)) {
                            bcst = make_unique<builtin_commandline_scoped_transient_t>(cmdline);
                        }
                        // Now invoke any custom completions for this command.
                        if (!complete_param(cmd, previous_argument_unescape,
                                            current_argument_unescape, !had_ddash)) {
                            do_file = false;
                        }
                    };
                    walk_wrap_chain(cmd, *cmd_node.source_range(), receiver);
                }

                // Hack. If we're cd, handle it specially (issue #1059, others).
                handle_as_special_cd = (current_command_unescape == L"cd");

                // And if we're autosuggesting, and the token is empty, don't do file suggestions.
                if ((flags & COMPLETION_REQUEST_AUTOSUGGESTION) &&
                    current_argument_unescape.empty()) {
                    do_file = false;
                }
            }

            // This function wants the unescaped string.
            complete_param_expand(current_token, do_file, handle_as_special_cd);

            // Lastly mark any completions that appear to already be present in arguments.
            mark_completions_duplicating_arguments(current_token, all_arguments);
        }
    }
}

void complete(const wcstring &cmd_with_subcmds, std::vector<completion_t> *out_comps,
              completion_request_flags_t flags, const environment_t &vars) {
    // Determine the innermost subcommand.
    const wchar_t *cmdsubst_begin, *cmdsubst_end;
    parse_util_cmdsubst_extent(cmd_with_subcmds.c_str(), cmd_with_subcmds.size(), &cmdsubst_begin,
                               &cmdsubst_end);
    assert(cmdsubst_begin != NULL && cmdsubst_end != NULL && cmdsubst_end >= cmdsubst_begin);
    wcstring cmd = wcstring(cmdsubst_begin, cmdsubst_end - cmdsubst_begin);
    completer_t completer(vars, std::move(cmd), flags);
    completer.perform();
    *out_comps = completer.acquire_completions();
}

/// Print the GNU longopt style switch \c opt, and the argument \c argument to the specified
/// stringbuffer, but only if arguemnt is non-null and longer than 0 characters.
static void append_switch(wcstring &out, const wcstring &opt, const wcstring &argument) {
    if (argument.empty()) return;

    wcstring esc = escape_string(argument, 1);
    append_format(out, L" --%ls %ls", opt.c_str(), esc.c_str());
}

wcstring complete_print() {
    wcstring out;
    auto completion_set = s_completion_set.acquire();

    // Get a list of all completions in a vector, then sort it by order.
    std::vector<std::reference_wrapper<const completion_entry_t>> all_completions;
    for (const completion_entry_t &i : *completion_set) {
        all_completions.emplace_back(i);
    }
    sort(all_completions.begin(), all_completions.end(), compare_completions_by_order);

    for (const completion_entry_t &e : all_completions) {
        const option_list_t &options = e.get_options();
        for (const complete_entry_opt_t &o : options) {
            const wchar_t *modestr[] = {L"", L" --no-files", L" --require-parameter",
                                        L" --exclusive"};

            append_format(out, L"complete%ls", modestr[o.result_mode]);

            append_switch(out, e.cmd_is_path ? L"path" : L"command",
                          escape_string(e.cmd, ESCAPE_ALL));

            switch (o.type) {
                case option_type_args_only: {
                    break;
                }
                case option_type_short: {
                    assert(!o.option.empty());  //!OCLINT(multiple unary operator)
                    append_format(out, L" --short-option '%lc'", o.option.at(0));
                    break;
                }
                case option_type_single_long:
                case option_type_double_long: {
                    append_switch(
                        out, o.type == option_type_single_long ? L"old-option" : L"long-option",
                        o.option);
                    break;
                }
            }

            append_switch(out, L"description", C_(o.desc));
            append_switch(out, L"arguments", o.comp);
            append_switch(out, L"condition", o.condition);
            out.append(L"\n");
        }
    }

    // Append wraps.
    auto locked_wrappers = wrapper_map.acquire();
    for (const auto &entry : *locked_wrappers) {
        const wcstring &src = entry.first;
        for (const wcstring &target : entry.second) {
            append_format(out, L"complete --command %ls --wraps %ls\n", src.c_str(),
                          target.c_str());
        }
    }
    return out;
}

void complete_invalidate_path() { completion_autoloader.invalidate(); }

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
    wrapper_map_t::iterator current_targets_iter = wraps.find(command);
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
