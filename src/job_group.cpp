#include "config.h"

#include "job_group.h"

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "proc.h"

// Basic thread safe sorted vector of job IDs in use.
// This is deliberately leaked to avoid dtor ordering issues - see #6539.
static const auto locked_consumed_job_ids = new owning_lock<std::vector<job_id_t>>();

static job_id_t acquire_job_id() {
    auto consumed_job_ids = locked_consumed_job_ids->acquire();

    // The new job ID should be larger than the largest currently used ID (#6053).
    job_id_t jid = consumed_job_ids->empty() ? 1 : consumed_job_ids->back() + 1;
    consumed_job_ids->push_back(jid);
    return jid;
}

static void release_job_id(job_id_t jid) {
    assert(jid > 0);
    auto consumed_job_ids = locked_consumed_job_ids->acquire();

    // Our job ID vector is sorted, but the number of jobs is typically 1 or 2 so a binary search
    // isn't worth it.
    auto where = std::find(consumed_job_ids->begin(), consumed_job_ids->end(), jid);
    assert(where != consumed_job_ids->end() && "Job ID was not in use");
    consumed_job_ids->erase(where);
}

job_group_t::~job_group_t() {
    if (props_.job_id > 0) {
        release_job_id(props_.job_id);
    }
}

void job_group_t::set_pgid(pid_t pgid) {
    // Note we need not be concerned about thread safety. job_groups are intended to be shared
    // across threads, but their pgid should always have been set beforehand.
    assert(needs_pgid_assignment() && "We should not be setting a pgid");
    assert(pgid >= 0 && "Invalid pgid");
    pgid_ = pgid;
}

maybe_t<pid_t> job_group_t::get_pgid() const { return pgid_; }

// static
job_group_ref_t job_group_t::resolve_group_for_job(const job_t &job,
                                                   const cancellation_group_ref_t &cancel_group,
                                                   const job_group_ref_t &proposed) {
    assert(!job.group && "Job already has a group");
    assert(cancel_group && "Null cancel group");
    // Note there's three cases to consider:
    //  nullptr         -> this is a root job, there is no inherited job group
    //  internal        -> the parent is running as part of a simple function execution
    //                      We may need to create a new job group if we are going to fork.
    //  non-internal    -> we are running as part of a real pipeline
    // Decide if this job can use an internal group.
    // This is true if it's a simple foreground execution of an internal proc.
    bool initial_bg = job.is_initially_background();
    bool first_proc_internal = job.processes.front()->is_internal();
    bool can_use_internal =
        !initial_bg && job.processes.size() == 1 && job.processes.front()->is_internal();

    bool needs_new_group = false;
    if (!proposed) {
        // We don't have a group yet.
        needs_new_group = true;
    } else if (initial_bg) {
        // Background jobs always get a new group.
        needs_new_group = true;
    } else if (proposed->is_internal() && !can_use_internal) {
        // We cannot use the internal group for this job.
        needs_new_group = true;
    }

    if (!needs_new_group) return proposed;

    // We share a cancel group unless we are a background job.
    // For example, if we write "begin ; true ; sleep 1 &; end" the `begin` and `true` should cancel
    // together, but the `sleep` should not.
    cancellation_group_ref_t resolved_cg =
        initial_bg ? cancellation_group_t::create() : cancel_group;

    properties_t props{};
    props.job_control = job.wants_job_control();
    props.wants_terminal = job.wants_job_control() && !job.from_event_handler();
    props.is_internal = can_use_internal;
    props.job_id = can_use_internal ? -1 : acquire_job_id();

    job_group_ref_t result{new job_group_t(props, resolved_cg, job.command())};

    // Mark if it's foreground.
    result->set_is_foreground(!initial_bg);

    // Perhaps this job should immediately live in fish's pgroup.
    // There's two reasons why it may be so:
    //  1. The job doesn't need job control.
    //  2. The first process in the job is internal to fish; this needs to own the tty.
    if (!can_use_internal && (!props.job_control || first_proc_internal)) {
        result->set_pgid(getpgrp());
    }
    return result;
}
