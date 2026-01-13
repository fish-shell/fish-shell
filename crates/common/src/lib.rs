use bitflags::bitflags;
use libc::{SIG_IGN, SIGTTOU, STDIN_FILENO};
use std::cell::{Cell, RefCell};
use std::io::Read;
use std::ops::{Deref, DerefMut};
use std::os::fd::{AsRawFd, BorrowedFd, RawFd};
use std::os::unix::ffi::OsStrExt;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicI32, AtomicU32, Ordering};
use std::{env, mem, time};
use widestring::Utf32Str as wstr;

pub const PACKAGE_NAME: &str = env!("CARGO_PKG_NAME");

// Highest legal ASCII value.
pub const ASCII_MAX: char = 127 as char;

// Highest legal 16-bit Unicode value.
pub const UCS2_MAX: char = '\u{FFFF}';

// Highest legal byte value.
pub const BYTE_MAX: char = 0xFF as char;

// Unicode BOM value.
pub const UTF8_BOM_WCHAR: char = '\u{FEFF}';

// Use Unicode "non-characters" for internal characters as much as we can. This
// gives us 32 "characters" for internal use that we can guarantee should not
// appear in our input stream. See http://www.unicode.org/faq/private_use.html.
pub const RESERVED_CHAR_BASE: char = '\u{FDD0}';
pub const RESERVED_CHAR_END: char = '\u{FDF0}';
// Split the available non-character values into two ranges to ensure there are
// no conflicts among the places we use these special characters.
pub const EXPAND_RESERVED_BASE: char = RESERVED_CHAR_BASE;
pub const EXPAND_RESERVED_END: char = char_offset(EXPAND_RESERVED_BASE, 16);
pub const WILDCARD_RESERVED_BASE: char = EXPAND_RESERVED_END;
pub const WILDCARD_RESERVED_END: char = char_offset(WILDCARD_RESERVED_BASE, 16);
// Make sure the ranges defined above don't exceed the range for non-characters.
// This is to make sure we didn't do something stupid in subdividing the
// Unicode range for our needs.
const _: () = assert!(WILDCARD_RESERVED_END <= RESERVED_CHAR_END);

// These are in the Unicode private-use range. We really shouldn't use this
// range but have little choice in the matter given how our lexer/parser works.
// We can't use non-characters for these two ranges because there are only 66 of
// them and we need at least 256 + 64.
//
// If sizeof(wchar_t))==4 we could avoid using private-use chars; however, that
// would result in fish having different behavior on machines with 16 versus 32
// bit wchar_t. It's better that fish behave the same on both types of systems.
//
// Note: We don't use the highest 8 bit range (0xF800 - 0xF8FF) because we know
// of at least one use of a codepoint in that range: the Apple symbol (0xF8FF)
// on Mac OS X. See http://www.unicode.org/faq/private_use.html.
pub const ENCODE_DIRECT_BASE: char = '\u{F600}';
pub const ENCODE_DIRECT_END: char = char_offset(ENCODE_DIRECT_BASE, 256);

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

impl TryFrom<&wstr> for EscapeStringStyle {
    type Error = &'static wstr;
    fn try_from(s: &wstr) -> Result<Self, Self::Error> {
        use EscapeStringStyle::*;
        match s {
            s if s == "script" => Ok(Self::default()),
            s if s == "var" => Ok(Var),
            s if s == "url" => Ok(Url),
            s if s == "regex" => Ok(Regex),
            _ => Err(widestring::utf32str!("Invalid escape style")),
        }
    }
}

