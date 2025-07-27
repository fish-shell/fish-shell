use std::collections::HashSet;
use std::ffi::CString;
use std::os::unix::ffi::OsStrExt;
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
    let localedir = CString::new(localepath.as_os_str().as_bytes()).unwrap();
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
fn wgettext_impl(key: &wstr) -> &'static wstr {
    // Preserve errno across this since this is often used in printing error messages.
    let err = errno();

    wgettext_init_if_necessary();

    debug_assert!(!key.contains('\0'), "key should not contain NUL");

    // If a translation exists, gettext will return a static reference to it.
    // Otherwise, a pointer to the input will be returned.
    // Because the input is not necessarily static, we need an allocation here.
    let localized_message = charptr2wcstring(fish_gettext(&wcs2zstring(key)));

    // Stores the results of the gettext calls to prevent leaking the same string repeatedly.
    static MESSAGE_CACHE: Lazy<Mutex<HashSet<&'static wstr>>> =
        Lazy::new(|| Mutex::new(HashSet::new()));
    let mut message_cache = MESSAGE_CACHE.lock().unwrap();

    let static_localized_message =
        if let Some(cached_message) = message_cache.get(localized_message.as_utfstr()) {
            cached_message
        } else {
            // Leak the value into the heap.
            let static_localized_message: &'static wstr =
                wstr::from_char_slice(Box::leak(localized_message.as_char_slice().into()));
            message_cache.insert(static_localized_message);
            static_localized_message
        };

    set_errno(err);

    static_localized_message
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
                    wgettext_impl(s)
                }
            }
            Self::Owned(s) => {
                if s.is_empty() {
                    L!("")
                } else {
                    wgettext_impl(s)
                }
            }
        }
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
            $(#[$attr])*
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
    let abc_str = LocalizableString::from_external_source(WString::from("abc"));
    let s: &'static wstr = wgettext!(abc_str);
    assert_eq!(s, "abc");
    let static_str = LocalizableString::from_external_source(WString::from("static"));
    let s2: &'static wstr = wgettext!(static_str);
    assert_eq!(s2, "static");
}
