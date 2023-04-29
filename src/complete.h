/// Prototypes for functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
#ifndef FISH_COMPLETE_H
#define FISH_COMPLETE_H

#include "config.h"  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#if INCLUDE_RUST_HEADERS
#include "complete.rs.h"
#else
struct CompletionListFfi;
#endif

#include "common.h"
#include "expand.h"
#include "parser.h"
#include "wcstringutil.h"

using completion_list_t = CompletionListFfi;

struct completion_mode_t {
    /// If set, skip file completions.
    bool no_files{false};
    bool force_files{false};

    /// If set, require a parameter after completion.
    bool requires_param{false};
};

/// Character that separates the completion and description on programmable completions.
#define PROG_COMPLETE_SEP L'\t'

enum {
    /// Do not insert space afterwards if this is the only completion. (The default is to try insert
    /// a space).
    COMPLETE_NO_SPACE = 1 << 0,
    /// This is not the suffix of a token, but replaces it entirely.
    COMPLETE_REPLACES_TOKEN = 1 << 1,
    /// This completion may or may not want a space at the end - guess by checking the last
    /// character of the completion.
    COMPLETE_AUTO_SPACE = 1 << 2,
    /// This completion should be inserted as-is, without escaping.
    COMPLETE_DONT_ESCAPE = 1 << 3,
    /// If you do escape, don't escape tildes.
    COMPLETE_DONT_ESCAPE_TILDES = 1 << 4,
    /// Do not sort supplied completions
    COMPLETE_DONT_SORT = 1 << 5,
    /// This completion looks to have the same string as an existing argument.
    COMPLETE_DUPLICATES_ARGUMENT = 1 << 6,
    /// This completes not just a token but replaces the entire commandline.
    COMPLETE_REPLACES_COMMANDLINE = 1 << 7,
};
using complete_flags_t = uint8_t;

#if 0

/// std::function which accepts a completion string and returns its description.
using description_func_t = std::function<wcstring(const wcstring &)>;

/// Helper to return a description_func_t for a constant string.
description_func_t const_desc(const wcstring &s);

/// This is an individual completion entry, i.e. the result of an expansion of a completion rule.
class completion_t {
   private:
    // No public default constructor.
    completion_t();

   public:
    // Destructor. Not inlining it saves code size.
    ~completion_t();

    /// The completion string.
    wcstring completion;
    /// The description for this completion.
    wcstring description;
    /// The type of fuzzy match.
    string_fuzzy_match_t match;
    /// Flags determining the completion behavior.
    complete_flags_t flags;

    // Construction.
    explicit completion_t(wcstring comp, wcstring desc = wcstring(),
                          string_fuzzy_match_t match = string_fuzzy_match_t::exact_match(),
                          complete_flags_t flags_val = 0);
    completion_t(const completion_t &);
    completion_t &operator=(const completion_t &);

    // noexcepts are required for push_back to use the move ctor.
    completion_t(completion_t &&) noexcept;
    completion_t &operator=(completion_t &&) noexcept;

    /// \return whether this replaces its token.
    bool replaces_token() const { return flags & COMPLETE_REPLACES_TOKEN; }

    /// \return whether this replaces the entire commandline.
    bool replaces_commandline() const { return flags & COMPLETE_REPLACES_COMMANDLINE; }

    /// \return the completion's match rank. Lower ranks are better completions.
    uint32_t rank() const { return match.rank(); }

    // If this completion replaces the entire token, prepend a prefix. Otherwise do nothing.
    void prepend_token_prefix(const wcstring &prefix);
};

using completion_list_t = std::vector<completion_t>;

struct completion_request_options_t {
    bool autosuggestion{};  // requesting autosuggestion
    bool descriptions{};    // make descriptions
    bool fuzzy_match{};     // if set, we do not require a prefix match

    // Options for an autosuggestion.
    static completion_request_options_t autosuggest() {
        completion_request_options_t res{};
        res.autosuggestion = true;
        res.descriptions = false;
        res.fuzzy_match = false;
        return res;
    }

    // Options for a "normal" completion.
    static completion_request_options_t normal() {
        completion_request_options_t res{};
        res.autosuggestion = false;
        res.descriptions = true;
        res.fuzzy_match = true;
        return res;
    }
};

using completion_list_t = std::vector<completion_t>;

/// A completion receiver accepts completions. It is essentially a wrapper around std::vector with
/// some conveniences.
class completion_receiver_t {
   public:
    /// Construct as empty, with a limit.
    explicit completion_receiver_t(size_t limit) : limit_(limit) {}

    /// Acquire an existing list, with a limit.
    explicit completion_receiver_t(completion_list_t &&v, size_t limit)
        : completions_(std::move(v)), limit_(limit) {}

    /// Add a completion.
    /// \return true on success, false if this would overflow the limit.
    __warn_unused bool add(completion_t &&comp);

    /// Add a completion with the given string, and default other properties.
    /// \return true on success, false if this would overflow the limit.
    __warn_unused bool add(wcstring &&comp);

    /// Add a completion with the given string, description, flags, and fuzzy match.
    /// \return true on success, false if this would overflow the limit.
    /// The 'desc' parameter is not && because if gettext is not enabled, then we end
    /// up passing a 'const wcstring &' here.
    __warn_unused bool add(wcstring &&comp, wcstring desc, complete_flags_t flags = 0,
                           string_fuzzy_match_t match = string_fuzzy_match_t::exact_match());

