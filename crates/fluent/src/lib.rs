use std::{
    borrow::Cow,
    collections::{HashMap, HashSet},
    sync::Mutex,
};

use fluent::{FluentArgs, FluentResource, concurrent::FluentBundle};
use rust_embed::{EmbeddedFile, RustEmbed};

#[cfg(feature = "fluent-extract")]
pub extern crate fish_fluent_extraction;

use once_cell::sync::Lazy;
use unic_langid::LanguageIdentifier;

static FALLBACK_LANGUAGE: &str = "en";

static LANGUAGE_BUNDLES: Lazy<Mutex<HashMap<String, &'static FluentBundle<FluentResource>>>> =
    Lazy::new(|| Mutex::new(HashMap::new()));

static LANGUAGE_BUNDLE_PRECEDENCE: Lazy<Mutex<Vec<&'static FluentBundle<FluentResource>>>> =
    Lazy::new(|| Mutex::new(vec![get_fallback_bundle()]));

#[derive(RustEmbed)]
#[folder = "../../fluent/"]
struct FtlFiles;

fn add_file(language: &str, file: EmbeddedFile) -> &'static FluentBundle<FluentResource> {
    let mut bundles = LANGUAGE_BUNDLES.lock().unwrap();
    // Reuse existing bundles.
    if let Some(bundle) = bundles.get(language) {
        return bundle;
    }
    let langid: LanguageIdentifier = language
        .parse()
        .map_err(|e| format!("Failed to parse language identifier {language}: {e}"))
        .unwrap();
    let mut bundle = FluentBundle::new_concurrent(vec![langid]);
    match FluentResource::try_new(
        String::from_utf8(Vec::from(file.data))
            .unwrap_or_else(|e| panic!("FTL file for {language} is not valid UTF-8: {e}")),
    ) {
        Ok(res) => {
            bundle
                .add_resource(res)
                .map_err(|e| {
                    format!(
                        "Failed to add FTL resources to the bundle for language {language}: {e:?}"
                    )
                })
                .unwrap();
            // Leak to create static reference.
            let bundle = Box::leak(Box::new(bundle));
            bundles.insert(language.into(), bundle);
            bundle
        }
        Err((_resource, errors)) => {
            let mut error_string = format!("Errors parsing FTL file for {language}:\n");
            for error in errors {
                error_string.push_str(&format!("{error}\n"));
            }
            panic!("{error_string}");
        }
    }
}

/// Tries to add a bundle for the given language and adds it to the `LANGUAGE_BUNDLES` map.
fn add_language(language: &str) -> Vec<&'static FluentBundle<FluentResource>> {
    if let Some(file) = FtlFiles::get(&format!("{language}.ftl")) {
        // exact match
        return vec![add_file(language, file)];
    }
    let language_without_country_code = language.split_once('_').map_or(language, |(ll, _cc)| ll);
    if language == language_without_country_code {
        // `language` is in `ll` format.
        // In this case, we already know that there is no exact match,
        // so we add all languages starting with `ll_` in arbitrary order.
        // Note that it is important to include the underscore in the pattern, otherwise `ll` might
        // fall back to `llx_CC`, where `llx` is a 3-letter language identifier.
        let ll_prefix = format!("{language}_");
        let mut langs = vec![];
        for lang_name in FtlFiles::iter() {
            if lang_name.starts_with(&ll_prefix) {
                langs.push(add_file(
                    lang_name.strip_suffix(".ftl").unwrap(),
                    FtlFiles::get(&lang_name).unwrap(),
                ));
            }
        }
        return langs;
    } else {
        // If `language` contained a country code, we only try to fall back to a catalog
        // without a country code.
        if let Some(file) = FtlFiles::get(&format!("{language_without_country_code}.ftl")) {
            vec![add_file(language_without_country_code, file)]
        } else {
            vec![]
        }
    }
}

/// Gets the bundle for the fallback language.
/// Panics if the fallback language does not match exactly one available bundle.
fn get_fallback_bundle() -> &'static FluentBundle<FluentResource> {
    let fallback_bundles = add_language(FALLBACK_LANGUAGE);
    assert_eq!(
        fallback_bundles.len(),
        1,
        "add_language must return exactly one bundle for the fallback language"
    );
    fallback_bundles[0]
}