bitflags! {
    /// Flags for the [`escape_string()`] function. These are only applicable when the escape style is
    /// [`EscapeStringStyle::Script`].
    #[derive(Copy, Clone, Debug, Default, Eq, PartialEq)]
    pub struct EscapeFlags: u32 {
        /// Do not escape special fish syntax characters like the semicolon. Only escape non-printable
        /// characters and backslashes.
        const NO_PRINTABLES = 1 << 0;
        /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
        /// string.
        const NO_QUOTED = 1 << 1;
        /// Do not escape tildes.
        const NO_TILDE = 1 << 2;
        /// Replace non-printable control characters with Unicode symbols.
        const SYMBOLIC = 1 << 3;
        /// Escape ,
        const COMMA = 1 << 4;
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnescapeStringStyle {
    Script(UnescapeFlags),
    Url,
    Var,
}

impl Default for UnescapeStringStyle {
    fn default() -> Self {
        Self::Script(UnescapeFlags::default())
    }
}

impl TryFrom<&wstr> for UnescapeStringStyle {
    type Error = &'static wstr;
    fn try_from(s: &wstr) -> Result<Self, Self::Error> {
        use UnescapeStringStyle::*;
        match s {
            s if s == "script" => Ok(Self::default()),
            s if s == "var" => Ok(Var),
            s if s == "url" => Ok(Url),
            _ => Err(widestring::utf32str!("Invalid escape style")),
        }
    }
}

bitflags! {
    /// Flags for unescape_string functions.
    #[derive(Copy, Clone, Debug, Default, Eq, PartialEq)]
    pub struct UnescapeFlags: u32 {
        /// escape special fish syntax characters like the semicolon
        const SPECIAL = 1 << 0;
        /// allow incomplete escape sequences
        const INCOMPLETE = 1 << 1;
        /// don't handle backslash escapes
        const NO_BACKSLASHES = 1 << 2;
    }
}

pub const fn char_offset(base: char, offset: u32) -> char {
    match char::from_u32(base as u32 + offset) {
        Some(c) => c,
        None => panic!("not a valid char"),
    }
}

pub fn subslice_position<T: Eq>(a: &[T], b: &[T]) -> Option<usize> {
    if b.is_empty() {
        return Some(0);
    }
    a.windows(b.len()).position(|aw| aw == b)
}

/// This function attempts to distinguish between a console session (at the actual login vty) and a
/// session within a terminal emulator inside a desktop environment or over SSH. Unfortunately
/// there are few values of $TERM that we can interpret as being exclusively console sessions, and
/// most common operating systems do not use them. The value is cached for the duration of the fish
/// session. We err on the side of assuming it's not a console session. This approach isn't
/// bullet-proof and that's OK.
pub fn is_console_session() -> bool {
    static IS_CONSOLE_SESSION: OnceLock<bool> = OnceLock::new();
    // TODO(terminal-workaround)
    *IS_CONSOLE_SESSION.get_or_init(|| {
        nix::unistd::ttyname(unsafe { std::os::fd::BorrowedFd::borrow_raw(STDIN_FILENO) })
            .is_ok_and(|buf| {
                // Check if the tty matches /dev/(console|dcons|tty[uv\d])
                let is_console_tty = match buf.as_os_str().as_bytes() {
                    b"/dev/console" => true,
                    b"/dev/dcons" => true,
                    bytes => bytes.strip_prefix(b"/dev/tty").is_some_and(|rest| {
                        matches!(rest.first(), Some(b'u' | b'v' | b'0'..=b'9'))
                    }),
                };

                // and that $TERM is simple, e.g. `xterm` or `vt100`, not `xterm-something` or `sun-color`.
                is_console_tty && env::var_os("TERM").is_none_or(|t| !t.as_bytes().contains(&b'-'))
            })
    })
}

/// Exits without invoking destructors (via _exit), useful for code after fork.
pub fn exit_without_destructors(code: libc::c_int) -> ! {
    unsafe { libc::_exit(code) };
}

/// The character to use where the text has been truncated.
pub fn get_ellipsis_char() -> char {
    '\u{2026}' // ('…')
}

/// The character or string to use where text has been truncated (ellipsis if possible, otherwise
/// ...)
pub fn get_ellipsis_str() -> &'static wstr {
    widestring::utf32str!("\u{2026}")
}

// Only pub for `src/common.rs`
pub static OBFUSCATION_READ_CHAR: AtomicU32 = AtomicU32::new(0);

pub fn get_obfuscation_read_char() -> char {
    char::from_u32(OBFUSCATION_READ_CHAR.load(Ordering::Relaxed)).unwrap()
}

/// Call read, blocking and repeating on EINTR. Exits on EAGAIN.
/// Return the number of bytes read, or 0 on EOF, or an error.
pub fn read_blocked(fd: RawFd, buf: &mut [u8]) -> nix::Result<usize> {
    loop {
        let res = nix::unistd::read(unsafe { BorrowedFd::borrow_raw(fd) }, buf);
        if let Err(nix::Error::EINTR) = res {
            continue;
        }
        return res;
    }
}

pub trait ReadExt {
    /// Like [`std::io::Read::read_to_end`], but does not retry on EINTR.
    fn read_to_end_interruptible(&mut self, buf: &mut Vec<u8>) -> std::io::Result<()>;
}

impl<T: Read + ?Sized> ReadExt for T {
    fn read_to_end_interruptible(&mut self, buf: &mut Vec<u8>) -> std::io::Result<()> {
        let mut chunk = [0_u8; 4096];
        loop {
            match self.read(&mut chunk)? {
                0 => return Ok(()),
                n => buf.extend_from_slice(&chunk[..n]),
            }
        }
    }
}

/// A rusty port of the C++ `write_loop()` function from `common.cpp`. This should be deprecated in
/// favor of native rust read/write methods at some point.
pub fn safe_write_loop<Fd: AsRawFd>(fd: &Fd, buf: &[u8]) -> std::io::Result<()> {
    let fd = fd.as_raw_fd();
    let mut total = 0;
    while total < buf.len() {
        match nix::unistd::write(unsafe { BorrowedFd::borrow_raw(fd) }, &buf[total..]) {
            Ok(written) => {
                total += written;
            }
            Err(err) => {
                if matches!(err, nix::Error::EAGAIN | nix::Error::EINTR) {
                    continue;
                }
                return Err(std::io::Error::from(err));
            }
        }
    }
    Ok(())
}

pub use safe_write_loop as write_loop;

pub const fn help_section_exists(section: &str) -> bool {
    let haystack = include_str!("../../../share/help_sections");
    let needle = section;

    let needle = needle.as_bytes();
    let haystack = haystack.as_bytes();
    let nlen = needle.len();
    let mut line_start = 0;
    let mut i = 0;
    while i <= haystack.len() {
        if i == haystack.len() || haystack[i] == b'\n' {
            let line_len = i - line_start;

            if line_len == nlen {
                let mut j = 0;
                while j < nlen && haystack[line_start + j] == needle[j] {
                    j += 1;
                }
                if j == nlen {
                    return true;
                }
            }
            line_start = i + 1;
        }
        i += 1;
    }
    false
}

#[macro_export]
macro_rules! help_section {
    ($section:expr) => {{
        const _: () = assert!($crate::help_section_exists($section));
        $section
    }};
}

pub type Timepoint = f64;

/// Return the number of seconds from the UNIX epoch, with subsecond precision. This function uses
/// the gettimeofday function and will have the same precision as that function.
pub fn timef() -> Timepoint {
    match time::SystemTime::now().duration_since(time::UNIX_EPOCH) {
        Ok(difference) => difference.as_secs() as f64,
        Err(until_epoch) => -(until_epoch.duration().as_secs() as f64),
    }
}

/// Be able to restore the term's foreground process group.
/// This is set during startup and not modified after.
static INITIAL_FG_PROCESS_GROUP: AtomicI32 = AtomicI32::new(-1); // HACK, should be pid_t
const _: () = assert!(mem::size_of::<i32>() >= mem::size_of::<libc::pid_t>());

/// Save the value of tcgetpgrp so we can restore it on exit.
pub fn save_term_foreground_process_group() {
    INITIAL_FG_PROCESS_GROUP.store(unsafe { libc::tcgetpgrp(STDIN_FILENO) }, Ordering::Relaxed);
}

pub fn restore_term_foreground_process_group_for_exit() {
    // We wish to restore the tty to the initial owner. There's two ways this can go wrong:
    //  1. We may steal the tty from someone else (#7060).
    //  2. The call to tcsetpgrp may deliver SIGSTOP to us, and we will not exit.
    // Hanging on exit seems worse, so ensure that SIGTTOU is ignored so we do not get SIGSTOP.
    // Note initial_fg_process_group == 0 is possible with Linux pid namespaces.
    // This is called during shutdown and from a signal handler. We don't bother to complain on
    // failure because doing so is unlikely to be noticed.
    // Safety: All of getpgrp, signal, and tcsetpgrp are async-signal-safe.
    let initial_fg_process_group = INITIAL_FG_PROCESS_GROUP.load(Ordering::Relaxed);
    if initial_fg_process_group > 0 && initial_fg_process_group != unsafe { libc::getpgrp() } {
        unsafe {
            libc::signal(SIGTTOU, SIG_IGN);
            libc::tcsetpgrp(STDIN_FILENO, initial_fg_process_group);
        }
    }
}

/// A wrapper around Cell which supports modifying the contents, scoped to a region of code.
/// This provides a somewhat nicer API than ScopedRefCell because you can directly modify the value,
/// instead of requiring an accessor function which returns a mutable reference to a field.
pub struct ScopedCell<T>(Cell<T>);

impl<T> Deref for ScopedCell<T> {
    type Target = Cell<T>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for ScopedCell<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<T: Copy> ScopedCell<T> {
    pub fn new(value: T) -> Self {
        Self(Cell::new(value))
    }

    /// Temporarily modify a value in the ScopedCell, restoring it when the returned object is dropped.
    ///
    /// This is useful when you want to apply a change for the duration of a scope
    /// without having to manually restore the previous value.
    ///
    /// # Example
    ///
    /// ```
    /// use fish_common::ScopedCell;
    ///
    /// let cell = ScopedCell::new(5);
    /// assert_eq!(cell.get(), 5);
    ///
    /// {
    ///     let _guard = cell.scoped_mod(|v| *v += 10);
    ///     assert_eq!(cell.get(), 15);
    /// }
    ///
    /// // Restored after scope
    /// assert_eq!(cell.get(), 5);
    /// ```
    pub fn scoped_mod<'a, Modifier: FnOnce(&mut T)>(
        &'a self,
        modifier: Modifier,
    ) -> impl ScopeGuarding + 'a {
        let mut val = self.get();
        modifier(&mut val);
        let saved = self.replace(val);
        ScopeGuard::new(self, move |cell| cell.set(saved))
    }
}

