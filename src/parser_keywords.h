// Functions having to do with parser keywords, like testing if a function is a block command.
#ifndef FISH_PARSER_KEYWORD_H
#define FISH_PARSER_KEYWORD_H

#include <stdbool.h>

#include "common.h"

/// Tests if the specified commands parameters should be interpreted as another command, which will
/// be true if the command is either 'command', 'exec', 'if', 'while', or 'builtin'.  This does not
/// handle "else if" which is more complicated.
///
/// \param cmd The command name to test
/// \return 1 of the command parameter is a command, 0 otherwise
bool parser_keywords_is_subcommand(const wcstring &cmd);

/// Tests if the specified command is a reserved word, i.e. if it is the name of one of the builtin
/// functions that change the block or command scope, like 'for', 'end' or 'command' or 'exec'.
/// These functions may not be overloaded, so their names are reserved.
///
/// \param word The command name to test
/// \return 1 of the command parameter is a command, 0 otherwise
bool parser_keywords_is_reserved(const wcstring &word);

/// Test if the specified string is command that opens a new block.
bool parser_keywords_is_block(const wcstring &word);

/// Check if the specified command is one of the builtins that cannot have arguments, any followin
/// argument is interpreted as a new command.
bool parser_keywords_skip_arguments(const wcstring &cmd);

#endif
