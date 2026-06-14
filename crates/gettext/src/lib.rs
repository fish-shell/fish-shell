use fish_gettext_maps::CATALOGS;
use fish_localization::{Language, LocalizationLanguage, define_localization_language_type};
use std::{
    collections::HashSet,
    sync::{LazyLock, Mutex},
};

type Catalog = &'static phf::Map<&'static str, &'static str>;

static LANGUAGE_PRECEDENCE: Mutex<Vec<(Language, Catalog)>> = Mutex::new(Vec::new());

pub fn gettext(message_str: &'static str) -> Option<&'static str> {
    let language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();

    // Use the localization from the highest-precedence language that has one available.
    for (_, catalog) in language_precedence.iter() {
        if let Some(localized_str) = catalog.get(message_str) {
            return Some(localized_str);
        }
    }
    None
}

define_localization_language_type! {GettextLocalizationLanguage}

static AVAILABLE_LANGUAGES: LazyLock<HashSet<GettextLocalizationLanguage>> = LazyLock::new(|| {
    HashSet::from_iter(
        CATALOGS
            .entries()
            .map(|(&language, _)| GettextLocalizationLanguage(language)),
    )
});

pub fn get_available_languages() -> &'static HashSet<GettextLocalizationLanguage> {
    &AVAILABLE_LANGUAGES
}

pub fn set_language_precedence(new_precedence: &[GettextLocalizationLanguage]) {
    let catalogs = new_precedence
        .iter()
        .map(|lang| {
            (
                lang.into(),
                *CATALOGS
                    .get(lang.as_ref())
                    .expect("Only languages for which catalogs exist may be passed to gettext."),
            )
        })
        .collect();
    *LANGUAGE_PRECEDENCE.lock().unwrap() = catalogs;
}

pub fn get_language_precedence() -> Vec<Language<'static>> {
    let language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();
    language_precedence.iter().map(|&(lang, _)| lang).collect()
}
