// Prototypes for functions for executing a program.
#ifndef FISH_EXEC_H
#define FISH_EXEC_H

#include <stddef.h>

#include <vector>

#include "common.h"

/// Pipe redirection error message.
#define PIPE_ERROR _(L"An error occurred while setting up pipe")

/// Execute the processes specified by \p j in the parser \p.
class job_t;
struct job_lineage_t;
class parser_t;
bool exec_job(parser_t &parser, const std::shared_ptr<job_t> &j, const job_lineage_t &lineage);

/// Evaluate a command.
///
/// \param cmd the command to execute
/// \param parser the parser with which to execute code
/// \param outputs the list to insert output into.
/// \param apply_exit_status if set, update $status within the parser, otherwise do not.
///
/// \return a value appropriate for populating $status.
int exec_subshell(const wcstring &cmd, parser_t &parser, bool apply_exit_status);
int exec_subshell(const wcstring &cmd, parser_t &parser, wcstring_list_t &outputs,
                  bool apply_exit_status);

/// Like exec_subshell, but only returns expansion-breaking errors. That is, a zero return means
/// "success" (even though the command may have failed), a non-zero return means that we should
/// halt expansion.
int exec_subshell_for_expand(const wcstring &cmd, parser_t &parser, wcstring_list_t &outputs);

/// Loops over close until the syscall was run without being interrupted.
void exec_close(int fd);

/// Gets the interpreter for a given command.
char *get_interpreter(const char *command, char *interpreter, size_t buff_size);

#endif
