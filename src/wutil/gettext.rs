use std::collections::HashSet;
use std::sync::Mutex;

#[cfg(feature = "localize-messages")]
use crate::env::EnvStack;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use once_cell::sync::Lazy;

#[cfg(feature = "localize-messages")]
mod gettext_impl {
    use std::sync::Mutex;

    use once_cell::sync::Lazy;

    pub(super) use fish_gettext_maps::CATALOGS;
    type Catalog = &'static phf::Map<&'static str, &'static str>;

    use crate::env::{EnvStack, Environment};

    /// Tries to find a catalog for `language`.
    /// `language` must be an ISO 639 language code, optionally followed by an underscore and an ISO
    /// 3166 country/territory code.
    /// Always prefers the catalog with the exact same name as `language` if it exists.
    /// If a country code is present (`ll_CC`), only the catalog named `ll` will be considered as a fallback.
    /// If no country code is present (`ll`), an arbitrary catalog whose name starts with `ll_`
    /// will be used as a fallback, if one exists.
    /// If there is a catalog for the language, then `Some(catalog)` will be returned.
    /// `None` will be returned if no variant of the language has localizations.
    fn find_existing_catalog(language: &str) -> Option<Catalog> {
        // Try the exact name first.
        // If there already is a corresponding catalog return the language.
        if let Some(catalog) = CATALOGS.get(language) {
            return Some(catalog);
        }
        let language_without_country_code =
            language.split_once('_').map_or(language, |(ll, _cc)| ll);
        if language == language_without_country_code {
            // We have `ll` format. In this case, try to find any catalog whose name starts with `ll_`.
            // Note that it is important to include the underscore in the pattern, otherwise `ll` might
            // fall back to `llx_CC`, where `llx` is a 3-letter language identifier.
            let ll_prefix = format!("{language}_");
            for (&lang_name, &catalog) in CATALOGS.entries() {
                if lang_name.starts_with(&ll_prefix) {
                    return Some(catalog);
                }
            }
            // No localizations for the language (and any regional variations) exist.
            None
        } else {
            // If `language` contained a country code, we only try to fall back to a catalog
            // without a country code.
            CATALOGS.get(language_without_country_code).copied()
        }
    }

    /// The precedence list of user-preferred languages, obtained from the relevant environment
    /// variables.
    /// This should be updated when the relevant variables change.
    pub(super) static LANGUAGE_PRECEDENCE: Lazy<Mutex<Vec<Catalog>>> =
        Lazy::new(|| Mutex::new(Vec::new()));

    /// Four environment variables can be used to select languages.
    /// A detailed description is available at
    /// <https://www.gnu.org/software/gettext/manual/html_node/Setting-the-POSIX-Locale.html>
    /// Our does not replicate the behavior exactly.
    /// See the following description.
    ///
    /// There are three variables which can be used for setting the locale for messages:
    /// 1. `LC_ALL`
    /// 2. `LC_MESSAGES`
    /// 3. `LANG`
    /// The value of the first one set to a non-zero value will be considered.
    /// If it is set to the `C` locale (we consider any value starting with `C` as the `C` locale),
    /// localization will be disabled.
    /// Otherwise, the variable `LANGUAGE` is checked. If it is non-empty, it is considered a
    /// colon-separated list of languages. Languages are listed with descending priority, meaning
    /// we will localize each message into the first language with a localization available.
    /// Each language is specified by a 2 or 3 letter ISO 639 language code, optionally followed by
    /// an underscore and an ISO 3166 country/territory code. If the second part is omitted, some
    /// variant of the language will be used if localizations exist for one. We make no guarantees
    /// about which variant that will be.
    /// In addition to the colon-separated format, using a list with one language per element is
    /// also supported.
    ///
    /// Returns the (possibly empty) preference list of languages.
    fn get_language_preferences_from_env(vars: &EnvStack) -> Vec<String> {
        use crate::wchar::L;

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

        fn check_language_var(vars: &EnvStack) -> Option<Vec<String>> {
            let langs = vars.get(L!("LANGUAGE"))?;
            let langs = langs.as_list();
            let filtered_langs: Vec<String> = langs
                .iter()
                .filter(|lang| !lang.is_empty())
                .map(|lang| normalize_locale_name(&lang.to_string()))
                .collect();
            if filtered_langs.is_empty() {
                return None;
            }
            Some(filtered_langs)
        }

        // Locale value is determined by the first of these three variables set to a non-zero
        // value.
        if let Some(locale) = vars
            .get(L!("LC_ALL"))
            .or_else(|| vars.get(L!("LC_MESSAGES")).or_else(|| vars.get(L!("LANG"))))
        {
            let locale = locale.as_string().to_string();
            if locale.starts_with('C') {
                // Do not localize in C locale.
                return vec![];
            }
            // `LANGUAGE` has higher precedence than the locale value.
            if let Some(precedence_list) = check_language_var(vars) {
                return precedence_list;
            }
            // Use the locale value if `LANGUAGE` is not set.
            vec![normalize_locale_name(&locale)]
        } else if let Some(precedence_list) = check_language_var(vars) {
            // Use the `LANGUAGE` value if locale is not set.
            return precedence_list;
        } else {
            // None of the relevant variables are set, so we will not localize.
            vec![]
        }
    }

