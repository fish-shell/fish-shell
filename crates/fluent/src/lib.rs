use std::{
    borrow::Cow,
    collections::HashMap,
    fmt::Write as _,
    sync::{LazyLock, Mutex, MutexGuard},
};

use cfg_if::cfg_if;
use fish_build_helper::FALLBACK_LANGUAGE;
use fluent::{
    FluentArgs, FluentResource, FluentValue, concurrent::FluentBundle, types::FluentNumber,
};
use rust_embed::RustEmbed;

#[cfg(feature = "fluent-extract")]
pub extern crate fish_fluent_extraction;

use unic_langid::LanguageIdentifier;

type Bundle = &'static FluentBundle<FluentResource>;
type NamedBundle = (&'static str, Bundle);

static LANGUAGE_BUNDLES: LazyLock<Mutex<HashMap<&'static str, Bundle>>> =
    LazyLock::new(|| Mutex::new(HashMap::new()));

static LANGUAGE_BUNDLE_PRECEDENCE: LazyLock<Mutex<Vec<NamedBundle>>> = LazyLock::new(|| {
    let mut bundles = LANGUAGE_BUNDLES.lock().unwrap();
    Mutex::new(vec![get_bundle(FALLBACK_LANGUAGE, &mut bundles)])
});

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

#[derive(Clone, Copy)]
pub struct FluentLocalizationLanguage {
    language: &'static str,
}

static AVAILABLE_LANGUAGES: LazyLock<HashMap<&'static str, FluentLocalizationLanguage>> =
    LazyLock::new(|| {
        HashMap::from_iter(FtlFiles::iter().map(|language| {
            let suffix = ".ftl";
            let language: &'static str = match language {
                Cow::Borrowed(language) => language.strip_suffix(suffix).unwrap(),
                Cow::Owned(mut language) => {
                    assert!(language.ends_with(suffix));
                    language.truncate(language.len() - suffix.len());
                    Box::leak(Box::new(language))
                }
            };
            (language, FluentLocalizationLanguage { language })
        }))
    });

pub fn get_available_languages() -> &'static HashMap<&'static str, FluentLocalizationLanguage> {
    &AVAILABLE_LANGUAGES
}

fn get_bundle(
    language: &'static str,
    bundles: &mut MutexGuard<HashMap<&'static str, Bundle>>,
) -> NamedBundle {
    if let Some(bundle) = bundles.get(language) {
        return (language, bundle);
    }
    let langid: LanguageIdentifier = language
        .parse()
        .map_err(|e| format!("Failed to parse language identifier {language}: {e}"))
        .unwrap();
    let mut bundle = FluentBundle::new_concurrent(vec![langid]);
    let file = FtlFiles::get(&format!("{language}.ftl"))
        .expect("Tried to get bundle for language with no corresponding FTL file.");
    // Error handling could use fluent_ftl_tools::display_parse_errors(), but that would require
    // making the crate a regular dependency.
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
            let bundle = Box::leak(Box::new(bundle));
            bundles.insert(language, bundle);
            (language, bundle)
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
/// The fallback language (en) will be added to the end of the list.
/// If the provided list contains `en`, anything after it will be ignored, since we assume that every
/// message can be localized in English, so there is no point in specifying further fallback
/// options.
/// This function also takes care of lazily loading and parsing data from the embedded FTL files,
/// so no additional initialization is needed.
pub fn set_language_precedence(precedence: &[FluentLocalizationLanguage]) {
    let new_precedence = {
        let mut bundles = LANGUAGE_BUNDLES.lock().unwrap();
        let fallback_bundle = get_bundle(FALLBACK_LANGUAGE, &mut bundles);
        // Only take the languages preceding the fallback language, since it is assumed that everything
        // is translatable in the fallback language.
        let mut new_precedence: Vec<_> = precedence
            .iter()
            .take_while(|&&lang| lang.language != FALLBACK_LANGUAGE)
            .map(|&lang| get_bundle(lang.language, &mut bundles))
            .collect();
        // Add the fallback language at the end of the precedence list.
        new_precedence.push(fallback_bundle);
        new_precedence
    };
    *LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap() = new_precedence;
}

pub fn get_language_precedence() -> Vec<&'static str> {
    let language_precedence = LANGUAGE_BUNDLE_PRECEDENCE.lock().unwrap();
    language_precedence.iter().map(|&(lang, _)| lang).collect()
}

/// Use the [`localize!`] macro instead of calling this directly.
/// Panics on errors.
pub fn format_localized(id: &FluentID, args: &FluentArgs) -> Cow<'static, str> {
    let id = id.0;
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

cfg_if!(
    if #[cfg(feature = "fluent-extract")] {
        /// Use this to construct `FluentID`s.
        #[macro_export]
        macro_rules! fluent_id {
            ($id:literal) => {
                $crate::FluentID::new(fish_fluent_extraction::fluent_extract!($id))
            };
        }
    } else {
        #[macro_export]
        macro_rules! fluent_id {
            ($id:literal) => {
                $crate::FluentID::new($id)
            };
        }
    }
);

