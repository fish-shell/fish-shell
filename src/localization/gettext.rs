use fish_wchar::{L, WString, wstr};
use std::sync::{LazyLock, Mutex};

/// Use this function to localize a message.
/// The [`MaybeStatic`] wrapper type allows avoiding allocating and leaking a new [`wstr`] when no
/// localization is found and the input is returned, but as a static reference.
fn gettext(message: MaybeStatic) -> &'static wstr {
    use std::collections::HashMap;

    #[cfg(not(feature = "localize-messages"))]
    type NarrowMessage = ();
    #[cfg(feature = "localize-messages")]
    type NarrowMessage = &'static str;

    let message_wstr = match message {
        MaybeStatic::Static(s) => s,
        MaybeStatic::Local(s) => s,
    };
    static MESSAGE_TO_NARROW: LazyLock<Mutex<HashMap<&'static wstr, NarrowMessage>>> =
        LazyLock::new(|| Mutex::new(HashMap::default()));
    let mut message_to_narrow = MESSAGE_TO_NARROW.lock().unwrap();
    if !message_to_narrow.contains_key(message_wstr) {
        let message_wstr: &'static wstr = match message {
            MaybeStatic::Static(s) => s,
            MaybeStatic::Local(l) => wstr::from_char_slice(Box::leak(l.as_char_slice().into())),
        };
        #[cfg(not(feature = "localize-messages"))]
        let message_str = ();
        #[cfg(feature = "localize-messages")]
        let message_str = Box::leak(message_wstr.to_string().into_boxed_str());
        message_to_narrow.insert(message_wstr, message_str);
    }
    let (message_static_wstr, message_str) = message_to_narrow.get_key_value(message_wstr).unwrap();

    #[cfg(not(feature = "localize-messages"))]
    let () = message_str;
    #[cfg(feature = "localize-messages")]
    {
        if let Some(localized_str) = fish_gettext::gettext(message_str) {
            static LOCALIZATION_TO_WIDE: LazyLock<Mutex<HashMap<&'static str, &'static wstr>>> =
                LazyLock::new(|| Mutex::new(HashMap::default()));
            let mut localizations_to_wide = LOCALIZATION_TO_WIDE.lock().unwrap();
            if !localizations_to_wide.contains_key(localized_str) {
                let localization_wstr =
                    Box::leak(WString::from_str(localized_str).into_boxed_utfstr());
                localizations_to_wide.insert(localized_str, localization_wstr);
            }
            return localizations_to_wide.get(localized_str).unwrap();
        }
    }

    // No localization found.
    message_static_wstr
}

/// A type that can be either a static or local string.
enum MaybeStatic<'a> {
    Static(&'static wstr),
    Local(&'a wstr),
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
                    gettext(MaybeStatic::Static(s))
                }
            }
            Self::Owned(s) => {
                if s.is_empty() {
                    L!("")
                } else {
                    gettext(MaybeStatic::Local(s))
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
#[cfg(feature = "gettext-extract")]
macro_rules! localizable_string {
    ($string:literal) => {
        $crate::localization::LocalizableString::Static(widestring::utf32str!(
            fish_gettext_extraction::gettext_extract!($string)
        ))
    };
}
#[macro_export]
#[cfg(not(feature = "gettext-extract"))]
macro_rules! localizable_string {
    ($string:literal) => {
        $crate::localization::LocalizableString::Static(widestring::utf32str!($string))
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
            $vis const $name: $crate::localization::LocalizableString =
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tests::prelude::*;

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
}
