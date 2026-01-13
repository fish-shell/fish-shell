use std::{
    borrow::Cow,
    collections::{HashMap, HashSet},
    fmt::Write as _,
    sync::{LazyLock, Mutex},
};

use cfg_if::cfg_if;
use fish_localization::{
    DEFAULT_LANGUAGE, Language, LocalizationLanguage, define_localization_language_type,
};
use fluent::{
    FluentArgs, FluentResource, FluentValue, concurrent::FluentBundle, types::FluentNumber,
};
use rust_embed::RustEmbed;

#[cfg(feature = "fluent-extract")]
pub extern crate fish_fluent_extraction;

use unic_langid::LanguageIdentifier;

type Bundle = &'static FluentBundle<FluentResource>;

type NamedBundle = (Language<'static>, Bundle);
pub type LocalizedMessage = Cow<'static, str>;

static LANGUAGE_BUNDLES: LazyLock<Mutex<HashMap<Language, Bundle>>> =
    LazyLock::new(|| Mutex::new(HashMap::new()));

static DEFAULT_BUNDLE: LazyLock<NamedBundle> =
    LazyLock::new(|| (DEFAULT_LANGUAGE, make_bundle(DEFAULT_LANGUAGE)));

static LANGUAGE_BUNDLE_PRECEDENCE: LazyLock<Mutex<Vec<NamedBundle>>> =
    LazyLock::new(|| Mutex::new(vec![*DEFAULT_BUNDLE]));

cfg_if!(
    if #[cfg(feature = "localize-messages")] {
        #[derive(RustEmbed)]
        #[folder = "../../localization/fluent/"]
        #[include = "*.ftl"]
        struct FtlFiles;
    } else {
        #[derive(RustEmbed)]
        #[folder = "../../localization/fluent/"]
        #[include = "en.ftl"]
        struct FtlFiles;
    }
);

define_localization_language_type! {FluentLocalizationLanguage}

static AVAILABLE_LANGUAGES: LazyLock<HashSet<FluentLocalizationLanguage>> = LazyLock::new(|| {
    HashSet::from_iter(FtlFiles::iter().map(|language| {
        let suffix = ".ftl";
        let language = Language(match language {
            Cow::Borrowed(language) => language.strip_suffix(suffix).unwrap(),
            Cow::Owned(mut language) => {
                assert!(language.ends_with(suffix));
                language.truncate(language.len() - suffix.len());
                Box::leak(Box::new(language))
            }
        });
        FluentLocalizationLanguage(language)
    }))
});

pub fn get_available_languages() -> &'static HashSet<FluentLocalizationLanguage> {
    &AVAILABLE_LANGUAGES
}

fn read_ftl_file(path: &str) -> String {
    let file = FtlFiles::get(path)
        .unwrap_or_else(|| panic!("Tried to get FTL file {path} which does not exist."));
    String::from_utf8(Vec::from(file.data))
        .unwrap_or_else(|e| panic!("Content of {path} is not valid UTF-8: {e}"))
}

fn make_bundle(language: Language<'static>) -> Bundle {
    let langid: LanguageIdentifier = language
        .parse()
        .map_err(|e| format!("Failed to parse language identifier {language}: {e}"))
        .unwrap();
    let mut bundle = FluentBundle::new_concurrent(vec![langid]);
    let file_data = read_ftl_file(&format!("{language}.ftl"));
    // Error handling could use fluent_ftl_tools::display_parse_errors(), but that would require
    // making the crate a regular dependency.
    match FluentResource::try_new(file_data) {
        Ok(res) => {
            bundle
                .add_resource(res)
                .map_err(|e| {
                    format!(
                        "Failed to add FTL resources to the bundle for language {language}: {e:?}"
                    )
                })
                .unwrap();
            // Isolation marks can result in undesirable behavior if the terminal does not support
            // them properly.
            // Turn this off for now.
            // If we add detection for terminal support, we could enable it conditionally.
            // Without these marks, text order can be incorrect when right-to-left characters are
            // involved.
            // https://www.w3.org/International/questions/qa-bidi-unicode-controls
            // https://github.com/fish-shell/fish-shell/pull/11928#discussion_r2488850606
            bundle.set_use_isolating(false);
            // Leak to create static reference.
            Box::leak(Box::new(bundle))
        }
        Err((_resource, errors)) => {
            let mut error_string = format!("Errors parsing FTL file for {language}:\n");
            for error in errors {
                let _ = writeln!(error_string, "{error}");
            }
            panic!("{error_string}");
        }
    }
}