/// Declare an arbitrary number of [`FluentID`]s.
/// Useful for IDs which are used in multiple locations.
#[macro_export]
macro_rules! fluent_ids {
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
            $vis const $name: $crate::FluentID =
                $crate::fluent_id!($string);
        )*
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
        $crate::format_localized(&$crate::ensure_fluent_id!($id), $args)
    };
}

/// Call this to localize a message with Fluent.
/// The first argument can be a `&FluentID` or a string literal, which will then be converted to a
/// `&FluentID` implicitly.
/// Note that Fluent attributes are not supported.
/// If arguments need to be passed, this can be done by specifying a `&FluentArgs` value as the
/// second argument, or by adding comma-separated `key = value` pairs,
/// where the key is syntactically a Rust identifier.
///
/// ```
/// # use fish_fluent::localize;
/// let mut args = fluent::FluentArgs::new();
/// args.set("first", "foo");
/// args.set("second", 42);
/// let message_fluent_args = localize!("test-with-args", &args);
/// let message_key_value = localize!("test-with-args", first = "foo", second = 42);
/// assert_eq!(message_fluent_args, message_key_value);
/// ```
#[macro_export]
macro_rules! localize {
    // For passing pre-built `&FluentArgs`
    ($id:expr, $args:expr) => {
        $crate::localize_impl!($id, $args)
    };
    // For passing args in `key = value` format (or no arguments)
    ($id:expr $(, $key:ident = $value:expr)* $(,)?) => {
        {
            use fish_fluent::ToFluentValue;
            let mut args = fluent::FluentArgs::new();
            $(
                args.set(stringify!($key), $value.to_fluent_value());
            )*
            $crate::localize_impl!($id, &args)
        }
    };
}

/// Wrapper type which must not be constructed directly, only via the [`fluent_id!`] macro.
/// This ensures that we can extract the IDs from the source code via a proc macro when needed.
pub struct FluentID(&'static str);

impl FluentID {
    pub const fn new(id: &'static str) -> Self {
        Self(id)
    }
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

impl<'a> ToFluentValue<'a> for Cow<'a, str> {
    fn to_fluent_value(self) -> FluentValue<'a> {
        FluentValue::String(self)
    }
}

impl<'a, T: Into<FluentValue<'a>>> ToFluentValue<'a> for Option<T> {
    fn to_fluent_value(self) -> FluentValue<'a> {
        self.into()
    }
}

impl<'a> ToFluentValue<'a> for char {
    fn to_fluent_value(self) -> FluentValue<'a> {
        FluentValue::String(self.to_string().into())
    }
}

impl<'a> ToFluentValue<'a> for &char {
    fn to_fluent_value(self) -> FluentValue<'a> {
        FluentValue::String(self.to_string().into())
    }
}

impl<'a> ToFluentValue<'a> for widestring::Utf32String {
    fn to_fluent_value(self) -> FluentValue<'a> {
        FluentValue::String(self.to_string().into())
    }
}

impl<'a> ToFluentValue<'a> for &widestring::Utf32String {
    fn to_fluent_value(self) -> FluentValue<'a> {
        FluentValue::String(self.to_string().into())
    }
}

impl<'a> ToFluentValue<'a> for &widestring::Utf32Str {
    fn to_fluent_value(self) -> FluentValue<'a> {
        FluentValue::String(self.to_string().into())
    }
}

#[cfg(test)]
mod tests {
    use super::{FALLBACK_LANGUAGE, FtlFiles};
    use fluent::FluentResource;
    use fluent_ftl_tools::{consistency::check_all_resources, display_parse_errors};

    #[test]
    fn check_ftl_files() {
        let default_resource_name = format!("{FALLBACK_LANGUAGE}.ftl");
        let default = FtlFiles::get(&default_resource_name)
            .expect("Could not get FTL file for fallback language.");
        let default_string = String::from_utf8(Vec::from(default.data)).unwrap_or_else(|e| {
            panic!("Content of {default_resource_name} is not valid UTF-8: {e}")
        });
        let default_resource =
            FluentResource::try_new(default_string).unwrap_or_else(|(resource, errors)| {
                panic!(
                    "Failed to parse {default_resource_name}:\n{}",
                    display_parse_errors(&errors, resource.source())
                )
            });
        let other_resources = FtlFiles::iter()
            .map(|file_path| {
                let file_string =
                    String::from_utf8(Vec::from(FtlFiles::get(&file_path).unwrap().data))
                        .unwrap_or_else(|e| {
                            panic!("Content of {file_path} is not valid UTF-8: {e}")
                        });
                let resource =
                    FluentResource::try_new(file_string).unwrap_or_else(|(resource, errors)| {
                        panic!(
                            "Failed to parse {file_path}:\n{}",
                            display_parse_errors(&errors, resource.source())
                        )
                    });
                (file_path.to_string(), resource)
            })
            .collect::<Vec<_>>();
        check_all_resources(
            (&default_resource_name, &default_resource),
            &other_resources,
        )
        .unwrap_or_else(|e| panic!("FTL resource checks failed:\n{e}"));
    }
}
