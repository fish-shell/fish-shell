//! The killring.
//!
//! Works like the killring in emacs and readline. The killring is cut and paste with a memory of
//! previous cuts.

use cxx::{CxxWString, UniquePtr};
use once_cell::sync::Lazy;
use std::collections::VecDeque;
use std::sync::Mutex;

use crate::ffi::wcstring_list_ffi_t;
use crate::wchar::prelude::*;
use crate::wchar_ffi::{AsWstr, WCharFromFFI, WCharToFFI};

#[cxx::bridge]
mod kill_ffi {
    extern "C++" {
        include!("wutil.h");
        type wcstring_list_ffi_t = super::wcstring_list_ffi_t;
    }

    extern "Rust" {
        #[cxx_name = "kill_add"]
        fn kill_add_ffi(new_entry: &CxxWString);
        #[cxx_name = "kill_replace"]
        fn kill_replace_ffi(old_entry: &CxxWString, new_entry: &CxxWString);
        #[cxx_name = "kill_yank_rotate"]
        fn kill_yank_rotate_ffi() -> UniquePtr<CxxWString>;
        #[cxx_name = "kill_yank"]
        fn kill_yank_ffi() -> UniquePtr<CxxWString>;
    }
}

struct KillRing(VecDeque<WString>);

static KILL_RING: Lazy<Mutex<KillRing>> = Lazy::new(|| Mutex::new(KillRing::new()));

impl KillRing {
    /// Create a new killring.
    fn new() -> Self {
        Self(VecDeque::new())
    }

    /// Return whether this killring is empty.
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
    KILL_RING.lock().unwrap().add(new_entry)
}

/// Replace the specified string in the killring.
pub fn kill_replace(old_entry: &wstr, new_entry: WString) {
    KILL_RING.lock().unwrap().replace(old_entry, new_entry)
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

fn kill_add_ffi(new_entry: &CxxWString) {
    kill_add(new_entry.from_ffi());
}

fn kill_replace_ffi(old_entry: &CxxWString, new_entry: &CxxWString) {
    kill_replace(old_entry.as_wstr(), new_entry.from_ffi())
}

fn kill_yank_ffi() -> UniquePtr<CxxWString> {
    kill_yank().to_ffi()
}

fn kill_yank_rotate_ffi() -> UniquePtr<CxxWString> {
    kill_yank_rotate().to_ffi()
}

#[cfg(test)]
fn test_killring() {
    let mut kr = KillRing::new();

    assert!(kr.is_empty());

    kr.add(WString::from_str("a"));
    kr.add(WString::from_str("b"));
    kr.add(WString::from_str("c"));

    assert!((kr.entries() == [L!("c"), L!("b"), L!("a")]));

    assert!(kill_yank_rotate() == L!("b"));
    assert!((kr.entries() == [L!("b"), L!("a"), L!("c")]));

    assert!(kill_yank_rotate() == L!("a"));
    assert!((kr.entries() == [L!("a"), L!("c"), L!("b")]));

    kr.add(WString::from_str("d"));

    assert!((kr.entries() == [L!("d"), L!("a"), L!("c"), L!("b")]));

    assert!(kr.yank_rotate() == L!("a"));
    assert!((kr.entries() == [L!("a"), L!("c"), L!("b"), L!("d")]));
}
