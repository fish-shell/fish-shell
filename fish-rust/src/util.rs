//! Generic utilities library.

use crate::ffi::wcharz_t;
use crate::wchar::prelude::*;
use std::cmp::Ordering;
use std::time;

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("wutil.h");
        type wcharz_t = super::wcharz_t;
    }

    extern "Rust" {
        #[cxx_name = "wcsfilecmp"]
        fn wcsfilecmp_ffi(a: wcharz_t, b: wcharz_t) -> i32;
        #[cxx_name = "wcsfilecmp_glob"]
        fn wcsfilecmp_glob_ffi(a: wcharz_t, b: wcharz_t) -> i32;
        fn get_time() -> i64;
    }
}

fn ordering_to_int(ord: Ordering) -> i32 {
    match ord {
        Ordering::Less => -1,
        Ordering::Equal => 0,
        Ordering::Greater => 1,
    }
}

fn wcsfilecmp_glob_ffi(a: wcharz_t, b: wcharz_t) -> i32 {
    ordering_to_int(wcsfilecmp_glob(a.into(), b.into()))
}

fn wcsfilecmp_ffi(a: wcharz_t, b: wcharz_t) -> i32 {
    ordering_to_int(wcsfilecmp(a.into(), b.into()))
}

/// Compares two wide character strings with an (arguably) intuitive ordering. This function tries
/// to order strings in a way which is intuitive to humans with regards to sorting strings
/// containing numbers.
///
/// Most sorting functions would sort the strings 'file1.txt' 'file5.txt' and 'file12.txt' as:
///
/// file1.txt
/// file12.txt
/// file5.txt
///
/// This function regards any sequence of digits as a single entity when performing comparisons, so
/// the output is instead:
///
/// file1.txt
/// file5.txt
/// file12.txt
///
/// Which most people would find more intuitive.
///
/// This won't return the optimum results for numbers in bases higher than ten, such as hexadecimal,
/// but at least a stable sort order will result.
///
/// This function performs a two-tiered sort, where difference in case and in number of leading
/// zeroes in numbers only have effect if no other differences between strings are found. This way,
/// a 'file1' and 'File1' will not be considered identical, and hence their internal sort order is
/// not arbitrary, but the names 'file1', 'File2' and 'file3' will still be sorted in the order
/// given above.
pub fn wcsfilecmp(a: &wstr, b: &wstr) -> Ordering {
    let mut retval = Ordering::Equal;
    let mut ai = 0;
    let mut bi = 0;
    while ai < a.len() && bi < b.len() {
        let ac = a.as_char_slice()[ai];
        let bc = b.as_char_slice()[bi];
        if ac.is_ascii_digit() && bc.is_ascii_digit() {
            let (ad, bd);
            (retval, ad, bd) = wcsfilecmp_leading_digits(&a[ai..], &b[bi..]);
            ai += ad;
            bi += bd;
            if retval != Ordering::Equal || ai == a.len() || bi == b.len() {
                break;
            }
            continue;
        }

        // Fast path: Skip towupper.
        if ac == bc {
            ai += 1;
            bi += 1;
            continue;
        }

        // Sort dashes after Z - see #5634
        let mut acl = if ac == '-' { '[' } else { ac };
        let mut bcl = if bc == '-' { '[' } else { bc };
        // TODO Compare the tail (enabled by Rust's Unicode support).
        acl = acl.to_uppercase().next().unwrap();
        bcl = bcl.to_uppercase().next().unwrap();

        match acl.cmp(&bcl) {
            Ordering::Equal => {
                ai += 1;
                bi += 1;
            }
            o => {
                retval = o;
                break;
            }
        }
    }

    if retval != Ordering::Equal {
        return retval; // we already know the strings aren't logically equal
    }

    if ai == a.len() {
        if bi == b.len() {
            // The strings are logically equal. They may or may not be the same length depending on
            // whether numbers were present but that doesn't matter. Disambiguate strings that
            // differ by letter case or length. We don't bother optimizing the case where the file
            // names are literally identical because that won't occur given how this function is
            // used. And even if it were to occur (due to being reused in some other context) it
            // would be so rare that it isn't worth optimizing for.
            a.cmp(b)
        } else {
            Ordering::Less // string a is a prefix of b and b is longer
        }
    } else {
        assert!(bi == b.len());
        Ordering::Greater // string b is a prefix of a and a is longer
    }
}