/// A wrapper around RefCell which supports modifying the contents, scoped to a region of code.
pub struct ScopedRefCell<T>(RefCell<T>);

impl<T> Deref for ScopedRefCell<T> {
    type Target = RefCell<T>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for ScopedRefCell<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<T> ScopedRefCell<T> {
    pub fn new(value: T) -> Self {
        Self(RefCell::new(value))
    }

    /// Temporarily modify a field in the ScopedRefCell, restoring it when the returned guard is dropped.
    ///
    /// This is useful when you want to change part of a data structure for the duration of a scope,
    /// and automatically restore the original value afterward.
    ///
    /// The `accessor` function selects the field to modify by returning a mutable reference to it.
    ///
    /// # Example
    /// ```
    /// use fish_common::ScopedRefCell;
    ///
    /// struct State { flag: bool }
    ///
    /// let cell = ScopedRefCell::new(State { flag: false });
    /// assert_eq!(cell.borrow().flag, false);
    ///
    /// {
    ///     let _guard = cell.scoped_set(true, |s| &mut s.flag);
    ///     assert_eq!(cell.borrow().flag, true);
    /// }
    ///
    /// // Restored after scope
    /// assert_eq!(cell.borrow().flag, false);
    /// ```
    pub fn scoped_set<'a, Accessor, Value: 'a>(
        &'a self,
        value: Value,
        accessor: Accessor,
    ) -> impl ScopeGuarding + 'a
    where
        Accessor: Fn(&mut T) -> &mut Value + 'a,
    {
        let mut data = self.borrow_mut();
        let mut saved = std::mem::replace(accessor(&mut data), value);
        ScopeGuard::new(self, move |cell| {
            let mut data = cell.borrow_mut();
            std::mem::swap((accessor)(&mut data), &mut saved);
        })
    }

    /// Convenience method for replacing the entire contents of the ScopedRefCell, restoring it when dropped.
    ///
    /// Equivalent to `scoped_set(value, |s| s)`.
    ///
    /// # Example
    /// ```
    /// use fish_common::ScopedRefCell;
    ///
    /// let cell = ScopedRefCell::new(10);
    /// assert_eq!(*cell.borrow(), 10);
    ///
    /// {
    ///     let _guard = cell.scoped_replace(99);
    ///     assert_eq!(*cell.borrow(), 99);
    /// }
    ///
    /// assert_eq!(*cell.borrow(), 10);
    /// ```
    pub fn scoped_replace<'a>(&'a self, value: T) -> impl ScopeGuarding + 'a {
        self.scoped_set(value, |s| s)
    }
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
/// use fish_common::ScopeGuard;
///
/// let file = std::fs::File::create("/dev/null").unwrap();
/// // Create a scope guard to write to the file when the scope expires.
/// // To be able to still use the file, shadow `file` with the ScopeGuard itself.
/// let mut file = ScopeGuard::new(file, |mut file| file.write_all(b"goodbye\n").unwrap());
/// // Now write to the file normally "through" the capturing ScopeGuard instance.
/// file.write_all(b"hello\n").unwrap();
///
/// // hello will be written first, then goodbye.
/// ```
pub struct ScopeGuard<T, F: FnOnce(T)>(Option<(T, F)>);

