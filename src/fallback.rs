//! This file only contains fallback implementations of functions which have been found to be missing
//! or broken by the configuration scripts.
//!
//! Many of these functions are more or less broken and incomplete.

use crate::wchar::prelude::*;
use crate::widecharwidth::{WcLookupTable, WcWidth};
use once_cell::sync::Lazy;
use std::cmp;
use std::sync::atomic::{AtomicIsize, Ordering};

/// Width of ambiguous East Asian characters and, as of TR11, all private-use characters.
/// 1 is the typical default, but we accept any non-negative override via `$fish_ambiguous_width`.
pub static FISH_AMBIGUOUS_WIDTH: AtomicIsize = AtomicIsize::new(1);

/// Width of emoji characters.
///
/// This must be configurable because the value changed between Unicode 8 and Unicode 9, `wcwidth()`
/// is emoji-unaware, and terminal emulators do different things.
///
/// See issues like [#4539](https://github.com/fish-shell/fish-shell/issues/4539) and <https://github.com/neovim/neovim/issues/4976> for how painful this is.
///
/// Valid values are 1, and 2. 1 is the typical emoji width used in Unicode 8 while some newer
/// terminals use a width of 2 since Unicode 9.
// For some reason, this is declared here and exposed here, but is set in `env_dispatch`.
pub static FISH_EMOJI_WIDTH: AtomicIsize = AtomicIsize::new(1);

static WC_LOOKUP_TABLE: Lazy<WcLookupTable> = Lazy::new(WcLookupTable::new);

/// A safe wrapper around the system `wcwidth()` function
#[cfg(not(cygwin))]
pub fn wcwidth(c: char) -> isize {
    extern "C" {
        pub fn wcwidth(c: libc::wchar_t) -> libc::c_int;
    }

    const _: () = assert!(std::mem::size_of::<libc::wchar_t>() >= std::mem::size_of::<char>());
    let width = unsafe { wcwidth(c as libc::wchar_t) };
    isize::try_from(width).unwrap()
}

// Big hack to use our versions of wcswidth where we know them to be broken, which is
// EVERYWHERE (https://github.com/fish-shell/fish-shell/issues/2199)
pub fn fish_wcwidth(c: char) -> isize {
    // The system version of wcwidth should accurately reflect the ability to represent characters
    // in the console session, but knows nothing about the capabilities of other terminal emulators
    // or ttys. Use it from the start only if we are logged in to the physical console.
    #[cfg(not(cygwin))]
    if crate::common::is_console_session() {
        return wcwidth(c);
    }

    // Check for VS16 which selects emoji presentation. This "promotes" a character like U+2764
    // (width 1) to an emoji (probably width 2). So treat it as width 1 so the sums work. See #2652.
    // VS15 selects text presentation.
    let variation_selector_16 = '\u{FE0F}';
    let variation_selector_15 = '\u{FE0E}';
    if c == variation_selector_16 {
        return 1;
    } else if c == variation_selector_15 {
        return 0;
    }

    // Check for Emoji_Modifier property. Only the Fitzpatrick modifiers have this, in range
    // 1F3FB..1F3FF. This is a hack because such an emoji appearing on its own would be drawn as
    // width 2, but that's unlikely to be useful. See #8275.
    if ('\u{1F3FB}'..='\u{1F3FF}').contains(&c) {
        return 0;
    }

    let width = WC_LOOKUP_TABLE.classify(c);
    match width {
        WcWidth::NonCharacter | WcWidth::NonPrint | WcWidth::Combining | WcWidth::Unassigned => {
            #[cfg(not(cygwin))]
            {
                // Fall back to system wcwidth in this case.
                wcwidth(c)
            }
            #[cfg(cygwin)]
            {
                // No system wcwidth for UTF-32 on cygwin.
                0
            }
        }
        WcWidth::Ambiguous | WcWidth::PrivateUse => {
            // TR11: "All private-use characters are by default classified as Ambiguous".
            FISH_AMBIGUOUS_WIDTH.load(Ordering::Relaxed)
        }
        WcWidth::One => 1,
        WcWidth::Two => 2,
        WcWidth::WidenedIn9 => FISH_EMOJI_WIDTH.load(Ordering::Relaxed),
    }
}

