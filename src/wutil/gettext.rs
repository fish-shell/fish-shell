use std::sync::Mutex;

#[cfg(feature = "localize-messages")]
use crate::env::{EnvStack, Environment};
use crate::wchar::prelude::*;
use once_cell::sync::Lazy;

#[cfg(feature = "localize-messages")]
mod gettext_impl {
    use fish_gettext_maps::CATALOGS;
    use once_cell::sync::Lazy;
    use std::{collections::HashSet, sync::Mutex};

    type Catalog = &'static phf::Map<&'static str, &'static str>;

    pub struct SetLanguageLints<'a> {
        pub duplicates: Vec<&'a str>,
        pub non_existing: Vec<&'a str>,
    }

    #[derive(PartialEq, Eq, Clone, Copy)]
    pub enum LanguagePrecedenceOrigin {
        Default,
        LocaleVariable(LocaleVariable),
        LanguageEnvVar,
        StatusLanguage,
    }

    #[derive(PartialEq, Eq, Clone, Copy)]
    pub enum LocaleVariable {
        #[allow(clippy::upper_case_acronyms)]
        LANG,
        LC_MESSAGES,
        LC_ALL,
    }

    impl LocaleVariable {
        fn as_language_precedence_origin(&self) -> LanguagePrecedenceOrigin {
            LanguagePrecedenceOrigin::LocaleVariable(*self)
        }

        pub fn as_str(&self) -> &'static str {
            match self {
                Self::LANG => "LANG",
                Self::LC_MESSAGES => "LC_MESSAGES",
                Self::LC_ALL => "LC_ALL",
            }
        }
    }

    impl std::fmt::Display for LocaleVariable {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            write!(f, "{}", self.as_str())
        }
    }

    struct InternalLocalizationState {
        precedence_origin: LanguagePrecedenceOrigin,
        language_precedence: Vec<(String, Catalog)>,
    }

    pub struct PublicLocalizationState {
        pub precedence_origin: LanguagePrecedenceOrigin,
        pub language_precedence: Vec<String>,
    }

    /// Stores the current localization status.
    /// `is_active` indicates whether localization is currently active, and the reason if it is
    /// not.
    /// The `origin` indicates where the values in `language_precedence` were taken from.
    /// `language_precedence` stores the catalogs in the order they should be used.
    ///
    /// This struct should be updated when the relevant variables change or `status language` is used
    /// to modify the localization state.
    static LOCALIZATION_STATE: Lazy<Mutex<InternalLocalizationState>> =
        Lazy::new(|| Mutex::new(InternalLocalizationState::new()));

    impl InternalLocalizationState {
        fn new() -> Self {
            Self {
                precedence_origin: LanguagePrecedenceOrigin::Default,
                language_precedence: vec![],
            }
        }

        fn to_public(&self) -> PublicLocalizationState {
            PublicLocalizationState {
                precedence_origin: self.precedence_origin,
                language_precedence: self
                    .language_precedence
                    .iter()
                    .map(|(lang, _)| lang.to_owned())
                    .collect(),
            }
        }

        fn update_from_env(
            &mut self,
            message_locale: Option<(LocaleVariable, String)>,
            language_var: Option<Vec<String>>,
        ) {
            // Do not override values set via `status language`.
            if self.precedence_origin == LanguagePrecedenceOrigin::StatusLanguage {
                return;
            }

            if let Some((precedence_origin, locale)) = &message_locale {
                // Regular locale names start with lowercase letters (`ll_CC`, followed by some suffix).
                // The C or POSIX locale is special, and often used to disable localization.
                // Their names are upper-case, but variants with suffixes (`C.UTF-8`) exist.
                // To ensure that such variants are accounted for, we match on prefixes of the
                // locale name.
                // https://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html#tag_07_02
                fn is_c_locale(locale: &str) -> bool {
                    locale.starts_with('C') || locale.starts_with("POSIX")
                }
                if is_c_locale(locale) {
                    self.precedence_origin =
                        LanguagePrecedenceOrigin::LocaleVariable(*precedence_origin);
                    self.language_precedence.clear();
                    return;
                }
            }

            let (precedence_origin, language_list) = if let Some(list) = language_var {
                (LanguagePrecedenceOrigin::LanguageEnvVar, list)
            } else if let Some((precedence_origin, locale)) = message_locale {
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
                (
                    precedence_origin.as_language_precedence_origin(),
                    vec![normalized_name],
                )
            } else {
                (LanguagePrecedenceOrigin::Default, vec![])
            };

            let mut seen_languages = HashSet::new();
            self.language_precedence = language_list
                .into_iter()
                .flat_map(|lang| find_existing_catalogs(&lang))
                .filter(|(lang, _)| seen_languages.insert(lang.to_owned()))
                .collect();
            self.precedence_origin = precedence_origin;
        }

        fn update_from_status_language_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
            &mut self,
            langs: &'b [S],
        ) -> SetLanguageLints<'a> {
            let mut seen = HashSet::new();
            let mut duplicates = vec![];
            for lang in langs {
                let lang = lang.as_ref();
                if !seen.insert(lang) {
                    duplicates.push(lang)
                }
            }
            let mut existing_langs = vec![];
            let mut non_existing = vec![];
            for lang in langs {
                let lang = lang.as_ref();
                if let Some(catalog) = CATALOGS.get(lang) {
                    existing_langs.push((lang.to_owned(), *catalog));
                } else {
                    non_existing.push(lang);
                }
            }

            let mut seen = HashSet::new();
            let unique_langs = existing_langs
                .into_iter()
                .filter(|(lang, _)| seen.insert(lang.to_owned()))
                .collect();
            self.language_precedence = unique_langs;
            self.precedence_origin = LanguagePrecedenceOrigin::StatusLanguage;

            SetLanguageLints {
                duplicates,
                non_existing,
            }
        }
    }

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

    pub(super) fn update_from_env(
        locale: Option<(LocaleVariable, String)>,
        language_var: Option<Vec<String>>,
    ) {
        let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
        localization_state.update_from_env(locale, language_var);
    }

    pub(super) fn update_from_status_language_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
        langs: &'b [S],
    ) -> SetLanguageLints<'a> {
        let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
        localization_state.update_from_status_language_builtin(langs)
    }

    pub(super) fn unset_from_status_language_builtin(
        locale: Option<(LocaleVariable, String)>,
        language_var: Option<Vec<String>>,
    ) {
        let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
        localization_state.precedence_origin = LanguagePrecedenceOrigin::Default;
        localization_state.update_from_env(locale, language_var);
    }

    pub(super) fn status_language() -> PublicLocalizationState {
        let localization_state = LOCALIZATION_STATE.lock().unwrap();
        localization_state.to_public()
    }

    pub(super) fn gettext(message_str: &'static str) -> Option<&'static str> {
        let localization_state = LOCALIZATION_STATE.lock().unwrap();

        // Use the localization from the highest-precedence language that has one available.
        for (_, catalog) in localization_state.language_precedence.iter() {
            if let Some(localized_str) = catalog.get(message_str) {
                return Some(localized_str);
            }
        }
        None
    }

    pub(super) fn list_available_languages() -> Vec<&'static str> {
        let mut langs: Vec<_> = CATALOGS.entries().map(|(&lang, _)| lang).collect();
        langs.sort();
        langs
    }
}

