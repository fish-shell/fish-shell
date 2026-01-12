use super::{localizable_consts, localizable_string, wgettext, wgettext_fmt};
use crate::env::{EnvStack, Environment};
use fish_widestring::{L, WString, wstr};
use std::collections::{HashMap, HashSet};
use std::sync::{LazyLock, Mutex};

#[derive(PartialEq, Eq, Clone, Copy)]
enum LanguagePrecedenceOrigin {
    Default,
    LocaleVariable(LocaleVariable),
    LanguageEnvVar,
    StatusLanguage,
}

#[derive(PartialEq, Eq, Clone, Copy)]
enum LocaleVariable {
    #[allow(clippy::upper_case_acronyms)]
    LANG,
    #[allow(non_camel_case_types)]
    LC_MESSAGES,
    #[allow(non_camel_case_types)]
    LC_ALL,
}

impl LocaleVariable {
    fn as_language_precedence_origin(&self) -> LanguagePrecedenceOrigin {
        LanguagePrecedenceOrigin::LocaleVariable(*self)
    }

    fn as_str(&self) -> &'static str {
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

struct LocalizationVariables {
    message_locale: Option<(LocaleVariable, String)>,
    language: Option<Vec<String>>,
}

impl LocalizationVariables {
    fn get_message_locale(env: &EnvStack) -> Option<(LocaleVariable, String)> {
        let get = |var_str: &wstr, var: LocaleVariable| {
            env.get_unless_empty(var_str)
                .map(|val| (var, val.as_string().to_string()))
        };
        get(L!("LC_ALL"), LocaleVariable::LC_ALL)
            .or_else(|| get(L!("LC_MESSAGES"), LocaleVariable::LC_MESSAGES))
            .or_else(|| get(L!("LANG"), LocaleVariable::LANG))
    }

    fn get_language_var(env: &EnvStack) -> Option<Vec<String>> {
        let langs = env.get_unless_empty(L!("LANGUAGE"))?;
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

    fn from_env(env: &EnvStack) -> Self {
        Self {
            message_locale: Self::get_message_locale(env),
            language: Self::get_language_var(env),
        }
    }
}

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

pub struct SetLanguageLints<'a> {
    duplicates: Vec<&'a str>,
    non_existing: Vec<&'a str>,
}

impl<'a> SetLanguageLints<'a> {
    fn display_duplicates(&self) -> WString {
        let mut result = WString::new();
        if self.duplicates.is_empty() {
            return result;
        }
        result.push_utfstr(wgettext!("Language specifiers appear repeatedly:"));
        append_space_separated_list(&mut result, &self.duplicates);
        result.push('\n');
        result
    }

