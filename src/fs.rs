use crate::{
    common::{str2wcstring, wcs2zstring, ScopeGuard},
    fds::wopen_cloexec,
    global_safety::RelaxedAtomicBool,
    path::{path_get_data_remoteness, DirRemoteness},
    wchar::prelude::*,
    wutil::{file_id_for_file, file_id_for_path, wrename, wunlink, FileId, INVALID_FILE_ID},
    FLOG,
};
use errno::errno;
use libc::{c_int, fchown, flock, EINTR, LOCK_EX, LOCK_SH, LOCK_UN};
use nix::{fcntl::OFlag, sys::stat::Mode};
use std::{
    ffi::CString,
    fs::File,
    io::Seek,
    os::{
        fd::{AsRawFd, FromRawFd},
        unix::fs::MetadataExt,
    },
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

/// Runs the `load` function, which should see a consistent state of the file at `path`.
/// To ensure a consistent state, we use locks to prevent modifications by others.
/// If locking is unavailable, the `load` function might be executed multiple times,
/// until it manages a run without the file being modified in the meantime, or until the maximum
/// number of allowed attempts is reached.
/// If the file does not exist this function will return an error.
pub fn lock_and_load<F, U>(
    path: &wstr,
    locking_enabled: &RelaxedAtomicBool,
    load: F,
) -> std::io::Result<U>
where
    F: Fn(&File) -> std::io::Result<U>,
{
    if locking_enabled.load() {
        match LockedFile::new(LockingMode::Shared, path, OFlag::O_RDONLY, Mode::empty()) {
            Ok(locked_file) => {
                return load(locked_file.get_file());
            }
            Err(e) => {
                FLOG!(history_file, "Error when trying to lock file:", e);
                locking_enabled.store(false);
            }
        }
    }

    // Fallback implementation for situations where locking is unavailable.
    let max_attempts = 10;
    for _ in 0..max_attempts {
        let initial_file_id = file_id_for_path(path);
        // If we cannot open the file, there is nothing we can do,
        // so just return immediately.
        let file = wopen_cloexec(path, OFlag::O_RDONLY, Mode::empty())?;
        let loaded_data = match load(&file) {
            Ok(update_data) => update_data,
            Err(_) => {
                // Retry if load function failed. Because we do not hold a lock, this might be
                // caused by concurrent modifications.
                continue;
            }
        };

        let final_file_id = file_id_for_path(path);
        if initial_file_id != final_file_id {
            continue;
        }
        // If the file id did not change, we assume that we loaded a consistent state.
        return Ok(loaded_data);
    }
    Err(std::io::Error::new(std::io::ErrorKind::Other, "Failed to update the file. Locking is disabled, and the fallback code did not succeed within the permissible number of attempts."))
}

/// Use this function for updating a file based on its current content,
/// for files which might be accessed at the same time by other threads/processes.
/// The basic principle is to create a temporary file, take a lock on the file to be rewritten,
/// do the rewrite (which might involve reading the old file contents),
/// and finally release the lock.
/// Error handling (especially the case where locking is not possible) makes the implementation
/// non-straightforward.
///
/// # Arguments
///
/// - `path`: The path to the file which should be updated.
/// - `locking_enabled`: To indicate whether locking should be attempted. This function might
/// update the value stored in `locking_enabled`.
/// - `rewrite`: The function which handles reading from the file and writing to a temporary file.
/// The first argument is for the file to read from, the second for the temporary file to write to.
/// On success, the value returned by `rewrite` is included in this functions return value. Be
/// careful about side effects of `rewrite`. It might get executed multiple times. Try to avoid
/// side effects and instead extract any data you might need and return them on success.
/// Then, apply the desired side effects once this function has returned successfully.
///
/// # Return value
///
/// On success, the [`FileId`] of the rewritten file is returned, alongside the value returned by
/// `rewrite`. Note that if locking is unavailable, the [`FileId`] might be the id of a different
/// version of the file, which was written after this function renamed the temporary file to `path`
/// but before we obtained the [`FileId`] from `path`. This is a race condition we do not detect.
pub fn rewrite_via_temporary_file<F, U>(
    path: &wstr,
    locking_enabled: &RelaxedAtomicBool,
    rewrite: F,
) -> std::io::Result<(FileId, U)>
where
    F: Fn(&mut File, &mut File) -> std::io::Result<U>,
{
    // TODO: Do not log in history-file category.

    /// Updates the metadata of the `new_file` to match the `old_file`.
    /// Also updates the mtime of the `new_file` to the current time manually, to work around
    /// operating systems which do not update the internal time used for such time stamps
    /// frequently enough. This is important for [`FileId`] comparisons.
    fn update_metadata(old_file: &File, new_file: &File) {
        // Ensure we maintain the ownership and permissions of the original (#2355). If the
        // stat fails, we assume (hope) our default permissions are correct. This
        // corresponds to e.g. someone running sudo -E as the very first command. If they
        // did, it would be tricky to set the permissions correctly. (bash doesn't get this
        // case right either).
        if let Ok(md) = old_file.metadata() {
            // TODO(MSRV): Consider replacing with std::os::unix::fs::fchown when MSRV >= 1.73
            if unsafe { fchown(new_file.as_raw_fd(), md.uid(), md.gid()) } == -1 {
                FLOG!(
                    history_file,
                    "Error when changing ownership of file:",
                    errno::errno()
                );
            }
            if let Err(e) = new_file.set_permissions(md.permissions()) {
                FLOG!(history_file, "Error when changing mode of file:", e);
            }
        } else {
            FLOG!(history_file, "Could not get metadata for file");
        }
        // Linux by default stores the mtime with low precision, low enough that updates that occur
        // in quick succession may result in the same mtime (even the nanoseconds field). So
        // manually set the mtime of the new file to a high-precision clock. Note that this is only
        // necessary because Linux aggressively reuses inodes, causing the ABA problem; on other
        // platforms we tend to notice the file has changed due to a different inode.
        //
        // The current time within the Linux kernel is cached, and generally only updated on a timer
        // interrupt. So if the timer interrupt is running at 10 milliseconds, the cached time will
        // only be updated once every 10 milliseconds.
        #[cfg(any(target_os = "linux", target_os = "android"))]
        {
            let mut times: [libc::timespec; 2] = unsafe { std::mem::zeroed() };
            times[0].tv_nsec = libc::UTIME_OMIT; // don't change atime
            if unsafe { libc::clock_gettime(libc::CLOCK_REALTIME, &mut times[1]) } == 0 {
                unsafe {
                    // This accesses both times[0] and times[1]. Check `utimensat(2)` for details.
                    libc::futimens(new_file.as_raw_fd(), &times[0]);
                }
            }
        }
    }

    /// Renames a file from `old_name` to `new_name`.
    fn rename(old_name: &wstr, new_name: &wstr) -> std::io::Result<()> {
        if wrename(old_name, new_name) == -1 {
            let error_number = errno::errno();
            FLOG!(
                error,
                wgettext_fmt!("Error when renaming file: %s", error_number.to_string())
            );
            return Err(std::io::Error::from(error_number));
        }
        Ok(())
    }

    const TMP_FILE_SUFFIX: &wstr = L!(".XXXXXX");
    let mut tmp_file_template = path.to_owned();
    tmp_file_template.push_utfstr(TMP_FILE_SUFFIX);
    let tmp_file_template = tmp_file_template;
    let (tmp_file, tmp_name) = create_temporary_file(&tmp_file_template)?;
    // Ensure that the temporary file is unlinked when this function returns.
    let mut tmp_file = ScopeGuard::new(tmp_file, |_| {
        wunlink(&tmp_name);
    });

    // We want to rewrite the file.
    // To avoid issues with crashes during writing,
    // we write to a temporary file and once we are done, this file is renamed such that it
    // replaces the original file.
    if locking_enabled.load() {
        // To avoid races, we need to have exclusive access to the file for the entire
        // duration, which we get via an exclusive lock on the corresponding lock file.
        // Taking a shared lock first and later upgrading to an exclusive one could result in a
        // deadlock, so we take an exclusive one immediately.
        match LockedFile::new(
            LockingMode::Exclusive,
            path,
            OFlag::O_RDONLY | OFlag::O_CREAT,
            Mode::S_IRUSR | Mode::S_IWUSR,
        ) {
            Ok(mut locked_file) => match rewrite(locked_file.get_file_mut(), &mut tmp_file) {
                Ok(update_data) => {
                    update_metadata(locked_file.get_file(), &tmp_file);
                    rename(&tmp_name, path)?;
                    return Ok((file_id_for_path(path), update_data));
                }
                Err(e) => {
                    return Err(e);
                }
            },
            Err(e) => {
                FLOG!(history_file, "Error when trying to lock file:", e);
                locking_enabled.store(false);
            }
        }
    }

    // If this is reached, we assume that locking is not available so we use a fallback
    // implementation which tries to avoid race conditions, but in the case of contention it is
    // possible that some writes are lost.

    // Give up after this many unsuccessful attempts.
    let max_attempts = 10;
    for _ in 0..max_attempts {
        // Truncate tmp_file for the next attempt to get rid of data potentially written in the
        // previous iteration.
        tmp_file.set_len(0).unwrap();
        tmp_file.seek(std::io::SeekFrom::Start(0)).unwrap();

        // If the file does not exist yet, this will be `INVALID_FILE_ID`.
        let initial_file_id = file_id_for_path(path);
        // If we cannot open the file, there is nothing we can do,
        // so just return immediately.
        let mut old_file = wopen_cloexec(
            path,
            OFlag::O_RDONLY | OFlag::O_CREAT,
            Mode::S_IRUSR | Mode::S_IWUSR,
        )?;
        let opened_file_id = file_id_for_file(&old_file);
        if initial_file_id != INVALID_FILE_ID && initial_file_id != opened_file_id {
            // File ID changed (and not just because the file was created by us).
            continue;
        }
        let update_data = match rewrite(&mut old_file, &mut tmp_file) {
            Ok(update_data) => update_data,
            Err(_) => {
                // Retry if rewrite function failed. Because we do not hold a lock, this might be
                // caused by concurrent modifications.
                continue;
            }
        };

        update_metadata(&old_file, &tmp_file);

        let final_file_id = file_id_for_path(path);
        if opened_file_id != final_file_id {
            continue;
        }

        // If we reach this point, the file ID did not change while we read the old file and wrote
        // to `tmp_file`. Now we replace the old file with the `tmp_file`.
        // Note that we cannot prevent races here.
        // If the file is modified by someone else between the syscall for determining the [`FileId`]
        // and the rename syscall, these modifications will be lost.

        // Do not retry on rename failures, as it is unlikely that these will disappear if we retry.
        rename(&tmp_name, path)?;
        // Note that this might not match the version of the file we just wrote.
        return Ok((file_id_for_path(path), update_data));
    }
    Err(std::io::Error::new(std::io::ErrorKind::Other, "Failed to update the file. Locking is disabled, and the fallback code did not succeed within the permissible number of attempts."))
}