#[cfg(feature = "localize-messages")]
fn get_message_locale(vars: &EnvStack) -> Option<(gettext_impl::LocaleVariable, String)> {
    use gettext_impl::LocaleVariable;
    let get = |var_str: &wstr, var: LocaleVariable| {
        vars.get_unless_empty(var_str)
            .map(|val| (var, val.as_string().to_string()))
    };
    get(L!("LC_ALL"), LocaleVariable::LC_ALL)
        .or_else(|| get(L!("LC_MESSAGES"), LocaleVariable::LC_MESSAGES))
        .or_else(|| get(L!("LANG"), LocaleVariable::LANG))
}

#[cfg(feature = "localize-messages")]
fn get_language_var(vars: &EnvStack) -> Option<Vec<String>> {
    let langs = vars.get_unless_empty(L!("LANGUAGE"))?;
    let langs = langs.as_list();
    let filtered_langs: Vec<String> = langs
        .iter()
        .filter(|lang| !lang.is_empty())
        .map(|lang| lang.to_string())
        .collect();
    if filtered_langs.is_empty() {
        return None;
    }
    Some(filtered_langs)
}

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
///
/// For deciding how to localize, the following is done:
///
/// 1. If the language precedence was set via `status language`, env vars are ignored.
/// 2. Check the first non-empty value of the env vars `LC_ALL`, `LC_MESSAGES`, `LANG`. If it
///    starts with `C` we consider this a C locale and disable localization.
/// 3. Otherwise, the value of the `LANGUAGE` env var is used, if non-empty. This allows specifying
///    multiple languages, with languages specified first taking precedence, e.g.
///    `LANGUAGE=zh_TW:zh_CN:pt_BR`
/// 4. Otherwise, the first non-empty value of the env vars `LC_ALL`, `LC_MESSAGES`, `LANG` is
///    used. This can only specify a single language, e.g. `LANG=de_AT.UTF-8`.
///    There, we normalize locale names by stripping off the suffix, leaving only the `ll_CC` part.
/// 5. Otherwise, localization will not happen.
///
/// If users specify `ll_CC` as a language and we don't have a catalog for this language, but we
/// have one for `ll`, that will be used instead. If users specify `ll` (without specifying a
/// language variant), which we discourage, and we don't have a catalog for `ll`, but we do have
/// one for `ll_CC`, that will be used as a fallback. If we have multiple `ll_*` catalogs, all of
/// them will be used, in arbitrary order.
#[cfg(feature = "localize-messages")]
pub fn update_from_env(vars: &EnvStack) {
    gettext_impl::update_from_env(get_message_locale(vars), get_language_var(vars));
}