    fn display_non_existing(&self) -> WString {
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

struct LocalizationState {
    precedence_origin: LanguagePrecedenceOrigin,
}

impl LocalizationState {
    fn new() -> Self {
        Self {
            precedence_origin: LanguagePrecedenceOrigin::Default,
        }
    }

    /// Tries to find catalogs for `language`.
    /// `language` must be an ISO 639 language code, optionally followed by an underscore and an ISO
    /// 3166 country/territory code.
    /// Uses the catalog with the exact same name as `language` if it exists.
    /// If a country code is present (`ll_CC`), only the catalog named `ll` will be considered as a fallback.
    /// If no country code is present (`ll`), all catalogs whose names start with `ll_` will be used in
    /// arbitrary order.
    fn find_best_matches<'a, 'b: 'a, L: Copy>(
        language: &str,
        available_languages: &'a HashMap<&'b str, L>,
    ) -> Vec<(&'b str, L)> {
        // Try the exact name first.
        // If there already is a corresponding catalog return the language.
        if let Some((&lang_str, &lang_value)) = available_languages.get_key_value(language) {
            return vec![(lang_str, lang_value)];
        }
        let language_without_country_code =
            language.split_once('_').map_or(language, |(ll, _cc)| ll);
        if language == language_without_country_code {
            // We have `ll` format. In this case, try to find any catalog whose name starts with `ll_`.
            // Note that it is important to include the underscore in the pattern, otherwise `ll` might
            // fall back to `llx_CC`, where `llx` is a 3-letter language identifier.
            let ll_prefix = format!("{language}_");
            let mut lang_catalogs = vec![];
            for (&lang_str, &localization_lang) in available_languages.iter() {
                if lang_str.starts_with(&ll_prefix) {
                    lang_catalogs.push((lang_str, localization_lang));
                }
            }
            lang_catalogs
        } else {
            // If `language` contained a country code, we only try to fall back to a catalog
            // without a country code.
            if let Some((&lang_str, &lang_value)) =
                available_languages.get_key_value(language_without_country_code)
            {
                vec![(lang_str, lang_value)]
            } else {
                vec![]
            }
        }
    }

    fn update_from_env(&mut self, localization_vars: LocalizationVariables) {
        // Do not override values set via `status language`.
        if self.precedence_origin == LanguagePrecedenceOrigin::StatusLanguage {
            return;
        }

        if let Some((precedence_origin, locale)) = &localization_vars.message_locale {
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
                fish_gettext::set_language_precedence(&[]);
                return;
            }
        }

        let (precedence_origin, language_list) = if let Some(list) = localization_vars.language {
            (LanguagePrecedenceOrigin::LanguageEnvVar, list)
        } else if let Some((precedence_origin, locale)) = &localization_vars.message_locale {
            // Strip off encoding and modifier. (We always expect UTF-8 and don't support modifiers.)
            let normalized_name = locale
                .split_once(|c: char| !(c.is_ascii_alphabetic() || c == '_'))
                .map_or(locale.as_str(), |(lang_name, _)| lang_name)
                .to_owned();
            // At this point, the normalized_name should have the shape `ll` or `ll_CC`.
            (
                precedence_origin.as_language_precedence_origin(),
                vec![normalized_name],
            )
        } else {
            (LanguagePrecedenceOrigin::Default, vec![])
        };
        fn update_precedence<'a, 'b: 'a, LocalizationLanguage: Copy + 'a>(
            language_list: &[String],
            get_available_languages: fn() -> &'a HashMap<&'b str, LocalizationLanguage>,
            set_language_precedence: fn(&[LocalizationLanguage]),
        ) {
            let available_langs = get_available_languages();
            let mut seen_languages = HashSet::new();
            let language_precedence: Vec<_> = language_list
                .iter()
                .flat_map(|lang| LocalizationState::find_best_matches(lang, available_langs))
                .filter(|&(lang_str, _)| seen_languages.insert(lang_str))
                .map(|(_, localization_lang)| localization_lang)
                .collect();
            set_language_precedence(&language_precedence);
        }
        update_precedence(
            &language_list,
            fish_gettext::get_available_languages,
            fish_gettext::set_language_precedence,
        );
        self.precedence_origin = precedence_origin;
    }

    fn update_from_status_language_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
        &mut self,
        langs: &'b [S],
    ) -> SetLanguageLints<'a> {
        let mut seen_in_input = HashSet::new();
        let mut unique_lang_strs = vec![];
        let mut duplicates = vec![];
        for lang in langs {
            let lang = lang.as_ref();
            if seen_in_input.insert(lang) {
                unique_lang_strs.push(lang);
            } else {
                duplicates.push(lang)
            }
        }
        let mut all_available_langs = HashSet::new();
        fn update_precedence<'a, 'b, 'c: 'a + 'b, LocalizationLanguage: Copy + 'a>(
            unique_lang_strs: &[&str],
            get_available_languages: fn() -> &'a HashMap<&'c str, LocalizationLanguage>,
            set_language_precedence: fn(&[LocalizationLanguage]),
            all_available_langs: &'b mut HashSet<&'c str>,
        ) {
            let available_langs = get_available_languages();
            for &lang in available_langs.keys() {
                all_available_langs.insert(lang);
            }
            let mut existing_langs = vec![];
            for lang in unique_lang_strs {
                if let Some((&lang_str, &lang_value)) = available_langs.get_key_value(lang) {
                    existing_langs.push((lang_str, lang_value));
                }
            }

            let mut seen = HashSet::new();
            let unique_langs: Vec<_> = existing_langs
                .into_iter()
                .filter(|&(lang, _)| seen.insert(lang))
                .map(|(_, localization_lang)| localization_lang)
                .collect();
            set_language_precedence(&unique_langs);
        }
        update_precedence(
            &unique_lang_strs,
            fish_gettext::get_available_languages,
            fish_gettext::set_language_precedence,
            &mut all_available_langs,
        );

