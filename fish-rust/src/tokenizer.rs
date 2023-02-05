//! A specialized tokenizer for tokenizing the fish language. In the future, the tokenizer should be
//! extended to support marks, tokenizing multiple strings and disposing of unused string segments.
use crate::ffi::{valid_var_name_char, wchar_t};
use crate::wchar::wstr;
use crate::wchar_ffi::WCharFromFFI;
use cxx::{CxxWString, SharedPtr};

#[cxx::bridge]
mod tokenizer_ffi {
    extern "Rust" {
        #[cxx_name = "variable_assignment_equals_pos"]
        fn variable_assignment_equals_pos_ffi(txt: &CxxWString) -> SharedPtr<usize>;
    }
}

/// The position of the equal sign in a variable assignment like foo=bar.
///
/// Return the location of the equals sign, or none if the string does
/// not look like a variable assignment like FOO=bar.  The detection
/// works similar as in some POSIX shells: only letters and numbers qre
/// allowed on the left hand side, no quotes or escaping.
pub fn variable_assignment_equals_pos(txt: &wstr) -> Option<usize> {
    let mut found_potential_variable = false;

    // TODO bracket indexing
    for (i, c) in txt.chars().enumerate() {
        if !found_potential_variable {
            if !valid_var_name_char(c as wchar_t) {
                return None;
            }
            found_potential_variable = true;
        } else {
            if c == '=' {
                return Some(i);
            }
            if !valid_var_name_char(c as wchar_t) {
                return None;
            }
        }
    }
    None
}

fn variable_assignment_equals_pos_ffi(txt: &CxxWString) -> SharedPtr<usize> {
    match variable_assignment_equals_pos(&txt.from_ffi()) {
        Some(p) => SharedPtr::new(p),
        None => SharedPtr::null(),
    }
}
