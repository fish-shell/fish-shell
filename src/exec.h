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
class parser_t;
bool exec_job(parser_t &parser, std::shared_ptr<job_t> j);

/// Evaluate the expression cmd in a subshell, add the outputs into the list l. On return, the
/// status flag as returned bu \c proc_gfet_last_status will not be changed.
///
/// \param cmd the command to execute
/// \param outputs The list to insert output into.
///
/// \return the status of the last job to exit, or -1 if en error was encountered.
int exec_subshell(const wcstring &cmd, parser_t &parser, std::vector<wcstring> &outputs,
                  bool preserve_exit_status, bool is_subcmd = false);
int exec_subshell(const wcstring &cmd, parser_t &parser, bool preserve_exit_status,
                  bool is_subcmd = false);

/// Loops over close until the syscall was run without being interrupted.
void exec_close(int fd);

/// Call pipe(), and add resulting fds to open_fds, the list of opened file descriptors for pipes.
/// The pipes are marked CLO_EXEC.
int exec_pipe(int fd[2]);

/// Gets the interpreter for a given command.
char *get_interpreter(const char *command, char *interpreter, size_t buff_size);

#endif