impl<T, F: FnOnce(T)> ScopeGuard<T, F> {
    /// Creates a new `ScopeGuard` wrapping `value`. The `on_drop` callback is executed when the
    /// ScopeGuard's lifetime expires or when it is manually dropped.
    pub fn new(value: T, on_drop: F) -> Self {
        Self(Some((value, on_drop)))
    }

    /// Invokes the callback, consuming the ScopeGuard.
    pub fn commit(guard: Self) {
        std::mem::drop(guard)
    }

    /// Cancels the invocation of the callback, returning the original wrapped value.
    pub fn cancel(mut guard: Self) -> T {
        let (value, _) = guard.0.take().expect("Should always have Some value");
        value
    }
}

impl<T, F: FnOnce(T)> Deref for ScopeGuard<T, F> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0.as_ref().unwrap().0
    }
}

impl<T, F: FnOnce(T)> DerefMut for ScopeGuard<T, F> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0.as_mut().unwrap().0
    }
}

impl<T, F: FnOnce(T)> Drop for ScopeGuard<T, F> {
    fn drop(&mut self) {
        if let Some((value, on_drop)) = self.0.take() {
            on_drop(value);
        }
    }
}

/// A trait expressing what ScopeGuard can do. This is necessary because our scoped cells return an
/// `impl Trait` object and therefore methods on ScopeGuard which take a self parameter cannot be
/// used.
pub trait ScopeGuarding: DerefMut + Sized {
    /// Invokes the callback, consuming the guard.
    fn commit(guard: Self) {
        std::mem::drop(guard);
    }
}

