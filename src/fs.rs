use crate::{
    common::{str2wcstring, wcs2osstring, wcs2zstring},
    fds::wopen_cloexec,
    path::{path_remoteness, DirRemoteness},
    wchar::prelude::*,
    wutil::{
        file_id_for_file, file_id_for_path, wdirname, wrename, wunlink, FileId, INVALID_FILE_ID,
    },
    FLOG, FLOGF,
};
use errno::errno;
use libc::{c_int, fchown, flock, LOCK_EX, LOCK_SH};
use nix::{fcntl::OFlag, sys::stat::Mode};
use std::{
    ffi::CString,
    fs::{File, OpenOptions},
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
                    FLOG!(
                        error,
                        wgettext_fmt!("Unable to create temporary file '%s': %s", name_template, e)
                    );

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
    /// The file descriptor of the parent directory, used for locking.
    /// The lock is not placed on the data file directly due to issues with renaming.
    /// If the data file is renamed after opening and before locking it,
    /// There are two independent files around, whose locks do not interact.
    /// In some cases this can be identified by checking file identifiers and timestamps,
    /// but even with such checks races and corresponding file corruption can occur.
    /// It is simpler to lock a different path, which does not change.
    ///
    /// We may fail to lock (e.g. on lockless NFS - see issue #685.
    /// In that case, we proceed as if locking succeeded.
    /// This might result in corruption,
    /// but the alternative of not being able to access the file at all is not desirable either.
    _locked_fd: File,
}

pub const LOCKED_FILE_MODE: Mode = Mode::from_bits_truncate(0o600);

pub enum LockingMode {
    Shared,
    Exclusive(WriteMethod),
}
pub enum WriteMethod {
    Append,
    RenameIntoPlace,
}

impl LockingMode {
    pub fn flock_op(&self) -> c_int {
        match self {
            Self::Shared => LOCK_SH,
            Self::Exclusive(_) => LOCK_EX,
        }
    }

    pub fn file_flags(&self) -> OFlag {
        match self {
            Self::Shared => OFlag::O_RDONLY,
            Self::Exclusive(WriteMethod::Append) => {
                OFlag::O_WRONLY | OFlag::O_APPEND | OFlag::O_CREAT
            }
            Self::Exclusive(WriteMethod::RenameIntoPlace) => OFlag::O_RDONLY | OFlag::O_CREAT,
        }
    }
}

impl LockedFile {
    /// Creates a [`LockedFile`].
    /// Use this for any access to a which requires mutual exclusion, as it ensures correct locking.
    /// Use [`LockingMode::Exclusive`] if you want to modify the file in any way.
    /// Otherwise you should use [`LockingMode::Shared`].
    /// Two modes of modification are supported:
    /// - Appending
    /// - Writing to a temporary file which is then renamed into place.
    /// File flags are derived from the [`LockingMode`].
    /// `file_name` should just be a name, not a full path.
    pub fn new(locking_mode: LockingMode, file_path: &wstr) -> std::io::Result<Self> {
        let dir_path = wdirname(file_path);

        if path_remoteness(dir_path) == DirRemoteness::remote {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Unsupported,
                "Directory considered remote. Locking is disabled on remote file systems.",
            ));
        }

        // We start by locking the directory.
        // This is required to avoid racing modifications by other threads/processes.
        let dir_fd = wopen_cloexec(dir_path, OFlag::O_RDONLY, Mode::empty())?;

        // Try locking the directory. Retry if locking was interrupted.
        while unsafe { flock(dir_fd.as_raw_fd(), locking_mode.flock_op()) } == -1 {
            let err = std::io::Error::last_os_error();
            if err.kind() != std::io::ErrorKind::Interrupted {
                return Err(err);
            }
        }

        // Open the data file
        let data_file = wopen_cloexec(file_path, locking_mode.file_flags(), LOCKED_FILE_MODE)?;

        Ok(Self {
            data_file,
            _locked_fd: dir_fd,
        })
    }

    pub fn get(&self) -> &File {
        &self.data_file
    }

    pub fn get_mut(&mut self) -> &mut File {
        &mut self.data_file
    }

    /// Calling `fsync` and then closing the file is required for correctness on some file systems
    /// before renaming the file.
    /// <https://www.comp.nus.edu.sg/~lijl/papers/ferrite-asplos16.pdf>
    /// <https://archive.kernel.org/oldwiki/btrfs.wiki.kernel.org/index.php/FAQ.html#What_are_the_crash_guarantees_of_overwrite-by-rename.3F>
    /// Returns the lock file so that the lock can be kept.
    pub fn fsync_close_and_keep_lock(self) -> std::io::Result<File> {
        fsync(&self.data_file)?;
        Ok(self._locked_fd)
    }
}

