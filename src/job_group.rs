use crate::global_safety::RelaxedAtomicBool;
use crate::proc::JobGroupRef;
use crate::signal::Signal;
use crate::wchar::prelude::*;
use std::cell::RefCell;
use std::num::NonZeroU32;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::{Arc, Mutex};

/// A job id, corresponding to what is printed by `jobs`. 1 is the first valid job id.
#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct JobId(NonZeroU32);

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct MaybeJobId(pub Option<JobId>);

impl std::ops::Deref for MaybeJobId {
    type Target = Option<JobId>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl MaybeJobId {
    pub fn as_num(&self) -> i64 {
        self.0.map(|j| i64::from(u32::from(j.0))).unwrap_or(-1)
    }
}

impl std::fmt::Display for MaybeJobId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.as_num().fmt(f)
    }
}

impl ToWString for MaybeJobId {
    fn to_wstring(&self) -> WString {
        self.as_num().to_wstring()
    }
}

impl<'a> fish_printf::ToArg<'a> for MaybeJobId {
    fn to_arg(self) -> fish_printf::Arg<'a> {
        self.as_num().to_arg()
    }
}

/// `JobGroup` is conceptually similar to the idea of a process group. It represents data which
/// is shared among all of the "subjobs" that may be spawned by a single job.
/// For example, two fish functions in a pipeline may themselves spawn multiple jobs, but all will
/// share the same job group.
/// There is also a notion of a "internal" job group. Internal groups are used when executing a
/// foreground function or block with no pipeline. These are not jobs as the user understands them -
/// they do not consume a job id, they do not show up in job lists, and they do not have a pgid
/// because they contain no external procs. Note that `JobGroup` is intended to eventually be
/// shared between threads, and so must be thread safe.
#[derive(Debug)]
pub struct JobGroup {
    /// If set, the saved terminal modes of this job. This needs to be saved so that we can restore
    /// the terminal to the same state when resuming a stopped job.
    pub tmodes: RefCell<Option<libc::termios>>,
    /// Whether job control is enabled in this `JobGroup` or not.
    ///
    /// If this is set, then the first process in the root job must be external, as it will become
    /// the process group leader.
    pub job_control: bool,
    /// Whether we should `tcsetpgrp()` the job when it runs in the foreground. Should be checked
    /// via [`Self::wants_terminal()`] only.
    wants_term: bool,
    /// Whether we are in the foreground, meaning the user is waiting for this job to complete.
    pub is_foreground: RelaxedAtomicBool,
    /// The pgid leading our group. This is only ever set if [`job_control`](Self::JobControl) is
    /// true. We ensure the value (when set) is always non-negative.
    pgid: RefCell<Option<libc::pid_t>>,
    /// The original command which produced this job tree.
    pub command: WString,
    /// Our job id, if any. `None` here should evaluate to `-1` for ffi purposes.
    /// "Simple block" groups like function calls do not have a job id.
    pub job_id: MaybeJobId,
    /// The signal causing the group to cancel or `0` if none.
    /// Not using an `Option<Signal>` to be able to atomically load/store to this field.
    signal: AtomicI32,
}

// safety: all fields without interior mutabillity are only written to once
unsafe impl Send for JobGroup {}
unsafe impl Sync for JobGroup {}

impl JobGroup {
    /// Whether this job wants job control.
    pub fn wants_job_control(&self) -> bool {
        self.job_control
    }

    /// If this job should own the terminal when it runs. True only if both [`Self::wants_term]` and
    /// [`Self::is_foreground`] are true.
    pub fn wants_terminal(&self) -> bool {
        self.wants_term && self.is_foreground()
    }

    /// Whether we are the currently the foreground group. Should never be true for more than one
    /// `JobGroup` at any given moment.
    pub fn is_foreground(&self) -> bool {
        self.is_foreground.load()
    }

    /// Mark whether we are in the foreground.
    pub fn set_is_foreground(&self, in_foreground: bool) {
        self.is_foreground.store(in_foreground);
    }

    /// Returns whether we have valid job id. "Simple block" groups like function calls do not.
    pub fn has_job_id(&self) -> bool {
        self.job_id.is_some()
    }

    /// Gets the cancellation signal, if any.
    pub fn get_cancel_signal(&self) -> Option<Signal> {
        match self.signal.load(Ordering::Relaxed) {
            0 => None,
            s => Some(Signal::new(s)),
        }
    }

    /// Mark that a process in this group got a signal and should cancel.
    pub fn cancel_with_signal(&self, signal: Signal) {
        // We only assign the signal if one hasn't yet been assigned. This means the first signal to
        // register wins over any that come later.
        self.signal
            .compare_exchange(0, signal.code(), Ordering::Relaxed, Ordering::Relaxed)
            .ok();
    }

