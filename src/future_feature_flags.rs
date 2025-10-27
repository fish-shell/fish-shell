//! Flags to enable upcoming features

use crate::wchar::prelude::*;
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering;

#[cfg(test)]
use std::cell::RefCell;

/// The list of flags.
#[repr(u8)]
#[derive(Clone, Copy)]
pub enum FeatureFlag {
    /// Whether ^ is supported for stderr redirection.
    stderr_nocaret,

    /// Whether ? is supported as a glob.
    qmark_noglob,

    /// Whether string replace -r double-unescapes the replacement.
    string_replace_backslash,

    /// Whether "&" is not-special if followed by a word character.
    ampersand_nobg_in_token,
    /// Whether "%self" is expanded to fish's pid
    remove_percent_self,

    /// Remove `test`'s one and zero arg mode (make `test -n` return false etc)
    test_require_arg,

    /// Whether to write OSC 133 prompt markers
    mark_prompt,

    /// Do not look up $TERM in terminfo database.
    ignore_terminfo,

    /// Whether we are allowed to query the TTY for extra information.
    query_term,
}

struct Features {
    // Values for the flags.
    // These are atomic to "fix" a race reported by tsan where tests of feature flags and other
    // tests which use them conceptually race.
    values: [AtomicBool; METADATA.len()],
}

/// Metadata about feature flags.
pub struct FeatureMetadata {
    /// The flag itself.
    pub flag: FeatureFlag,

    /// User-presentable short name of the feature flag.
    pub name: &'static wstr,

    /// Comma-separated list of feature groups.
    pub groups: &'static wstr,

    /// User-presentable description of the feature flag.
    pub description: &'static wstr,

    /// Default flag value.
    pub default_value: bool,

    /// Whether the value can still be changed or not.
    pub read_only: bool,
}

/// The metadata, indexed by flag.
pub const METADATA: &[FeatureMetadata] = &[
    FeatureMetadata {
        flag: FeatureFlag::stderr_nocaret,
        name: L!("stderr-nocaret"),
        groups: L!("3.0"),
        description: L!("^ no longer redirects stderr (historical, can no longer be changed)"),
        default_value: true,
        read_only: true,
    },
    FeatureMetadata {
        flag: FeatureFlag::qmark_noglob,
        name: L!("qmark-noglob"),
        groups: L!("3.0"),
        description: L!("? no longer globs"),
        default_value: true,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::string_replace_backslash,
        name: L!("regex-easyesc"),
        groups: L!("3.1"),
        description: L!("string replace -r needs fewer \\'s"),
        default_value: true,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::ampersand_nobg_in_token,
        name: L!("ampersand-nobg-in-token"),
        groups: L!("3.4"),
        description: L!("& only backgrounds if followed by a separator"),
        default_value: true,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::remove_percent_self,
        name: L!("remove-percent-self"),
        groups: L!("4.0"),
        description: L!("%self is no longer expanded (use $fish_pid)"),
        default_value: false,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::test_require_arg,
        name: L!("test-require-arg"),
        groups: L!("4.0"),
        description: L!("builtin test requires an argument"),
        default_value: false,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::mark_prompt,
        name: L!("mark-prompt"),
        groups: L!("4.0"),
        description: L!("write OSC 133 prompt markers to the terminal"),
        default_value: true,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::ignore_terminfo,
        name: L!("ignore-terminfo"),
        groups: L!("4.1"),
        description: L!("do not look up $TERM in terminfo database"),
        default_value: true,
        read_only: false,
    },
    FeatureMetadata {
        flag: FeatureFlag::query_term,
        name: L!("query-term"),
        groups: L!("4.1"),
        description: L!("query the TTY to enable extra functionality"),
        default_value: true,
        read_only: false,
    },
];

thread_local!(
    #[cfg(test)]
    static LOCAL_FEATURES: RefCell<Option<Features>> = const { RefCell::new(None) };
);

/// The singleton shared feature set.
static FEATURES: Features = Features::new();

/// Perform a feature test on the global set of features.
pub fn test(flag: FeatureFlag) -> bool {
    #[cfg(test)]
    {
        LOCAL_FEATURES.with(|fc| fc.borrow().as_ref().unwrap_or(&FEATURES).test(flag))
    }
    #[cfg(not(test))]
    {
        FEATURES.test(flag)
    }
}

pub use test as feature_test;

/// Set a flag.
#[cfg(test)]
pub fn set(flag: FeatureFlag, value: bool) {
    LOCAL_FEATURES.with(|fc| fc.borrow().as_ref().unwrap_or(&FEATURES).set(flag, value));
}