    /// Add a list of completions.
    /// \return true on success, false if this would overflow the limit.
    __warn_unused bool add_list(completion_list_t &&lst);

    /// Swap our completions with a new list.
    void swap(completion_list_t &lst) { std::swap(completions_, lst); }

    /// Clear the list of completions. This retains the storage inside completions_ which can be
    /// useful to prevent allocations.
    void clear() { completions_.clear(); }

    /// \return whether our completion list is empty.
    bool empty() const { return completions_.empty(); }

    /// \return how many completions we have stored.
    size_t size() const { return completions_.size(); }

    /// \return a completion at an index.
    completion_t &at(size_t idx) { return completions_.at(idx); }
    const completion_t &at(size_t idx) const { return completions_.at(idx); }

    /// \return the list of completions. Do not modify the size of the list via this function, as it
    /// may exceed our completion limit.
    const completion_list_t &get_list() const { return completions_; }
    completion_list_t &get_list() { return completions_; }

    /// \return the list of completions, clearing it.
    completion_list_t take();

    /// \return a new, empty receiver whose limit is our remaining capacity.
    /// This is useful for e.g. recursive calls when you want to act on the result before adding it.
    completion_receiver_t subreceiver() const;

   private:
    // Our list of completions.
    completion_list_t completions_;

    // The maximum number of completions to add. If our list length exceeds this, then new
    // completions are not added. Note 0 has no special significance here - use
    // numeric_limits<size_t>::max() instead.
    const size_t limit_;
};

enum complete_option_type_t : uint8_t {
    option_type_args_only,    // no option
    option_type_short,        // -x
    option_type_single_long,  // -foo
    option_type_double_long   // --foo
};

/// Sorts and remove any duplicate completions in the completion list, then puts them in priority
/// order.
void completions_sort_and_prioritize(completion_list_t *comps,
                                     completion_request_options_t flags = {});

/// Add an unexpanded completion "rule" to generate completions from for a command.
///
/// Examples:
///
/// The command 'gcc -o' requires that a file follows it, so the requires_param mode is suitable.
/// This can be done using the following line:
///
/// complete -c gcc -s o -r
///
/// The command 'grep -d' required that one of the strings 'read', 'skip' or 'recurse' is used. As
/// such, it is suitable to specify that a completion requires one of them. This can be done using
/// the following line:
///
/// complete -c grep -s d -x -a "read skip recurse"
///
/// \param cmd Command to complete.
/// \param cmd_is_path If cmd_is_path is true, cmd will be interpreted as the absolute
///   path of the program (optionally containing wildcards), otherwise it
///   will be interpreted as the command name.
/// \param option The name of an option.
/// \param option_type The type of option: can be option_type_short (-x),
///        option_type_single_long (-foo), option_type_double_long (--bar).
/// \param result_mode Controls how to search further completions when this completion has been
/// successfully matched.
/// \param comp A space separated list of completions which may contain subshells.
/// \param desc A description of the completion.
/// \param condition a command to be run to check it this completion should be used. If \c condition
/// is empty, the completion is always used.
/// \param flags A set of completion flags
void complete_add(const wcstring &cmd, bool cmd_is_path, const wcstring &option,
                  complete_option_type_t option_type, completion_mode_t result_mode,
                  std::vector<wcstring> condition, const wchar_t *comp, const wchar_t *desc,
                  complete_flags_t flags);

/// Remove a previously defined completion.
void complete_remove(const wcstring &cmd, bool cmd_is_path, const wcstring &option,
                     complete_option_type_t type);

/// Removes all completions for a given command.
void complete_remove_all(const wcstring &cmd, bool cmd_is_path);

/// Load command-specific completions for the specified command.
/// \return true if something new was loaded, false if not.
bool complete_load(const wcstring &cmd, parser_t &parser);

/// \return all completions of the command cmd.
/// If \p ctx contains a parser, this will autoload functions and completions as needed.
/// If it does not contain a parser, then any completions which need autoloading will be returned in
/// \p needs_load, if not null.
completion_list_t complete(const wcstring &cmd, completion_request_options_t flags,
                           const operation_context_t &ctx,
                           std::vector<wcstring> *out_needs_load = nullptr);

/// Return a list of all current completions.
wcstring complete_print(const wcstring &cmd = L"");

/// Create a new completion entry.
///
/// \param completions The array of completions to append to
/// \param comp The completion string
/// \param desc The description of the completion
/// \param flags completion flags
void append_completion(completion_list_t *completions, wcstring comp, wcstring desc = wcstring(),
                       complete_flags_t flags = 0,
                       string_fuzzy_match_t match = string_fuzzy_match_t::exact_match());

/// Support for "wrap targets." A wrap target is a command that completes like another command.
bool complete_add_wrapper(const wcstring &command, const wcstring &new_target);
bool complete_remove_wrapper(const wcstring &command, const wcstring &target_to_remove);

/// Returns a list of wrap targets for a given command.
std::vector<wcstring> complete_get_wrap_targets(const wcstring &command);

// Observes that fish_complete_path has changed.
void complete_invalidate_path();

#endif

#endif
