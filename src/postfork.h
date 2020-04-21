// Functions that we may safely call after fork(), of which there are very few. In particular we
// cannot allocate memory, since we're insane enough to call fork from a multithreaded process.
#ifndef FISH_POSTFORK_H
#define FISH_POSTFORK_H

#include "config.h"

#include <stddef.h>
#include <unistd.h>
#if HAVE_SPAWN_H
#include <spawn.h>
#endif
#ifndef FISH_USE_POSIX_SPAWN
#define FISH_USE_POSIX_SPAWN HAVE_SPAWN_H
#endif

class dup2_list_t;
class job_t;
class process_t;

bool set_child_group(job_t *j, pid_t child_pid);  // called by parent
bool child_set_group(job_t *j, process_t *p);     // called by child

/// Initialize a new child process. This should be called right away after forking in the child
/// process. If job control is enabled for this job, the process is put in the process group of the
/// job, all signal handlers are reset, signals are unblocked (this function may only be called
/// inside the exec function, which blocks all signals), and all IO redirections and other file
/// descriptor actions are performed.
///
/// Assign the terminal to new_termowner unless it is INVALID_PID.
///
/// \return 0 on success, -1 on failure. When this function returns, signals are always unblocked.
/// On failure, signal handlers, io redirections and process group of the process is undefined.
int child_setup_process(pid_t new_termowner, const job_t &job, bool is_forked,
                        const dup2_list_t &dup2s);

/// Call fork(), retrying on failure a few times.
pid_t execute_fork();

/// Report an error from failing to exec or posix_spawn a command.
void safe_report_exec_error(int err, const char *actual_cmd, const char *const *argv,
                            const char *const *envv);

#ifdef FISH_USE_POSIX_SPAWN
/// Initializes and fills in a posix_spawnattr_t; on success, the caller should destroy it via
/// posix_spawnattr_destroy.
bool fork_actions_make_spawn_properties(posix_spawnattr_t *attr,
                                        posix_spawn_file_actions_t *actions, const job_t *j,
                                        const dup2_list_t &dup2s);
#endif

#endif
