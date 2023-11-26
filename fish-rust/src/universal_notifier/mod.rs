use once_cell::sync::OnceCell;
use std::os::fd::RawFd;

#[cfg(target_os = "macos")]
mod notifyd;

#[cfg(any(target_os = "android", target_os = "linux"))]
mod inotify;

/// The "universal notifier" is an object responsible for broadcasting and receiving universal
/// variable change notifications. These notifications do not contain the change, but merely
/// indicate that the uvar file has changed. It is up to the uvar subsystem to re-read the file.
///
/// Notifiers may provide a file descriptor to be watched for readability in
/// select().
///
/// To provide a file descriptor, the notifier overrides notification_fd() to return a non-negative
/// fd. This will be added to the "read" file descriptor list in select(). If the fd is readable,
/// notification_fd_became_readable() will be called; that function should be overridden to return
/// true if the file may have changed.
pub trait UniversalNotifier: Send + Sync {
    // Triggers a notification.
    fn post_notification(&self);

    // Returns the fd from which to watch for events.
    fn notification_fd(&self) -> Option<RawFd>;

    // The notification_fd is readable; drain it. Returns true if a notification is considered to
    // have been posted.
    fn notification_fd_became_readable(&self, fd: RawFd) -> bool;
}

/// A notifier which does nothing.
pub struct NullNotifier;

impl UniversalNotifier for NullNotifier {
    fn post_notification(&self) {}

    fn notification_fd(&self) -> Option<RawFd> {
        None
    }

    fn notification_fd_became_readable(&self, _fd: RawFd) -> bool {
        false
    }
}

/// Create a notifier.
pub fn create_notifier() -> Box<dyn UniversalNotifier> {
    #[cfg(target_os = "macos")]
    {
        if let Some(notifier) = notifyd::NotifydNotifier::new() {
            return Box::new(notifier);
        }
    }
    #[cfg(any(target_os = "android", target_os = "linux"))]
    {
        if let Some(notifier) = inotify::InotifyNotifier::new() {
            return Box::new(notifier);
        }
    }
    Box::new(NullNotifier)
}

// Default instance. Other instances are possible for testing.
static DEFAULT_NOTIFIER: OnceCell<Box<dyn UniversalNotifier>> = OnceCell::new();

pub fn default_notifier() -> &'static dyn UniversalNotifier {
    DEFAULT_NOTIFIER.get_or_init(create_notifier).as_ref()
}

#[cfg(test)]
use crate::wchar::prelude::*;

// Test a slice of notifiers.
// fish_variables_path is the path to the (simulated) fish_variables file.
#[cfg(test)]
pub fn test_notifiers(notifiers: &[&dyn UniversalNotifier], fish_variables_path: Option<&wstr>) {
    let poll_notifier = |n: &dyn UniversalNotifier| -> bool {
        let Some(fd) = n.notification_fd() else {
            return false;
        };
        if crate::fd_readable_set::poll_fd_readable(fd) {
            n.notification_fd_became_readable(fd)
        } else {
            false
        }
    };

    // Helper to simulate modifying a file, using the atomic rename() approach.
    let modify_path = |path: &wstr| -> Result<(), std::io::Error> {
        use crate::common::wcs2osstring;
        use std::fs;
        use std::io::Write;
        let path = wcs2osstring(path);
        let mut new_path = std::path::PathBuf::from(path.clone());
        new_path.set_extension("new");

        let mut file = fs::File::create(&new_path)?;
        file.write_all(b"Random text")?;
        std::mem::drop(file);
        fs::rename(new_path, path)?;
        Ok(())
    };

    // Nobody should poll yet.
    for (idx, &n) in notifiers.iter().enumerate() {
        assert!(
            !poll_notifier(n),
            "notifier {} polled before notification",
            idx
        );
    }

    // Tweak each notifier. Verify that others see it.
    for (idx1, &n1) in notifiers.iter().enumerate() {
        n1.post_notification();

        // If we're using inotify, simulate modifying the file.
        if let Some(path) = fish_variables_path {
            modify_path(path).expect("failed to modify file");
        }

        // notifyd requires a round trip to the notifyd server, which means we have to wait a
        // little bit to receive it. In practice 40 ms seems to be enough.
        unsafe { libc::usleep(40000) };

        for (idx2, &n2) in notifiers.iter().enumerate() {
            let mut polled = poll_notifier(n2);

            // We aren't concerned with the one who posted, except we do need to poll to drain it.
            if idx1 == idx2 {
                continue;
            }
            assert!(
                polled,
                "notifier {} did not see notification from {}",
                idx2, idx1
            );
            // It should not poll again immediately.
            polled = poll_notifier(n2);
            assert!(
                !polled,
                "notifier {} polled twice after notification from {}",
                idx2, idx1
            );
        }
    }

    // Nobody should poll now.
    for (idx, &n) in notifiers.iter().enumerate() {
        assert!(
            !poll_notifier(n),
            "notifier {} polled after all changes",
            idx
        );
    }
}
