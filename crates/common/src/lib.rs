use libc::STDIN_FILENO;
use std::env;
use std::os::unix::ffi::OsStrExt;
use std::sync::OnceLock;

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