    /// Set the pgid for this job group, latching it to this value. This should only be called if
    /// job control is active for this group. The pgid should not already have been set, and should
    /// be different from fish's pgid. Of course this does not keep the pgid alive by itself.
    ///
    /// Note we need not be concerned about thread safety. job_groups are intended to be shared
    /// across threads, but any pgid should always have been set beforehand, since it's set
    /// immediately after the first process launches.
    ///
    /// As such, this method takes `&mut self` rather than `&self` to enforce that this operation is
    /// only available during initial construction/initialization.
    pub fn set_pgid(&self, pgid: libc::pid_t) {
        assert!(
            self.wants_job_control(),
            "Should not set a pgid for a group that doesn't want job control!"
        );
        assert!(pgid >= 0, "Invalid pgid!");
        assert!(self.pgid.borrow().is_none(), "JobGroup::pgid already set!");

        self.pgid.replace(Some(pgid));
    }

    /// Returns the value of [`JobGroup::pgid`]. This is never fish's own pgid!
    pub fn get_pgid(&self) -> Option<libc::pid_t> {
        *self.pgid.borrow()
    }
}

/// Basic thread-safe sorted vector of job ids currently in use.
///
/// In the C++ codebase, this is deliberately leaked to avoid destructor ordering issues - see
/// #6539. Rust automatically "leaks" all `static` variables (does not call their `Drop` impls)
/// because of the inherent difficulty in doing that correctly (i.e. what we ran into).
static CONSUMED_JOB_IDS: Mutex<Vec<JobId>> = Mutex::new(Vec::new());

impl JobId {
    pub const NONE: MaybeJobId = MaybeJobId(None);

    pub fn new(value: NonZeroU32) -> Self {
        JobId(value)
    }

    /// Return a `JobId` that is greater than all extant job ids stored in [`CONSUMED_JOB_IDS`].
    /// The `JobId` should be freed with [`JobId::release()`] when it is no longer in use.
    fn acquire() -> JobId {
        let mut consumed_job_ids = CONSUMED_JOB_IDS.lock().expect("Poisoned mutex!");

        // The new job id should be greater than the largest currently used id (#6053). The job ids
        // in CONSUMED_JOB_IDS are sorted in ascending order, so we just have to check the last.
        let job_id = consumed_job_ids
            .last()
            .map(JobId::next)
            .unwrap_or(JobId(1.try_into().unwrap()));
        consumed_job_ids.push(job_id);
        job_id
    }

    /// Remove the provided `JobId` from [`CONSUMED_JOB_IDS`].
    fn release(id: JobId) {
        let mut consumed_job_ids = CONSUMED_JOB_IDS.lock().expect("Poisoned mutex!");

        let pos = consumed_job_ids
            .binary_search(&id)
            .expect("Job id was not in use!");
        consumed_job_ids.remove(pos);
    }

    /// Increments the internal id and returns it wrapped in a new `JobId`.
    fn next(&self) -> JobId {
        JobId(self.0.checked_add(1).expect("Job id overflow!"))
    }
}

impl JobGroup {
    pub fn new(command: WString, id: MaybeJobId, job_control: bool, wants_term: bool) -> Self {
        // We *can* have a job id without job control, but not the reverse.
        if job_control {
            assert!(id.is_some(), "Cannot have job control without a job id!");
        }
        if wants_term {
            assert!(job_control, "Cannot take terminal without job control!");
        }

        Self {
            job_id: id,
            job_control,
            wants_term,
            command,
            tmodes: RefCell::default(),
            signal: 0.into(),
            is_foreground: RelaxedAtomicBool::new(false),
            pgid: RefCell::default(),
        }
    }

    /// Return a new `JobGroup` with the provided `command`. The `JobGroup` is only assigned a
    /// `JobId` if `wants_job_id` is true and is created with job control disabled and
    /// [`JobGroup::wants_term`] set to false.
    pub fn create(command: WString, wants_job_id: bool) -> JobGroupRef {
        Arc::new(JobGroup::new(
            command,
            if wants_job_id {
                MaybeJobId(Some(JobId::acquire()))
            } else {
                JobId::NONE
            },
            false, /* job_control */
            false, /* wants_term */
        ))
    }

    /// Return a new `JobGroup` with the provided `command` with job control enabled. A [`JobId`] is
    /// automatically acquired and assigned. If `wants_term` is true then [`JobGroup::wants_term`]
    /// is also set to `true` accordingly.
    pub fn create_with_job_control(command: WString, wants_term: bool) -> JobGroupRef {
        Arc::new(JobGroup::new(
            command,
            MaybeJobId(Some(JobId::acquire())),
            true, /* job_control */
            wants_term,
        ))
    }
}

impl Drop for JobGroup {
    fn drop(&mut self) {
        if let Some(job_id) = *self.job_id {
            JobId::release(job_id);
        }
    }
}
