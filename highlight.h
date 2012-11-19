/** \file highlight.h
  Prototypes for functions for syntax highlighting
*/

#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <wchar.h>

#include "env.h"
#include "util.h"
#include "screen.h"
#include "color.h"

/**
   Internal value representing highlighting of normal text
*/
#define HIGHLIGHT_NORMAL 0x1
/**
   Internal value representing highlighting of an error
*/
#define HIGHLIGHT_ERROR 0x2
/**
   Internal value representing highlighting of a command
*/
#define HIGHLIGHT_COMMAND 0x4
/**
   Internal value representing highlighting of a process separator
*/
#define HIGHLIGHT_END 0x8
/**
   Internal value representing highlighting of a regular command parameter
*/
#define HIGHLIGHT_PARAM 0x10
/**
   Internal value representing highlighting of a comment
*/
#define HIGHLIGHT_COMMENT 0x20
/**
   Internal value representing highlighting of a matching parenteses, etc.
*/
#define HIGHLIGHT_MATCH 0x40
/**
   Internal value representing highlighting of a search match
*/
#define HIGHLIGHT_SEARCH_MATCH 0x80
/**
   Internal value representing highlighting of an operator
*/
#define HIGHLIGHT_OPERATOR 0x100
/**
   Internal value representing highlighting of an escape sequence
*/
#define HIGHLIGHT_ESCAPE 0x200
/**
   Internal value representing highlighting of a quoted string
*/
#define HIGHLIGHT_QUOTE 0x400
/**
   Internal value representing highlighting of an IO redirection
*/
#define HIGHLIGHT_REDIRECTION 0x800
/**
   Internal value representing highlighting a potentially valid path
*/
#define HIGHLIGHT_VALID_PATH 0x1000

/**
   Internal value representing highlighting an autosuggestion
*/
#define HIGHLIGHT_AUTOSUGGESTION 0x2000

class history_item_t;
struct file_detection_context_t;

/**
   Perform syntax highlighting for the shell commands in buff. The result is
   stored in the color array as a color_code from the HIGHLIGHT_ enum
   for each character in buff.

   \param buff The buffer on which to perform syntax highlighting
   \param color The array in wchich to store the color codes. The first 8 bits are used for fg color, the next 8 bits for bg color.
   \param pos the cursor position. Used for quote matching, etc.
   \param error a list in which a description of each error will be inserted. May be 0, in whcich case no error descriptions will be generated.
*/
void highlight_shell(const wcstring &buffstr, std::vector<int> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars);

/**
   Perform syntax highlighting for the text in buff. Matching quotes and paranthesis are highlighted. The result is
   stored in the color array as a color_code from the HIGHLIGHT_ enum
   for each character in buff.

   \param buff The buffer on which to perform syntax highlighting
   \param color The array in wchich to store the color codes. The first 8 bits are used for fg color, the next 8 bits for bg color.
   \param pos the cursor position. Used for quote matching, etc.
   \param error a list in which a description of each error will be inserted. May be 0, in whcich case no error descriptions will be generated.
*/
void highlight_universal(const wcstring &buffstr, std::vector<int> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars);

/**
   Translate from HIGHLIGHT_* to FISH_COLOR_* according to environment
   variables. Defaults to FISH_COLOR_NORMAL.

   Example:

   If the environment variable FISH_FISH_COLOR_ERROR is set to 'red', a
   call to highlight_get_color( HIGHLIGHT_ERROR) will return
   FISH_COLOR_RED.
*/
rgb_color_t highlight_get_color(int highlight, bool is_background);

/** Given a command 'str' from the history, try to determine whether we ought to suggest it by specially recognizing the command.
    Returns true if we validated the command. If so, returns by reference whether the suggestion is valid or not.
*/
bool autosuggest_validate_from_history(const history_item_t &item, file_detection_context_t &detector, const wcstring &working_directory, const env_vars_snapshot_t &vars);

/** Given the command line contents 'str', return via reference a suggestion by specially recognizing the command. The suggestion is escaped. Returns true if we recognized the command (even if we couldn't think of a suggestion for it).
*/
bool autosuggest_suggest_special(const wcstring &str, const wcstring &working_directory, wcstring &outString);

/* Tests whether the specified string cpath is the prefix of anything we could cd to. directories is a list of possible parent directories (typically either the working directory, or the cdpath). This does I/O!

    This is used only internally to this file, and is exposed only for testing.
*/
enum
{
    /* The path must be to a directory */
    PATH_REQUIRE_DIR = 1 << 0,

    /* Expand any leading tilde in the path */
    PATH_EXPAND_TILDE = 1 << 1
};
typedef unsigned int path_flags_t;
bool is_potential_path(const wcstring &const_path, const wcstring_list_t &directories, path_flags_t flags, wcstring *out_path = NULL);

#endif