impl<T, F: FnOnce(T)> ScopeGuarding for ScopeGuard<T, F> {}

pub const fn assert_send<T: Send>() {}
pub const fn assert_sync<T: Sync>() {}

/// Asserts that a slice is alphabetically sorted by a <code>&[wstr]</code> `name` field.
///
/// Mainly useful for static asserts/const eval.
///
/// # Panics
///
/// This function panics if the given slice is unsorted.
///
/// # Examples
///
/// ```
/// use widestring::{utf32str as L,Utf32Str as wstr};
/// use fish_common::assert_sorted_by_name;
///
/// const COLORS: &[(&wstr, u32)] = &[
///     // must be in alphabetical order
///     (L!("blue"), 0x0000ff),
///     (L!("green"), 0x00ff00),
///     (L!("red"), 0xff0000),
/// ];
///
/// assert_sorted_by_name!(COLORS, 0);
/// ```
///
/// While this example would fail to compile:
///
/// ```compile_fail
/// use widestring::{utf32str as L,Utf32Str as wstr};
/// use fish_common::assert_sorted_by_name;
///
/// const COLORS: &[(&wstr, u32)] = &[
///     // not in alphabetical order
///     (L!("green"), 0x00ff00),
///     (L!("blue"), 0x0000ff),
///     (L!("red"), 0xff0000),
/// ];
///
/// assert_sorted_by_name!(COLORS, 0);
/// ```
#[macro_export]
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

