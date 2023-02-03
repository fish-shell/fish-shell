//! Flags to enable upcoming features

use crate::ffi::wcharz_t;
use crate::wchar::wstr;
use crate::wchar_ffi::WCharToFFI;
use std::array;
use std::cell::UnsafeCell;
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering;
use widestring_suffix::widestrs;

#[cxx::bridge]
mod future_feature_flags_ffi {
    extern "C++" {
        include!("wutil.h");
        type wcharz_t = super::wcharz_t;
    }

    /// The list of flags.
    #[repr(u8)]
    enum feature_flag_t {
        /// Whether ^ is supported for stderr redirection.
        stderr_nocaret,

        /// Whether ? is supported as a glob.
        qmark_noglob,

        /// Whether string replace -r double-unescapes the replacement.
        string_replace_backslash,

        /// Whether "&" is not-special if followed by a word character.
        ampersand_nobg_in_token,
    }

    /// Metadata about feature flags.
    struct feature_metadata_t {
        flag: feature_flag_t,
        name: UniquePtr<CxxWString>,
        groups: UniquePtr<CxxWString>,
        description: UniquePtr<CxxWString>,
        default_value: bool,
        read_only: bool,
    }

    extern "Rust" {
        type features_t;
        fn test(self: &features_t, flag: feature_flag_t) -> bool;
        fn set(self: &mut features_t, flag: feature_flag_t, value: bool);
        fn set_from_string(self: &mut features_t, str: wcharz_t);
        fn fish_features() -> *const features_t;
        fn feature_test(flag: feature_flag_t) -> bool;
        fn mutable_fish_features() -> *mut features_t;
        fn feature_metadata() -> [feature_metadata_t; 4];
    }
}

pub use future_feature_flags_ffi::{feature_flag_t, feature_metadata_t};

pub struct features_t {
    // Values for the flags.
    // These are atomic to "fix" a race reported by tsan where tests of feature flags and other
    // tests which use them conceptually race.
    values: [AtomicBool; metadata.len()],
}

/// Metadata about feature flags.
struct FeatureMetadata {
    /// The flag itself.
    flag: feature_flag_t,

    /// User-presentable short name of the feature flag.
    name: &'static wstr,

    /// Comma-separated list of feature groups.
    groups: &'static wstr,

    /// User-presentable description of the feature flag.
    description: &'static wstr,

    /// Default flag value.
    default_value: bool,

    /// Whether the value can still be changed or not.
    read_only: bool,
}

impl From<&FeatureMetadata> for feature_metadata_t {
    fn from(md: &FeatureMetadata) -> feature_metadata_t {
        feature_metadata_t {
            flag: md.flag,
            name: md.name.to_ffi(),
            groups: md.groups.to_ffi(),
            description: md.description.to_ffi(),
            default_value: md.default_value,
            read_only: md.read_only,
        }
    }
}

/// The metadata, indexed by flag.
#[widestrs]
const metadata: [FeatureMetadata; 4] = [
    FeatureMetadata {
        flag: feature_flag_t::stderr_nocaret,
        name: "stderr-nocaret"L,
        groups: "3.0"L,
        description: "^ no longer redirects stderr (historical, can no longer be changed)"L,
        default_value: true,
        read_only: true,
    },
    FeatureMetadata {
        flag: feature_flag_t::qmark_noglob,
        name: "qmark-noglob"L,
        groups: "3.0"L,
        description: "? no longer globs"L,
        default_value: false,
        read_only: false,
    },
    FeatureMetadata {
        flag: feature_flag_t::string_replace_backslash,
        name: "regex-easyesc"L,
        groups: "3.1"L,
        description: "string replace -r needs fewer \\'s"L,
        default_value: true,
        read_only: false,
    },
    FeatureMetadata {
        flag: feature_flag_t::ampersand_nobg_in_token,
        name: "ampersand-nobg-in-token"L,
        groups: "3.4"L,
        description: "& only backgrounds if followed by a separator"L,
        default_value: true,
        read_only: false,
    },
];

/// The singleton shared feature set.
static mut global_features: *const UnsafeCell<features_t> = std::ptr::null();