/// wcsfilecmp, but frozen in time for glob usage.
pub fn wcsfilecmp_glob(a: &wstr, b: &wstr) -> Ordering {
    let mut retval = Ordering::Equal;
    let mut ai = 0;
    let mut bi = 0;
    while ai < a.len() && bi < b.len() {
        let ac = a.as_char_slice()[ai];
        let bc = b.as_char_slice()[bi];
        if ac.is_ascii_digit() && bc.is_ascii_digit() {
            let (ad, bd);
            (retval, ad, bd) = wcsfilecmp_leading_digits(&a[ai..], &b[bi..]);
            ai += ad;
            bi += bd;
            // If we know the strings aren't logically equal or we've reached the end of one or both
            // strings we can stop iterating over the chars in each string.
            if retval != Ordering::Equal || ai == a.len() || bi == b.len() {
                break;
            }
            continue;
        }

        // Fast path: Skip towlower.
        if ac == bc {
            ai += 1;
            bi += 1;
            continue;
        }

        // TODO Compare the tail (enabled by Rust's Unicode support).
        let acl = ac.to_lowercase().next().unwrap();
        let bcl = bc.to_lowercase().next().unwrap();
        match acl.cmp(&bcl) {
            Ordering::Equal => {
                ai += 1;
                bi += 1;
            }
            o => {
                retval = o;
                break;
            }
        }
    }

    if retval != Ordering::Equal {
        return retval; // we already know the strings aren't logically equal
    }

    if ai == a.len() {
        if bi == b.len() {
            // The strings are logically equal. They may or may not be the same length depending on
            // whether numbers were present but that doesn't matter. Disambiguate strings that
            // differ by letter case or length. We don't bother optimizing the case where the file
            // names are literally identical because that won't occur given how this function is
            // used. And even if it were to occur (due to being reused in some other context) it
            // would be so rare that it isn't worth optimizing for.
            a.cmp(b)
        } else {
            Ordering::Less // string a is a prefix of b and b is longer
        }
    } else {
        assert!(bi == b.len());
        Ordering::Greater // string b is a prefix of a and a is longer
    }
}

/// Get the current time in microseconds since Jan 1, 1970.
pub fn get_time() -> i64 {
    match time::SystemTime::now().duration_since(time::UNIX_EPOCH) {
        Ok(difference) => difference.as_micros() as i64,
        Err(until_epoch) => -(until_epoch.duration().as_micros() as i64),
    }
}

// Compare the strings to see if they begin with an integer that can be compared and return the
// result of that comparison.
fn wcsfilecmp_leading_digits(a: &wstr, b: &wstr) -> (Ordering, usize, usize) {
    // Ignore leading 0s.
    let mut ai = a.as_char_slice().iter().take_while(|c| **c == '0').count();
    let mut bi = b.as_char_slice().iter().take_while(|c| **c == '0').count();

    let mut ret = Ordering::Equal;
    loop {
        let ac = a.as_char_slice().get(ai).unwrap_or(&'\0');
        let bc = b.as_char_slice().get(bi).unwrap_or(&'\0');
        if ac.is_ascii_digit() && bc.is_ascii_digit() {
            // We keep the cmp value for the
            // first differing digit.
            //
            // If the numbers have the same length, that's the value.
            if ret == Ordering::Equal {
                // Comparing the string value is the same as numerical
                // for wchar_t digits!
                ret = ac.cmp(bc);
            }
        } else {
            // We don't have negative numbers and we only allow ints,
            // and we have already skipped leading zeroes,
            // so the longer number is larger automatically.
            if ac.is_ascii_digit() {
                ret = Ordering::Greater;
            }
            if bc.is_ascii_digit() {
                ret = Ordering::Less;
            }
            break;
        }
        ai += 1;
        bi += 1;
    }

    // For historical reasons, we skip trailing whitespace
    // like fish_wcstol does!
    // This is used in sorting globs, and that's supposed to be stable.
    ai += a
        .as_char_slice()
        .iter()
        .skip(ai)
        .take_while(|c| c.is_whitespace())
        .count();
    bi += b
        .as_char_slice()
        .iter()
        .skip(bi)
        .take_while(|c| c.is_whitespace())
        .count();
    (ret, ai, bi)
}