    /// Implementation of the function with the same name in super.
    pub(super) fn update_locale_from_env(vars: &EnvStack) {
        let mut language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();
        *language_precedence = get_language_preferences_from_env(vars)
            .iter()
            .filter_map(|lang| find_existing_catalog(lang))
            .collect();
    }
}

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
#[cfg(feature = "localize-messages")]
pub fn update_locale_from_env(vars: &EnvStack) {
    gettext_impl::update_locale_from_env(vars);
}

/// This function only exists to provide a way for initializing gettext before an [`EnvStack`] is
/// available. Without this, early error messages cannot be localized.
#[cfg(feature = "localize-messages")]
pub fn initialize_gettext() {
    use crate::env::EnvMode;

    let locale_vars = EnvStack::new();
    if let Ok(language) = std::env::var("LANGUAGE") {
        locale_vars.set_one(
            L!("LANGUAGE"),
            EnvMode::GLOBAL,
            WString::from_str(&language),
        );
    }
    if let Ok(lc_all) = std::env::var("LC_ALL") {
        locale_vars.set_one(L!("LC_ALL"), EnvMode::GLOBAL, WString::from_str(&lc_all));
    }
    if let Ok(lc_messages) = std::env::var("LC_MESSAGES") {
        locale_vars.set_one(
            L!("LC_MESSAGES"),
            EnvMode::GLOBAL,
            WString::from_str(&lc_messages),
        );
    }
    if let Ok(lang) = std::env::var("LANG") {
        locale_vars.set_one(L!("LANG"), EnvMode::GLOBAL, WString::from_str(&lang));
    }
    gettext_impl::update_locale_from_env(&locale_vars);
}

/// Use this function to localize a message.
/// The [`MaybeStatic`] wrapper type allows avoiding allocating and leaking a new [`wstr`] when no
/// localization is found and the input is returned, but as a static reference.
fn gettext(message: MaybeStatic) -> &'static wstr {
    fn intern_message(message: &wstr) -> &'static wstr {
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
    #[cfg(feature = "localize-messages")]
    {
        let key = match message {
            MaybeStatic::Static(s) => s,
            MaybeStatic::Local(s) => s,
        };
        let language_precedence = gettext_impl::LANGUAGE_PRECEDENCE.lock().unwrap();
        // Use the localization from the highest-precedence language that has one available.
        for catalog in language_precedence.iter() {
            if let Some(localization) = catalog.get(&key.to_string()) {
                return intern_message(&WString::from_str(localization));
            }
        }
        // release lock on LANGUAGE_PRECEDENCE
    }
    // No localization found.
    match message {
        MaybeStatic::Static(s) => s,
        MaybeStatic::Local(s) => intern_message(s),
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
#[cfg(feature = "gettext-extract")]
macro_rules! localizable_string {
    ($string:literal) => {
        $crate::wutil::gettext::LocalizableString::Static(widestring::utf32str!(
            fish_gettext_extraction::gettext_extract!($string)
        ))
    };
}
#[macro_export]
#[cfg(not(feature = "gettext-extract"))]
macro_rules! localizable_string {
    ($string:literal) => {
        $crate::wutil::gettext::LocalizableString::Static(widestring::utf32str!($string))
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
