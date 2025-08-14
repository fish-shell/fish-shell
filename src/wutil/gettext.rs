use std::collections::{HashMap, HashSet};
use std::ffi::CStr;
use std::sync::{Mutex, MutexGuard};

use crate::common::str2wcstring;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use once_cell::sync::Lazy;
use rust_embed::Embed;

/// A type that can be either a static or local string.
enum MaybeStatic<'a> {
    Static(&'static wstr),
    Local(&'a wstr),
}

#[derive(Embed)]
#[folder = "$FISH_BUILD_DIR/fish-locale"]
struct MoFiles;

fn add_catalog_from_cstr_map(
    language: String,
    map: HashMap<&CStr, &CStr>,
    catalogs: &mut MutexGuard<Catalogs>,
) {
    let mut static_map = HashMap::with_capacity(map.len());
    let mut static_wstr_from_cstr = |cstr: &CStr| -> &'static wstr {
        let wstring = str2wcstring(cstr.to_bytes());
        if let Some(static_wstr) = catalogs.leaked_messages.get(wstring.as_utfstr()) {
            static_wstr
        } else {
            let static_wstr: &'static wstr =
                wstr::from_char_slice(Box::leak(wstring.as_char_slice().into()));
            catalogs.leaked_messages.insert(static_wstr);
            static_wstr
        }
    };
    for (k, v) in map {
        static_map.insert(static_wstr_from_cstr(k), static_wstr_from_cstr(v));
    }
    catalogs.language_to_catalog.insert(language, static_map);
}

/// `language` must be an ISO 639 language code, optionally followed by an underscore and an ISO
/// 3166 country/territory code.
fn find_mo_file_for_language(
    language: &str,
    catalogs: &mut MutexGuard<Catalogs>,
) -> Option<String> {
    // Try the exact name first.
    // If there already is a corresponding catalog return the language.
    if catalogs.language_to_catalog.contains_key(language) {
        return Some(language.to_owned());
    }
    if let Some(file) = MoFiles::get(language) {
        if let Ok(mo_file_localization_map) = parse_mo::parse_mo_file(&file.data) {
            add_catalog_from_cstr_map(language.to_owned(), mo_file_localization_map, catalogs);
            return Some(language.to_owned());
        }
    }

    let language_without_country_code: String =
        language.chars().take_while(|&c| c != '_').collect();

    // Use the first file whose name starts with the same language code.
    for file_path in MoFiles::iter() {
        if let Some(file_name) = file_path.split('/').next_back() {
            if file_name.starts_with(&language_without_country_code) {
                if catalogs.language_to_catalog.contains_key(file_name) {
                    return Some(file_name.to_owned());
                }
                let file = MoFiles::get(&file_path).unwrap();
                if let Ok(mo_file_localization_map) = parse_mo::parse_mo_file(&file.data) {
                    add_catalog_from_cstr_map(
                        file_name.to_owned(),
                        mo_file_localization_map,
                        catalogs,
                    );
                    return Some(file_name.to_owned());
                }
            }
        }
    }

    // No MO file for the language (and any regional variations) was found.
    None
}

mod parse_mo {

    use std::{collections::HashMap, ffi::CStr};

    fn read_le_u32(bytes: &[u8]) -> u32 {
        u32::from_le_bytes(bytes[..4].try_into().unwrap())
    }

    fn read_be_u32(bytes: &[u8]) -> u32 {
        u32::from_be_bytes(bytes[..4].try_into().unwrap())
    }

    fn get_u32_reader_from_magic_number(magic_number: &[u8]) -> std::io::Result<fn(&[u8]) -> u32> {
        match magic_number {
        [0x95, 0x04, 0x12, 0xde] => Ok(read_be_u32),
        [0xde, 0x12, 0x04, 0x95] => Ok(read_le_u32),
        _ => Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "First 4 bytes of MO file must correspond to magic number 0x950412de, either big or little endian.",
        )),
    }
    }

    /// Returns an error if an unknown major revision is detected.
    /// There are no relevant differences between supported revisions.
    fn check_if_revision_is_supported(revision: u32) -> std::io::Result<()> {
        // From the reference:
        // A program seeing an unexpected major revision number should stop reading the MO file entirely;
        // whereas an unexpected minor revision number means that the file can be read
        // but will not reveal its full contents,
        // when parsed by a program that supports only smaller minor revision numbers.
        let major_revision = revision >> 16;
        match major_revision {
            0 | 1 => {
                // At time of writing, these are the only major revisions which exist.
                // There is no documented difference and the GNU gettext code does not seem to
                // differentiate between the two either.
                // All features we care about are supported in minor revision 0,
                // so we do not need to care about the minor revision.
                Ok(())
            }
            _ => Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Major revision must be 0 or 1",
            )),
        }
    }

    fn parse_strings(
        file_content: &[u8],
        num_strings: usize,
        table_offset: usize,
        read_u32: fn(&[u8]) -> u32,
    ) -> std::io::Result<Vec<&CStr>> {
        let file_too_short_error = Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "MO file is too short.",
        ));
        if table_offset + num_strings * 8 > file_content.len() {
            return file_too_short_error;
        }
        let mut strings = Vec::with_capacity(num_strings);
        let mut offset = table_offset;
        let mut get_next_u32 = || {
            let val = read_u32(&file_content[offset..]);
            offset += 4;
            val
        };
        for _ in 0..num_strings {
            // Does not include NUL terminator.
            let string_length = get_next_u32() as usize;
            let string_offset = get_next_u32() as usize;
            if string_offset + string_length + 1 > file_content.len() {
                return file_too_short_error;
            }
            // Contexts are stored by storing the concatenation of the context, a EOT byte, and the original string, instead of the original string.
            // Contexts are not supported by this implementation.
            // The format allows plural forms to appear behind singular forms, separated by a NUL byte,
            // where `string_length` includes the length of both.
            // This is not supported here.
            // Account for NUL terminator with the + 1.
            strings.push(CStr::from_bytes_with_nul(
            &file_content[string_offset..(string_offset + string_length + 1)],
        ).map_err(|_| std::io::Error::new(std::io::ErrorKind::InvalidData, "String does not contain NUL byte in expected position (missing NUL terminator or NUL in non-last position)"))?);
        }
        Ok(strings)
    }

    /// Parse a MO file.
    /// Format reference used: <https://www.gnu.org/software/gettext/manual/html_node/MO-Files.html>
    pub fn parse_mo_file(file_content: &[u8]) -> std::io::Result<HashMap<&CStr, &CStr>> {
        if file_content.len() < 28 {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "File too short to contain header.",
            ));
        }
        // The first 4 bytes are a magic number, from which the endianness can be determined.
        let read_u32 = get_u32_reader_from_magic_number(&file_content[0..4])?;
        let mut offset = 4;
        let mut get_next_u32 = || {
            let val = read_u32(&file_content[offset..]);
            offset += 4;
            val
        };
        let file_format_revision = get_next_u32();
        check_if_revision_is_supported(file_format_revision)?;
        let num_strings = get_next_u32() as usize;
        let original_strings_offset = get_next_u32() as usize;
        let translation_strings_offset = get_next_u32() as usize;
        let original_strings =
            parse_strings(file_content, num_strings, original_strings_offset, read_u32)?;
        let translated_strings = parse_strings(
            file_content,
            num_strings,
            translation_strings_offset,
            read_u32,
        )?;
        let mut translation_map = HashMap::with_capacity(num_strings);
        for i in 0..num_strings {
            translation_map.insert(original_strings[i], translated_strings[i]);
        }
        Ok(translation_map)
    }
}

