//! The killring.
//!
//! Works like the killring in emacs and readline. The killring is cut and paste with a memory of
//! previous cuts.

use cxx::CxxWString;
use std::collections::VecDeque;
use std::pin::Pin;
use std::sync::Mutex;

use crate::ffi::wcstring_list_ffi_t;
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharFromFFI;

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
        fn kill_yank_rotate_ffi(mut out_front: Pin<&mut CxxWString>);
        #[cxx_name = "kill_yank"]
        fn kill_yank_ffi(mut out_front: Pin<&mut CxxWString>);
        #[cxx_name = "kill_entries"]
        fn kill_entries_ffi(mut out: Pin<&mut wcstring_list_ffi_t>);
    }
}

static KILL_LIST: once_cell::sync::Lazy<Mutex<VecDeque<WString>>> =
    once_cell::sync::Lazy::new(|| Mutex::new(VecDeque::new()));

fn kill_add_ffi(new_entry: &CxxWString) {
    kill_add(new_entry.from_ffi());
}

/// Add a string to the top of the killring.
pub fn kill_add(new_entry: WString) {
    if !new_entry.is_empty() {
        KILL_LIST.lock().unwrap().push_front(new_entry);
    }
}

fn kill_replace_ffi(old_entry: &CxxWString, new_entry: &CxxWString) {
    kill_replace(old_entry.from_ffi(), new_entry.from_ffi())
}

/// Replace the specified string in the killring.
pub fn kill_replace(old_entry: WString, new_entry: WString) {
    let mut kill_list = KILL_LIST.lock().unwrap();
    if let Some(old_entry_idx) = kill_list.iter().position(|entry| entry == &old_entry) {
        kill_list.remove(old_entry_idx);
    }
    if !new_entry.is_empty() {
        kill_list.push_front(new_entry);
    }
}

fn kill_yank_rotate_ffi(mut out_front: Pin<&mut CxxWString>) {
    out_front.as_mut().clear();
    out_front
        .as_mut()
        .push_chars(kill_yank_rotate().as_char_slice());
}

/// Rotate the killring.
pub fn kill_yank_rotate() -> WString {
    let mut kill_list = KILL_LIST.lock().unwrap();
    kill_list.rotate_left(1);
    kill_list.front().cloned().unwrap_or_default()
}

fn kill_yank_ffi(mut out_front: Pin<&mut CxxWString>) {
    out_front.as_mut().clear();
    out_front.as_mut().push_chars(kill_yank().as_char_slice());
}

/// Paste from the killring.
pub fn kill_yank() -> WString {
    let kill_list = KILL_LIST.lock().unwrap();
    kill_list.front().cloned().unwrap_or_default()
}

fn kill_entries_ffi(mut out_entries: Pin<&mut wcstring_list_ffi_t>) {
    out_entries.as_mut().clear();
    for kill_entry in KILL_LIST.lock().unwrap().iter() {
        out_entries.as_mut().push(kill_entry);
    }
}

pub fn kill_entries() -> Vec<WString> {
    KILL_LIST.lock().unwrap().iter().cloned().collect()
}
