// Prototypes for functions for executing a program.
#ifndef FISH_EXEC_H
#define FISH_EXEC_H

#include <stddef.h>

#include <vector>

#include "common.h"
#include "proc.h"

/// Pipe redirection error message.
#define PIPE_ERROR _(L"An error occurred while setting up pipe")

/// Execute the processes specified by \p j in the parser \p.
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
/// halt expansion. If the \p pgid is supplied, then any spawned external commands should join that
/// pgroup.
int exec_subshell_for_expand(const wcstring &cmd, parser_t &parser, maybe_t<pid_t> parent_pgid,
                             wcstring_list_t &outputs);

/// Loops over close until the syscall was run without being interrupted.
void exec_close(int fd);

/// Compute the "pgroup provenance" for a job. This is a description of how the pgroup is
/// assigned. It's factored out because the logic has subtleties, and this centralizes it.
pgroup_provenance_t get_pgroup_provenance(const std::shared_ptr<job_t> &j,
                                          const job_lineage_t &lineage);

/// Add signals that should be masked for external processes in this job.
bool blocked_signals_for_job(const job_t &job, sigset_t *sigmask);

#endif
