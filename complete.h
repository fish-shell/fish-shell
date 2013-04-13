/** \file complete.h
  Prototypes for functions related to tab-completion.

  These functions are used for storing and retrieving tab-completion
  data, as well as for performing tab-completion.
*/

#ifndef FISH_COMPLETE_H

/**
   Header guard
*/
#define FISH_COMPLETE_H


#include <wchar.h>

#include "util.h"
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
 * option. This is the same as (NO_FILES & NO_COMMON)
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
 * Separator between completion and description
 */
#define COMPLETE_SEP_STR L"\004"

/**
 * Separator between completion items in fish_pager. This is used for
 * completion grouping, e.g. when putting completions with the same
 * descriptions on the same line.
 */
#define COMPLETE_ITEM_SEP L'\uf500'

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

    /**
       This completion is case insensitive.

       Warning: The contents of the completion_t structure is actually
       different if this flag is set! Specifically, the completion string
       contains the _entire_ completion token, not merely its suffix.
    */
    COMPLETE_NO_CASE = 1 << 1,

    /**
       This completion may or may not want a space at the end - guess by
       checking the last character of the completion.
    */
    COMPLETE_AUTO_SPACE = 1 << 2,

    /**
       This completion should be inserted as-is, without escaping.
    */
    COMPLETE_DONT_ESCAPE = 1 << 3
};
typedef int complete_flags_t;


class completion_t
{

private:
    /* No public default constructor */
    completion_t();
public:

    /**
       The completion string
    */
    wcstring completion;

    /**
       The description for this completion
    */
    wcstring description;

    /**
       Flags determining the completion behaviour.

       Determines whether a space should be inserted after this
       completion if it is the only possible completion using the
       COMPLETE_NO_SPACE flag.

       The COMPLETE_NO_CASE can be used to signal that this completion
       is case insensitive.
    */
    int flags;

    bool is_case_insensitive() const
    {
        return !!(flags & COMPLETE_NO_CASE);
    }

    /* Construction. Note: defining these so that they are not inlined reduces the executable size. */
    completion_t(const wcstring &comp, const wcstring &desc = L"", int flags_val = 0);
    completion_t(const completion_t &);
    completion_t &operator=(const completion_t &);

    /* The following are needed for sorting and uniquing completions */
    bool operator < (const completion_t& rhs) const;
    bool operator == (const completion_t& rhs) const;
    bool operator != (const completion_t& rhs) const;
};

enum complete_type_t
{
    COMPLETE_DEFAULT,
    COMPLETE_AUTOSUGGEST
};

/** Given a list of completions, returns a list of their completion fields */
wcstring_list_t completions_to_wcstring_list(const std::vector<completion_t> &completions);

/** Sorts a list of completions */
void sort_completions(std::vector<completion_t> &completions);

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
void complete_add(const wchar_t *cmd,
                  bool cmd_is_path,
                  wchar_t short_opt,
                  const wchar_t *long_opt,
                  int long_mode,
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
void complete_remove(const wchar_t *cmd,
                     bool cmd_is_path,
                     wchar_t short_opt,
                     const wchar_t *long_opt);


/** Find all completions of the command cmd, insert them into out. If to_load is
 * not NULL, append all commands that we would autoload, but did not (presumably
 * because this is not the main thread)
 */
void complete(const wcstring &cmd,
              std::vector<completion_t> &comp,
              complete_type_t type,
              wcstring_list_t *to_load = NULL);

/**
   Print a list of all current completions into the string.

   \param out The string to write completions to
*/
void complete_print(wcstring &out);

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
   Load command-specific completions for the specified command. This
   is done automatically whenever completing any given command, so
   there is no need to call this except in the case of completions
   with internal dependencies.

   \param cmd the command for which to load command-specific completions
   \param reload should the commands completions be reloaded, even if they where
      previously loaded. (This is set to true on actual completions, so that
      changed completion are updated in running shells)
*/
void complete_load(const wcstring &cmd, bool reload);

/**
   Create a new completion entry

   \param completions The array of completions to append to
   \param comp The completion string
   \param desc The description of the completion
   \param flags completion flags

*/
void append_completion(std::vector<completion_t> &completions, const wcstring &comp, const wcstring &desc = L"", int flags = 0);


#endif
