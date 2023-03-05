use crate::ffi;
use crate::wchar::{wstr, WString};
use crate::wchar_ext::WExt;
use crate::wchar_ffi::c_str;
use crate::wchar_ffi::WCharFromFFI;
use std::os::fd::AsRawFd;
use std::{ffi::c_uint, mem};

/// A scoped manager to save the current value of some variable, and optionally set it to a new
/// value. When dropped, it restores the variable to its old value.
///
/// This can be handy when there are multiple code paths to exit a block.
pub struct ScopedPush<'a, T> {
    var: &'a mut T,
    saved_value: Option<T>,
}

impl<'a, T> ScopedPush<'a, T> {
    pub fn new(var: &'a mut T, new_value: T) -> Self {
        let saved_value = mem::replace(var, new_value);

        Self {
            var,
            saved_value: Some(saved_value),
        }
    }

    pub fn restore(&mut self) {
        if let Some(saved_value) = self.saved_value.take() {
            *self.var = saved_value;
        }
    }
}

impl<'a, T> Drop for ScopedPush<'a, T> {
    fn drop(&mut self) {
        self.restore()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EscapeStringStyle {
    Script(EscapeFlags),
    Url,
    Var,
    Regex,
}

/// Flags for the [`escape_string()`] function. These are only applicable when the escape style is
/// [`EscapeStringStyle::Script`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct EscapeFlags {
    /// Do not escape special fish syntax characters like the semicolon. Only escape non-printable
    /// characters and backslashes.
    pub no_printables: bool,
    /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
    /// string.
    pub no_quoted: bool,
    /// Do not escape tildes.
    pub no_tilde: bool,
    /// Replace non-printable control characters with Unicode symbols.
    pub symbolic: bool,
}

/// Replace special characters with backslash escape sequences. Newline is replaced with `\n`, etc.
pub fn escape_string(s: &wstr, style: EscapeStringStyle) -> WString {
    let mut flags_int = 0;

    let style = match style {
        EscapeStringStyle::Script(flags) => {
            const ESCAPE_NO_PRINTABLES: c_uint = 1 << 0;
            const ESCAPE_NO_QUOTED: c_uint = 1 << 1;
            const ESCAPE_NO_TILDE: c_uint = 1 << 2;
            const ESCAPE_SYMBOLIC: c_uint = 1 << 3;

            if flags.no_printables {
                flags_int |= ESCAPE_NO_PRINTABLES;
            }
            if flags.no_quoted {
                flags_int |= ESCAPE_NO_QUOTED;
            }
            if flags.no_tilde {
                flags_int |= ESCAPE_NO_TILDE;
            }
            if flags.symbolic {
                flags_int |= ESCAPE_SYMBOLIC;
            }

            ffi::escape_string_style_t::STRING_STYLE_SCRIPT
        }
        EscapeStringStyle::Url => ffi::escape_string_style_t::STRING_STYLE_URL,
        EscapeStringStyle::Var => ffi::escape_string_style_t::STRING_STYLE_VAR,
        EscapeStringStyle::Regex => ffi::escape_string_style_t::STRING_STYLE_REGEX,
    };

    ffi::escape_string(c_str!(s), flags_int.into(), style).from_ffi()
}

/// Test if the string is a valid function name.
pub fn valid_func_name(name: &wstr) -> bool {
    if name.is_empty() {
        return false;
    };
    if name.char_at(0) == '-' {
        return false;
    };
    // A function name needs to be a valid path, so no / and no NULL.
    if name.find_char('/').is_some() {
        return false;
    };
    if name.find_char('\0').is_some() {
        return false;
    };
    true
}

pub const fn assert_send<T: Send>() {}

pub const fn assert_sync<T: Sync>() {}

/// A rusty port of the C++ `write_loop()` function from `common.cpp`. This should be deprecated in
/// favor of native rust read/write methods at some point.
///
/// Returns the number of bytes written or an IO error.
pub fn write_loop<Fd: AsRawFd>(fd: &Fd, buf: &[u8]) -> std::io::Result<usize> {
    let fd = fd.as_raw_fd();
    let mut total = 0;
    while total < buf.len() {
        let written =
            unsafe { libc::write(fd, buf[total..].as_ptr() as *const _, buf.len() - total) };
        if written < 0 {
            let errno = errno::errno().0;
            if matches!(errno, libc::EAGAIN | libc::EINTR) {
                continue;
            }
            return Err(std::io::Error::from_raw_os_error(errno));
        }
        total += written as usize;
    }
    Ok(total)
}

/// A rusty port of the C++ `read_loop()` function from `common.cpp`. This should be deprecated in
/// favor of native rust read/write methods at some point.
///
/// Returns the number of bytes read or an IO error.
pub fn read_loop<Fd: AsRawFd>(fd: &Fd, buf: &mut [u8]) -> std::io::Result<usize> {
    let fd = fd.as_raw_fd();
    loop {
        let read = unsafe { libc::read(fd, buf.as_mut_ptr() as *mut _, buf.len()) };
        if read < 0 {
            let errno = errno::errno().0;
            if matches!(errno, libc::EAGAIN | libc::EINTR) {
                continue;
            }
            return Err(std::io::Error::from_raw_os_error(errno));
        }
        return Ok(read as usize);
    }
}

/// Asserts that a slice is alphabetically sorted by a [`&wstr`] `name` field.
///
/// Mainly useful for static asserts/const eval.
///
/// # Panics
///
/// This function panics if the given slice is unsorted.
///
/// # Examples
///
/// ```rust
/// const COLORS: &[(&wstr, u32)] = &[
///     // must be in alphabetical order
///     (L!("blue"), 0x0000ff),
///     (L!("green"), 0x00ff00),
///     (L!("red"), 0xff0000),
/// ];
///
/// assert_sorted_by_name!(COLORS, 0);
/// ```
macro_rules! assert_sorted_by_name {
    ($slice:expr, $field:tt) => {
        const _: () = {
            use std::cmp::Ordering;

            // ugly const eval workarounds below.
            const fn cmp_i32(lhs: i32, rhs: i32) -> Ordering {
                match lhs - rhs {
                    ..=-1 => Ordering::Less,
                    0 => Ordering::Equal,
                    1.. => Ordering::Greater,
                }
            }

            const fn cmp_slice(s1: &[char], s2: &[char]) -> Ordering {
                let mut i = 0;
                while i < s1.len() && i < s2.len() {
                    match cmp_i32(s1[i] as i32, s2[i] as i32) {
                        Ordering::Equal => i += 1,
                        other => return other,
                    }
                }
                cmp_i32(s1.len() as i32, s2.len() as i32)
            }

            let mut i = 1;
            while i < $slice.len() {
                let prev = $slice[i - 1].$field.as_char_slice();
                let cur = $slice[i].$field.as_char_slice();
                if matches!(cmp_slice(prev, cur), Ordering::Greater) {
                    panic!("array must be sorted");
                }
                i += 1;
            }
        };
    };
    ($slice:expr) => {
        assert_sorted_by_name!($slice, name);
    };
}
mod tests {
    use crate::{
        common::{escape_string, EscapeStringStyle},
        wchar::widestrs,
    };

    #[widestrs]
    pub fn test_escape_string() {
        let regex = |input| escape_string(input, EscapeStringStyle::Regex);

        // plain text should not be needlessly escaped
        assert_eq!(regex("hello world!"L), "hello world!"L);

        // all the following are intended to be ultimately matched literally - even if they don't look
        // like that's the intent - so we escape them.
        assert_eq!(regex(".ext"L), "\\.ext"L);
        assert_eq!(regex("{word}"L), "\\{word\\}"L);
        assert_eq!(regex("hola-mundo"L), "hola\\-mundo"L);
        assert_eq!(
            regex("$17.42 is your total?"L),
            "\\$17\\.42 is your total\\?"L
        );
        assert_eq!(
            regex("not really escaped\\?"L),
            "not really escaped\\\\\\?"L
        );
    }
}

crate::ffi_tests::add_test!("escape_string", tests::test_escape_string);
