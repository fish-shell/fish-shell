use crate::wchar::WString;
use crate::wchar_ffi::WCharFromFFI;
use cxx::CxxWString;
use libc::pid_t;
use std::cell::Cell;
use std::rc::Rc;

#[cxx::bridge]
mod wait_handle_ffi {
    extern "Rust" {
        type WaitHandleRefFFI;
        fn new_wait_handle_ffi(
            pid: i32,
            internal_job_id: u64,
            base_name: &CxxWString,
        ) -> Box<WaitHandleRefFFI>;
        fn set_status_and_complete(self: &mut WaitHandleRefFFI, status: i32);

        type WaitHandleStoreFFI;
        fn new_wait_handle_store_ffi() -> Box<WaitHandleStoreFFI>;
        fn remove_by_pid(self: &mut WaitHandleStoreFFI, pid: i32);
        fn get_job_id_by_pid(self: &WaitHandleStoreFFI, pid: i32) -> u64;

        fn try_get_status_and_job_id(
            self: &WaitHandleStoreFFI,
            pid: i32,
            only_if_complete: bool,
            status: &mut i32,
            job_id: &mut u64,
        ) -> bool;

        fn add(self: &mut WaitHandleStoreFFI, wh: *const Box<WaitHandleRefFFI>);
    }
}

pub struct WaitHandleRefFFI(WaitHandleRef);

impl WaitHandleRefFFI {
    #[allow(clippy::wrong_self_convention)]
    pub fn from_ffi(&self) -> &WaitHandleRef {
        &self.0
    }

    #[allow(clippy::wrong_self_convention)]
    pub fn from_ffi_mut(&mut self) -> &mut WaitHandleRef {
        &mut self.0
    }

    fn set_status_and_complete(self: &mut WaitHandleRefFFI, status: i32) {
        let wh = self.from_ffi();
        assert!(!wh.is_completed(), "wait handle already completed");
        wh.status.set(Some(status));
    }
}

pub struct WaitHandleStoreFFI(WaitHandleStore);

impl WaitHandleStoreFFI {
    #[allow(clippy::wrong_self_convention)]
    pub fn from_ffi_mut(&mut self) -> &mut WaitHandleStore {
        &mut self.0
    }

    #[allow(clippy::wrong_self_convention)]
    pub fn from_ffi(&self) -> &WaitHandleStore {
        &self.0
    }

    /// \return the job ID for a pid, or 0 if None.
    fn get_job_id_by_pid(&self, pid: i32) -> u64 {
        self.from_ffi()
            .get_by_pid(pid)
            .map(|wh| wh.internal_job_id)
            .unwrap_or(0)
    }

    /// Try getting the status and job ID of a job.
    /// \return true if the job was found.
    /// If only_if_complete is true, then only return true if the job is completed.
    fn try_get_status_and_job_id(
        self: &WaitHandleStoreFFI,
        pid: i32,
        only_if_complete: bool,
        status: &mut i32,
        job_id: &mut u64,
    ) -> bool {
        let whs = self.from_ffi();
        let Some(wh) = whs.get_by_pid(pid) else {
            return false;
        };
        if only_if_complete && !wh.is_completed() {
            return false;
        }
        *status = wh.status.get().unwrap_or(0);
        *job_id = wh.internal_job_id;
        true
    }

    /// Remove the wait handle for a pid, if present in this store.
    fn remove_by_pid(&mut self, pid: i32) {
        self.from_ffi_mut().remove_by_pid(pid);
    }

    fn add(self: &mut WaitHandleStoreFFI, wh: *const Box<WaitHandleRefFFI>) {
        if wh.is_null() {
            return;
        }
        let wh = unsafe { (*wh).from_ffi() };
        self.from_ffi_mut().add(wh.clone());
    }
}

fn new_wait_handle_store_ffi() -> Box<WaitHandleStoreFFI> {
    Box::new(WaitHandleStoreFFI(WaitHandleStore::new()))
}

fn new_wait_handle_ffi(
    pid: i32,
    internal_job_id: u64,
    base_name: &CxxWString,
) -> Box<WaitHandleRefFFI> {
    Box::new(WaitHandleRefFFI(WaitHandle::new(
        pid as pid_t,
        internal_job_id,
        base_name.from_ffi(),
    )))
}

pub type InternalJobId = u64;

/// The bits of a job necessary to support 'wait' and '--on-process-exit'.
/// This may outlive the job.
pub struct WaitHandle {
    /// The pid of this process.
    pub pid: pid_t,

    /// The internal job id of the job which contained this process.
    pub internal_job_id: InternalJobId,

    /// The "base name" of this process.
    /// For example if the process is "/bin/sleep" then this will be 'sleep'.
    pub base_name: WString,

