#if 0
// Functions that we may safely call after fork(), of which there are very few. In particular we
// cannot allocate memory, since we're insane enough to call fork from a multithreaded process.
#ifndef FISH_POSTFORK_H
#define FISH_POSTFORK_H

#include "config.h"

#ifdef HAVE_SPAWN_H
#include <spawn.h>
#endif
#ifndef FISH_USE_POSIX_SPAWN
#define FISH_USE_POSIX_SPAWN HAVE_SPAWN_H
#endif

#include "common.h"
#include "maybe.h"

struct Dup2List;
using dup2_list_t = Dup2List;
class job_t;
class process_t;

/// Tell the proc \p pid to join process group \p pgroup.
/// If \p is_child is true, we are the child process; otherwise we are fish.
/// Called by both parent and child; this is an unavoidable race inherent to Unix.
/// If is_parent is set, then we are the parent process and should swallow EACCESS.
/// \return 0 on success, an errno error code on failure.
int execute_setpgid(pid_t pid, pid_t pgroup, bool is_parent);

/// Report the error code \p err for a failed setpgid call.
/// Note not all errors should be reported; in particular EACCESS is expected and benign in the
/// parent only.
void report_setpgid_error(int err, bool is_parent, pid_t desired_pgid, const job_t *j,
                          const process_t *p);

/// Initialize a new child process. This should be called right away after forking in the child
/// process. This resets signal handlers and applies IO redirections.
///
/// If \p claim_tty_from is >= 0 and owns the tty, use tcsetpgrp() to claim it.
///
/// \return 0 on success, -1 on failure, in which case an error will be printed.
int child_setup_process(pid_t claim_tty_from, const job_t &job, bool is_forked,
                        const dup2_list_t &dup2s);

/// Call fork(), retrying on failure a few times.
pid_t execute_fork();

/// Report an error from failing to exec or posix_spawn a command.
void safe_report_exec_error(int err, const char *actual_cmd, const char *const *argv,
                            const char *const *envv);

#endif
#endif
