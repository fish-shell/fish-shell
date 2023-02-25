use self::job_group::pgid_t;
use crate::common::{assert_send, assert_sync};
use crate::wchar_ffi::{WCharFromFFI, WCharToFFI};
use cxx::{CxxWString, UniquePtr};
use std::num::{NonZeroI32, NonZeroU32};
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};
use std::sync::Mutex;
use widestring::WideUtfString;

#[cxx::bridge]
mod job_group {
    // Not only does cxx bridge not recognize libc::pid_t, it doesn't even recognize i32 as a POD
    // type! :sadface:
    struct pgid_t {
        value: i32,
    }

    extern "Rust" {
        #[cxx_name = "job_group_t"]
        type JobGroup;

        fn wants_job_control(&self) -> bool;
        fn wants_terminal(&self) -> bool;
        fn is_foreground(&self) -> bool;
        fn set_is_foreground(&self, value: bool);
        #[cxx_name = "get_command"]
        fn get_command_ffi(&self) -> UniquePtr<CxxWString>;
        #[cxx_name = "get_job_id"]
        fn get_job_id_ffi(&self) -> i32;
        #[cxx_name = "get_cancel_signal"]
        fn get_cancel_signal_ffi(&self) -> i32;
        #[cxx_name = "cancel_with_signal"]
        fn cancel_with_signal_ffi(&self, signal: i32);
        fn set_pgid(&mut self, pgid: i32);
        #[cxx_name = "get_pgid"]
        fn get_pgid_ffi(&self) -> UniquePtr<pgid_t>;
        fn has_job_id(&self) -> bool;

        // cxx bridge doesn't recognize `libc::*` as being POD types, so it won't let us use them in
        // a SharedPtr/UniquePtr/Box and won't let us pass/return them by value/reference, either.
        unsafe fn get_modes_ffi(&self, size: usize) -> *const u8; /* actually `* const libc::termios` */
        unsafe fn set_modes_ffi(&mut self, modes: *const u8, size: usize); /* actually `* const libc::termios` */

        // The C++ code uses `shared_ptr<JobGroup>` but cxx bridge doesn't support returning a
        // `SharedPtr<OpaqueRustType>` nor does it implement `Arc<T>` so we return a box and then
        // convert `rust::box<T>` to `std::shared_ptr<T>` with `box_to_shared_ptr()` (from ffi.h).
        fn create_job_group_ffi(command: &CxxWString, wants_job_id: bool) -> Box<JobGroup>;
        fn create_job_group_with_job_control_ffi(
            command: &CxxWString,
            wants_term: bool,
        ) -> Box<JobGroup>;
    }
}

fn create_job_group_ffi(command: &CxxWString, wants_job_id: bool) -> Box<JobGroup> {
    let job_group = JobGroup::create(command.from_ffi(), wants_job_id);
    Box::new(job_group)
}

fn create_job_group_with_job_control_ffi(command: &CxxWString, wants_term: bool) -> Box<JobGroup> {
    let job_group = JobGroup::create_with_job_control(command.from_ffi(), wants_term);
    Box::new(job_group)
}

/// A job id, corresponding to what is printed by `jobs`. 1 is the first valid job id.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct JobId(NonZeroU32);

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
    pub tmodes: Option<libc::termios>,
    /// Whether job control is enabled in this `JobGroup` or not.
    ///
    /// If this is set, then the first process in the root job must be external, as it will become
    /// the process group leader.
    pub job_control: bool,
    /// Whether we should `tcsetpgrp()` the job when it runs in the foreground. Should be checked
    /// via [`Self::wants_terminal()`] only.
    wants_term: bool,
    /// Whether we are in the foreground, meaning the user is waiting for this job to complete.
    pub is_foreground: AtomicBool,
    /// The pgid leading our group. This is only ever set if [`job_control`](Self::JobControl) is
    /// true. We ensure the value (when set) is always non-negative.
    pgid: Option<libc::pid_t>,
    /// The original command which produced this job tree.
    pub command: WideUtfString,
    /// Our job id, if any. `None` here should evaluate to `-1` for ffi purposes.
    /// "Simple block" groups like function calls do not have a job id.
    pub job_id: Option<JobId>,
    /// The signal causing the group to cancel or `0` if none.
    /// Not using an `Option<NonZeroI32>` to be able to atomically load/store to this field.
    signal: AtomicI32,
}

