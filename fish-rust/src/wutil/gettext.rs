use std::borrow::Cow;
use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::ffi::CString;
use std::pin::Pin;
use std::sync::Mutex;

use crate::common::{charptr2wcstring, wcs2zstring};
use crate::fish::PACKAGE_NAME;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wchar_ffi::wchar_t;
use errno::{errno, set_errno};
use once_cell::sync::{Lazy, OnceCell};
use widestring::U32CString;

#[cfg(feature = "gettext")]
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
#[cfg(not(feature = "gettext"))]
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
    let localedir = CString::new(option_env!("LOCALEDIR").unwrap_or("UNDEFINED")).unwrap();
    fish_bindtextdomain(&package_name, &localedir);
    fish_textdomain(&package_name);
}

fn wgettext_init_if_necessary() {
    static INIT: OnceCell<()> = OnceCell::new();
    INIT.get_or_init(wgettext_really_init);
}

/// Implementation detail for wgettext!.
/// Wide character wrapper around the gettext function. For historic reasons, unlike the real
/// gettext function, wgettext takes care of setting the correct domain, etc. using the textdomain
/// and bindtextdomain functions. This should probably be moved out of wgettext, so that wgettext
/// will be nothing more than a wrapper around gettext, like all other functions in this file.
pub fn wgettext_impl_do_not_use_directly(text: Cow<'static, [wchar_t]>) -> &'static wstr {
    assert_eq!(text.last(), Some(&0), "should be nul-terminated");
    // Preserve errno across this since this is often used in printing error messages.
    let err = errno();

    wgettext_init_if_necessary();
    #[allow(clippy::type_complexity)]
    static WGETTEXT_MAP: Lazy<Mutex<HashMap<Cow<'static, [wchar_t]>, Pin<Box<WString>>>>> =
        Lazy::new(|| Mutex::new(HashMap::new()));
    let mut wmap = WGETTEXT_MAP.lock().unwrap();
    let v = match wmap.entry(text) {
        Entry::Occupied(v) => Pin::get_ref(Pin::as_ref(v.get())) as *const WString,
        Entry::Vacant(v) => {
            let key = wstr::from_slice(v.key()).unwrap();
            let mbs_in = wcs2zstring(key);
            let out = fish_gettext(&mbs_in);
            let out = charptr2wcstring(out);
            let res = Pin::new(Box::new(out));
            let value = v.insert(res);
            Pin::get_ref(Pin::as_ref(value)) as *const WString
        }
    };

    set_errno(err);

    // The returned string is stored in the map.
    // TODO: If we want to shrink the map, this would be a problem.
    unsafe { v.as_ref().unwrap() }.as_utfstr()
}

/// Get a (possibly translated) string from a non-literal.
pub fn wgettext_str(s: &wstr) -> &'static wstr {
    let cstr: U32CString = U32CString::from_chars_truncate(s.as_char_slice());
    wgettext_impl_do_not_use_directly(Cow::Owned(cstr.into_vec_with_nul()))
}

/// Get a (possibly translated) string from a string literal.
/// This returns a &'static wstr.
macro_rules! wgettext {
    ($string:expr) => {
        crate::wutil::gettext::wgettext_impl_do_not_use_directly(std::borrow::Cow::Borrowed(
            widestring::u32cstr!($string).as_slice_with_nul(),
        ))
    };
}
pub(crate) use wgettext;

/// Like wgettext, but for non-literals.
macro_rules! wgettext_expr {
    ($string:expr) => {
        crate::wutil::gettext::wgettext_impl_do_not_use_directly(std::borrow::Cow::Owned(
            widestring::U32CString::from_ustr_truncate($string).into_vec_with_nul(),
        ))
    };
}
pub(crate) use wgettext_expr;

/// Like wgettext, but applies a sprintf format string.
/// The result is a WString.
macro_rules! wgettext_fmt {
    (
    $string:expr, // format string
    $($args:expr),+ // list of expressions
    $(,)?   // optional trailing comma
    ) => {
        crate::wutil::sprintf!(&crate::wutil::wgettext!($string), $($args),+)
    };
}
pub(crate) use wgettext_fmt;

/// Like wgettext_fmt, but doesn't require an argument to format.
/// For use in macros.
macro_rules! wgettext_maybe_fmt {
    (
    $string:expr // format string
    $(, $args:expr)* // list of expressions
    $(,)?   // optional trailing comma
    ) => {
        crate::wutil::sprintf!(&crate::wutil::wgettext!($string), $($args),*)
    };
}
pub(crate) use wgettext_maybe_fmt;

#[test]
#[serial]
fn test_untranslated() {
    test_init();
    let s: &'static wstr = wgettext!("abc");
    assert_eq!(s, "abc");
    let s2: &'static wstr = wgettext!("static");
    assert_eq!(s2, "static");
}
