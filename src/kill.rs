//! The killring.
//!
//! Works like the killring in emacs and readline. The killring is cut and paste with a memory of
//! previous cuts.

use std::collections::VecDeque;
use std::sync::{LazyLock, Mutex};

use crate::prelude::*;

struct KillRing(VecDeque<WString>);

static KILL_RING: LazyLock<Mutex<KillRing>> = LazyLock::new(|| Mutex::new(KillRing::new()));

impl KillRing {
    /// Create a new killring.
    fn new() -> Self {
        Self(VecDeque::new())
    }

    /// Return whether this killring is empty.
    #[cfg(test)]
    fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    /// Add a string to the top of the killring.
    fn add(&mut self, new_entry: WString) {
        if !new_entry.is_empty() {
            self.0.push_front(new_entry);
        }
    }

    /// Replace the specified string in the killring.
    pub fn replace(&mut self, old_entry: &wstr, new_entry: WString) {
        if let Some(old_entry_idx) = self.0.iter().position(|entry| entry == old_entry) {
            self.0.remove(old_entry_idx);
        }
        if !new_entry.is_empty() {
            self.add(new_entry);
        }
    }

    /// Paste from the killring.
    pub fn yank(&mut self) -> WString {
        self.0.front().cloned().unwrap_or_default()
    }

    /// Rotate the killring.
    pub fn yank_rotate(&mut self) -> WString {
        self.0.rotate_left(1);
        self.yank()
    }

    /// Return a copy of the list of entries.
    pub fn entries(&self) -> Vec<WString> {
        self.0.iter().cloned().collect()
    }
}

/// Add a string to the top of the killring.
pub fn kill_add(new_entry: WString) {
    KILL_RING.lock().unwrap().add(new_entry);
}

/// Replace the specified string in the killring.
pub fn kill_replace(old_entry: &wstr, new_entry: WString) {
    KILL_RING.lock().unwrap().replace(old_entry, new_entry);
}

/// Rotate the killring.
pub fn kill_yank_rotate() -> WString {
    KILL_RING.lock().unwrap().yank_rotate()
}

/// Paste from the killring.
pub fn kill_yank() -> WString {
    KILL_RING.lock().unwrap().yank()
}

pub fn kill_entries() -> Vec<WString> {
    KILL_RING.lock().unwrap().entries()
}

#[cfg(test)]
mod tests {
    use super::KillRing;
    use crate::prelude::*;

    #[test]
    fn test_killring() {
        let mut kr = KillRing::new();

        assert!(kr.is_empty());

        kr.add(L!("a").to_owned());
        kr.add(L!("b").to_owned());
        kr.add(L!("c").to_owned());

        assert_eq!(kr.entries(), [L!("c"), L!("b"), L!("a")]);

        assert_eq!(kr.yank_rotate(), "b");
        assert_eq!(kr.entries(), [L!("b"), L!("a"), L!("c")]);

        assert_eq!(kr.yank_rotate(), "a");
        assert_eq!(kr.entries(), [L!("a"), L!("c"), L!("b")]);

        kr.add(L!("d").to_owned());

        assert_eq!(kr.entries(), [L!("d"), L!("a"), L!("c"), L!("b")]);

        assert_eq!(kr.yank_rotate(), "a");
        assert_eq!(kr.entries(), [L!("a"), L!("c"), L!("b"), L!("d")]);
    }
}