#[cfg(feature = "localize-messages")]
fn append_space_separated_list<S: AsRef<str>>(
    string: &mut WString,
    list: impl IntoIterator<Item = S>,
) {
    for lang in list.into_iter() {
        string.push(' ');
        string.push_utfstr(&crate::common::escape(
            WString::from_str(lang.as_ref()).as_utfstr(),
        ));
    }
}

#[cfg(feature = "localize-messages")]
pub struct SetLanguageLints<'a> {
    duplicates: Vec<&'a str>,
    non_existing: Vec<&'a str>,
}

#[cfg(feature = "localize-messages")]
impl<'a> From<gettext_impl::SetLanguageLints<'a>> for SetLanguageLints<'a> {
    fn from(lints: gettext_impl::SetLanguageLints<'a>) -> Self {
        Self {
            duplicates: lints.duplicates,
            non_existing: lints.non_existing,
        }
    }
}

#[cfg(feature = "localize-messages")]
impl<'a> SetLanguageLints<'a> {
    pub fn display_duplicates(&self) -> WString {
        let mut result = WString::new();
        if self.duplicates.is_empty() {
            return result;
        }
        result.push_utfstr(wgettext!("Language specifiers appear repeatedly:"));
        append_space_separated_list(&mut result, &self.duplicates);
        result.push('\n');
        result
    }

    pub fn display_non_existing(&self) -> WString {
        let mut result = WString::new();
        if self.non_existing.is_empty() {
            return result;
        }
        result.push_utfstr(wgettext!("No catalogs available for language specifiers:"));
        append_space_separated_list(&mut result, &self.non_existing);
        result.push('\n');
        result
    }

    pub fn display_all(&self) -> WString {
        let mut result = WString::new();
        result.push_utfstr(&self.display_duplicates());
        result.push_utfstr(&self.display_non_existing());
        result
    }
}
/// Call this when the `status language` builtin should update the language precedence.
/// `langs` should be the list of languages the precedence should be set to.
#[cfg(feature = "localize-messages")]
pub fn update_from_status_language_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
    langs: &'b [S],
) -> SetLanguageLints<'a> {
    gettext_impl::update_from_status_language_builtin(langs).into()
}

#[cfg(feature = "localize-messages")]
pub fn unset_from_status_language_builtin(vars: &EnvStack) {
    gettext_impl::unset_from_status_language_builtin(
        get_message_locale(vars),
        get_language_var(vars),
    );
}

#[cfg(feature = "localize-messages")]
pub fn status_language() -> WString {
    use gettext_impl::LanguagePrecedenceOrigin;
    let localization_state = gettext_impl::status_language();
    let mut result = WString::new();
    localizable_consts!(
        LANGUAGE_LIST_VARIABLE_ORIGIN "from variable %s"
    );
    let origin_string = match localization_state.precedence_origin {
        LanguagePrecedenceOrigin::Default => wgettext!("default").to_owned(),
        LanguagePrecedenceOrigin::LocaleVariable(var) => {
            wgettext_fmt!(LANGUAGE_LIST_VARIABLE_ORIGIN, var.as_str())
        }
        LanguagePrecedenceOrigin::LanguageEnvVar => {
            wgettext_fmt!(LANGUAGE_LIST_VARIABLE_ORIGIN, "LANGUAGE")
        }
        LanguagePrecedenceOrigin::StatusLanguage => {
            wgettext!("from command `status language set`").to_owned()
        }
    };
    result.push_utfstr(&wgettext_fmt!("Active languages (%s):", origin_string));
    append_space_separated_list(&mut result, &localization_state.language_precedence);
    result.push('\n');

    result
}

#[cfg(feature = "localize-messages")]
pub fn list_available_languages() -> WString {
    let mut languages = WString::new();
    for lang in gettext_impl::list_available_languages() {
        languages.push_str(lang);
        languages.push('\n');
    }
    languages
}

#[cfg(not(feature = "localize-messages"))]
pub fn initialize_gettext() {}

/// This function only exists to provide a way for initializing gettext before an [`EnvStack`] is
/// available. Without this, early error messages cannot be localized.
#[cfg(feature = "localize-messages")]
pub fn initialize_gettext() {
    let vars = EnvStack::new();
    env_stack_set_from_env!(vars, "LANGUAGE");
    env_stack_set_from_env!(vars, "LC_ALL");
    env_stack_set_from_env!(vars, "LC_MESSAGES");
    env_stack_set_from_env!(vars, "LANG");

    gettext_impl::update_from_env(get_message_locale(&vars), get_language_var(&vars));
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
