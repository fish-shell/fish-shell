use crate::{
    common::{str2wcstring, wcs2zstring},
    fds::wopen_cloexec,
    history::CHAOS_MODE,
    path::{path_get_data_remoteness, DirRemoteness},
    wchar::prelude::*,
};
use errno::errno;
use libc::{c_int, flock, EINTR, LOCK_EX, LOCK_SH, LOCK_UN};
use nix::{fcntl::OFlag, sys::stat::Mode};
use std::{
    ffi::CString,
    fs::File,
    os::fd::{AsRawFd, FromRawFd},
};

// Replacement for mkostemp(str, O_CLOEXEC)
// This uses mkostemp if available,
// otherwise it uses mkstemp followed by fcntl
fn fish_mkstemp_cloexec(name_template: CString) -> std::io::Result<(File, CString)> {
    let name = name_template.into_raw();
    #[cfg(not(apple))]
    let fd = {
        use libc::O_CLOEXEC;
        unsafe { libc::mkostemp(name, O_CLOEXEC) }
    };
    #[cfg(apple)]
    let fd = {
        use libc::{FD_CLOEXEC, F_SETFD};
        let fd = unsafe { libc::mkstemp(name) };
        if fd != -1 {
            unsafe { libc::fcntl(fd, F_SETFD, FD_CLOEXEC) };
        }
        fd
    };
    if fd == -1 {
        Err(std::io::Error::from(errno()))
    } else {
        unsafe { Ok((File::from_raw_fd(fd), CString::from_raw(name))) }
    }
}

/// Creates a temporary file created according to the template and its name if successful.
pub fn create_temporary_file(name_template: &wstr) -> std::io::Result<(File, WString)> {
    let (fd, c_string_template) = loop {
        match fish_mkstemp_cloexec(wcs2zstring(name_template)) {
            Ok(tmp_file_data) => break tmp_file_data,
            Err(e) => match e.kind() {
                std::io::ErrorKind::Interrupted => {}
                _ => {
                    return Err(e);
                }
            },
        }
    };
    Ok((fd, str2wcstring(c_string_template.to_bytes())))
}

/// Use this struct for all accesses to file which need mutual exclusion.
/// Otherwise, races on the file are possible.
/// The lock is released when this struct is dropped.
pub struct LockedFile {
    /// This is the file which requires mutual exclusion.
    /// It should only be accessed through this struct,
    /// because the locks used here do not protect from other accesses to the file.
    data_file: File,
    /// This file is the one being locked.
    /// The lock is not placed on the data file directly due to issues with renaming.
    /// If the data file is renamed after opening and before locking it,
    /// There are two independent files around, whose locks do not interact.
    /// In some cases this can be identified by checking file identifiers and timestamps,
    /// but even with such checks races and corresponding file corruption can occur.
    /// It is simpler to have a separate lock file, which does not change.
    ///
    /// We may fail to lock (e.g. on lockless NFS - see issue #685.
    /// In that case, we proceed as if locking succeeded.
    /// This might result in corruption,
    /// but the alternative of not being able to access the file at all is not desirable either.
    _lock_file: File,
}

pub enum LockingMode {
    Shared,
    Exclusive,
}

impl LockingMode {
    pub fn to_flock_op(&self) -> c_int {
        match self {
            LockingMode::Shared => LOCK_SH,
            LockingMode::Exclusive => LOCK_EX,
        }
    }
}

impl LockedFile {
    /// Maybe lock a file.
    /// Returns `true` if successful, `false` if locking was skipped.
    ///
    /// # Safety
    ///
    /// The file descriptor of `file` and `lock_type` must be valid arguments to `flock(2)`.
    unsafe fn maybe_lock_file(file: &mut File, lock_type: libc::c_int) -> bool {
        assert!(lock_type & LOCK_UN == 0, "Do not use lock_file to unlock");

        if CHAOS_MODE.load() {
            return false;
        }
        if path_get_data_remoteness() == DirRemoteness::remote {
            return false;
        }

        loop {
            if unsafe { flock(file.as_raw_fd(), lock_type) } != -1 {
                return true;
            }
            if errno::errno().0 != EINTR {
                return false;
            }
        }
    }

    /// Creates a `LockedFile`.
    /// Use this for any access to a which requires mutual exclusion, as it ensures correct locking.
    /// Use `LockingMode::Exclusive` if you want to modify the file in any way.
    /// Otherwise you should use `LockingMode::Shared`.
    /// `file_name` should just be a name, not a full path.
    /// `file_flags` and `file_mode` are used for opening (and potentially creating) the file.
    /// Note that this tries to create a separate lock file in the same directory as the file.
    pub fn new(
        locking_mode: LockingMode,
        file_path: &wstr,
        file_flags: OFlag,
        file_mode: nix::sys::stat::Mode,
    ) -> std::io::Result<Self> {
        const LOCK_FILE_SUFFIX: &wstr = L!(".lock");
        let mut lock_file_path = file_path.to_owned();
        lock_file_path.push_utfstr(LOCK_FILE_SUFFIX);
        let lock_file_path = lock_file_path;

        // We start by locking the dedicated lock file.
        // This is required to avoid racing modifications by other threads/processes.
        // If the lock is not released explicitly, it is released when the file descriptor of
        // `lock_file` is closed, which happens in the `Drop` implementation of `File`.
        let mut lock_file = wopen_cloexec(
            &lock_file_path,
            OFlag::O_RDONLY | OFlag::O_CREAT,
            Mode::S_IRUSR,
        )?;
        unsafe {
            Self::maybe_lock_file(&mut lock_file, locking_mode.to_flock_op());
        }

        // Open the data file
        let data_file = wopen_cloexec(file_path, file_flags, file_mode)?;

        Ok(Self {
            data_file,
            _lock_file: lock_file,
        })
    }

    pub fn get_file(&self) -> &File {
        &self.data_file
    }

    pub fn get_file_mut(&mut self) -> &mut File {
        &mut self.data_file
    }
}
