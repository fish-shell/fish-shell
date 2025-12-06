use fish_gettext_maps::CATALOGS;
use once_cell::sync::Lazy;
use std::{collections::HashSet, sync::Mutex};

type Catalog = &'static phf::Map<&'static str, &'static str>;

pub struct SetLocaleLints<'a> {
    pub malformed: Vec<&'a str>,
    pub duplicates: Vec<&'a str>,
    pub non_existing: Vec<&'a str>,
}

#[derive(PartialEq, Eq, Clone, Copy)]
pub enum LanguagePrecedenceOrigin {
    Default,
    LocaleVariable(LocaleVariable),
    LanguageEnvVar,
    StatusLocale,
}

#[derive(PartialEq, Eq, Clone, Copy)]
pub enum LocaleVariable {
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

#[derive(PartialEq, Eq, Clone, Copy)]
pub enum LocalizationActive {
    Active,
    CLocale(LocaleVariable),
}

struct InternalLocalizationState {
    is_active: LocalizationActive,
    precedence_origin: LanguagePrecedenceOrigin,
    language_precedence: Vec<(String, Catalog)>,
}

pub struct PublicLocalizationState {
    pub is_active: LocalizationActive,
    pub precedence_origin: LanguagePrecedenceOrigin,
    pub language_precedence: Vec<String>,
}

/// Stores the current localization status.
/// `is_active` indicates whether localization is currently active, and the reason if it is
/// not.
/// The `origin` indicates where the values in `language_precedence` were taken from.
/// `language_precedence` stores the catalogs in the order they should be used.
///
/// This struct should be updated when the relevant variables change or `status locale` is used
/// to modify the localization state.
static LOCALIZATION_STATE: Lazy<Mutex<InternalLocalizationState>> =
    Lazy::new(|| Mutex::new(InternalLocalizationState::new()));

impl InternalLocalizationState {
    fn new() -> Self {
        Self {
            is_active: LocalizationActive::Active,
            precedence_origin: LanguagePrecedenceOrigin::Default,
            language_precedence: vec![],
        }
    }

    fn to_public(&self) -> PublicLocalizationState {
        PublicLocalizationState {
            is_active: self.is_active,
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
        fn is_c_locale(locale: &str) -> bool {
            locale.starts_with('C')
        }

        self.is_active = LocalizationActive::Active;
        if let Some((precedence_origin, locale)) = &message_locale {
            if is_c_locale(locale) {
                self.is_active = LocalizationActive::CLocale(*precedence_origin);
            }
        }

        // Do not override values set via `status locale`.
        if self.precedence_origin == LanguagePrecedenceOrigin::StatusLocale {
            return;
        }

        let (precedence_origin, language_list) = if let Some(list) = language_var {
            (LanguagePrecedenceOrigin::LanguageEnvVar, list)
        } else if let Some((precedence_origin, locale)) = message_locale {
            // TODO(MSRV>=1.88) if let chain
            if is_c_locale(&locale) {
                (LanguagePrecedenceOrigin::Default, vec![])
            } else {
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
            }
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

    fn update_from_status_locale_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
        &mut self,
        langs: &'b [S],
    ) -> SetLocaleLints<'a> {
        fn name_has_correct_shape(name: &str) -> bool {
            let Some((ll, cc)) = name.split_once('_') else {
                return false;
            };
            (ll.len() == 2 || ll.len() == 3)
                && cc.len() == 2
                && ll.chars().all(|c| c.is_ascii_lowercase())
                && cc.chars().all(|c| c.is_ascii_uppercase())
        }

        let (well_formed, malformed): (Vec<&str>, Vec<&str>) = langs
            .iter()
            .map(|lang| lang.as_ref())
            .partition(|lang| name_has_correct_shape(lang));
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
        for lang in well_formed {
            let mut catalogs = find_existing_catalogs(lang).into_iter();
            if let Some(catalog) = catalogs.next() {
                existing_langs.push(catalog);
            } else {
                non_existing.push(lang);
            }
            if catalogs.next().is_some() {
                panic!(
                    "Got more than 1 catalog for {lang}. The ll_CC format should never result in more than one catalog."
                )
            }
        }

        let mut seen = HashSet::new();
        let unique_langs = existing_langs
            .into_iter()
            .filter(|(lang, _)| seen.insert(lang.to_owned()))
            .collect();
        self.language_precedence = unique_langs;
        self.precedence_origin = LanguagePrecedenceOrigin::StatusLocale;

        SetLocaleLints {
            malformed,
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
    let language_without_country_code = language.split_once('_').map_or(language, |(ll, _cc)| ll);
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

pub fn update_from_env(
    locale: Option<(LocaleVariable, String)>,
    language_var: Option<Vec<String>>,
) {
    let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.update_from_env(locale, language_var);
}

pub fn update_from_status_locale_builtin<'a, 'b: 'a, S: AsRef<str> + 'a>(
    langs: &'b [S],
) -> SetLocaleLints<'a> {
    let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.update_from_status_locale_builtin(langs)
}

pub fn unset_from_status_locale_builtin(
    locale: Option<(LocaleVariable, String)>,
    language_var: Option<Vec<String>>,
) {
    let mut localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.precedence_origin = LanguagePrecedenceOrigin::Default;
    localization_state.update_from_env(locale, language_var);
}

pub fn status_locale_get() -> PublicLocalizationState {
    let localization_state = LOCALIZATION_STATE.lock().unwrap();
    localization_state.to_public()
}

pub fn gettext(message_str: &'static str) -> Option<&'static str> {
    let localization_state = LOCALIZATION_STATE.lock().unwrap();

    if localization_state.is_active != LocalizationActive::Active {
        return None;
    }

    // Use the localization from the highest-precedence language that has one available.
    for (_, catalog) in localization_state.language_precedence.iter() {
        if let Some(localized_str) = catalog.get(message_str) {
            return Some(localized_str);
        }
    }
    None
}
