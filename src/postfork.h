// Functions that we may safely call after fork(), of which there are very few. In particular we
// cannot allocate memory, since we're insane enough to call fork from a multithreaded process.
#ifndef FISH_POSTFORK_H
#define FISH_POSTFORK_H

#include "config.h"

#include <stddef.h>
#include <unistd.h>

#include "maybe.h"
#if HAVE_SPAWN_H
#include <spawn.h>
#endif
#ifndef FISH_USE_POSIX_SPAWN
#define FISH_USE_POSIX_SPAWN HAVE_SPAWN_H
#endif

class dup2_list_t;
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
/// process. If job control is enabled for this job, the process is put in the process group of the
/// job, all signal handlers are reset, signals are unblocked (this function may only be called
/// inside the exec function, which blocks all signals), and all IO redirections and other file
/// descriptor actions are performed.
///
/// Assign the terminal to new_termowner unless it is INVALID_PID.
///
/// \return 0 on success, -1 on failure. When this function returns, signals are always unblocked.
/// On failure, signal handlers, io redirections and process group of the process is undefined.
int child_setup_process(pid_t new_termowner, pid_t fish_pgrp, const job_t &job, bool is_forked,
                        const dup2_list_t &dup2s);

/// Call fork(), retrying on failure a few times.
pid_t execute_fork();

/// Report an error from failing to exec or posix_spawn a command.
void safe_report_exec_error(int err, const char *actual_cmd, const char *const *argv,
                            const char *const *envv);

#ifdef FISH_USE_POSIX_SPAWN
/// A RAII type which wraps up posix_spawn's data structures.
class posix_spawner_t {
   public:
    /// Attempt to construct from a job and dup2 list.
    /// The caller must check the error function, as this may fail.
    posix_spawner_t(const job_t *j, const dup2_list_t &dup2s);

    /// \return the last error code, or 0 if there is no error.
    int get_error() const { return error_; }

    /// If this spawner does not have an error, invoke posix_spawn. Parameters are the same as
    /// posix_spawn.
    /// \return the pid, or none() on failure, in which case our error will be set.
    maybe_t<pid_t> spawn(const char *cmd, char *const argv[], char *const envp[]);

    ~posix_spawner_t();

    posix_spawner_t(const posix_spawner_t &) = delete;
    void operator=(const posix_spawner_t &) = delete;
    void operator=(posix_spawner_t &&) = delete;
    posix_spawner_t(posix_spawner_t &&) = delete;

   private:
    bool check_fail(int err);
    posix_spawnattr_t *attr() { return &*attr_; }
    posix_spawn_file_actions_t *actions() { return &*actions_; }

    posix_spawner_t();
    int error_{0};
    maybe_t<posix_spawnattr_t> attr_{};
    maybe_t<posix_spawn_file_actions_t> actions_{};
};

#endif

#endif