pub trait Named {
    fn name(&self) -> &'static wstr;
}

/// Return a reference to the first entry with the given name, assuming the entries are sorted by
/// name. Return None if not found.
pub fn get_by_sorted_name<T: Named>(name: &wstr, vals: &'static [T]) -> Option<&'static T> {
    match vals.binary_search_by_key(&name, |val| val.name()) {
        Ok(index) => Some(&vals[index]),
        Err(_) => None,
    }
}

/// Given an input string, return a prefix of the string up to the first NUL character,
/// or the entire string if there is no NUL character.
pub fn truncate_at_nul(input: &wstr) -> &wstr {
    match input.chars().position(|c| c == '\0') {
        Some(nul_pos) => &input[..nul_pos],
        None => input,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scoped_cell() {
        let cell = ScopedCell::new(42);

        {
            let _guard = cell.scoped_mod(|x| *x += 1);
            assert_eq!(cell.get(), 43);
        }

        assert_eq!(cell.get(), 42);
    }

    #[test]
    fn test_scoped_refcell() {
        #[derive(Debug, PartialEq, Clone)]
        struct Data {
            x: i32,
            y: i32,
        }

        let cell = ScopedRefCell::new(Data { x: 1, y: 2 });

        {
            let _guard = cell.scoped_set(10, |d| &mut d.x);
            assert_eq!(cell.borrow().x, 10);
        }
        assert_eq!(cell.borrow().x, 1);

        {
            let _guard = cell.scoped_replace(Data { x: 42, y: 99 });
            assert_eq!(*cell.borrow(), Data { x: 42, y: 99 });
        }
        assert_eq!(*cell.borrow(), Data { x: 1, y: 2 });
    }

    #[test]
    fn test_scope_guard() {
        let relaxed = std::sync::atomic::Ordering::Relaxed;
        let counter = std::sync::atomic::AtomicUsize::new(0);
        {
            let guard = ScopeGuard::new(123, |arg| {
                assert_eq!(arg, 123);
                counter.fetch_add(1, relaxed);
            });
            assert_eq!(counter.load(relaxed), 0);
            std::mem::drop(guard);
            assert_eq!(counter.load(relaxed), 1);
        }
        // commit also invokes the callback.
        {
            let guard = ScopeGuard::new(123, |arg| {
                assert_eq!(arg, 123);
                counter.fetch_add(1, relaxed);
            });
            assert_eq!(counter.load(relaxed), 1);
            ScopeGuard::commit(guard);
            assert_eq!(counter.load(relaxed), 2);
        }
    }

    #[test]
    fn test_truncate_at_nul() {
        use widestring::utf32str as L;
        assert_eq!(truncate_at_nul(L!("abc\0def")), L!("abc"));
        assert_eq!(truncate_at_nul(L!("abc")), L!("abc"));
        assert_eq!(truncate_at_nul(L!("\0abc")), L!(""));
    }
}