/// Reimplementation of `std::sys::fs::unix::File::fsync` using publicly accessible functionality.
/// This function is used instead of `sync_all` due to concerns of that being too slow on macOS,
/// since there `libc::fcntl(fd, libc::F_FULLFSYNC)` is used internally.
/// This weakens our guarantees on macOS.
pub fn fsync(file: &File) -> std::io::Result<()> {
    let fd = file.as_raw_fd();
    loop {
        match unsafe { libc::fsync(fd) } {
            0 => return Ok(()),
            -1 => {
                let os_error = std::io::Error::last_os_error();
                if os_error.kind() != std::io::ErrorKind::Interrupted {
                    return Err(os_error);
                }
            }
            _ => panic!("fsync should only ever return 0 or -1"),
        }
    }
}

/// Runs the `load` function, which should see a consistent state of the file at `path`.
/// To ensure a consistent state, we use locks to prevent modifications by others.
/// If locking is unavailable, the `load` function might be executed multiple times,
/// until it manages a run without the file being modified in the meantime, or until the maximum
/// number of allowed attempts is reached.
/// If the file does not exist this function will return an error.
pub fn lock_and_load<F, UserData>(path: &wstr, load: F) -> std::io::Result<(FileId, UserData)>
where
    F: Fn(&File) -> std::io::Result<UserData>,
{
    match LockedFile::new(LockingMode::Shared, path) {
        Ok(locked_file) => {
            return Ok((
                file_id_for_file(locked_file.get()),
                load(locked_file.get())?,
            ));
        }
        Err(e) => {
            FLOGF!(
                synced_file_access,
                "Error acquiring shared lock on the directory of '%s': %s",
                path,
                e,
            );
            // This function might be called when the file does not exist.
            if e.kind() == std::io::ErrorKind::NotFound {
                // There is no point in continuing in this function if the file does not
                // exist.
                return Err(e);
            }
        }
    }

    FLOG!(
        synced_file_access,
        "flock-based locking is disabled. Using fallback implementation."
    );

    // Fallback implementation for situations where locking is unavailable.
    let max_attempts = 1000;
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
        return Ok((final_file_id, loaded_data));
    }
    Err(std::io::Error::new(std::io::ErrorKind::Other, "Failed to update the file. Locking is disabled, and the fallback code did not succeed within the permissible number of attempts."))
}

