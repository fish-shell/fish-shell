use std::sync::Mutex;

#[cfg(feature = "localize-messages")]
use crate::env::EnvStack;
use crate::wchar::prelude::*;
use once_cell::sync::Lazy;

#[cfg(feature = "localize-messages")]
mod gettext_impl {
    use crate::env::{EnvStack, Environment};
    use fish_gettext_maps::CATALOGS;
    use once_cell::sync::Lazy;
    use std::{collections::HashSet, sync::Mutex};

    type Catalog = &'static phf::Map<&'static str, &'static str>;

    /// Tries to find catalogs for `language`.
    /// `language` must be an ISO 639 language code, optionally followed by an underscore and an ISO
    /// 3166 country/territory code.
    /// Uses the catalog with the exact same name as `language` if it exists.
    /// If a country code is present (`ll_CC`), only the catalog named `ll` will be considered as a fallback.
    /// If no country code is present (`ll`), all catalogs whose names start with `ll_` will be used in
    /// arbitrary order.
    fn find_existing_catalogs(language: &str) -> Vec<(String, Catalog)> {
        // Try the exact name first.
        // If there already is a corresponding catalog return the language.
        if let Some(catalog) = CATALOGS.get(language) {
            return vec![(language.to_owned(), catalog)];
        }
        let language_without_country_code =
            language.split_once('_').map_or(language, |(ll, _cc)| ll);
        if language == language_without_country_code {
            // We have `ll` format. In this case, try to find any catalog whose name starts with `ll_`.
            // Note that it is important to include the underscore in the pattern, otherwise `ll` might
            // fall back to `llx_CC`, where `llx` is a 3-letter language identifier.
            let ll_prefix = format!("{language}_");
            let mut lang_catalogs = vec![];
            for (&lang_name, &catalog) in CATALOGS.entries() {
                if lang_name.starts_with(&ll_prefix) {
                    lang_catalogs.push((lang_name.to_owned(), catalog));
                }
            }
            lang_catalogs
        } else {
            // If `language` contained a country code, we only try to fall back to a catalog
            // without a country code.
            if let Some(catalog) = CATALOGS.get(language_without_country_code) {
                vec![(language_without_country_code.to_owned(), catalog)]
            } else {
                vec![]
            }
        }
    }

    /// The precedence list of user-preferred languages, obtained from the relevant environment
    /// variables.
    /// This should be updated when the relevant variables change.
    static LANGUAGE_PRECEDENCE: Lazy<Mutex<Vec<Catalog>>> = Lazy::new(|| Mutex::new(Vec::new()));

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
    ///
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
        let mut seen_languages = HashSet::new();
        let mut language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();
        *language_precedence = get_language_preferences_from_env(vars)
            .into_iter()
            .flat_map(|lang| find_existing_catalogs(&lang))
            .filter(|(lang, _)| seen_languages.insert(lang.to_owned()))
            .map(|(_, catalog)| catalog)
            .collect();
    }

    pub(super) fn gettext(message_str: &'static str) -> Option<&'static str> {
        let language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();

        // Use the localization from the highest-precedence language that has one available.
        for catalog in language_precedence.iter() {
            if let Some(localized_str) = catalog.get(message_str) {
                return Some(localized_str);
            }
        }
        None
    }
}

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
#[cfg(feature = "localize-messages")]
pub fn update_locale_from_env(vars: &EnvStack) {
    gettext_impl::update_locale_from_env(vars);
}

#[cfg(not(feature = "localize-messages"))]
pub fn initialize_gettext() {}

/// This function only exists to provide a way for initializing gettext before an [`EnvStack`] is
/// available. Without this, early error messages cannot be localized.
#[cfg(feature = "localize-messages")]
pub fn initialize_gettext() {
    let locale_vars = EnvStack::new();
    env_stack_set_from_env!(locale_vars, "LANGUAGE");
    env_stack_set_from_env!(locale_vars, "LC_ALL");
    env_stack_set_from_env!(locale_vars, "LC_MESSAGES");
    env_stack_set_from_env!(locale_vars, "LANG");

    gettext_impl::update_locale_from_env(&locale_vars);
}

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
    static MESSAGE_TO_NARROW: Lazy<Mutex<HashMap<&'static wstr, NarrowMessage>>> =
        Lazy::new(|| Mutex::new(HashMap::default()));
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
        if let Some(localized_str) = gettext_impl::gettext(message_str) {
            static LOCALIZATION_TO_WIDE: Lazy<Mutex<HashMap<&'static str, &'static wstr>>> =
                Lazy::new(|| Mutex::new(HashMap::default()));
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

#[cfg(test)]
mod tests {
    use super::LocalizableString;
    use crate::tests::prelude::*;
    use crate::wchar::prelude::*;

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