struct Catalogs {
    /// Maps from language identifier to catalog.
    language_to_catalog: HashMap<String, HashMap<&'static wstr, &'static wstr>>,
    /// Keeps track of messages which have been leaked, so that we don't need to leak more than
    /// once.
    leaked_messages: HashSet<&'static wstr>,
    /// The precedence list of user-preferred languages, obtained from the relevant environment
    /// variables.
    /// This should be updated when the relevant variables change.
    language_precedence: Vec<String>,
}

impl Catalogs {
    pub fn new() -> Self {
        Self {
            language_to_catalog: HashMap::new(),
            leaked_messages: HashSet::new(),
            language_precedence: Vec::new(),
        }
    }
}

static CATALOGS: Lazy<Mutex<Catalogs>> = Lazy::new(|| Mutex::new(Catalogs::new()));

fn gettext(message: MaybeStatic) -> &'static wstr {
    let key = match message {
        MaybeStatic::Static(s) => s,
        MaybeStatic::Local(s) => s,
    };
    let mut catalogs = CATALOGS.lock().unwrap();

    for lang in &catalogs.language_precedence {
        if let Some(catalog) = catalogs.language_to_catalog.get(lang) {
            if let Some(localization) = catalog.get(key) {
                return localization;
            }
        }
    }
    // No localization found.
    if let Some(static_message) = catalogs.leaked_messages.get(key) {
        static_message
    } else {
        let static_message = match message {
            MaybeStatic::Static(s) => s,
            MaybeStatic::Local(s) => wstr::from_char_slice(Box::leak(s.as_char_slice().into())),
        };
        catalogs.leaked_messages.insert(static_message);
        static_message
    }
}

/// Four environment variables can be used to select languages.
/// A detailed description is available at
/// <https://www.gnu.org/software/gettext/manual/html_node/Setting-the-POSIX-Locale.html>
/// Our does not replicate the behavior exactly.
/// See the following description.
///
/// The first variable in the following list that is set will be used to determine the language
/// preferences and the other variables will be ignored, regardless of localization availability.
/// 1. `LANGUAGE`: This is the only variable which can specify a list of languages. The entries
///    of the list are colon-separated. Each entry identifies a language, with earlier languages
///    having higher precedence. A language is specified by a 2 or 3 letter ISO 639 language
///    code, optionally followed by an underscore and an ISO 3166 country/territory code.
///    If the second part is omitted, some variant of the language will be used if localizations
///    exist for one. We make no guarantees about which variant that will be.
/// 2. `LC_ALL`: a single locale name as described in
///    <https://www.gnu.org/software/gettext/manual/html_node/Locale-Names.html>
/// 3. `LC_MESSAGES`: like `LC_ALL`
/// 4. `LANG`: like `LC_ALL`
///
/// Returns the (possibly empty) preference list of languages.
fn get_language_preferences_from_env() -> Vec<String> {
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

    if let Ok(langs) = std::env::var("LANGUAGE") {
        return langs.split(':').map(normalize_locale_name).collect();
    }
    if let Ok(locale) = std::env::var("LC_ALL")
        .or_else(|_| std::env::var("LC_MESSAGES").or_else(|_| std::env::var("LANG")))
    {
        if locale.starts_with('C') {
            // Do not localize in the C locale.
            vec![]
        } else {
            vec![normalize_locale_name(&locale)]
        }
    } else {
        vec![]
    }
}

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
pub fn update_locale_from_env() {
    let mut catalogs = CATALOGS.lock().unwrap();
    catalogs.language_precedence = get_language_preferences_from_env()
        .iter()
        .filter_map(|lang| find_mo_file_for_language(lang, &mut catalogs))
        .collect();
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
macro_rules! localizable_string {
    ($string:literal) => {
        $crate::wutil::gettext::LocalizableString::Static(widestring::utf32str!(
            fish_gettext_extraction::gettext_extract!($string)
        ))
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
