use crate::FLOGF;
use crate::common::wcs2osstring;
use crate::env_universal_common::default_vars_path;
use crate::universal_notifier::UniversalNotifier;
use crate::wchar::prelude::*;
use crate::wutil::wdirname;
use nix::sys::event::{EvFlags, EventFilter, FilterFlag, KEvent, Kqueue};
use std::fs::File;
use std::os::fd::AsFd;
use std::os::unix::fs::MetadataExt;
use std::os::unix::io::{AsRawFd, RawFd};
use std::path::PathBuf;
use std::sync::Mutex;
use std::time::SystemTime;

/// A notifier based on kqueue.
pub struct KqueueNotifier {
    kq: Kqueue,
    /// The file we are watching for changes
    path: PathBuf,
    inner: Mutex<KqueueNotifierInner>,
    #[allow(dead_code)]
    dir_fd: File,
}

struct KqueueNotifierInner {
    last_size: Option<u64>,
    last_mtime: Option<SystemTime>,
    last_inode: Option<u64>,
}

impl KqueueNotifier {
    /// Create a notifier at the default fish_variables path.
    pub fn new() -> Option<Self> {
        Self::new_at(&default_vars_path())
    }

    /// Create a notifier at a given path.
    /// The path should be the full path to the fish_variables file.
    pub fn new_at(path: &wstr) -> Option<Self> {
        let dirname = wdirname(path);
        let dir = PathBuf::from(wcs2osstring(dirname));
        let path = std::path::PathBuf::from(wcs2osstring(path));

        // Normal for this to be None if the file doesn't yet exist
        let meta = path.metadata().ok();
        let meta = meta.as_ref();

        // Open the directory to get a valid file descriptor or bail if it doesn't exist
        let dir_fd = File::open(dir.as_os_str()).ok()?;

        // Add a watch for EVFILT_VNODE events
        let change_event = KEvent::new(
            dir_fd.as_raw_fd() as usize,
            EventFilter::EVFILT_VNODE,
            EvFlags::EV_ADD | EvFlags::EV_CLEAR,
            // NOTE_EXTEND w/ a dir fd: dir entry added or removed as the result of a rename,
            // but NOT if the rename was within the directory.
            // NOTE_LINK w/ a dir fd: subdirectory created or deleted
            // NOTE_WRITE w/ a dir fd: file created in directory
            // NOTE_DELETE w/ a dir fd: file removed from directory
            FilterFlag::NOTE_EXTEND | FilterFlag::NOTE_WRITE | FilterFlag::NOTE_DELETE,
            0,
            0,
        );

        let kq = match nix::sys::event::Kqueue::new() {
            Ok(kq) => kq,
            Err(e) => {
                FLOGF!(warning, "Failed to create kqueue: {}", e.desc());
                return None;
            }
        };
        // Calling kevent with an empty event list causes it to add without watching for events.
        if let Err(e) = kq.kevent(&[change_event], &mut [], None) {
            FLOGF!(
                warning,
                "Could not register fs watch event with kqueue: {}",
                e.desc()
            );
            return None;
        }

        Some(KqueueNotifier {
            kq,
            path,
            inner: Mutex::new(KqueueNotifierInner {
                last_size: meta.map(|m| m.len()),
                last_mtime: meta.and_then(|m| m.modified().ok()),
                last_inode: meta.map(|m| m.ino()),
            }),
            // Move dir_fd to keep it open so the associated kqueue events aren't removed
            dir_fd,
        })
    }
}

impl UniversalNotifier for KqueueNotifier {
    /// Do nothing to trigger a notification.
    /// The notifications are generated from changes to the file itself.
    fn post_notification(&self) {}

    /// Returns the fd from which to watch for events.
    fn notification_fd(&self) -> Option<RawFd> {
        Some(self.kq.as_fd().as_raw_fd())
    }

    /// The notification_fd is readable; drain it.
    /// Returns true if a notification is considered to have been posted.
    fn notification_fd_became_readable(&self, fd: RawFd) -> bool {
        let mut have_event = false;
        let mut events = [KEvent::new(
            0,
            EventFilter::EVFILT_READ,
            EvFlags::empty(),
            FilterFlag::empty(),
            0,
            0,
        )];
        // Use a zero timeout because we check for more events than we have notifications for, to
        // drain all events up front.
        let timeout = libc::timespec {
            tv_nsec: 0,
            tv_sec: 0,
        };
        loop {
            let event_count = self
                .kq
                .kevent(&[], &mut events[..], Some(timeout))
                // Safe to unwrap because kevent(2) returns an error via the EV_ERROR flag if there's
                // room for at least one event in the event list.
                .unwrap();
            if event_count > 0 {
                have_event = true;
                for event in &events[..event_count] {
                    if event.flags().contains(EvFlags::EV_ERROR) {
                        // Error encountered processing this changelist item
                        FLOGF!(
                            warning,
                            "EV_ERROR in kqueue uvar monitor! Errno: {}",
                            event.data()
                        );
                        return false;
                    }
                }
            } else if !have_event {
                // Spurious wake, no event available.
                return false;
            } else {
                // All pending events drained
                break;
            }
        }

        let mut inner = self.inner.lock().expect("Mutex poisoned!");
        assert_eq!(fd, self.kq.as_fd().as_raw_fd(), "unexpected fd");

        // kqueue doesn't return any data associated with its notification events, so we need to
        // manually figure out what changed (if any). This can be omitted if we don't care if we
        // receive spurious notifications for other changes in the same directory, but it doesn't
        // cost much when we're only checking one file/path.
        let meta = self.path.metadata().ok();
        let meta = meta.as_ref();
        let mtime = meta.and_then(|m| m.modified().ok());
        let size = meta.map(|m| m.len());
        let inode = meta.map(|m| m.ino());

        if inode != inner.last_inode || mtime != inner.last_mtime || size != inner.last_size {
            inner.last_inode = inode;
            inner.last_mtime = mtime;
            inner.last_size = size;
            return true;
        }

        // Change notification received but it didn't apply to the specific file we care about
        // within the directory we are watching.
        return false;
    }
}

#[cfg(test)]
mod tests {
    use super::KqueueNotifier;
    use crate::{
        universal_notifier::{UniversalNotifier, test_helpers::test_notifiers},
        wchar::WString,
    };

    #[test]
    fn test_kqueue_notifiers() {
        let temp_dir = fish_tempfile::new_dir().unwrap();
        let fake_uvars_path =
            WString::from(temp_dir.path().join("fish_variables").to_str().unwrap());

        let mut notifiers = Vec::new();
        for _ in 0..16 {
            notifiers
                .push(KqueueNotifier::new_at(&fake_uvars_path).expect("failed to create notifier"));
        }
        let notifiers = notifiers
            .iter()
            .map(|n| n as &dyn UniversalNotifier)
            .collect::<Vec<_>>();
        test_notifiers(&notifiers, Some(&fake_uvars_path));
    }
}
