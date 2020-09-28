/// Prototypes for functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
#ifndef FISH_COMPLETE_H
#define FISH_COMPLETE_H

#include <stdint.h>

#include <functional>
#include <vector>

#include "common.h"
#include "enum_set.h"

struct completion_mode_t {
    /// If set, skip file completions.
    bool no_files{false};
    bool force_files{false};

    /// If set, require a parameter after completion.
    bool requires_param{false};
};

/// Character that separates the completion and description on programmable completions.
#define PROG_COMPLETE_SEP L'\t'

class environment_t;

enum {
    /// Do not insert space afterwards if this is the only completion. (The default is to try insert
    /// a space).
    COMPLETE_NO_SPACE = 1 << 0,
    /// This is not the suffix of a token, but replaces it entirely.
    COMPLETE_REPLACES_TOKEN = 1 << 2,
    /// This completion may or may not want a space at the end - guess by checking the last
    /// character of the completion.
    COMPLETE_AUTO_SPACE = 1 << 3,
    /// This completion should be inserted as-is, without escaping.
    COMPLETE_DONT_ESCAPE = 1 << 4,
    /// If you do escape, don't escape tildes.
    COMPLETE_DONT_ESCAPE_TILDES = 1 << 5,
    /// Do not sort supplied completions
    COMPLETE_DONT_SORT = 1 << 6,
    /// This completion looks to have the same string as an existing argument.
    COMPLETE_DUPLICATES_ARGUMENT = 1 << 7
};
typedef int complete_flags_t;

/// std::function which accepts a completion string and returns its description.
using description_func_t = std::function<wcstring(const wcstring &)>;

/// Helper to return a description_func_t for a constant string.
description_func_t const_desc(const wcstring &s);

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
    ///
    /// Determines whether a space should be inserted after this completion if it is the only
    /// possible completion using the COMPLETE_NO_SPACE flag. The COMPLETE_NO_CASE can be used to
    /// signal that this completion is case insensitive.
    complete_flags_t flags;

    // Construction.
    explicit completion_t(wcstring comp, wcstring desc = wcstring(),
                          string_fuzzy_match_t match = string_fuzzy_match_t(fuzzy_match_exact),
                          complete_flags_t flags_val = 0);
    completion_t(const completion_t &);
    completion_t &operator=(const completion_t &);

    // noexcepts are required for push_back to use the move ctor.
    completion_t(completion_t &&) noexcept;
    completion_t &operator=(completion_t &&) noexcept;

    // Compare two completions. No operating overlaoding to make this always explicit (there's
    // potentially multiple ways to compare completions).
    //
    // "Naturally less than" means in a natural ordering, where digits are treated as numbers. For
    // example, foo10 is naturally greater than foo2 (but alphabetically less than it).
    static bool is_naturally_less_than(const completion_t &a, const completion_t &b);

    // Deduplicate a potentially-unsorted vector, preserving the order
    template <class Iterator, class HashFunction>
    static Iterator unique_unsorted(Iterator begin, Iterator end, HashFunction hash);

    // If this completion replaces the entire token, prepend a prefix. Otherwise do nothing.
    void prepend_token_prefix(const wcstring &prefix);
};

using completion_list_t = std::vector<completion_t>;

enum class completion_request_t {
    autosuggestion,  // indicates the completion is for an autosuggestion
    descriptions,    // indicates that we want descriptions
    fuzzy_match,     // indicates that we don't require a prefix match
    COUNT
};

template <>
struct enum_info_t<completion_request_t> {
    static constexpr auto count = completion_request_t::COUNT;
};

using completion_request_flags_t = enum_set_t<completion_request_t>;

class completion_t;
using completion_list_t = std::vector<completion_t>;

enum complete_option_type_t {
    option_type_args_only,    // no option
    option_type_short,        // -x
    option_type_single_long,  // -foo
    option_type_double_long   // --foo
};

/// Sorts and remove any duplicate completions in the completion list, then puts them in priority
/// order.
void completions_sort_and_prioritize(completion_list_t *comps,
                                     completion_request_flags_t flags = {});

/// Add a completion.
///
/// All supplied values are copied, they should be freed by or otherwise disposed by the caller.
///
/// Examples:
///
/// The command 'gcc -o' requires that a file follows it, so the NO_COMMON option is suitable. This
/// can be done using the following line:
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
void complete_add(const wchar_t *cmd, bool cmd_is_path, const wcstring &option,
                  complete_option_type_t option_type, completion_mode_t result_mode,
                  const wchar_t *condition, const wchar_t *comp, const wchar_t *desc, int flags);

/// Remove a previously defined completion.
void complete_remove(const wcstring &cmd, bool cmd_is_path, const wcstring &option,
                     complete_option_type_t type);

/// Removes all completions for a given command.
void complete_remove_all(const wcstring &cmd, bool cmd_is_path);

/// \return all completions of the command cmd.
class operation_context_t;
completion_list_t complete(const wcstring &cmd, completion_request_flags_t flags,
                           const operation_context_t &ctx);

/// Return a list of all current completions.
wcstring complete_print(const wcstring &cmd = L"");

/// Tests if the specified option is defined for the specified command.
int complete_is_valid_option(const wcstring &str, const wcstring &opt,
                             wcstring_list_t *inErrorsOrNull, bool allow_autoload);

/// Tests if the specified argument is valid for the specified option and command.
bool complete_is_valid_argument(const wcstring &str, const wcstring &opt, const wcstring &arg);

/// Create a new completion entry.
///
/// \param completions The array of completions to append to
/// \param comp The completion string
/// \param desc The description of the completion
/// \param flags completion flags
void append_completion(completion_list_t *completions, wcstring comp, wcstring desc = wcstring(),
                       int flags = 0,
                       string_fuzzy_match_t match = string_fuzzy_match_t(fuzzy_match_exact));

/// Support for "wrap targets." A wrap target is a command that completes like another command.
bool complete_add_wrapper(const wcstring &command, const wcstring &new_target);
bool complete_remove_wrapper(const wcstring &command, const wcstring &target_to_remove);

/// Returns a list of wrap targets for a given command.
wcstring_list_t complete_get_wrap_targets(const wcstring &command);

// Observes that fish_complete_path has changed.
void complete_invalidate_path();

#endif