/// Finds `needle` in a `haystack` and returns the index of the first matching element, if any.
///
/// # Examples
///
/// ```
/// let haystack = b"ABC ABCDAB ABCDABCDABDE";
///
/// assert_eq!(find_subslice(b"ABCDABD", haystack), Some(15));
/// assert_eq!(find_subslice(b"ABCDE", haystack), None);
/// ```
pub fn find_subslice<T: PartialEq>(
    needle: impl AsRef<[T]>,
    haystack: impl AsRef<[T]>,
) -> Option<usize> {
    let needle = needle.as_ref();
    if needle.is_empty() {
        return Some(0);
    }
    let haystack = haystack.as_ref();
    haystack.windows(needle.len()).position(|w| w == needle)
}

/// Verify the behavior of the `wcsfilecmp()` function.
#[test]
fn test_wcsfilecmp() {
    macro_rules! validate {
        ($str1:expr, $str2:expr, $expected_rc:expr) => {
            assert_eq!(wcsfilecmp(L!($str1), L!($str2)), $expected_rc)
        };
    }

    // Not using L as suffix because the macro munges error locations.
    validate!("", "", Ordering::Equal);
    validate!("", "def", Ordering::Less);
    validate!("abc", "", Ordering::Greater);
    validate!("abc", "def", Ordering::Less);
    validate!("abc", "DEF", Ordering::Less);
    validate!("DEF", "abc", Ordering::Greater);
    validate!("abc", "abc", Ordering::Equal);
    validate!("ABC", "ABC", Ordering::Equal);
    validate!("AbC", "abc", Ordering::Less);
    validate!("AbC", "ABC", Ordering::Greater);
    validate!("def", "abc", Ordering::Greater);
    validate!("1ghi", "1gHi", Ordering::Greater);
    validate!("1ghi", "2ghi", Ordering::Less);
    validate!("1ghi", "01ghi", Ordering::Greater);
    validate!("1ghi", "02ghi", Ordering::Less);
    validate!("01ghi", "1ghi", Ordering::Less);
    validate!("1ghi", "002ghi", Ordering::Less);
    validate!("002ghi", "1ghi", Ordering::Greater);
    validate!("abc01def", "abc1def", Ordering::Less);
    validate!("abc1def", "abc01def", Ordering::Greater);
    validate!("abc12", "abc5", Ordering::Greater);
    validate!("51abc", "050abc", Ordering::Greater);
    validate!("abc5", "abc12", Ordering::Less);
    validate!("5abc", "12ABC", Ordering::Less);
    validate!("abc0789", "abc789", Ordering::Less);
    validate!("abc0xA789", "abc0xA0789", Ordering::Greater);
    validate!("abc002", "abc2", Ordering::Less);
    validate!("abc002g", "abc002", Ordering::Greater);
    validate!("abc002g", "abc02g", Ordering::Less);
    validate!("abc002.txt", "abc02.txt", Ordering::Less);
    validate!("abc005", "abc012", Ordering::Less);
    validate!("abc02", "abc002", Ordering::Greater);
    validate!("abc002.txt", "abc02.txt", Ordering::Less);
    validate!("GHI1abc2.txt", "ghi1abc2.txt", Ordering::Less);
    validate!("a0", "a00", Ordering::Less);
    validate!("a00b", "a0b", Ordering::Less);
    validate!("a0b", "a00b", Ordering::Greater);
    validate!("a-b", "azb", Ordering::Greater);
}