    /// The status, if completed; None if not completed.
    status: Cell<Option<i32>>,
}

impl WaitHandle {
    /// \return true if this wait handle is completed.
    pub fn is_completed(&self) -> bool {
        self.status.get().is_some()
    }
}

impl WaitHandle {
    /// Construct from a pid, job id, and base name.
    pub fn new(pid: pid_t, internal_job_id: InternalJobId, base_name: WString) -> WaitHandleRef {
        Rc::new(WaitHandle {
            pid,
            internal_job_id,
            base_name,
            status: Default::default(),
        })
    }
}

pub type WaitHandleRef = Rc<WaitHandle>;

const WAIT_HANDLE_STORE_DEFAULT_LIMIT: usize = 1024;

/// Support for storing a list of wait handles, with a max limit set at initialization.
/// Note this class is not safe for concurrent access.
pub struct WaitHandleStore {
    // Map from pid to wait handles.
    cache: lru::LruCache<pid_t, WaitHandleRef>,
}

impl WaitHandleStore {
    /// Construct with the default capacity.
    pub fn new() -> WaitHandleStore {
        Self::new_with_capacity(WAIT_HANDLE_STORE_DEFAULT_LIMIT)
    }

    pub fn new_with_capacity(capacity: usize) -> WaitHandleStore {
        let capacity = std::num::NonZeroUsize::new(capacity).unwrap();
        WaitHandleStore {
            cache: lru::LruCache::new(capacity),
        }
    }

    /// Add a wait handle to the store. This may remove the oldest handle, if our limit is exceeded.
    /// It may also remove any existing handle with that pid.
    pub fn add(&mut self, wh: WaitHandleRef) {
        self.cache.put(wh.pid, wh);
    }

    /// \return the wait handle for a pid, or None if there is none.
    /// This is a fast lookup.
    pub fn get_by_pid(&self, pid: pid_t) -> Option<WaitHandleRef> {
        self.cache.peek(&pid).cloned()
    }

    /// Remove a given wait handle, if present in this store.
    pub fn remove(&mut self, wh: &WaitHandleRef) {
        // Note: this differs from remove_by_pid because we verify that the handle is the same.
        if let Some(key) = self.cache.peek(&wh.pid) {
            if Rc::ptr_eq(key, wh) {
                self.cache.pop(&wh.pid);
            }
        }
    }

    /// Remove the wait handle for a pid, if present in this store.
    pub fn remove_by_pid(&mut self, pid: pid_t) {
        self.cache.pop(&pid);
    }

    /// Iterate over wait handles.
    pub fn iter(&self) -> impl Iterator<Item = &WaitHandleRef> {
        self.cache.iter().map(|(_, wh)| wh)
    }

    /// Copy out the list of all wait handles, returning the most-recently-used first.
    pub fn get_list(&self) -> Vec<WaitHandleRef> {
        self.cache.iter().map(|(_, wh)| wh.clone()).collect()
    }

    /// Convenience to return the size, for testing.
    pub fn size(&self) -> usize {
        self.cache.len()
    }
}

#[test]
fn test_wait_handles() {
    use crate::wchar::L;

    let limit: usize = 4;
    let mut whs = WaitHandleStore::new_with_capacity(limit);
    assert_eq!(whs.size(), 0);

    assert!(whs.get_by_pid(5).is_none());

    // Duplicate pids drop oldest.
    whs.add(WaitHandle::new(5, 0, L!("first").to_owned()));
    whs.add(WaitHandle::new(5, 0, L!("second").to_owned()));
    assert_eq!(whs.size(), 1);
    assert_eq!(whs.get_by_pid(5).unwrap().base_name, "second");

    whs.remove_by_pid(123);
    assert_eq!(whs.size(), 1);
    whs.remove_by_pid(5);
    assert_eq!(whs.size(), 0);

    // Test evicting oldest.
    whs.add(WaitHandle::new(1, 0, L!("1").to_owned()));
    whs.add(WaitHandle::new(2, 0, L!("2").to_owned()));
    whs.add(WaitHandle::new(3, 0, L!("3").to_owned()));
    whs.add(WaitHandle::new(4, 0, L!("4").to_owned()));
    whs.add(WaitHandle::new(5, 0, L!("5").to_owned()));
    assert_eq!(whs.size(), 4);

    let entries = whs.get_list();
    let mut iter = entries.iter();
    assert_eq!(iter.next().unwrap().base_name, "5");
    assert_eq!(iter.next().unwrap().base_name, "4");
    assert_eq!(iter.next().unwrap().base_name, "3");
    assert_eq!(iter.next().unwrap().base_name, "2");
    assert!(iter.next().is_none());
}
