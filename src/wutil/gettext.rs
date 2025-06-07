use std::collections::HashMap;
use std::ffi::CString;
use std::sync::Mutex;

use crate::common::{charptr2wcstring, wcs2zstring, PACKAGE_NAME};
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
    let Some(ref localepath) = CONFIG_PATHS.locale else {
        return;
    };
    let package_name = CString::new(PACKAGE_NAME).unwrap();
    // This contains `datadir`; which when replaced to make the binary relocatable,
    // causes null bytes at the end of the string.
    let localedir = CString::new(localepath.display().to_string()).unwrap();
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

    debug_assert!(!key.contains('\0'), "key should not contain NUL");

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

/// A string which can be localized.
/// The wrapped string itself is the original, unlocalized version.
/// Use [`LocalizableString::localize`] to obtain the localized version.
///
/// Do not construct this type directly.
/// For string literals defined in fish's Rust sources,
/// use the macros defined in this file.
/// For strings defined elsewhere, use [`LocalizableString::from_external_source`].
/// Use this function with caution. If the string is not extracted into the gettext PO files from
/// which fish obtains localizations, localization will not work.
#[derive(Debug, Clone)]
pub enum LocalizableString {
    Static(&'static wstr),
    Owned(WString),
}

impl LocalizableString {
    /// Create a [`LocalizableString`] from a string which is not from fish's own Rust sources.
    /// Localizations will only work if this string is extracted into the localization files some
    /// other way.
    pub fn from_external_source(s: WString) -> Self {
        Self::Owned(s)
    }

    /// Get the localization of a [`LocalizableString`].
    /// If original string is empty, an empty `wstr` is returned,
    /// instead of the gettext metadata.
    pub fn localize(&self) -> &'static wstr {
        match self {
            Self::Static(s) => {
                if s.is_empty() {
                    L!("")
                } else {
                    wgettext_impl(MaybeStatic::Static(s))
                }
            }
            Self::Owned(s) => {
                if s.is_empty() {
                    L!("")
                } else {
                    wgettext_impl(MaybeStatic::Local(s))
                }
            }
        }
    }
}

impl std::ops::Deref for LocalizableString {
    type Target = wstr;
    fn deref(&self) -> &Self::Target {
        self.localize()
    }
}

impl AsRef<wstr> for LocalizableString {
    fn as_ref(&self) -> &wstr {
        self
    }
}
impl std::fmt::Display for LocalizableString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.localize())
    }
}

/// This macro takes a string literal and produces a [`LocalizableString`].
/// The essential part is the invocation of the proc macro,
/// which ensures that the string gets extracted for localization.
#[macro_export]
macro_rules! localizable_string {
    ($string:literal) => {
        $crate::wutil::gettext::LocalizableString::Static(widestring::utf32str!(
            fish_gettext_extraction::gettext_extract!($string)
        ))
    };
}
pub use localizable_string;

/// Macro for declaring string consts which should be localized.
#[macro_export]
macro_rules! localizable_consts {
    (
        $(
            $(#[$attr:meta])*
            $vis:vis
            $name:ident
            $string:literal
        )*
    ) => {
        $(
            $vis const $name: $crate::wutil::gettext::LocalizableString =
                localizable_string!($string);
        )*
    };
}
pub use localizable_consts;

/// Takes a string literal of a [`LocalizableString`].
/// Given a string literal, it is extracted for localization.
/// Returns a possibly localized `&'static wstr`.
#[macro_export]
macro_rules! wgettext {
    (
        $string:literal
    ) => {
        localizable_string!($string).localize()
    };
    (
        $string:expr // format string (LocalizableString)
    ) => {
        $string.localize()
    };
}
pub use wgettext;

/// Like wgettext, but applies a sprintf format string.
/// The result is a WString.
#[macro_export]
macro_rules! wgettext_fmt {
    (
        $string:literal // format string
        $(, $args:expr)* // list of expressions
        $(,)?   // optional trailing comma
    ) => {
        $crate::wutil::sprintf!(
            localizable_string!($string).localize(),
            $($args),*
        )
    };
    (
        $string:expr // format string (LocalizableString)
        $(, $args:expr)* // list of expressions
        $(,)?   // optional trailing comma
    ) => {
        $crate::wutil::sprintf!($string.localize(), $($args),*)
    };
}
pub use wgettext_fmt;

#[test]
#[serial]
fn test_unlocalized() {
    let _cleanup = test_init();
    let s: &'static wstr = wgettext!("abc");
    assert_eq!(s, "abc");
    let s2: &'static wstr = wgettext!("static");
    assert_eq!(s2, "static");
}