pub fn future_feature_flags_init() {
    unsafe {
        // Leak it for now.
        global_features = Box::into_raw(Box::new(UnsafeCell::new(features_t::new())));
    }
}

impl features_t {
    fn new() -> Self {
        features_t {
            values: array::from_fn(|i| AtomicBool::new(metadata[i].default_value)),
        }
    }

    /// Return whether a flag is set.
    pub fn test(&self, flag: feature_flag_t) -> bool {
        self.values[flag.repr as usize].load(Ordering::SeqCst)
    }

    /// Set a flag.
    pub fn set(&mut self, flag: feature_flag_t, value: bool) {
        self.values[flag.repr as usize].store(value, Ordering::SeqCst)
    }

    /// Parses a comma-separated feature-flag string, updating ourselves with the values.
    /// Feature names or group names may be prefixed with "no-" to disable them.
    /// The special group name "all" may be used for those who like to live on the edge.
    /// Unknown features are silently ignored.
    #[widestrs]
    pub fn set_from_string(&mut self, str: wcharz_t) {
        let str: &wstr = str.into();
        let whitespace = "\t\n\0x0B\0x0C\r "L.as_char_slice();
        for entry in str.as_char_slice().split(|c| *c == ',') {
            if entry.is_empty() {
                continue;
            }

            // Trim leading and trailing whitespace
            let entry = &entry[entry.iter().take_while(|c| whitespace.contains(c)).count()..];
            let entry =
                &entry[..entry.len() - entry.iter().take_while(|c| whitespace.contains(c)).count()];

            // A "no-" prefix inverts the sense.
            let (name, value) = match entry.strip_prefix("no-"L.as_char_slice()) {
                Some(suffix) => (suffix, false),
                None => (entry, true),
            };
            // Look for a feature with this name. If we don't find it, assume it's a group name and set
            // all features whose group contain it. Do nothing even if the string is unrecognized; this
            // is to allow uniform invocations of fish (e.g. disable a feature that is only present in
            // future versions).
            // The special name 'all' may be used for those who like to live on the edge.
            if let Some(md) = metadata.iter().find(|md| md.name == name) {
                // Only change it if it's not read-only.
                // Don't complain if it is, this is typically set from a variable.
                if !md.read_only {
                    self.set(md.flag, value);
                }
            } else {
                for md in &metadata {
                    if md.groups == name || name == "all"L {
                        if !md.read_only {
                            self.set(md.flag, value);
                        }
                    }
                }
            }
        }
    }
}

/// Return the global set of features for fish. This is const to prevent accidental mutation.
pub fn fish_features() -> *const features_t {
    unsafe { (*global_features).get() }
}

/// Perform a feature test on the global set of features.
pub fn feature_test(flag: feature_flag_t) -> bool {
    unsafe { &*(*global_features).get() }.test(flag)
}

/// Return the global set of features for fish, but mutable. In general fish features should be set
/// at startup only.
pub fn mutable_fish_features() -> *mut features_t {
    unsafe { (*global_features).get() }
}

// The metadata, indexed by flag.
pub fn feature_metadata() -> [feature_metadata_t; metadata.len()] {
    array::from_fn(|i| (&metadata[i]).into())
}

#[test]
#[widestrs]
fn test_feature_flags() {
    use crate::wchar_ffi::wcharz;

    let mut f = features_t::new();
    f.set_from_string(wcharz!("stderr-nocaret,nonsense"L));
    assert!(f.test(feature_flag_t::stderr_nocaret));
    f.set_from_string(wcharz!("stderr-nocaret,no-stderr-nocaret,nonsense"L));
    assert!(f.test(feature_flag_t::stderr_nocaret));

    // Ensure every metadata is represented once.
    let mut counts: [usize; metadata.len()] = [0; metadata.len()];
    for md in &metadata {
        counts[md.flag.repr as usize] += 1;
    }
    for count in counts {
        assert_eq!(count, 1);
    }

    assert_eq!(
        metadata[feature_flag_t::stderr_nocaret.repr as usize].name,
        "stderr-nocaret"L
    );
}
