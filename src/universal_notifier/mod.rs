use once_cell::sync::OnceCell;
use std::os::fd::RawFd;

#[cfg(apple)]
mod notifyd;

#[cfg(any(target_os = "android", target_os = "linux"))]
mod inotify;

#[cfg(bsd)]
mod kqueue;

#[cfg(test)]
mod test_helpers;

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
    #[cfg(apple)]
    if let Some(notifier) = notifyd::NotifydNotifier::new() {
        return Box::new(notifier);
    }
    #[cfg(any(target_os = "android", target_os = "linux"))]
    if let Some(notifier) = inotify::InotifyNotifier::new() {
        return Box::new(notifier);
    }
    #[cfg(bsd)]
    if let Some(notifier) = kqueue::KqueueNotifier::new() {
        return Box::new(notifier);
    }
    Box::new(NullNotifier)
}

// Default instance. Other instances are possible for testing.
static DEFAULT_NOTIFIER: OnceCell<Box<dyn UniversalNotifier>> = OnceCell::new();

pub fn default_notifier() -> &'static dyn UniversalNotifier {
    DEFAULT_NOTIFIER.get_or_init(create_notifier).as_ref()
}
