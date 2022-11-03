// SPDX-FileCopyrightText: © 2020 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

#include "config.h"  // IWYU pragma: keep

#include "job_group.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
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

job_group_t::job_group_t(wcstring command, job_id_t job_id, bool job_control, bool wants_terminal)
    : job_control_(job_control),
      wants_terminal_(wants_terminal),
      command_(std::move(command)),
      job_id_(job_id) {}

job_group_t::~job_group_t() {
    if (job_id_ > 0) {
        release_job_id(job_id_);
    }
}

// static
job_group_ref_t job_group_t::create(wcstring command, bool wants_job_id) {
    job_id_t jid = wants_job_id ? acquire_job_id() : 0;
    return job_group_ref_t(new job_group_t(std::move(command), jid));
}

// static
job_group_ref_t job_group_t::create_with_job_control(wcstring command, bool wants_terminal) {
    return job_group_ref_t(new job_group_t(std::move(command), acquire_job_id(),
                                           true /* job_control */, wants_terminal));
}

void job_group_t::set_pgid(pid_t pgid) {
    // Note we need not be concerned about thread safety. job_groups are intended to be shared
    // across threads, but any pgid should always have been set beforehand, since it's set
    // immediately after the first process launches.
    assert(pgid >= 0 && "invalid pgid");
    assert(wants_job_control() && "should not set a pgid for this group");
    assert(!pgid_.has_value() && "pgid already set");
    pgid_ = pgid;
}
