use std::collections::HashSet;
use std::sync::Mutex;

#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use once_cell::sync::Lazy;

#[cfg(feature = "localize")]
mod gettext_impl {
    use std::sync::Mutex;

    use once_cell::sync::Lazy;

    include!(concat!(env!("OUT_DIR"), "/localization_maps.rs"));

    /// `language` must be an ISO 639 language code, optionally followed by an underscore and an ISO
    /// 3166 country/territory code.
    /// The return value will either be `Some(language)`, if `language` has localizations,
    /// or `Some(language_variant)` if there are no localizations for `language` but for a variant of
    /// `language`.
    /// `None` will be returned if no variant of the language has localizations.
    fn find_existing_language_name(language: &str) -> Option<String> {
        // Try the exact name first.
        // If there already is a corresponding catalog return the language.
        if CATALOGS.contains_key(language) {
            return Some(language.to_owned());
        }

        let language_without_country_code: String =
            language.chars().take_while(|&c| c != '_').collect();

        // Use the first file whose name starts with the same language code.
        for &lang_name in CATALOGS.keys() {
            if lang_name.starts_with(&language_without_country_code) {
                return Some(lang_name.to_owned());
            }
        }

        // No localizations for the language (and any regional variations) exist.
        None
    }

    /// The precedence list of user-preferred languages, obtained from the relevant environment
    /// variables.
    /// This should be updated when the relevant variables change.
    pub(super) static LANGUAGE_PRECEDENCE: Lazy<Mutex<Vec<String>>> =
        Lazy::new(|| Mutex::new(Vec::new()));

    /// Four environment variables can be used to select languages.
    /// A detailed description is available at
    /// <https://www.gnu.org/software/gettext/manual/html_node/Setting-the-POSIX-Locale.html>
    /// Our does not replicate the behavior exactly.
    /// See the following description.
    ///
    /// The first variable in the following list that is set will be used to determine the language
    /// preferences and the other variables will be ignored, regardless of localization availability.
    /// 1. `LANGUAGE`: This is the only variable which can specify a list of languages. The entries
    ///    of the list are colon-separated. Each entry identifies a language, with earlier languages
    ///    having higher precedence. A language is specified by a 2 or 3 letter ISO 639 language
    ///    code, optionally followed by an underscore and an ISO 3166 country/territory code.
    ///    If the second part is omitted, some variant of the language will be used if localizations
    ///    exist for one. We make no guarantees about which variant that will be.
    /// 2. `LC_ALL`: a single locale name as described in
    ///    <https://www.gnu.org/software/gettext/manual/html_node/Locale-Names.html>
    /// 3. `LC_MESSAGES`: like `LC_ALL`
    /// 4. `LANG`: like `LC_ALL`
    ///
    /// Returns the (possibly empty) preference list of languages.
    fn get_language_preferences_from_env() -> Vec<String> {
        fn normalize_locale_name(locale: &str) -> String {
            // Strips off the encoding and modifier parts.
            let mut normalized_name = String::new();
            // Strip off encoding and modifier. (We always expect UTF-8 and don't support modifiers.)
            for c in locale.chars() {
                if c.is_alphabetic() || c == '_' {
                    normalized_name.push(c);
                } else {
                    break;
                }
            }
            // At this point, the normalized_name should have the shape `ll` or `ll_CC`.
            normalized_name
        }

        if let Ok(langs) = std::env::var("LANGUAGE") {
            return langs.split(':').map(normalize_locale_name).collect();
        }
        if let Ok(locale) = std::env::var("LC_ALL")
            .or_else(|_| std::env::var("LC_MESSAGES").or_else(|_| std::env::var("LANG")))
        {
            if locale.starts_with('C') {
                // Do not localize in the C locale.
                vec![]
            } else {
                vec![normalize_locale_name(&locale)]
            }
        } else {
            vec![]
        }
    }

    /// Implementation of the function with the same name in super.
    pub(super) fn update_locale_from_env() {
        let mut language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();
        *language_precedence = get_language_preferences_from_env()
            .iter()
            .filter_map(|lang| find_existing_language_name(lang))
            .collect();
    }
}

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
#[cfg(feature = "localize")]
pub fn update_locale_from_env() {
    gettext_impl::update_locale_from_env();
}

/// Use this function to localize a message.
/// The [`MaybeStatic`] wrapper type allows avoiding allocating and leaking a new [`wstr`] when no
/// localization is found and the input is returned, but as a static reference.
fn gettext(message: MaybeStatic) -> &'static wstr {
    fn get_static_wstr(message: &wstr) -> &'static wstr {
        /// Keeps track of messages that have been leaked, so that we don't have to leak them more than
        /// once.
        static LEAKED_MESSAGES: Lazy<Mutex<HashSet<&'static wstr>>> =
            Lazy::new(|| Mutex::new(HashSet::new()));

        let mut leaked_messages = LEAKED_MESSAGES.lock().unwrap();
        if let Some(static_message) = leaked_messages.get(message) {
            static_message
        } else {
            let static_message = wstr::from_char_slice(Box::leak(message.as_char_slice().into()));
            leaked_messages.insert(static_message);
            static_message
        }
    }
    #[cfg(feature = "localize")]
    {
        let key = match message {
            MaybeStatic::Static(s) => s,
            MaybeStatic::Local(s) => s,
        };
        let language_precedence = gettext_impl::LANGUAGE_PRECEDENCE.lock().unwrap();
        // Use the localization from the highest-precedence language that has one available.
        for lang in language_precedence.iter() {
            if let Some(catalog) = gettext_impl::CATALOGS.get(lang) {
                if let Some(localization) = catalog.get(&key.to_string()) {
                    return get_static_wstr(&WString::from_str(localization));
                }
            }
        }
        // release lock on LANGUAGE_PRECEDENCE
    }
    // No localization found.
    match message {
        MaybeStatic::Static(s) => s,
        MaybeStatic::Local(s) => get_static_wstr(s),
    }
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
