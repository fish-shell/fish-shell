use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharFromFFI;
use cxx::CxxWString;
use libc::pid_t;
use std::cell::Cell;
use std::rc::Rc;

/// The non user-visible, never-recycled job ID.
/// Every job has a unique positive value for this.
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
    pub fn set_status_and_complete(&self, status: i32) {
        assert!(!self.is_completed(), "wait handle already completed");
        self.status.set(Some(status));
    }

    /// \return the status, or None if not yet completed.
    pub fn status(&self) -> Option<i32> {
        self.status.get()
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