const _: () = assert_send::<JobGroup>();
const _: () = assert_sync::<JobGroup>();

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
        self.is_foreground.load(Ordering::Relaxed)
    }

    /// Mark whether we are in the foreground.
    pub fn set_is_foreground(&self, in_foreground: bool) {
        self.is_foreground.store(in_foreground, Ordering::Relaxed);
    }

    /// Return the command which produced this job tree.
    pub fn get_command_ffi(&self) -> UniquePtr<CxxWString> {
        self.command.to_ffi()
    }

    /// Return the job id or -1 if none.
    pub fn get_job_id_ffi(&self) -> i32 {
        self.job_id.map(|j| u32::from(j.0) as i32).unwrap_or(-1)
    }

    /// Returns whether we have valid job id. "Simple block" groups like function calls do not.
    pub fn has_job_id(&self) -> bool {
        self.job_id.is_some()
    }

    /// Gets the cancellation signal, if any.
    pub fn get_cancel_signal(&self) -> Option<NonZeroI32> {
        match self.signal.load(Ordering::Relaxed) {
            0 => None,
            s => Some(NonZeroI32::new(s).unwrap()),
        }
    }

    /// Gets the cancellation signal or `0` if none.
    pub fn get_cancel_signal_ffi(&self) -> i32 {
        // Legacy C++ code expects a zero in case of no signal.
        self.get_cancel_signal().map(|s| s.into()).unwrap_or(0)
    }

    /// Mark that a process in this group got a signal and should cancel.
    pub fn cancel_with_signal(&self, signal: NonZeroI32) {
        // We only assign the signal if one hasn't yet been assigned. This means the first signal to
        // register wins over any that come later.
        self.signal
            .compare_exchange(0, signal.into(), Ordering::Relaxed, Ordering::Relaxed)
            .ok();
    }

    /// Mark that a process in this group got a signal and should cancel
    pub fn cancel_with_signal_ffi(&self, signal: i32) {
        self.cancel_with_signal(signal.try_into().expect("Invalid zero signal!"));
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
    pub fn set_pgid(&mut self, pgid: libc::pid_t) {
        assert!(
            self.wants_job_control(),
            "Should not set a pgid for a group that doesn't want job control!"
        );
        assert!(pgid >= 0, "Invalid pgid!");
        assert!(self.pgid.is_none(), "JobGroup::pgid already set!");

        self.pgid = Some(pgid);
    }

    /// Returns the value of [`JobGroup::pgid`]. This is never fish's own pgid!
    pub fn get_pgid(&self) -> Option<libc::pid_t> {
        self.pgid
    }

    /// Returns the value of [`JobGroup::pgid`] in a `UniquePtr<T>` to take the place of an
    /// `Option<T>` for ffi purposes. A null `UniquePtr` is equivalent to `None`.
    pub fn get_pgid_ffi(&self) -> cxx::UniquePtr<pgid_t> {
        match self.pgid {
            Some(value) => UniquePtr::new(pgid_t { value }),
            None => UniquePtr::null(),
        }
    }

    /// Returns the current terminal modes associated with the `JobGroup` for ffi purposes.
    unsafe fn get_modes_ffi(&self, size: usize) -> *const u8 {
        assert_eq!(
            size,
            core::mem::size_of::<libc::termios>(),
            "Mismatch between expected and actual ffi size of struct termios!"
        );

        self.tmodes
            .as_ref()
            // Really cool that type inference works twice in a row here. The first `_` is deduced
            // from the left and the second `_` is deduced from the right (the return type).
            .map(|val| val as *const _ as *const _)
            .unwrap_or(core::ptr::null())
    }

    /// Sets the current terminal modes associated with the `JobGroup`. Only use for ffi.
    ///
    /// Unlike `set_pgid()`, this isn't documented in the C++ codebase as being only called at
    /// initialization but as the underlying [`self.tmodes`] wasn't wrapped in any sort of
    /// thread-safe marshalling struct, we'll assume it can only be called from one thread and use
    /// `&mut self` for safety.
    unsafe fn set_modes_ffi(&mut self, modes: *const u8, size: usize) {
        assert_eq!(
            size,
            core::mem::size_of::<libc::termios>(),
            "Mismatch between expected and actual ffi size of struct termios!"
        );

        let modes = modes as *const libc::termios;
        if modes.is_null() {
            self.tmodes = None;
        } else {
            self.tmodes = Some(*modes);
        }
    }
}

/// Basic thread-safe sorted vector of job ids currently in use.
///
/// In the C++ codebase, this is deliberately leaked to avoid destructor ordering issues - see
/// #6539. Rust automatically "leaks" all `static` variables (does not call their `Drop` impls)
/// because of the inherent difficulty in doing that correctly (i.e. what we ran into).
static CONSUMED_JOB_IDS: Mutex<Vec<JobId>> = Mutex::new(Vec::new());

impl JobId {
    const NONE: Option<JobId> = None;

    /// Return a `JobId` that is greater than all extant job ids stored in [`CONSUMED_JOB_IDS`].
    /// The `JobId` should be freed with [`JobId::release()`] when it is no longer in use.
    fn acquire() -> Option<Self> {
        let mut consumed_job_ids = CONSUMED_JOB_IDS.lock().expect("Poisoned mutex!");

        // The new job id should be greater than the largest currently used id (#6053). The job ids
        // in CONSUMED_JOB_IDS are sorted in ascending order, so we just have to check the last.
        let job_id = consumed_job_ids
            .last()
            .map(JobId::next)
            .unwrap_or(JobId(1.try_into().unwrap()));
        consumed_job_ids.push(job_id);
        return Some(job_id);
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
    pub fn new(
        command: WideUtfString,
        id: Option<JobId>,
        job_control: bool,
        wants_term: bool,
    ) -> Self {
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
            tmodes: None,
            signal: 0.into(),
            is_foreground: false.into(),
            pgid: None,
        }
    }

    /// Return a new `JobGroup` with the provided `command`. The `JobGroup` is only assigned a
    /// `JobId` if `wants_job_id` is true and is created with job control disabled and
    /// [`JobGroup::wants_term`] set to false.
    pub fn create(command: WideUtfString, wants_job_id: bool) -> JobGroup {
        JobGroup::new(
            command,
            if wants_job_id {
                JobId::acquire()
            } else {
                JobId::NONE
            },
            false, /* job_control */
            false, /* wants_term */
        )
    }

    /// Return a new `JobGroup` with the provided `command` with job control enabled. A [`JobId`] is
    /// automatically acquired and assigned. If `wants_term` is true then [`JobGroup::wants_term`]
    /// is also set to `true` accordingly.
    pub fn create_with_job_control(command: WideUtfString, wants_term: bool) -> JobGroup {
        JobGroup::new(
            command,
            JobId::acquire(),
            true, /* job_control */
            wants_term,
        )
    }
}

impl Drop for JobGroup {
    fn drop(&mut self) {
        if let Some(job_id) = self.job_id {
            JobId::release(job_id);
        }
    }
}