        self.precedence_origin = LanguagePrecedenceOrigin::StatusLanguage;

        let mut seen_non_existing = HashSet::new();
        let non_existing: Vec<&str> = langs
            .iter()
            .map(|lang| lang.as_ref())
            .filter(|&lang| !all_available_langs.contains(lang) && seen_non_existing.insert(lang))
            .collect();

        SetLanguageLints {
            duplicates,
            non_existing,
        }
    }
}

/// Stores the current localization status.
/// `is_active` indicates whether localization is currently active, and the reason if it is
/// not.
/// The `origin` indicates where the values in `language_precedence` were taken from.
/// `language_precedence` stores the catalogs in the order they should be used.
///
/// This struct should be updated when the relevant variables change or `status language` is used
/// to modify the localization state.
static LOCALIZATION_STATE: LazyLock<Mutex<LocalizationState>> =
    LazyLock::new(|| Mutex::new(LocalizationState::new()));

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
///
/// For deciding how to localize, the following is done:
///
/// 1. If the language precedence was set via `status language`, env vars are ignored.
/// 2. Check the first non-empty value of the env vars `LC_ALL`, `LC_MESSAGES`, `LANG`. If it
///    starts with `C` or `POSIX` we consider this a C locale and disable localization.
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
pub fn update_from_env(env: &EnvStack) {
    let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.update_from_env(LocalizationVariables::from_env(env));
}

/// Call this when the `status language` builtin should update the language precedence.
/// `langs` should be the list of languages the precedence should be set to.
pub fn update_from_status_language_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
    langs: &'b [S],
) -> SetLanguageLints<'a> {
    let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.update_from_status_language_builtin(langs)
}

pub fn unset_from_status_language_builtin(env: &EnvStack) {
    let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.precedence_origin = LanguagePrecedenceOrigin::Default;
    localization_state.update_from_env(LocalizationVariables::from_env(env));
}

pub fn status_language() -> WString {
    let localization_state = LOCALIZATION_STATE.lock().unwrap();
    let mut result = WString::new();
    localizable_consts!(
        LANGUAGE_LIST_VARIABLE_ORIGIN "%s variable"
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
            wgettext_fmt!("%s command", "`status language set`")
        }
    };
    result.push_utfstr(&wgettext_fmt!(
        "Active languages (source: %s):",
        origin_string
    ));
    let gettext_language_precedence = fish_gettext::get_language_precedence();
    append_space_separated_list(&mut result, &gettext_language_precedence);
    result.push('\n');

    result
}

pub fn list_available_languages() -> WString {
    let mut language_set = HashSet::new();
    fn add_languages<'a, 'b: 'a, LocalizationLanguage: 'a>(
        language_set: &mut HashSet<&'b str>,
        get_available_languages: fn() -> &'a HashMap<&'b str, LocalizationLanguage>,
    ) {
        for &lang in get_available_languages().keys() {
            language_set.insert(lang);
        }
    }
    add_languages(&mut language_set, fish_gettext::get_available_languages);
    let mut language_list = Vec::from_iter(language_set);
    language_list.sort();
    let mut languages = WString::new();
    for lang in language_list {
        languages.push_str(lang);
        languages.push('\n');
    }
    languages
}
