use crate::env::{EnvStack, Environment};

#[macro_export]
macro_rules! write_newline_to_fd {
    ($fd:expr) => {{
        let _ = $crate::wutil::write_to_fd(b"\n", $fd);
    }};
}
pub use write_newline_to_fd;

#[macro_export]
macro_rules! fprint {
    ($fd:expr, $output:expr) => {{
        let _ = $crate::wutil::unescape_and_write_to_fd($output, $fd);
    }};
}
pub use fprint;

#[macro_export]
macro_rules! localized_fprint {
    ($fd:expr, $id:expr) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id))
    };
    ($fd:expr, $id:expr, $args:expr) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id, $args))
    };
    ($fd:expr, $id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id, $($key: $value),*))
    };
}
pub use localized_fprint;

#[macro_export]
macro_rules! localized_fprintln {
    ($fd:expr, $id:expr) => {{
        $crate::localization::localized_fprint!($fd, $id);
        $crate::localization::write_newline_to_fd!($fd);
    }};
    ($fd:expr, $id:expr, $args:expr) => {{
        $crate::localization::localized_fprint!($fd, $id, $args);
        $crate::localization::write_newline_to_fd!($fd);
    }};
    ($fd:expr, $id:expr, $($key:literal: $value:expr),+ $(,)?) => {{
        $crate::localization::localized_fprint!($fd, $id, $($key: $value),*);
        $crate::localization::write_newline_to_fd!($fd);
    }};
}
pub use localized_fprintln;

#[macro_export]
macro_rules! localized_print {
    ($id:expr) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id)
    };
    ($id:expr, $args:expr) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id, $args)
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id, $($key: $value),*)
    };
}
pub use localized_print;

#[macro_export]
macro_rules! localized_println {
    ($id:expr) => {{
        let fd = libc::STDOUT_FILENO;
        $crate::localization::localized_fprint!(fd, $id);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $args:expr) => {{
        let fd = libc::STDOUT_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $args);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {{
        let fd = libc::STDOUT_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $($key: $value),*);
        $crate::localization::write_newline_to_fd!(fd);
    }};
}
pub use localized_println;

#[macro_export]
macro_rules! localized_eprint {
    ($id:expr) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id)
    };
    ($id:expr, $args:expr) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id, $args)
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id, $($key: $value),*)
    };
}
pub use localized_eprint;

#[macro_export]
macro_rules! localized_eprintln {
    ($id:expr) => {{
        let fd = libc::STDERR_FILENO;
        $crate::localization::localized_fprint!(fd, $id);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $args:expr) => {{
        let fd = libc::STDERR_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $args);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {{
        let fd = libc::STDERR_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $($key: $value),*);
        $crate::localization::write_newline_to_fd!(fd);
    }};
}
pub use localized_eprintln;

#[macro_export]
macro_rules! localized_sprint {
    ($id:expr) => {
        fish_fluent::localize!($id)
    };
    ($id:expr, $args:expr) => {
        fish_fluent::localize!($id, $args)
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        fish_fluent::localize!($id, $($key: $value),*)
    };
}
pub use localized_sprint;

#[macro_export]
macro_rules! sprintln {
    ($input:expr) => {{
        let mut res = $input.into_owned();
        res.push('\n');
        res
    }};
}
pub use sprintln;

#[macro_export]
macro_rules! localized_sprintln {
    ($id:expr) => {
        $crate::localization::sprintln!(fish_fluent::localize!($id))
    };
    ($id:expr, $args:expr) => {
        $crate::localization::sprintln!(fish_fluent::localize!($id, $args))
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::sprintln!(fish_fluent::localize!($id, $($key: $value),*))
    };
}
pub use localized_sprintln;

/// Four environment variables can be used to select languages.
/// A detailed description is available at
/// <https://www.gnu.org/software/gettext/manual/html_node/Setting-the-POSIX-Locale.html>
/// Our does not replicate the behavior exactly.
/// See the following description.
///
/// There are three variables which can be used for setting the locale for messages:
/// 1. `LC_ALL`
/// 2. `LC_MESSAGES`
/// 3. `LANG`
///
/// The value of the first one set to a non-zero value will be considered.
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
pub fn set_language_precedence_from_env(vars: &EnvStack) {
    let language_precedence = get_language_preferences_from_env(vars);
    let languages = language_precedence
        .iter()
        .map(|lang| lang.as_str())
        .collect::<Vec<&str>>();
    fish_fluent::set_language_precedence(&languages);
    #[cfg(feature = "localize-messages")]
    crate::wutil::gettext::set_language_precedence(&languages);
}

/// This function only exists to provide a way for initializing localization before an [`EnvStack`]
/// is available. Without this, early error messages cannot be localized.
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

    #[test]
    fn without_args() {
        let id = fluent_id!("test");
        localized_print!(id);
        localized_println!(id);
        localized_eprint!(id);
        localized_eprintln!(id);
        localized_fprint!(libc::STDOUT_FILENO, id);
        localized_fprintln!(libc::STDOUT_FILENO, id);
        assert_eq!(localized_sprint!(id), "This is a test");
        assert_eq!(localized_sprintln!(id), "This is a test\n");
    }

    #[test]
    fn with_args() {
        let id = fluent_id!("test-with-args");
        localized_print!(id, "first": 1, "second": "two");
        localized_println!(id, "first": 1, "second": "two");
        localized_eprint!(id, "first": 1, "second": "two");
        localized_eprintln!(id, "first": 1, "second": "two");
        localized_fprint!(libc::STDOUT_FILENO, id, "first": 1, "second": "two");
        localized_fprintln!(libc::STDOUT_FILENO, id, "first": 1, "second": "two");
        assert_eq!(
            localized_sprint!(id, "first": 1, "second": "two"),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}"
        );
        assert_eq!(
            localized_sprintln!(id, "first": 1, "second": "two"),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}\n"
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
        assert_eq!(
            localized_sprint!(id, &args),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}"
        );
        assert_eq!(
            localized_sprintln!(id, &args),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}\n"
        );
    }
}