/// Parses a comma-separated feature-flag string, updating ourselves with the values.
/// Feature names or group names may be prefixed with "no-" to disable them.
/// The special group name "all" may be used for those who like to live on the edge.
/// Unknown features are silently ignored.
pub fn set_from_string<'a>(str: impl Into<&'a wstr>) {
    let wstr: &wstr = str.into();
    #[cfg(test)]
    {
        LOCAL_FEATURES.with(|fc| {
            fc.borrow()
                .as_ref()
                .unwrap_or(&FEATURES)
                .set_from_string(wstr)
        });
    }
    #[cfg(not(test))]
    {
        FEATURES.set_from_string(wstr)
    }
}

impl Features {
    const fn new() -> Self {
        Features {
            values: [
                AtomicBool::new(METADATA[0].default_value),
                AtomicBool::new(METADATA[1].default_value),
                AtomicBool::new(METADATA[2].default_value),
                AtomicBool::new(METADATA[3].default_value),
                AtomicBool::new(METADATA[4].default_value),
                AtomicBool::new(METADATA[5].default_value),
                AtomicBool::new(METADATA[6].default_value),
                AtomicBool::new(METADATA[7].default_value),
                AtomicBool::new(METADATA[8].default_value),
            ],
        }
    }

    fn test(&self, flag: FeatureFlag) -> bool {
        self.values[flag as usize].load(Ordering::SeqCst)
    }

    fn set(&self, flag: FeatureFlag, value: bool) {
        self.values[flag as usize].store(value, Ordering::SeqCst)
    }

    fn set_from_string(&self, str: &wstr) {
        let whitespace = L!("\t\n\0x0B\0x0C\r ").as_char_slice();
        for entry in str.as_char_slice().split(|c| *c == ',') {
            if entry.is_empty() {
                continue;
            }

            // Trim leading and trailing whitespace
            let entry = &entry[entry.iter().take_while(|c| whitespace.contains(c)).count()..];
            let entry =
                &entry[..entry.len() - entry.iter().take_while(|c| whitespace.contains(c)).count()];

            // A "no-" prefix inverts the sense.
            let (name, value) = match entry.strip_prefix(L!("no-").as_char_slice()) {
                Some(suffix) => (suffix, false),
                None => (entry, true),
            };
            // Look for a feature with this name. If we don't find it, assume it's a group name and set
            // all features whose group contain it. Do nothing even if the string is unrecognized; this
            // is to allow uniform invocations of fish (e.g. disable a feature that is only present in
            // future versions).
            // The special name 'all' may be used for those who like to live on the edge.
            if let Some(md) = METADATA.iter().find(|md| md.name == name) {
                // Only change it if it's not read-only.
                // Don't complain if it is, this is typically set from a variable.
                if !md.read_only {
                    self.set(md.flag, value);
                }
            } else {
                for md in METADATA {
                    if (md.groups == name || name == L!("all")) && !md.read_only {
                        self.set(md.flag, value);
                    }
                }
            }
        }
    }
}

#[cfg(test)]
pub fn scoped_test(flag: FeatureFlag, value: bool, test_fn: impl FnOnce()) {
    LOCAL_FEATURES.with(|fc| {
        assert!(
            fc.borrow().is_none(),
            "scoped_test() does not support nesting"
        );

        let f = Features::new();
        f.set(flag, value);
        *fc.borrow_mut() = Some(f);

        test_fn();

        *fc.borrow_mut() = None;
    });
}

#[cfg(test)]
mod tests {
    use super::{FeatureFlag, Features, METADATA, scoped_test, set, test};
    use crate::wchar::prelude::*;

    #[test]
    fn test_feature_flags() {
        let f = Features::new();
        f.set_from_string(L!("stderr-nocaret,nonsense"));
        assert!(f.test(FeatureFlag::stderr_nocaret));
        f.set_from_string(L!("stderr-nocaret,no-stderr-nocaret,nonsense"));
        assert!(f.test(FeatureFlag::stderr_nocaret));

        // Ensure every metadata is represented once.
        let mut counts: [usize; METADATA.len()] = [0; METADATA.len()];
        for md in METADATA {
            counts[md.flag as usize] += 1;
        }
        for count in counts {
            assert_eq!(count, 1);
        }

        assert_eq!(
            METADATA[FeatureFlag::stderr_nocaret as usize].name,
            L!("stderr-nocaret")
        );
    }

    #[test]
    fn test_scoped() {
        scoped_test(FeatureFlag::qmark_noglob, true, || {
            assert!(test(FeatureFlag::qmark_noglob));
        });

        set(FeatureFlag::qmark_noglob, true);

        scoped_test(FeatureFlag::qmark_noglob, false, || {
            assert!(!test(FeatureFlag::qmark_noglob));
        });

        set(FeatureFlag::qmark_noglob, false);
    }

    #[test]
    #[should_panic]
    fn test_nested_scopes_not_supported() {
        scoped_test(FeatureFlag::qmark_noglob, true, || {
            scoped_test(FeatureFlag::qmark_noglob, false, || {});
        });
    }
}
