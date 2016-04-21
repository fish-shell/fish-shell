/** \file complete.h
  Prototypes for functions related to tab-completion.

  These functions are used for storing and retrieving tab-completion
  data, as well as for performing tab-completion.
*/

#ifndef FISH_COMPLETE_H
#define FISH_COMPLETE_H

#include <vector>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"

/**
 * Use all completions
 */
#define SHARED 0
/**
 * Do not use file completion
 */
#define NO_FILES 1
/**
 * Require a parameter after completion
 */
#define NO_COMMON 2
/**
 * Only use the argument list specifies with completion after
 * option. This is the same as (NO_FILES | NO_COMMON)
 */
#define EXCLUSIVE 3

/**
 * Command is a path
 */
#define PATH 1
/**
 * Command is not a path
 */
#define COMMAND 0

/**
 * Separator between completion and description
 */
#define COMPLETE_SEP L'\004'

/**
 * Character that separates the completion and description on
 * programmable completions
 */
#define PROG_COMPLETE_SEP L'\t'

enum
{
    /**
       Do not insert space afterwards if this is the only completion. (The
       default is to try insert a space)
    */
    COMPLETE_NO_SPACE = 1 << 0,

    /** This is not the suffix of a token, but replaces it entirely */
    COMPLETE_REPLACES_TOKEN = 1 << 2,

    /** This completion may or may not want a space at the end - guess by
       checking the last character of the completion. */
    COMPLETE_AUTO_SPACE = 1 << 3,

    /** This completion should be inserted as-is, without escaping. */
    COMPLETE_DONT_ESCAPE = 1 << 4,

    /** If you do escape, don't escape tildes */
    COMPLETE_DONT_ESCAPE_TILDES = 1 << 5
};
typedef int complete_flags_t;


class completion_t
{

private:
    /* No public default constructor */
    completion_t();
public:

    /* Destructor. Not inlining it saves code size. */
    ~completion_t();

    /** The completion string */
    wcstring completion;

    /** The description for this completion */
    wcstring description;

    /** The type of fuzzy match */
    string_fuzzy_match_t match;

    /**
       Flags determining the completion behaviour.

       Determines whether a space should be inserted after this
       completion if it is the only possible completion using the
       COMPLETE_NO_SPACE flag.

       The COMPLETE_NO_CASE can be used to signal that this completion
       is case insensitive.
    */
    complete_flags_t flags;

    /* Construction. Note: defining these so that they are not inlined reduces the executable size. */
    explicit completion_t(const wcstring &comp, const wcstring &desc = wcstring(), string_fuzzy_match_t match = string_fuzzy_match_t(fuzzy_match_exact), complete_flags_t flags_val = 0);
    completion_t(const completion_t &);
    completion_t &operator=(const completion_t &);

    /* Compare two completions. No operating overlaoding to make this always explicit (there's potentially multiple ways to compare completions). */
    
    /* "Naturally less than" means in a natural ordering, where digits are treated as numbers. For example, foo10 is naturally greater than foo2 (but alphabetically less than it) */
    static bool is_naturally_less_than(const completion_t &a, const completion_t &b);
    static bool is_alphabetically_equal_to(const completion_t &a, const completion_t &b);
    
    /* If this completion replaces the entire token, prepend a prefix. Otherwise do nothing. */
    void prepend_token_prefix(const wcstring &prefix);
};

/** Sorts and remove any duplicate completions in the completion list, then puts them in priority order. */
void completions_sort_and_prioritize(std::vector<completion_t> *comps);

enum
{
    COMPLETION_REQUEST_DEFAULT = 0,
    COMPLETION_REQUEST_AUTOSUGGESTION = 1 << 0, // indicates the completion is for an autosuggestion
    COMPLETION_REQUEST_DESCRIPTIONS = 1 << 1, // indicates that we want descriptions
    COMPLETION_REQUEST_FUZZY_MATCH = 1 << 2 // indicates that we don't require a prefix match
};
typedef uint32_t completion_request_flags_t;

