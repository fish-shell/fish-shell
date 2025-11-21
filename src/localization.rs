#[cfg(feature = "localize-messages")]
use crate::env::{EnvStack, Environment};

#[macro_export]
macro_rules! fprint {
    ($fd:expr, $output:expr) => {{
        let _ = $crate::wutil::unescape_bytes_and_write_to_fd($output, $fd);
    }};
}
pub use fprint;

#[macro_export]
macro_rules! localized_fprint {
    ($fd:expr, $id:expr, $args:expr) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id, $args))
    };
    ($fd:expr, $id:expr $(, $key:ident = $value:expr)* $(,)?) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id $(, $key = $value)*))
    };
}
pub use localized_fprint;

#[macro_export]
macro_rules! localized_fprintln {
    ($fd:expr, $id:expr, $args:expr) => {{
        $crate::localization::localized_fprint!($fd, $id, $args);
        $crate::wutil::write_newline_to_fd($fd);
    }};
    ($fd:expr, $id:expr $(, $key:ident = $value:expr)* $(,)?) => {{
        $crate::localization::localized_fprint!($fd, $id $(, $key = $value)*);
        $crate::wutil::write_newline_to_fd($fd);
    }};
}
pub use localized_fprintln;

#[macro_export]
macro_rules! localized_print {
    ($id:expr, $args:expr) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id, $args)
    };
    ($id:expr $(, $key:ident = $value:expr)* $(,)?) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id $(, $key = $value)*)
    };
}
pub use localized_print;

#[macro_export]
macro_rules! localized_println {
    ($id:expr, $args:expr) => {{
        $crate::localization::localized_fprintln!(libc::STDOUT_FILENO, $id, $args);
    }};
    ($id:expr $(,$key:ident = $value:expr)* $(,)?) => {{
        $crate::localization::localized_fprintln!(libc::STDOUT_FILENO, $id $(, $key = $value)*);
    }};
}
pub use localized_println;

#[macro_export]
macro_rules! localized_eprint {
    ($id:expr, $args:expr) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id, $args)
    };
    ($id:expr $(, $key:ident = $value:expr)* $(,)?) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id $(, $key = $value)*)
    };
}
pub use localized_eprint;

#[macro_export]
macro_rules! localized_eprintln {
    ($id:expr, $args:expr) => {{
        $crate::localization::localized_fprintln!(libc::STDERR_FILENO, $id, $args);
    }};
    ($id:expr $(, $key:ident = $value:expr)* $(,)?) => {{
        $crate::localization::localized_fprintln!(libc::STDERR_FILENO, $id $(, $key = $value)*);
    }};
}
pub use localized_eprintln;

#[macro_export]
macro_rules! localized_format {
    ($id:expr, $args:expr) => {
        fish_fluent::localize!($id, $args)
    };
    ($id:expr $(, $key:ident = $value:expr)* $(,)?) => {
        fish_fluent::localize!($id $(, $key = $value)*)
    };
}
pub use localized_format;

pub fn append_newline<S: Into<String>>(s: S) -> String {
    let mut s: String = s.into();
    s.push('\n');
    s
}

#[macro_export]
macro_rules! localized_formatln {
    ($id:expr, $args:expr) => {
        $crate::localization::append_newline(fish_fluent::localize!($id, $args))
    };
    ($id:expr $(, $key:ident = $value:expr)* $(,)?) => {
        $crate::localization::append_newline(fish_fluent::localize!($id $(, $key = $value)*))
    };
}
pub use localized_formatln;

/// Four environment variables can be used to select languages.
/// A detailed description is available at
/// <https://www.gnu.org/software/gettext/manual/html_node/Setting-the-POSIX-Locale.html>
/// Our implementation does not replicate the behavior exactly.
/// See the following description.
///
/// There are three variables which can be used for setting the locale for messages:
/// 1. `LC_ALL`
/// 2. `LC_MESSAGES`
/// 3. `LANG`
///
/// The value of the first one set to a non-empty value will be considered.
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
#[cfg(feature = "localize-messages")]
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

/// Call this when one of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` changes.
/// Updates internal state such that the correct localizations will be used in subsequent
/// localization requests.
#[cfg(feature = "localize-messages")]
pub fn set_language_precedence_from_env(vars: &EnvStack) {
    let language_precedence = get_language_preferences_from_env(vars);
    let languages = language_precedence
        .iter()
        .map(|lang| lang.as_str())
        .collect::<Vec<&str>>();
    fish_fluent::set_language_precedence(&languages);
    crate::wutil::gettext::set_language_precedence(&languages);
}

/// This function only exists to provide a way for initializing localization before an [`EnvStack`]
/// is available. Without this, early error messages cannot be localized.
#[cfg(feature = "localize-messages")]
pub fn initialize_localization() {
    fn build_locale_env_stack() -> EnvStack {
        use crate::{L, env_stack_set_from_env};
        let locale_vars = EnvStack::new();
        env_stack_set_from_env!(locale_vars, "LANGUAGE");
        env_stack_set_from_env!(locale_vars, "LC_ALL");
        env_stack_set_from_env!(locale_vars, "LC_MESSAGES");
        env_stack_set_from_env!(locale_vars, "LANG");
        locale_vars
    }
    set_language_precedence_from_env(&build_locale_env_stack());
}

#[cfg(test)]
mod tests {
    use fish_fluent::fluent_id;
    use serial_test::serial;

    #[test]
    #[serial]
    fn without_args() {
        let id = fluent_id!("test");
        localized_print!(id);
        localized_println!(id);
        localized_eprint!(id);
        localized_eprintln!(id);
        localized_fprint!(libc::STDOUT_FILENO, id);
        localized_fprintln!(libc::STDOUT_FILENO, id);
        assert_eq!(localized_format!(id), "This is a test");
        assert_eq!(localized_formatln!(id), "This is a test\n");
    }

    #[test]
    #[serial]
    fn with_args() {
        let id = fluent_id!("test-with-args");
        localized_print!(id, first = 1, second = "two");
        localized_println!(id, first = 1, second = "two");
        localized_eprint!(id, first = 1, second = "two");
        localized_eprintln!(id, first = 1, second = "two");
        localized_fprint!(libc::STDOUT_FILENO, id, first = 1, second = "two");
        localized_fprintln!(libc::STDOUT_FILENO, id, first = 1, second = "two");
        assert_eq!(
            localized_format!(id, first = 1, second = "two"),
            "Two arguments: 1, two"
        );
        assert_eq!(
            localized_formatln!(id, first = 1, second = "two"),
            "Two arguments: 1, two\n"
        );

        let mut args = fluent::FluentArgs::new();
        args.set("first", 1);
        args.set("second", "two");

        localized_print!(id, &args);
        localized_println!(id, &args);
        localized_eprint!(id, &args);
        localized_eprintln!(id, &args);
        localized_fprint!(libc::STDOUT_FILENO, id, &args);
        localized_fprintln!(libc::STDOUT_FILENO, id, &args);
        assert_eq!(localized_format!(id, &args), "Two arguments: 1, two");
        assert_eq!(localized_formatln!(id, &args), "Two arguments: 1, two\n");
    }
}
