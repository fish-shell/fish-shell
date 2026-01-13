use crate::common::wcs2osstring;
use crate::env_universal_common::default_vars_path;
use crate::prelude::*;
use crate::universal_notifier::UniversalNotifier;
use crate::wutil::{wbasename, wdirname};
use nix::sys::inotify::{AddWatchFlags, InitFlags, Inotify};
use std::ffi::OsString;
use std::os::fd::{AsFd, AsRawFd, RawFd};

/// A notifier based on inotify.
pub struct InotifyNotifier {
    // The inotify instance.
    inotify: Inotify,
    // The basename of the file to watch.
    basename: OsString,
}

impl InotifyNotifier {
    /// Create a notifier at the default fish_variables path.
    pub fn new() -> Option<Self> {
        Self::new_at(&default_vars_path())
    }

    /// Create a notifier at a given path.
    /// The path should be the full path to the fish_variables file.
    /// InotifyNotifier will watch the parent directory for changes to that file.
    /// It should not watch for modifications to the file itself, because uvars are atomically
    /// swapped into place.
    pub fn new_at(path: &wstr) -> Option<Self> {
        let dirname = wdirname(path);
        let basename = wbasename(path);
        let inotify = Inotify::init(InitFlags::IN_CLOEXEC | InitFlags::IN_NONBLOCK).ok()?;
        inotify
            .add_watch(
                wcs2osstring(dirname).as_os_str(),
                AddWatchFlags::IN_MODIFY | AddWatchFlags::IN_MOVED_TO,
            )
            .ok()?;
        Some(InotifyNotifier {
            inotify,
            basename: wcs2osstring(basename),
        })
    }
}

impl UniversalNotifier for InotifyNotifier {
    // Do nothing to trigger a notification.
    // The notifications are generated from changes to the file itself.
    fn post_notification(&self) {}

    // Returns the fd from which to watch for events.
    fn notification_fd(&self) -> Option<RawFd> {
        Some(self.inotify.as_fd().as_raw_fd())
    }

    // The notification_fd is readable; drain it. Returns true if a notification is considered to
    // have been posted.
    fn notification_fd_became_readable(&self, fd: RawFd) -> bool {
        assert_eq!(fd, self.inotify.as_fd().as_raw_fd(), "unexpected fd");
        let Ok(evts) = self.inotify.read_events() else {
            return false;
        };
        evts.iter()
            .any(|evt| evt.name.as_ref() == Some(&self.basename))
    }
}

#[cfg(test)]
mod tests {
    use super::InotifyNotifier;
    use crate::universal_notifier::{UniversalNotifier, test_helpers::test_notifiers};
    use fish_wchar::WString;

    #[test]
    fn test_inotify_notifiers() {
        let temp_dir = fish_tempfile::new_dir().unwrap();
        let fake_uvars_path =
            WString::from(temp_dir.path().join("fish_variables").to_str().unwrap());

        let mut notifiers = Vec::new();
        for _ in 0..16 {
            notifiers.push(
                InotifyNotifier::new_at(&fake_uvars_path).expect("failed to create notifier"),
            );
        }
        let notifiers = notifiers
            .iter()
            .map(|n| n as &dyn UniversalNotifier)
            .collect::<Vec<_>>();
        test_notifiers(&notifiers, Some(&fake_uvars_path));
    }
}
