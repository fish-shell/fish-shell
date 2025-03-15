use std::collections::HashMap;
use std::ffi::CString;
use std::sync::Mutex;

use crate::common::{charptr2wcstring, truncate_at_nul, wcs2zstring, PACKAGE_NAME};
use crate::env::CONFIG_PATHS;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use errno::{errno, set_errno};
use once_cell::sync::{Lazy, OnceCell};

#[cfg(gettext)]
mod internal {
    use libc::c_char;
    use std::ffi::CStr;
    extern "C" {
        fn gettext(msgid: *const c_char) -> *mut c_char;
        fn bindtextdomain(domainname: *const c_char, dirname: *const c_char) -> *mut c_char;
        fn textdomain(domainname: *const c_char) -> *mut c_char;
    }
    pub fn fish_gettext(msgid: &CStr) -> *const c_char {
        unsafe { gettext(msgid.as_ptr()) }
    }
    pub fn fish_bindtextdomain(domainname: &CStr, dirname: &CStr) -> *mut c_char {
        unsafe { bindtextdomain(domainname.as_ptr(), dirname.as_ptr()) }
    }
    pub fn fish_textdomain(domainname: &CStr) -> *mut c_char {
        unsafe { textdomain(domainname.as_ptr()) }
    }
}
#[cfg(not(gettext))]
mod internal {
    use libc::c_char;
    use std::ffi::CStr;
    pub fn fish_gettext(msgid: &CStr) -> *const c_char {
        msgid.as_ptr()
    }
    pub fn fish_bindtextdomain(_domainname: &CStr, _dirname: &CStr) -> *mut c_char {
        std::ptr::null_mut()
    }
    pub fn fish_textdomain(_domainname: &CStr) -> *mut c_char {
        std::ptr::null_mut()
    }
}

use internal::*;

// Really init wgettext.
fn wgettext_really_init() {
    let package_name = CString::new(PACKAGE_NAME).unwrap();
    // This contains `datadir`; which when replaced to make the binary relocatable,
    // causes null bytes at the end of the string.
    let localedir = CString::new(CONFIG_PATHS.locale.display().to_string()).unwrap();
    fish_bindtextdomain(&package_name, &localedir);
    fish_textdomain(&package_name);
}

fn wgettext_init_if_necessary() {
    static INIT: OnceCell<()> = OnceCell::new();
    INIT.get_or_init(wgettext_really_init);
}

/// A type that can be either a static or local string.
enum MaybeStatic<'a> {
    Static(&'static wstr),
    Local(&'a wstr),
}

/// Implementation detail for wgettext!.
/// Wide character wrapper around the gettext function. For historic reasons, unlike the real
/// gettext function, wgettext takes care of setting the correct domain, etc. using the textdomain
/// and bindtextdomain functions. This should probably be moved out of wgettext, so that wgettext
/// will be nothing more than a wrapper around gettext, like all other functions in this file.
fn wgettext_impl(text: MaybeStatic) -> &'static wstr {
    // Preserve errno across this since this is often used in printing error messages.
    let err = errno();

    wgettext_init_if_necessary();

    let key = match text {
        MaybeStatic::Static(s) => s,
        MaybeStatic::Local(s) => s,
    };

    debug_assert!(
        truncate_at_nul(key).len() == key.len(),
        "key should not contain NUL"
    );

    // Note that because entries are immortal, we simply leak non-static keys, and all values.
    static WGETTEXT_MAP: Lazy<Mutex<HashMap<&'static wstr, &'static wstr>>> =
        Lazy::new(|| Mutex::new(HashMap::new()));
    let mut wmap = WGETTEXT_MAP.lock().unwrap();
    let res = match wmap.get(key) {
        Some(v) => *v,
        None => {
            let mbs_in = wcs2zstring(key);
            let out = fish_gettext(&mbs_in);
            let out = charptr2wcstring(out);
            // Leak the value into the heap.
            let value: &'static wstr = Box::leak(out.into_boxed_utfstr());

            // Get a static key, perhaps leaking it into the heap as well.
            let key: &'static wstr = match text {
                MaybeStatic::Static(s) => s,
                MaybeStatic::Local(s) => wstr::from_char_slice(Box::leak(s.as_char_slice().into())),
            };

            wmap.insert(key, value);
            value
        }
    };

    set_errno(err);

    res
}

/// Get a (possibly translated) string from a literal.
/// Note this assumes that the string does not contain interior NUL characters -
/// this is checked in debug mode.
pub fn wgettext_static_str(s: &'static wstr) -> &'static wstr {
    wgettext_impl(MaybeStatic::Static(s))
}

/// Get a (possibly translated) string from a non-literal.
/// This truncates at the first NUL character.
pub fn wgettext_str(s: &wstr) -> &'static wstr {
    wgettext_impl(MaybeStatic::Local(truncate_at_nul(s)))
}

/// Get a (possibly translated) string from a string literal.
/// This returns a &'static wstr.
#[macro_export]
macro_rules! wgettext {
    ($string:expr) => {
        $crate::wutil::gettext::wgettext_static_str(widestring::utf32str!($string))
    };
}
pub use wgettext;

/// Like wgettext, but applies a sprintf format string.
/// The result is a WString.
#[macro_export]
macro_rules! wgettext_fmt {
    (
    $string:expr, // format string
    $($args:expr),+ // list of expressions
    $(,)?   // optional trailing comma
    ) => {
        $crate::wutil::sprintf!($crate::wutil::wgettext!($string), $($args),+)
    };
}
pub use wgettext_fmt;

/// Like wgettext_fmt, but doesn't require an argument to format.
/// For use in macros.
#[macro_export]
macro_rules! wgettext_maybe_fmt {
    (
    $string:expr // format string
    $(, $args:expr)* // list of expressions
    $(,)?   // optional trailing comma
    ) => {
        $crate::wutil::sprintf!($crate::wutil::wgettext!($string), $($args),*)
    };
}
pub use wgettext_maybe_fmt;

#[test]
#[serial]
fn test_untranslated() {
    let _cleanup = test_init();
    let s: &'static wstr = wgettext!("abc");
    assert_eq!(s, "abc");
    let s2: &'static wstr = wgettext!("static");
    assert_eq!(s2, "static");
}
