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
bool maybe_assign_terminal(const job_t *j);

/// Initialize a new child process. This should be called right away after forking in the child
/// process. If job control is enabled for this job, the process is put in the process group of the
/// job, all signal handlers are reset, signals are unblocked (this function may only be called
/// inside the exec function, which blocks all signals), and all IO redirections and other file
/// descriptor actions are performed.
///
/// \param p the child process to set up
/// \param dup2 the dup2 list to apply
///
/// \return 0 on sucess, -1 on failiure. When this function returns, signals are always unblocked.
/// On failiure, signal handlers, io redirections and process group of the process is undefined.
int setup_child_process(process_t *p, const dup2_list_t &dup2s);

/// Call fork(), optionally waiting until we are no longer multithreaded. If the forked child
/// doesn't do anything that could allocate memory, take a lock, etc. (like call exec), then it's
/// not necessary to wait for threads to die. If the forked child may do those things, it should
/// wait for threads to die.
pid_t execute_fork(bool wait_for_threads_to_die);

/// Perform output from builtins. Returns true on success.
bool do_builtin_io(const char *out, size_t outlen, const char *err, size_t errlen);

/// Report an error from failing to exec or posix_spawn a command.
void safe_report_exec_error(int err, const char *actual_cmd, const char *const *argv,
                            const char *const *envv);

/// Runs the process as a keepalive, until the parent process given by parent_pid exits.
void run_as_keepalive(pid_t parent_pid);

#if FISH_USE_POSIX_SPAWN
/// Initializes and fills in a posix_spawnattr_t; on success, the caller should destroy it via
/// posix_spawnattr_destroy.
bool fork_actions_make_spawn_properties(posix_spawnattr_t *attr,
                                        posix_spawn_file_actions_t *actions, const job_t *j,
                                        const dup2_list_t &dup2s);
#endif

#endif