/// Set the order in which languages should be tried for localization.
/// The fallback language (en) will be added to the end of the list.
/// The provided list contains `en`, anything after it will be ignored, since we assume that every
/// message can be localized in English, so there is no point in specifying further fallback
/// options.
/// This function also takes care of lazily loading and parsing data from the embedded FTL files,
/// so no additional initialization is needed.
pub fn set_language_precedence(languages: &[&str]) {
    let fallback_bundle = get_fallback_bundle();
    // Only take the languages preceding the fallback language, since it is assumed that everything
    // is translatable in the fallback language.
    // Filter bundles such that only first occurrence is used, to speed up lookups.
    let mut seen_languages = HashSet::new();
    let mut new_precedence: Vec<_> = languages
        .iter()
        .take_while(|&&lang| lang != FALLBACK_LANGUAGE)
        .flat_map(|&lang| add_language(lang))
        .filter(|bundle| seen_languages.insert(bundle.locales[0].language))
        .collect();
    // Add the fallback language at the end of the precedence list.
    new_precedence.push(fallback_bundle);
    let mut bundle_precedence = LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap();
    *bundle_precedence = new_precedence;
}

/// Use the [`localize!`] macro instead of calling this directly.
pub fn format_localized(
    id: &FluentID,
    args: Option<&FluentArgs>,
) -> Result<Cow<'static, str>, String> {
    let id = id.0;
    let mut errors = vec![];
    let bundle_precedence = LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap();
    for bundle in bundle_precedence.iter() {
        match bundle.get_message(id) {
            Some(message) => {
                let pattern = message.value().expect("Message has no value.");
                let value = bundle.format_pattern(pattern, args, &mut errors);
                // NOTE: Unused arguments are not considered errors.
                if !errors.is_empty() {
                    let mut error_message = format!(
                        "Unexpected formatting errors occurred for message ID \"{id}\" in language \"{}\":\n",
                        bundle.locales[0].language
                    );
                    for error in errors {
                        error_message.push_str(&format!("{error}\n"));
                    }
                    error_message.push_str("\nPlease report this.");
                    return Err(error_message);
                }
                return Ok(value);
            }
            None => {
                continue;
            }
        }
    }
    Err(format!("Message \"{id}\" not available in any catalog."))
}

/// Use this to construct `FluentID`s.
#[macro_export]
#[cfg(feature = "fluent-extract")]
macro_rules! fluent_id {
    ($id:literal) => {
        $crate::FluentID::new(fish_fluent_extraction::fluent_extract!($id))
    };
}
#[macro_export]
#[cfg(not(feature = "fluent-extract"))]
macro_rules! fluent_id {
    ($id:literal) => {
        $crate::FluentID::new($id)
    };
}

/// Takes a string literal or a [`FluentID`].
/// If the argument is a string literal, it is converted to a [`FluentID`],
/// which marks it for extraction.
#[macro_export]
macro_rules! ensure_fluent_id {
    ($id:literal) => {
        $crate::fluent_id!($id)
    };
    ($id:expr) => {
        $id
    };
}

/// Takes a [`FluentID`] and an [`Option<&FluentArgs>`].
/// Returns a localized string according to the current language precedence,
/// or panics if an error occurs.
#[macro_export]
macro_rules! localize_impl {
    ($id:expr, $args:expr) => {
        match $crate::format_localized(&$crate::ensure_fluent_id!($id), $args) {
            Ok(localization) => localization,
            Err(e) => {
                panic!("{e}");
            }
        }
    };
}

/// Call this to localize a message with Fluent.
/// The first argument can be a `&FluentID` or a string literal, which will then be converted to a
/// `&FluentID` implicitly.
/// Note that Fluent attributes are not supported.
/// If arguments need to be passed, this can be done by specifying a `&FluentArgs` value as the
/// second argument, or by adding comma-separated `key: value` pairs.
#[macro_export]
macro_rules! localize {
    // If no args are needed
    ($id:expr) => {
        $crate::localize_impl!($id, None)
    };
    // For passing pre-built `&FluentArgs`
    ($id:expr, $args:expr) => {
        $crate::localize_impl!($id, Some($args))
    };
    // For passing args in `key: value` format
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        {
            let mut args = fluent::FluentArgs::new();
            $(
                args.set($key, $value);
            )*
            $crate::localize_impl!($id, Some(&args))
        }
    };
}

/// Wrapper type to ensure export automation can work.
/// Do not construct this directly.
/// Use the dedicated macro instead.
pub struct FluentID(&'static str);

impl FluentID {
    pub fn new(id: &'static str) -> Self {
        Self(id)
    }
}
