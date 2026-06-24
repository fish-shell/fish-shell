use std::{borrow::Borrow, hash::Hash};

use phf_shared::{FmtConst, PhfBorrow, PhfHash};

#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Language<'a>(pub &'a str);

pub const DEFAULT_LANGUAGE: Language = Language(fish_build_helper::DEFAULT_LANGUAGE);

impl<'a> std::ops::Deref for Language<'a> {
    type Target = &'a str;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<'a> std::fmt::Display for Language<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

impl AsRef<str> for Language<'_> {
    fn as_ref(&self) -> &str {
        self.0
    }
}

impl Borrow<str> for Language<'_> {
    fn borrow(&self) -> &str {
        self.0
    }
}

impl<'a> PhfHash for Language<'a> {
    fn phf_hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.phf_hash(state);
    }
}

impl<'a> FmtConst for Language<'a> {
    fn fmt_const(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Language(")?;
        self.0.fmt_const(f)?;
        f.write_str(")")
    }
}

impl<'a> PhfBorrow<Language<'a>> for Language<'a> {
    fn borrow(&self) -> &Language<'a> {
        self
    }
}

pub trait LocalizationLanguage:
    AsRef<Language<'static>> + Clone + Copy + Eq + Hash + Ord + PartialEq + PartialOrd + Borrow<str>
{
}

#[macro_export]
macro_rules! define_localization_language_type {
    ($name:ident) => {
        #[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
        pub struct $name(Language<'static>);

        impl LocalizationLanguage for $name {}

        impl AsRef<Language<'static>> for $name {
            fn as_ref(&self) -> &Language<'static> {
                &self.0
            }
        }

        impl From<&$name> for Language<'static> {
            fn from(value: &$name) -> Self {
                value.0
            }
        }

        impl std::borrow::Borrow<str> for $name {
            fn borrow(&self) -> &str {
                self.0.borrow()
            }
        }
    };
}
