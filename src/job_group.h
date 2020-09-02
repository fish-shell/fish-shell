#ifndef FISH_JOB_GROUP_H
#define FISH_JOB_GROUP_H
#include "config.h"  // IWYU pragma: keep

#include <termios.h>

#include <memory>

#include "common.h"
#include "global_safety.h"

/// A job ID, corresponding to what is printed in 'jobs'.
/// 1 is the first valid job ID.
using job_id_t = int;

/// A cancellation group is "a set of jobs that should cancel together." It's effectively just a
/// shared pointer to a bool which latches to true on cancel.
/// For example, in `begin ; true ; end | false`, we have two jobs: the outer pipline and the inner
/// 'true'. These share a cancellation group.
/// Note this is almost but not quite a job group. A job group is a "a set of jobs which share a
/// pgid" but cancellation groups may be bigger. For example in `begin ; sleep 1; sleep 2; end` we
/// have that 'begin' is an internal group (a simple function/block execution) without a pgid,
/// while each 'sleep' will be a different job, with its own pgid, and so be in a different job
/// group. But all share a cancellation group.
/// Note that a background job will always get a new cancellation group.
/// Cancellation groups must be thread safe.
class cancellation_group_t {
   public:
    /// \return true if we should cancel.
    bool should_cancel() const { return get_cancel_signal() != 0; }

    /// \return the signal indicating cancellation, or 0 if none.
    int get_cancel_signal() const { return signal_; }

    /// If we have not already cancelled, then trigger cancellation with the given signal.
    void cancel_with_signal(int signal) {
        assert(signal > 0 && "Invalid cancel signal");
        signal_.compare_exchange(0, signal);
    }

    /// Helper to return a new group.
    static std::shared_ptr<cancellation_group_t> create() {
        return std::make_shared<cancellation_group_t>();
    }

   private:
    /// If we cancelled from a signal, return that signal, else 0.
    relaxed_atomic_t<int> signal_{0};
};
using cancellation_group_ref_t = std::shared_ptr<cancellation_group_t>;

/// job_group_t is conceptually similar to the idea of a process group. It represents data which
/// is shared among all of the "subjobs" that may be spawned by a single job.
/// For example, two fish functions in a pipeline may themselves spawn multiple jobs, but all will
/// share the same job group.
/// There is also a notion of a "internal" job group. Internal groups are used when executing a
/// foreground function or block with no pipeline. These are not jobs as the user understands them -
/// they do not consume a job ID, they do not show up in job lists, and they do not have a pgid
/// because they contain no external procs. Note that job_group_t is intended to eventually be
/// shared between threads, and so must be thread safe.
class job_t;
class job_group_t;
using job_group_ref_t = std::shared_ptr<job_group_t>;

class job_group_t {
   public:
    /// Set the pgid for this job group, latching it to this value.
    /// The pgid should not already have been set.
    /// Of course this does not keep the pgid alive by itself.
    /// An internal job group does not have a pgid and it is an error to set it.
    void set_pgid(pid_t pgid);

    /// Get the pgid, or none() if it has not been set.
    maybe_t<pid_t> get_pgid() const;

    /// \return whether we want job control
    bool wants_job_control() const { return props_.job_control; }

    /// \return whether this is an internal group.
    bool is_internal() const { return props_.is_internal; }

    /// \return whether we are currently the foreground group.
    bool is_foreground() const { return is_foreground_; }

    /// Mark whether we are in the foreground.
    void set_is_foreground(bool flag) { is_foreground_ = flag; }

    /// \return if this job group should own the terminal when it runs.
    bool should_claim_terminal() const { return props_.wants_terminal && is_foreground(); }

    /// \return whether this job group is awaiting a pgid.
    /// This is true for non-internal trees that don't already have a pgid.
    bool needs_pgid_assignment() const { return !props_.is_internal && !pgid_.has_value(); }

    /// \return the job ID, or -1 if none.
    job_id_t get_id() const { return props_.job_id; }

    /// Get the cancel signal, or 0 if none.
    int get_cancel_signal() const { return cancel_group->get_cancel_signal(); }

    /// \return the command which produced this job tree.
    const wcstring &get_command() const { return command_; }

    /// Mark that a process in this group got a signal, and so should cancel.
    void cancel_with_signal(int sig) { cancel_group->cancel_with_signal(sig); }

    /// Mark the root as constructed.
    /// This is used to avoid reaping a process group leader while there are still procs that may
    /// want to enter its group.
    void mark_root_constructed() { root_constructed_ = true; };
    bool is_root_constructed() const { return root_constructed_; }

    /// Given a job and a proposed job group (possibly null), return a group for the job.
    /// The proposed group is the group from the parent job, or null if this is a root.
    /// This never returns null.
    static job_group_ref_t resolve_group_for_job(const job_t &job,
                                                 const cancellation_group_ref_t &cancel_group,
                                                 const job_group_ref_t &proposed_group);

    ~job_group_t();

    /// If set, the saved terminal modes of this job. This needs to be saved so that we can restore
    /// the terminal to the same state after temporarily taking control over the terminal when a job
    /// stops.
    maybe_t<struct termios> tmodes{};

    /// The cancellation group. This is never null.
    const cancellation_group_ref_t cancel_group{};

   private:
    // The pgid to assign to jobs, or none if not yet set.
    maybe_t<pid_t> pgid_{};

    // Set of properties, which are constant.
    struct properties_t {
        // Whether jobs in this group should have job control.
        bool job_control{};

        // Whether we should claim the terminal when we run in the foreground.
        // TODO: this is effectively the same as job control, rationalize this.
        bool wants_terminal{};

        // Whether we are an internal job group.
        bool is_internal{};

        // The job ID of this group.
        job_id_t job_id{};
    };
    const properties_t props_;

    // The original command which produced this job tree.
    const wcstring command_;

    // Whether we are in the foreground, meaning that the user is waiting for this.
    relaxed_atomic_bool_t is_foreground_{};

    // Whether the root job is constructed. If not, we cannot reap it yet.
    relaxed_atomic_bool_t root_constructed_{};

    job_group_t(const properties_t &props, cancellation_group_ref_t cg, wcstring command)
        : cancel_group(std::move(cg)), props_(props), command_(std::move(command)) {
        assert(cancel_group && "Null cancel group");
    }
};

#endif