/// Set the order in which languages should be tried for localization.
/// The default language (en) will be added to the end of the list.
/// If the provided list contains `en`, anything after it will be ignored, since we assume that every
/// message can be localized in English, so there is no point in specifying further fallback
/// options.
/// This function also takes care of lazily loading and parsing data from the embedded FTL files,
/// so no additional initialization is needed.
pub fn set_language_precedence(precedence: &[FluentLocalizationLanguage]) {
    let new_precedence = {
        let mut bundles = LANGUAGE_BUNDLES.lock().unwrap();
        // Only take the languages preceding the default language, since it is assumed that everything
        // is translatable in the default language.
        let mut new_precedence: Vec<_> = precedence
            .iter()
            .take_while(|&lang| Language::from(lang) != DEFAULT_LANGUAGE)
            .map(|lang| {
                let language = lang.into();
                // If a bundle already exists for the language, use it.
                // Otherwise create a new one and cache it.
                let bundle = bundles.get(&language).copied().unwrap_or_else(|| {
                    let bundle = make_bundle(language);
                    bundles.insert(language, bundle);
                    bundle
                });
                (language, bundle)
            })
            .collect();
        // Add the default language at the end of the precedence list.
        new_precedence.push(*DEFAULT_BUNDLE);
        new_precedence
    };
    *LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap() = new_precedence;
}

pub fn get_language_precedence() -> Vec<Language<'static>> {
    let language_precedence = LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap();
    language_precedence.iter().map(|&(lang, _)| lang).collect()
}

/// Use the [`localize!`] macro instead of calling this directly.
/// Panics on errors.
pub fn format_localized(id: &str, args: &FluentArgs) -> LocalizedMessage {
    let mut errors = vec![];
    let bundle_precedence = LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap();
    for (_, bundle) in bundle_precedence.iter() {
        let Some(message) = bundle.get_message(id) else {
            continue;
        };
        let pattern = message.value().expect("Message has no value.");
        let value = bundle.format_pattern(pattern, Some(args), &mut errors);
        // NOTE: Unused arguments are not considered errors.
        if !errors.is_empty() {
            let mut error_message = format!(
                "Unexpected formatting errors occurred for message ID '{id}' in language '{}':\n",
                bundle.locales[0].language
            );
            for error in errors {
                let _ = writeln!(error_message, "{error}");
            }
            panic!("{error_message}");
        }
        return value;
    }
    panic!("Message '{id}' not available in any catalog.")
}

/// Call this to localize a message with Fluent.
/// The first argument is a string literal, which defines a Fluent message ID.
/// It is followed by a mandatory `=` and the English version of the message as a string literal.
/// The message definition must be valid Fluent syntax. Specifically it must be permissible as the
/// definition of a Fluent message.
/// In the simplest case, it will just be a regular string.
/// If Fluent variables should be used, they need to appear in the message definition, e.g.
/// `{ $example_variable }`. Then, the variable must also be specified as a key-value pair in the
/// arguments of the [`localize!`] macro, as demonstrated in the example below.
/// The key is the Fluent variable name, which also needs to be a syntactically valid Rust
/// identifier, and the value is whatever the variable should be replaced by when formatting the
/// localized message.
/// It is considered an error if the variables specified in the message definition do not match the
/// variables specified via keys in subsequent arguments to the macro.
/// The order of key-value pairs can be chosen and modified arbitrarily, and variables may appear
/// more than once in the message definition.
///
/// ```
/// # use fish_fluent::localize;
/// # // Note that `localize!` usage in doc-texts is not checked.
/// let example_message = localize!("test-with-args" = "Two arguments: { $first }, { $second }", first = "foo", second = 42);
/// assert_eq!(example_message, "Two arguments: foo, 42");
/// ```
///
/// Note that changing the message ID or the message definition has consequences for translations.
/// If such a change is made, our tooling will automatically detect it and require the developer
/// making the change to specify what should happen to translations.
/// See `cargo xtask fluent resolve-outdated`.
#[macro_export]
macro_rules! localize {
    ($id:literal = $message:literal $(, $key:ident = $value:expr)* $(,)?) => {
        {
            use $crate::ToFluentValue as _;
            #[cfg(feature = "fluent-extract")]
            fish_fluent_extraction::fluent_extract!($id = $message $(, $key)*);
            let mut args = fluent::FluentArgs::new();
            $(
                args.set(stringify!($key), $value.to_fluent_value());
            )*
            $crate::format_localized($id, &args)
        }
    };
}