pub struct PotentialUpdate<UserData> {
    pub do_save: bool,
    pub data: UserData,
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
pub fn rewrite_via_temporary_file<F, UserData>(
    path: &wstr,
    rewrite: F,
) -> std::io::Result<(FileId, PotentialUpdate<UserData>)>
where
    F: Fn(&File, &mut File) -> std::io::Result<PotentialUpdate<UserData>>,
{
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
                    synced_file_access,
                    "Error when changing ownership of file:",
                    errno::errno()
                );
            }
            if let Err(e) = new_file.set_permissions(md.permissions()) {
                FLOG!(synced_file_access, "Error when changing mode of file:", e);
            }
        } else {
            FLOG!(synced_file_access, "Could not get metadata for file");
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

    // This contains the main body of the surrounding function.
    // It is wrapped as its own function to allow unlinking the tmpfile reliably.
    // This can be achieved more concisely using a `ScopeGuard` which unlinks the file when it is
    // closed, but this does not work if the file should be closed before renaming, which is
    // necessary for the whole process to work reliably on some filesystems.
    fn try_rewriting<F, UserData>(
        path: &wstr,
        rewrite: F,
        tmp_name: &wstr,
        mut tmp_file: File,
    ) -> std::io::Result<(FileId, PotentialUpdate<UserData>)>
    where
        F: Fn(&File, &mut File) -> std::io::Result<PotentialUpdate<UserData>>,
    {
        // We want to rewrite the file.
        // To avoid issues with crashes during writing,
        // we write to a temporary file and once we are done, this file is renamed such that it
        // replaces the original file.
        // To avoid races, we need to have exclusive access to the file for the entire
        // duration, which we get via an exclusive lock on the parent directory.
        // Taking a shared lock first and later upgrading to an exclusive one could result in a
        // deadlock, so we take an exclusive one immediately.
        match LockedFile::new(LockingMode::Exclusive(WriteMethod::RenameIntoPlace), path) {
            Ok(locked_file) => {
                let potential_update = rewrite(locked_file.get(), &mut tmp_file)?;
                // In case the `LockedFile` is destroyed, this variable keeps the lock file in
                // scope, to prevent releasing the lock too early.
                let mut _lock_file = None;
                if potential_update.do_save {
                    update_metadata(locked_file.get(), &tmp_file);
                    _lock_file = Some(locked_file.fsync_close_and_keep_lock()?);
                    rename(tmp_name, path)?;
                }
                return Ok((file_id_for_path(path), potential_update));
            }
            Err(e) => {
                FLOGF!(
                    synced_file_access,
                    "Error acquiring exclusive lock on the directory of '%s': %s",
                    path,
                    e,
                );
            }
        }

        // If this is reached, we assume that locking is not available so we use a fallback
        // implementation which tries to avoid race conditions, but in the case of contention it is
        // possible that some writes are lost.

        FLOG!(
            synced_file_access,
            "flock-based locking is disabled. Using fallback implementation."
        );

        // Give up after this many unsuccessful attempts.
        // TODO: which value, should this be a constant shared with other retrying logic?
        let max_attempts = 1000;
        for _ in 0..max_attempts {
            // Reopen the tmpfile. This is important because it might have been closed by
            // explicitly dropping it.
            tmp_file = OpenOptions::new()
                .write(true)
                .truncate(true)
                .open(wcs2osstring(tmp_name))?;

            // If the file does not exist yet, this will be `INVALID_FILE_ID`.
            let initial_file_id = file_id_for_path(path);
            // If we cannot open the file, there is nothing we can do,
            // so just return immediately.
            let old_file = wopen_cloexec(path, OFlag::O_RDONLY | OFlag::O_CREAT, LOCKED_FILE_MODE)?;
            let opened_file_id = file_id_for_file(&old_file);
            if initial_file_id != INVALID_FILE_ID && initial_file_id != opened_file_id {
                // File ID changed (and not just because the file was created by us).
                continue;
            }
            let Ok(potential_update) = rewrite(&old_file, &mut tmp_file) else {
                // Retry if rewrite function failed. Because we do not hold a lock, this might be
                // caused by concurrent modifications.
                continue;
            };

            if potential_update.do_save {
                update_metadata(&old_file, &tmp_file);
                // fsync + close as described in
                // https://archive.kernel.org/oldwiki/btrfs.wiki.kernel.org/index.php/FAQ.html#What_are_the_crash_guarantees_of_overwrite-by-rename.3F
                fsync(&tmp_file)?;
                std::mem::drop(tmp_file);
            }

            let mut final_file_id = file_id_for_path(path);
            if opened_file_id != final_file_id {
                continue;
            }

            // If we reach this point, the file ID did not change while we read the old file and wrote
            // to `tmp_file`. Now we replace the old file with the `tmp_file`.
            // Note that we cannot prevent races here.
            // If the file is modified by someone else between the syscall for determining the [`FileId`]
            // and the rename syscall, these modifications will be lost.

            if potential_update.do_save {
                // Do not retry on rename failures, as it is unlikely that these will disappear if we retry.
                rename(tmp_name, path)?;
                final_file_id = file_id_for_path(path);
            }
            // Note that this might not match the version of the file we just wrote.
            // (If we did write.)
            return Ok((final_file_id, potential_update));
        }
        Err(std::io::Error::new(std::io::ErrorKind::Other, "Failed to update the file. Locking is disabled, and the fallback code did not succeed within the permissible number of attempts."))
    }

    const TMP_FILE_SUFFIX: &wstr = L!(".XXXXXX");
    let tmp_file_template = path.to_owned() + TMP_FILE_SUFFIX;
    let (tmp_file, tmp_name) = create_temporary_file(&tmp_file_template)?;
    let result = try_rewriting(path, rewrite, &tmp_name, tmp_file);
    // Do not leave the tmpfile around.
    // Note that we do not unlink when renaming succeeded.
    // In that case, it would be unnecessary, because the file will no longer exist at the path,
    // but it also enables a race condition, where after renaming succeeded, a different fish
    // instance creates a tmpfile with the same name (unlikely but possible), which we would then
    // delete here.
    if result.is_err()
        || result
            .as_ref()
            .is_ok_and(|(_file_id, potential_update)| !potential_update.do_save)
    {
        wunlink(&tmp_name);
    }
    result
}
