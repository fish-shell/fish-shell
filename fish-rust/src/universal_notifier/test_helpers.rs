use super::UniversalNotifier;
use crate::wchar::prelude::*;

// Helper to test a slice of notifiers.
// fish_variables_path is the path to the (simulated) fish_variables file, if using the inotify notifier.
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