/// Define a function calling [`localize!`] and returning the result.
/// This macro can be used when multiple locations need access to the same message.
/// Do not use [`localize!`] with the same message ID more than once.
/// Instead, define a function which takes the Fluent variables of the message as arguments and
/// internally calls [`localize!`]. Then, use this function wherever the message is needed.
/// This macro helps with avoiding some boilerplate. Its first argument is the name of the function
/// which should be defined. Then, a key-value pair specifying the message ID and English definition
/// follows, in the same format as for [`localize!`]. The remaining arguments are the names of
/// Fluent variables which appear in the message definition. These will become the function's
/// arguments names and they will be used in the internal [`localize!`] call.
#[macro_export]
macro_rules! localize_fn {
    ($vis:vis $fn:ident, $id:literal = $message:literal $(, $key:ident )* $(,)?) => {
        $vis fn $fn<'a>($($key: impl $crate::ToFluentValue<'a>),*) -> $crate::LocalizedMessage {
            localize!(
                $id = $message,
                $($key = $key),*
            )
        }
    };
}

/// Trait to account for types which don't have a `Into<FluentValue>` implementation, i.e.
/// widestrings.
/// Can be removed once we no longer use such types.
pub trait ToFluentValue<'a> {
    fn to_fluent_value(self) -> FluentValue<'a>;
}

macro_rules! impl_to_fluent_value_wrapper {
    ($($t:ty),* $(,)?) => {
        $(
            impl<'a> ToFluentValue<'a> for $t {
                fn to_fluent_value(self) -> FluentValue<'a> {
                    self.into()
                }
            }
        )*
    };
}
impl_to_fluent_value_wrapper! {
    FluentNumber,
    String,
    f32, f64,
    i8, i16, i32, i64, i128, isize,
    u8, u16, u32, u64, u128, usize,
}

macro_rules! impl_to_fluent_value_wrapper_ref {
    ($($t:ty),* $(,)?) => {
        $(
            impl<'a> ToFluentValue<'a> for &'a $t {
                fn to_fluent_value(self) -> FluentValue<'a> {
                    self.into()
                }
            }
        )*
    };
}
impl_to_fluent_value_wrapper_ref! {
    String, str,
    f32, f64,
    i8, i16, i32, i64, i128, isize,
    u8, u16, u32, u64, u128, usize,
}

impl<'a, T: Into<FluentValue<'a>>> ToFluentValue<'a> for Option<T> {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.into()
    }
}

impl<'a> ToFluentValue<'a> for char {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.to_string().into()
    }
}

impl<'a> ToFluentValue<'a> for &char {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.to_string().into()
    }
}

impl<'a> ToFluentValue<'a> for widestring::Utf32String {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.to_string().into()
    }
}

impl<'a> ToFluentValue<'a> for &widestring::Utf32String {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.to_string().into()
    }
}

impl<'a> ToFluentValue<'a> for &widestring::Utf32Str {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.to_string().into()
    }
}

#[cfg(test)]
mod tests {
    use super::{DEFAULT_LANGUAGE, FtlFiles};
    use crate::read_ftl_file;
    use fluent_ftl_tools::consistency::check_all_resources;

    #[test]
    fn test_simple_message() {
        let message_key_value = localize!(
            "test-with-args" = "Two arguments: { $first }, { $second }",
            first = "foo",
            second = 42,
        );
        assert_eq!(message_key_value, "Two arguments: foo, 42");
    }

    #[test]
    fn check_ftl_files() {
        let default_resource_name = format!("{DEFAULT_LANGUAGE}.ftl");
        let default_string = read_ftl_file(&default_resource_name);
        let other_resources = FtlFiles::iter()
            .map(|file_path| {
                let file_string = read_ftl_file(&file_path);
                (file_path.to_string(), file_string)
            })
            .collect::<Vec<_>>();
        check_all_resources((&default_resource_name, default_string), other_resources)
            .unwrap_or_else(|e| panic!("FTL resource checks failed:\n{e}"));
    }
}