/**

  Add a completion.

  All supplied values are copied, they should be freed by or otherwise
  disposed by the caller.

  Examples:

  The command 'gcc -o' requires that a file follows it, so the
  NO_COMMON option is suitable. This can be done using the following
  line:

  complete -c gcc -s o -r

  The command 'grep -d' required that one of the strings 'read',
  'skip' or 'recurse' is used. As such, it is suitable to specify that
  a completion requires one of them. This can be done using the
  following line:

  complete -c grep -s d -x -a "read skip recurse"


  \param cmd Command to complete.
  \param cmd_type If cmd_type is PATH, cmd will be interpreted as the absolute
      path of the program (optionally containing wildcards), otherwise it
      will be interpreted as the command name.
  \param short_opt The single character name of an option. (-a is a short option,
      --all and  -funroll are long options)
  \param long_opt The multi character name of an option. (-a is a short option,
      --all and  -funroll are long options)
  \param long_mode Whether to use old style, single dash long options.
  \param result_mode Whether to search further completions when this
      completion has been succesfully matched. If result_mode is SHARED,
      any other completions may also be used. If result_mode is NO_FILES,
      file completion should not be used, but other completions may be
      used. If result_mode is NO_COMMON, on option may follow it - only a
      parameter. If result_mode is EXCLUSIVE, no option may follow it, and
      file completion is not performed.
  \param comp A space separated list of completions which may contain subshells.
  \param desc A description of the completion.
  \param condition a command to be run to check it this completion should be used.
      If \c condition is empty, the completion is always used.
  \param flags A set of completion flags
*/
enum complete_option_type_t
{
    option_type_args_only, // no option
    option_type_short, // -x
    option_type_single_long, // -foo
    option_type_double_long // --foo
};
void complete_add(const wchar_t *cmd,
                  bool cmd_is_path,
                  const wcstring &option,
                  complete_option_type_t option_type,
                  int result_mode,
                  const wchar_t *condition,
                  const wchar_t *comp,
                  const wchar_t *desc,
                  int flags);
/**
  Sets whether the completion list for this command is complete. If
  true, any options not matching one of the provided options will be
  flagged as an error by syntax highlighting.
*/
void complete_set_authoritative(const wchar_t *cmd, bool cmd_type, bool authoritative);

/**
  Remove a previously defined completion
*/
void complete_remove(const wcstring &cmd,
                     bool cmd_is_path,
                     const wcstring &option,
                     complete_option_type_t type);

/** Removes all completions for a given command */
void complete_remove_all(const wcstring &cmd, bool cmd_is_path);

/** Find all completions of the command cmd, insert them into out.
 */
class env_vars_snapshot_t;
void complete(const wcstring &cmd,
              std::vector<completion_t> *out_comps,
              completion_request_flags_t flags,
              const env_vars_snapshot_t &vars);

/**
   Return a list of all current completions.
*/
wcstring complete_print();

/**
   Tests if the specified option is defined for the specified command
*/
int complete_is_valid_option(const wcstring &str,
                             const wcstring &opt,
                             wcstring_list_t *inErrorsOrNull,
                             bool allow_autoload);

/**
   Tests if the specified argument is valid for the specified option
   and command
*/
bool complete_is_valid_argument(const wcstring &str,
                                const wcstring &opt,
                                const wcstring &arg);


/**
   Create a new completion entry

   \param completions The array of completions to append to
   \param comp The completion string
   \param desc The description of the completion
   \param flags completion flags

*/
void append_completion(std::vector<completion_t> *completions, const wcstring &comp, const wcstring &desc = wcstring(), int flags = 0, string_fuzzy_match_t match = string_fuzzy_match_t(fuzzy_match_exact));

/* Function used for testing */
void complete_set_variable_names(const wcstring_list_t *names);

/* Support for "wrap targets." A wrap target is a command that completes liek another command. The target chain is the sequence of wraps (A wraps B wraps C...). Any loops in the chain are silently ignored. */
bool complete_add_wrapper(const wcstring &command, const wcstring &wrap_target);
bool complete_remove_wrapper(const wcstring &command, const wcstring &wrap_target);
wcstring_list_t complete_get_wrap_chain(const wcstring &command);

/* Wonky interface: returns all wraps. Even-values are the commands, odd values are the targets. */
wcstring_list_t complete_get_wrap_pairs();

#endif
