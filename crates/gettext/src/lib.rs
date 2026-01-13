use fish_gettext_maps::CATALOGS;
use std::{
    collections::HashMap,
    sync::{LazyLock, Mutex},
};

type Catalog = &'static phf::Map<&'static str, &'static str>;

static LANGUAGE_PRECEDENCE: LazyLock<Mutex<Vec<(&'static str, Catalog)>>> =
    LazyLock::new(|| Mutex::new(vec![]));

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

#[derive(Clone, Copy)]
pub struct GettextLocalizationLanguage {
    language: &'static str,
}

static AVAILABLE_LANGUAGES: LazyLock<HashMap<&'static str, GettextLocalizationLanguage>> =
    LazyLock::new(|| {
        HashMap::from_iter(
            CATALOGS
                .entries()
                .map(|(&language, _)| (language, GettextLocalizationLanguage { language })),
        )
    });

pub fn get_available_languages() -> &'static HashMap<&'static str, GettextLocalizationLanguage> {
    &AVAILABLE_LANGUAGES
}

pub fn set_language_precedence(new_precedence: &[GettextLocalizationLanguage]) {
    let catalogs = new_precedence
        .iter()
        .map(|lang| {
            (
                lang.language,
                *CATALOGS
                    .get(lang.language)
                    .expect("Only languages for which catalogs exist may be passed to gettext."),
            )
        })
        .collect();
    *LANGUAGE_PRECEDENCE.lock().unwrap() = catalogs;
}

pub fn get_language_precedence() -> Vec<&'static str> {
    let language_precedence = LANGUAGE_PRECEDENCE.lock().unwrap();
    language_precedence.iter().map(|&(lang, _)| lang).collect()
}
