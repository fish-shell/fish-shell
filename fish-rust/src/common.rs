use crate::ffi;
use crate::wchar::{wstr, WString};
use crate::wchar_ext::WExt;
use crate::wchar_ffi::c_str;
use crate::wchar_ffi::WCharFromFFI;
use bitflags::bitflags;
use std::mem;
use std::mem::ManuallyDrop;
use std::ops::{Deref, DerefMut};
use std::os::fd::AsRawFd;

/// Like [`std::mem::replace()`] but provides a reference to the old value in a callback to obtain
/// the replacement value. Useful to avoid errors about multiple references (`&mut T` for `old` then
/// `&T` again in the `new` expression).
pub fn replace_with<T, F: FnOnce(&T) -> T>(old: &mut T, with: F) -> T {
    let new = with(&*old);
    std::mem::replace(old, new)
}

/// A RAII cleanup object. Unlike in C++ where there is no borrow checker, we can't just provide a
/// callback that modifies live objects willy-nilly because then there would be two &mut references
/// to the same object - the original variables we keep around to use and their captured references
/// held by the closure until its scope expires.
///
/// Instead we have a `ScopeGuard` type that takes exclusive ownership of (a mutable reference to)
/// the object to be managed. In lieu of keeping the original value around, we obtain a regular or
/// mutable reference to it via ScopeGuard's [`Deref`] and [`DerefMut`] impls.
///
/// The `ScopeGuard` is considered to be the exclusively owner of the passed value for the
/// duration of its lifetime. If you need to use the value again, use `ScopeGuard` to shadow the
/// value and obtain a reference to it via the `ScopeGuard` itself:
///
/// ```rust
/// use std::io::prelude::*;
///
/// let file = std::fs::File::open("/dev/null");
/// // Create a scope guard to write to the file when the scope expires.
/// // To be able to still use the file, shadow `file` with the ScopeGuard itself.
/// let mut file = ScopeGuard::new(file, |file| file.write_all(b"goodbye\n").unwrap());
/// // Now write to the file normally "through" the capturing ScopeGuard instance.
/// file.write_all(b"hello\n").unwrap();
///
/// // hello will be written first, then goodbye.
/// ```
pub struct ScopeGuard<T, F: FnOnce(&mut T)> {
    captured: ManuallyDrop<T>,
    on_drop: Option<F>,
}

impl<T, F: FnOnce(&mut T)> ScopeGuard<T, F> {
    /// Creates a new `ScopeGuard` wrapping `value`. The `on_drop` callback is executed when the
    /// ScopeGuard's lifetime expires or when it is manually dropped.
    pub fn new(value: T, on_drop: F) -> Self {
        Self {
            captured: ManuallyDrop::new(value),
            on_drop: Some(on_drop),
        }
    }

    /// Cancel the unwind operation, e.g. do not call the previously passed-in `on_drop` callback
    /// when the current scope expires.
    pub fn cancel(guard: &mut Self) {
        guard.on_drop.take();
    }

    /// Cancels the unwind operation like [`ScopeGuard::cancel()`] but also returns the captured
    /// value (consuming the `ScopeGuard` in the process).
    pub fn rollback(mut guard: Self) -> T {
        guard.on_drop.take();
        // Safety: we're about to forget the guard altogether
        let value = unsafe { ManuallyDrop::take(&mut guard.captured) };
        std::mem::forget(guard);
        value
    }

    /// Commits the unwind operation (i.e. applies the provided callback) and returns the captured
    /// value (consuming the `ScopeGuard` in the process).
    pub fn commit(mut guard: Self) -> T {
        (guard.on_drop.take().expect("ScopeGuard already canceled!"))(&mut guard.captured);
        // Safety: we're about to forget the guard altogether
        let value = unsafe { ManuallyDrop::take(&mut guard.captured) };
        std::mem::forget(guard);
        value
    }
}

impl<T, F: FnOnce(&mut T)> Deref for ScopeGuard<T, F> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.captured
    }
}

impl<T, F: FnOnce(&mut T)> DerefMut for ScopeGuard<T, F> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.captured
    }
}

impl<T, F: FnOnce(&mut T)> Drop for ScopeGuard<T, F> {
    fn drop(&mut self) {
        if let Some(on_drop) = self.on_drop.take() {
            on_drop(&mut self.captured);
        }
        // Safety: we're in the Drop so `self` will never be accessed again.
        unsafe { ManuallyDrop::drop(&mut self.captured) };
    }
}

/// A scoped manager to save the current value of some variable, and optionally set it to a new
/// value. When dropped, it restores the variable to its old value.
///
/// This can be handy when there are multiple code paths to exit a block. Note that this can only be
/// used if the code does not access the captured variable again for the duration of the scope. If
/// that's not the case (the code will refuse to compile), use a [`ScopeGuard`] instance instead.
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

impl Default for EscapeStringStyle {
    fn default() -> Self {
        Self::Script(EscapeFlags::default())
    }
}

bitflags! {
    /// Flags for the [`escape_string()`] function. These are only applicable when the escape style is
    /// [`EscapeStringStyle::Script`].
    #[derive(Default)]
    pub struct EscapeFlags : u32 {
        /// Do not escape special ghoti syntax characters like the semicolon. Only escape non-printable
        /// characters and backslashes.
        const NO_PRINTABLES = 1 << 0;
        /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
        /// string.
        const NO_QUOTED = 1 << 1;
        /// Do not escape tildes.
        const NO_TILDE = 1 << 2;
        /// Replace non-printable control characters with Unicode symbols.
        const SYMBOLIC = 1 << 3;
    }
}

/// Replace special characters with backslash escape sequences. Newline is replaced with `\n`, etc.
pub fn escape_string(s: &wstr, style: EscapeStringStyle) -> WString {
    let (style, flags) = match style {
        EscapeStringStyle::Script(flags) => {
            (ffi::escape_string_style_t::STRING_STYLE_SCRIPT, flags)
        }
        EscapeStringStyle::Url => (
            ffi::escape_string_style_t::STRING_STYLE_URL,
            Default::default(),
        ),
        EscapeStringStyle::Var => (
            ffi::escape_string_style_t::STRING_STYLE_VAR,
            Default::default(),
        ),
        EscapeStringStyle::Regex => (
            ffi::escape_string_style_t::STRING_STYLE_REGEX,
            Default::default(),
        ),
    };

    ffi::escape_string(c_str!(s), flags.bits().into(), style).from_ffi()
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