/// fish's internal versions of wcwidth and wcswidth, which can use an internal implementation if
/// the system one is busted.
pub fn fish_wcswidth(s: &wstr) -> isize {
    // ascii fast path; empty iterator returns true for .all()
    if s.chars().all(|c| c.is_ascii() && !c.is_ascii_control()) {
        return s.len() as isize;
    }

    let mut result = 0;
    for c in s.chars() {
        let w = fish_wcwidth(c);
        if w < 0 {
            return -1;
        }
        result += w;
    }
    result
}

pub fn wcscasecmp(lhs: &wstr, rhs: &wstr) -> cmp::Ordering {
    wcscasecmp_fuzzy(lhs, rhs, std::convert::identity)
}

/// Compare two wide strings in a case-insensitive fashion
pub fn wcscasecmp_fuzzy(lhs: &wstr, rhs: &wstr, canonicalize: fn(char) -> char) -> cmp::Ordering {
    use std::char::ToLowercase;
    use widestring::utfstr::CharsUtf32;

    /// This struct streams the underlying lowercase chars of a `UTF32String` without allocating.
    ///
    /// `char::to_lowercase()` returns an iterator of chars and we sometimes need to cmp the last
    /// char of one char's `to_lowercase()` with the first char of the other char's
    /// `to_lowercase()`. This makes that possible.
    struct ToLowerBuffer<'a, Canonicalize: Fn(char) -> char> {
        current: ToLowercase,
        chars: std::iter::Map<CharsUtf32<'a>, Canonicalize>,
    }

    impl<'a, Canonicalize: Fn(char) -> char> Iterator for ToLowerBuffer<'a, Canonicalize> {
        type Item = char;

        fn next(&mut self) -> Option<Self::Item> {
            if let Some(c) = self.current.next() {
                return Some(c);
            }

            self.current = self.chars.next()?.to_lowercase();
            self.current.next()
        }
    }

    impl<'a, Canonicalize: Fn(char) -> char> ToLowerBuffer<'a, Canonicalize> {
        pub fn new(mut chars: std::iter::Map<CharsUtf32<'a>, Canonicalize>) -> Self {
            Self {
                current: chars.next().map(|c| c.to_lowercase()).unwrap_or_else(|| {
                    let mut empty = 'a'.to_lowercase();
                    let _ = empty.next();
                    debug_assert!(empty.next().is_none());
                    empty
                }),
                chars,
            }
        }
    }

    let lhs = ToLowerBuffer::new(lhs.chars().map(canonicalize));
    let rhs = ToLowerBuffer::new(rhs.chars().map(canonicalize));
    lhs.cmp(rhs)
}

#[test]
fn test_wcscasecmp() {
    use std::cmp::Ordering;

    // Comparison with empty
    assert_eq!(wcscasecmp(L!("a"), L!("")), Ordering::Greater);
    assert_eq!(wcscasecmp(L!(""), L!("a")), Ordering::Less);
    assert_eq!(wcscasecmp(L!(""), L!("")), Ordering::Equal);

    // Basic comparison
    assert_eq!(wcscasecmp(L!("A"), L!("a")), Ordering::Equal);
    assert_eq!(wcscasecmp(L!("B"), L!("a")), Ordering::Greater);
    assert_eq!(wcscasecmp(L!("A"), L!("B")), Ordering::Less);

    // Multi-byte comparison
    assert_eq!(wcscasecmp(L!("İ"), L!("i\u{307}")), Ordering::Equal);
    assert_eq!(wcscasecmp(L!("ia"), L!("İa")), Ordering::Less);
}
