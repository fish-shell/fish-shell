#ifndef FISH_JOB_GROUP_H
#define FISH_JOB_GROUP_H
#include "config.h"  // IWYU pragma: keep

#include <termios.h>


#include <memory>

#include "common.h"
#include "global_safety.h"
#include "maybe.h"

/// A job ID, corresponding to what is printed in 'jobs'.
/// 1 is the first valid job ID.
using job_id_t = int;

/// job_group_t is conceptually similar to the idea of a process group. It represents data which
/// is shared among all of the "subjobs" that may be spawned by a single job.
/// For example, two fish functions in a pipeline may themselves spawn multiple jobs, but all will
/// share the same job group.
/// There is also a notion of a "internal" job group. Internal groups are used when executing a
/// foreground function or block with no pipeline. These are not jobs as the user understands them -
/// they do not consume a job ID, they do not show up in job lists, and they do not have a pgid
/// because they contain no external procs. Note that job_group_t is intended to eventually be
/// shared between threads, and so must be thread safe.
class job_group_t;
using job_group_ref_t = std::shared_ptr<job_group_t>;

class job_group_t {
   public:
    /// \return whether this group wants job control.
    bool wants_job_control() const { return job_control_; }

    /// \return if this job group should own the terminal when it runs.
    bool wants_terminal() const { return wants_terminal_ && is_foreground(); }

    /// \return whether we are currently the foreground group.
    bool is_foreground() const { return is_foreground_; }

    /// Mark whether we are in the foreground.
    void set_is_foreground(bool flag) { is_foreground_ = flag; }

    /// \return the command which produced this job tree.
    const wcstring &get_command() const { return command_; }

    /// \return the job ID, or -1 if none.
    job_id_t get_job_id() const { return job_id_; }

    /// \return whether we have a valid job ID. "Simple block" groups like function calls do not.
    bool has_job_id() const { return job_id_ > 0; }

    /// Get the cancel signal, or 0 if none.
    int get_cancel_signal() const { return signal_; }

    /// Mark that a process in this group got a signal, and so should cancel.
    void cancel_with_signal(int signal) {
        assert(signal > 0 && "Invalid cancel signal");
        signal_.compare_exchange(0, signal);
    }

    /// If set, the saved terminal modes of this job. This needs to be saved so that we can restore
    /// the terminal to the same state when resuming a stopped job.
    maybe_t<struct termios> tmodes{};

    /// Set the pgid for this job group, latching it to this value.
    /// This should only be called if job control is active for this group.
    /// The pgid should not already have been set, and should be different from fish's pgid.
    /// Of course this does not keep the pgid alive by itself.
    void set_pgid(pid_t pgid);

    /// Get the pgid. This never returns fish's pgid.
    maybe_t<pid_t> get_pgid() const { return pgid_; }

    /// Construct a group for a job that will live internal to fish, optionally claiming a job ID.
    static job_group_ref_t create(wcstring command, bool wants_job_id);

    /// Construct a group for a job which will assign its first process as pgroup leader.
    static job_group_ref_t create_with_job_control(wcstring command, bool wants_terminal);

    ~job_group_t();

   private:
    job_group_t(wcstring command, job_id_t job_id, bool job_control = false,
                bool wants_terminal = false);

    // Whether job control is enabled.
    // If this is set, then the first process in the root job must be external.
    // It will become the process group leader.
    const bool job_control_;

    // Whether we should tcsetpgrp to the job when it runs in the foreground.
    const bool wants_terminal_;

    // Whether we are in the foreground, meaning that the user is waiting for this.
    relaxed_atomic_bool_t is_foreground_{};

    // The pgid leading our group. This is only ever set if job_control_ is true.
    // This is never fish's pgid.
    maybe_t<pid_t> pgid_{};

    // The original command which produced this job tree.
    const wcstring command_;

    /// Our job ID. -1 if none.
    const job_id_t job_id_;

    /// The signal causing us the group to cancel, or 0.
    relaxed_atomic_t<int> signal_{0};
};

#endif
