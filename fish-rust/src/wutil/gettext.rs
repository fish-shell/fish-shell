use crate::ffi;
use crate::wchar::wstr;
use crate::wchar_ffi::{wchar_t, wcslen};
use widestring::U32CString;

/// Support for wgettext.

/// Implementation detail for wgettext!.
pub fn wgettext_impl_do_not_use_directly(text: &[wchar_t]) -> &'static wstr {
    assert_eq!(text.last(), Some(&0), "should be nul-terminated");
    let res: *const wchar_t = ffi::wgettext_ptr(text.as_ptr());
    #[allow(clippy::unnecessary_cast)]
    let slice = unsafe { std::slice::from_raw_parts(res as *const u32, wcslen(res)) };
    wstr::from_slice(slice).expect("Invalid UTF-32")
}

/// Get a (possibly translated) string from a non-literal.
pub fn wgettext_str(s: &wstr) -> &'static wstr {
    let cstr: U32CString = U32CString::from_chars_truncate(s.as_char_slice());
    wgettext_impl_do_not_use_directly(cstr.as_slice_with_nul())
}

/// Get a (possibly translated) string from a string literal.
/// This returns a &'static wstr.
macro_rules! wgettext {
    ($string:expr) => {
        crate::wutil::gettext::wgettext_impl_do_not_use_directly(
            crate::wchar_ffi::u32cstr!($string).as_slice_with_nul(),
        )
    };
}
pub(crate) use wgettext;

/// Like wgettext, but for non-literals.
macro_rules! wgettext_expr {
    ($string:expr) => {
        crate::wutil::gettext::wgettext_impl_do_not_use_directly(
            widestring::U32CString::from_ustr_truncate($string).as_slice_with_nul(),
        )
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

use crate::ffi_tests::add_test;
add_test!("test_untranslated", || {
    let s: &'static wstr = wgettext!("abc");
    assert_eq!(s, "abc");
    let s2: &'static wstr = wgettext!("static");
    assert_eq!(s2, "static");
});
