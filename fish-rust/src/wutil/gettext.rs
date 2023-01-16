use crate::ffi;
use crate::wchar::{wchar_t, wstr};
use crate::wchar_ffi::wcslen;

/// Support for wgettext.

/// Implementation detail for wgettext!.
pub fn wgettext_impl_do_not_use_directly(text: &[wchar_t]) -> &'static wstr {
    assert!(
        text.len() > 0 && text[text.len() - 1] == 0,
        "should be nul-terminated"
    );
    let res: *const wchar_t = ffi::wgettext_ptr(text.as_ptr());
    let slice = unsafe { std::slice::from_raw_parts(res as *const u32, wcslen(res)) };
    wstr::from_slice(slice).expect("Invalid UTF-32")
}

/// Get a (possibly translated) string from a string literal.
/// This returns a &'static wstr.
#[allow(unused_macros)]
macro_rules! wgettext {
    ($string:literal) => {
        crate::wutil::gettext::wgettext_impl_do_not_use_directly(
            crate::wchar_ffi::u32cstr!($string).as_slice_with_nul(),
        )
    };
}

/// Like wgettext, but applies a sprintf format string.
/// The result is a WString.
macro_rules! wgettext_fmt {
    (
    $string:literal, // format string
    $($args:expr),*, // list of expressions
    $(,)?   // optional trailing comma
    ) => {
        crate::wutil::sprintf!(&crate::wutil::wgettext!($string), $($args),*)
    };
}
pub(crate) use wgettext_fmt;

use crate::ffi_tests::add_test;
add_test!("test_untranslated", || {
    let s: &'static wstr = wgettext!("abc");
    assert_eq!(s, "abc");
    let s2: &'static wstr = wgettext!("static");
    assert_eq!(s2, "static");
});
