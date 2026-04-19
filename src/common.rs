//! Prototypes for various functions, mostly string utilities, that are used by most parts of fish.

use crate::{
    global_safety::{AtomicRef, RelaxedAtomicBool},
    prelude::*,
    terminal::Outputter,
    termsize::Termsize,
    wutil::fish_iswalnum,
};
use fish_fallback::fish_wcwidth;
use fish_widestring::subslice_position;
use nix::sys::termios::Termios;
use std::{
    env,
    sync::{MutexGuard, OnceLock, atomic::Ordering},
};

use fish_common::*;

pub const BUILD_DIR: &str = env!("FISH_RESOLVED_BUILD_DIR");

pub const PACKAGE_NAME: &str = env!("CARGO_PKG_NAME");

pub fn shell_modes() -> MutexGuard<'static, Termios> {
    crate::reader::SHELL_MODES.lock().unwrap()
}

/// Character representing an omitted newline at the end of text.
pub fn get_omitted_newline_str() -> &'static str {
    OMITTED_NEWLINE_STR.load()
}

static OMITTED_NEWLINE_STR: AtomicRef<str> = AtomicRef::new(&"");

/// Profiling flag. True if commands should be profiled.
pub static PROFILING_ACTIVE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Name of the current program. Should be set at startup. Used by the debug function.
pub static PROGRAM_NAME: OnceLock<&'static wstr> = OnceLock::new();

pub fn get_program_name() -> &'static wstr {
    PROGRAM_NAME.get().unwrap()
}

/// MS Windows tty devices do not currently have either a read or write timestamp - those respective
/// fields of `struct stat` are always set to the current time, which means we can't rely on them.
/// In this case, we assume no external program has written to the terminal behind our back, making
/// the multiline prompt usable. See [#2859](https://github.com/fish-shell/fish-shell/issues/2859)
/// and <https://github.com/Microsoft/BashOnWindows/issues/545>
pub fn has_working_tty_timestamps() -> bool {
    if cfg!(any(target_os = "windows", cygwin)) {
        false
    } else if cfg!(target_os = "linux") {
        !is_windows_subsystem_for_linux(WSL::V1)
    } else {
        true
    }
}

/// A function type to check for cancellation.
/// Return true if execution should cancel.
/// todo!("Maybe remove the box? It is only needed for get_bg_context.")
pub type CancelChecker = Box<dyn Fn() -> bool>;

pub fn init_special_chars_once() {
    if is_windows_subsystem_for_linux(WSL::Any) {
        // neither of \u23CE and \u25CF can be displayed in the default fonts on Windows, though
        // they can be *encoded* just fine. Use alternative glyphs.
        OMITTED_NEWLINE_STR.store(&"\u{00b6}"); // "pilcrow"
        OBFUSCATION_READ_CHAR.store(u32::from('\u{2022}'), Ordering::Relaxed); // "bullet" (•)
    } else if is_console_session() {
        OMITTED_NEWLINE_STR.store(&"^J");
        OBFUSCATION_READ_CHAR.store(u32::from('*'), Ordering::Relaxed);
    } else {
        OMITTED_NEWLINE_STR.store(&"\u{23CE}"); // "return symbol" (⏎)
        OBFUSCATION_READ_CHAR.store(
            u32::from(
                '\u{25CF}', // "black circle" (●)
            ),
            Ordering::Relaxed,
        );
    }
}

/// Test if the string is a valid function name.
pub fn valid_func_name(name: &wstr) -> bool {
    !(name.is_empty()
    || name.starts_with('-')
    // A function name needs to be a valid path, so no / and no NULL.
    || name.contains('/')
    || name.contains('\0'))
}

// Output writes always succeed; this adapter allows us to use it in a write-like macro.
struct OutputterWriteAdapter<'a>(&'a mut Outputter);

impl<'a> std::fmt::Write for OutputterWriteAdapter<'a> {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        self.0.write_bytes(s.as_bytes());
        Ok(())
    }
}

impl<'a> std::io::Write for OutputterWriteAdapter<'a> {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.0.write_bytes(buf);
        Ok(buf.len())
    }
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

pub(crate) fn do_write_to_output(writer: &mut Outputter, args: std::fmt::Arguments<'_>) {
    let mut adapter = OutputterWriteAdapter(writer);
    std::fmt::write(&mut adapter, args).unwrap();
}

#[macro_export]
macro_rules! write_to_output {
    ($out:expr, $($arg:tt)*) => {{
        $crate::common::do_write_to_output($out, format_args!($($arg)*));
    }};
}

/// Write the given paragraph of output, redoing linebreaks to fit `termsize`.
pub fn reformat_for_screen(msg: &wstr, termsize: &Termsize) -> WString {
    let mut buff = WString::new();

    let screen_width = termsize.width();
    if screen_width != 0 {
        let mut start = 0;
        let mut pos = start;
        let mut line_width = 0;
        while pos < msg.len() {
            let mut overflow = false;
            let mut tok_width = 0;

            // Tokenize on whitespace, and also calculate the width of the token.
            while pos < msg.len() && ![' ', '\n', '\r', '\t'].contains(&msg.char_at(pos)) {
                // Check is token is wider than one line. If so we mark it as an overflow and break
                // the token.
                let width = fish_wcwidth(msg.char_at(pos)).unwrap_or_default();
                if (tok_width + width) > (screen_width - 1) {
                    overflow = true;
                    break;
                }
                tok_width += width;
                pos += 1;
            }

            // If token is zero character long, we don't do anything.
            if pos == start {
                pos += 1;
            } else if overflow {
                // In case of overflow, we print a newline, except if we already are at position 0.
                let token = &msg[start..pos];
                if line_width != 0 {
                    buff.push('\n');
                }
                buff += &sprintf!("%s-\n", token)[..];
                line_width = 0;
            } else {
                // Print the token.
                let token = &msg[start..pos];
                let line_width_unit = if line_width != 0 { 1 } else { 0 };
                if (line_width + line_width_unit + tok_width) > screen_width {
                    buff.push('\n');
                    line_width = 0;
                }
                if line_width != 0 {
                    buff += L!(" ");
                }
                buff += token;
                line_width += line_width_unit + tok_width;
            }

            start = pos;
        }
    } else {
        buff += msg;
    }
    buff.push('\n');
    buff
}

#[allow(unused)]
// This function is unused in some configurations/on some platforms
fn slice_contains_slice<T: Eq>(a: &[T], b: &[T]) -> bool {
    subslice_position(a, b).is_some()
}

#[derive(Copy, Debug, Clone, PartialEq, Eq)]
pub enum WSL {
    Any,
    V1,
    V2,
}

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
///
/// See <https://github.com/Microsoft/WSL/issues/423> and [Microsoft/WSL#2997](https://github.com/Microsoft/WSL/issues/2997)
#[inline(always)]
#[cfg(not(target_os = "linux"))]
pub fn is_windows_subsystem_for_linux(_: WSL) -> bool {
    false
}

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
///
/// See <https://github.com/Microsoft/WSL/issues/423> and [Microsoft/WSL#2997](https://github.com/Microsoft/WSL/issues/2997)
#[cfg(target_os = "linux")]
pub fn is_windows_subsystem_for_linux(v: WSL) -> bool {
    use std::sync::OnceLock;
    static RESULT: OnceLock<Option<WSL>> = OnceLock::new();

    // This is called post-fork from [`report_setpgid_error()`], so the fast path must not involve
    // any allocations or mutexes. We can't rely on all the std functions to be alloc-free in both
    // Debug and Release modes, so we just mandate that the result already be available.
    //
    // is_wsl() is called by has_working_tty_timestamps() which is called by `screen.rs` in the main
    // process. If that's not good enough, we can call is_wsl() manually at shell startup.
    if crate::threads::is_forked_child() {
        debug_assert!(
            RESULT.get().is_some(),
            "is_wsl() should be called by main before forking!"
        );
    }

    let wsl = RESULT.get_or_init(|| {
        let release = unsafe {
            let mut info = std::mem::MaybeUninit::uninit();
            libc::uname(info.as_mut_ptr());
            let info = info.assume_init();
            info.release
        };
        let release: &[u8] = unsafe { std::mem::transmute(&release[..]) };

        // Sample utsname.release under WSLv2, testing for something like `4.19.104-microsoft-standard`
        // or `5.10.16.3-microsoft-standard-WSL2`
        if slice_contains_slice(release, b"microsoft-standard") {
            return Some(WSL::V2);
        }
        // Sample utsname.release under WSL, testing for something like `4.4.0-17763-Microsoft`
        if !slice_contains_slice(release, b"Microsoft") {
            return None;
        }

        let release: Vec<_> = release
            .iter()
            .copied()
            .skip_while(|c| *c != b'-')
            .skip(1) // the dash itself
            .take_while(|c| c.is_ascii_digit())
            .collect();
        let build: Result<u32, _> = std::str::from_utf8(&release).unwrap().parse();
        match build {
            Ok(17763..) => return Some(WSL::V1),
            Ok(_) => (),      // return true, but first warn (see below)
            _ => return None, // if parsing fails, assume this isn't WSL
        }

        // #5298, #5661: There are acknowledged, published, and (later) fixed issues with
        // job control under early WSL releases that prevent fish from running correctly,
        // with unexpected failures when piping. Fish 3.0 nightly builds worked around this
        // issue with some needlessly complicated code that was later stripped from the
        // fish 3.0 release, so we just bail. Note that fish 2.0 was also broken, but we
        // just didn't warn about it.

        // #6038 & 5101bde: It's been requested that there be some sort of way to disable
        // this check: if the environment variable FISH_NO_WSL_CHECK is present, this test
        // is bypassed. We intentionally do not include this in the error message because
        // it'll only allow fish to run but not to actually work. Here be dragons!
        use crate::flog::flog;
        if env::var_os("FISH_NO_WSL_CHECK").is_none() {
            flog!(
                error,
                concat!(
                    "This version of WSL has known bugs that prevent fish from working.\n",
                    "Please upgrade to Windows 10 1809 (17763) or higher to use fish!"
                )
            );
        }
        Some(WSL::V1)
    });

    wsl.is_some_and(|wsl| v == WSL::Any || wsl == v)
}

/// Test if the given char is valid in a variable name.
pub fn valid_var_name_char(chr: char) -> bool {
    fish_iswalnum(chr) || chr == '_'
}

/// Test if the given string is a valid variable name.
pub fn valid_var_name(s: &wstr) -> bool {
    // Note do not use c_str(), we want to fail on embedded nul bytes.
    !s.is_empty() && s.chars().all(valid_var_name_char)
}

#[macro_export]
macro_rules! env_stack_set_from_env {
    ($vars:ident, $var_name:literal) => {{
        if let Some(var) = std::env::var_os($var_name) {
            $vars.set_one(
                L!($var_name),
                $crate::env::EnvSetMode::new_at_early_startup($crate::env::EnvMode::GLOBAL),
                fish_widestring::osstr2wcstring(var),
            );
        }
    }};
}

/// The highest character number of character to try and escape.
#[cfg(test)]
pub const ESCAPE_TEST_CHAR: usize = 4000;

#[cfg(test)]
mod tests {
    use super::{
        ESCAPE_TEST_CHAR, EscapeFlags, EscapeStringStyle, UnescapeStringStyle, escape_string,
        escape_string_with_quote, unescape_string,
    };
    use crate::tests::prelude::*;
    use fish_util::get_seeded_rng;
    use fish_widestring::{
        ENCODE_DIRECT_BASE, ENCODE_DIRECT_END, L, WString, bytes2wcstring, decode_with_replacement,
        wcs2bytes, wstr,
    };
    use rand::{Rng as _, RngCore as _};
    use std::fmt::Write as _;

    #[test]
    fn test_escape_string() {
        let regex = |input| escape_string(input, EscapeStringStyle::Regex);

        // plain text should not be needlessly escaped
        assert_eq!(regex(L!("hello world!")), L!("hello world!"));

        // all the following are intended to be ultimately matched literally - even if they
        // don't look like that's the intent - so we escape them.
        assert_eq!(regex(L!(".ext")), L!("\\.ext"));
        assert_eq!(regex(L!("{word}")), L!("\\{word\\}"));
        assert_eq!(regex(L!("hola-mundo")), L!("hola\\-mundo"));
        assert_eq!(
            regex(L!("$17.42 is your total?")),
            L!("\\$17\\.42 is your total\\?")
        );
        assert_eq!(
            regex(L!("not really escaped\\?")),
            L!("not really escaped\\\\\\?")
        );
    }

    #[test]
    pub fn test_unescape_sane() {
        const TEST_CASES: &[(&wstr, &wstr)] = &[
            (L!("abcd"), L!("abcd")),
            (L!("'abcd'"), L!("abcd")),
            (L!("'abcd\\n'"), L!("abcd\\n")),
            (L!("\"abcd\\n\""), L!("abcd\\n")),
            (L!("\"abcd\\n\""), L!("abcd\\n")),
            (L!("\\143"), L!("c")),
            (L!("'\\143'"), L!("\\143")),
            (L!("\\n"), L!("\n")), // \n normally becomes newline
        ];

        for (input, expected) in TEST_CASES {
            let Some(output) = unescape_string(input, UnescapeStringStyle::default()) else {
                panic!("Failed to unescape string {input:?}");
            };

            assert_eq!(
                output, *expected,
                "In unescaping {input:?}, expected {expected:?} but got {output:?}\n"
            );
        }
    }

    #[test]
    fn test_escape_var() {
        const TEST_CASES: &[(&wstr, &wstr)] = &[
            (L!(" a"), L!("_20_a")),
            (L!("a B "), L!("a_20_42_20_")),
            (L!("a b "), L!("a_20_b_20_")),
            (L!(" B"), L!("_20_42_")),
            (L!(" f"), L!("_20_f")),
            (L!(" 1"), L!("_20_31_")),
            (L!("a\nghi_"), L!("a_0A_ghi__")),
        ];

        for (input, expected) in TEST_CASES {
            let output = escape_string(input, EscapeStringStyle::Var);

            assert_eq!(
                output, *expected,
                "In escaping {input:?} with style var, expected {expected:?} but got {output:?}\n"
            );
        }
    }

    fn escape_test(escape_style: EscapeStringStyle, unescape_style: UnescapeStringStyle) {
        let seed = rand::rng().next_u64();
        let mut rng = get_seeded_rng(seed);

        let mut random_string = WString::new();
        let mut escaped_string;
        for _ in 0..(ESCAPE_TEST_COUNT as u32) {
            random_string.clear();
            let length = rng.random_range(0..=(2 * ESCAPE_TEST_LENGTH));
            for _ in 0..length {
                random_string
                    .push(char::from_u32((rng.next_u32() % ESCAPE_TEST_CHAR as u32) + 1).unwrap());
            }

            escaped_string = escape_string(&random_string, escape_style);
            let Some(unescaped_string) = unescape_string(&escaped_string, unescape_style) else {
                let slice = escaped_string.as_char_slice();
                panic!("Failed to unescape string {slice:?}. Generated from seed {seed}.");
            };
            assert_eq!(
                random_string, unescaped_string,
                "Escaped and then unescaped string {random_string:?}, but got back a different string {unescaped_string:?}. \
                The intermediate escape looked like {escaped_string:?}. \
                Generated from seed {seed}."
            );
        }
    }

    #[test]
    fn test_escape_random_script() {
        escape_test(EscapeStringStyle::default(), UnescapeStringStyle::default());
    }

    #[test]
    fn test_escape_random_var() {
        escape_test(EscapeStringStyle::Var, UnescapeStringStyle::Var);
    }

    #[test]
    fn test_escape_random_url() {
        escape_test(EscapeStringStyle::Url, UnescapeStringStyle::Url);
    }

    #[test]
    fn test_escape_no_printables() {
        // Verify that ESCAPE_NO_PRINTABLES also escapes backslashes so we don't regress on issue #3892.
        let random_string = L!("line 1\\n\nline 2").to_owned();
        let escaped_string = escape_string(
            &random_string,
            EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
        );
        let Some(unescaped_string) =
            unescape_string(&escaped_string, UnescapeStringStyle::default())
        else {
            panic!("Failed to unescape string <{escaped_string}>");
        };

        assert_eq!(
            random_string, unescaped_string,
            "Escaped and then unescaped string '{random_string}', but got back a different string '{unescaped_string}'"
        );
    }

    #[test]
    #[serial]
    fn test_escape_quotes() {
        let _cleanup = test_init();
        macro_rules! validate {
            ($cmd:expr, $quote:expr, $no_tilde:expr, $expected:expr) => {
                assert_eq!(
                    escape_string_with_quote(
                        L!($cmd),
                        $quote,
                        if $no_tilde {
                            EscapeFlags::NO_TILDE
                        } else {
                            EscapeFlags::empty()
                        }
                    ),
                    L!($expected)
                );
            };
        }
        macro_rules! validate_no_quoted {
            ($cmd:expr, $quote:expr, $no_tilde:expr, $expected:expr) => {
                assert_eq!(
                    escape_string_with_quote(
                        L!($cmd),
                        $quote,
                        EscapeFlags::NO_QUOTED
                            | if $no_tilde {
                                EscapeFlags::NO_TILDE
                            } else {
                                EscapeFlags::empty()
                            }
                    ),
                    L!($expected)
                );
            };
        }

        validate!("abc~def", None, false, "'abc~def'");
        validate!("abc~def", None, true, "abc~def");
        validate!("~abc", None, false, "'~abc'");
        validate!("~abc", None, true, "~abc");

        // These are "raw string literals"
        validate_no_quoted!("abc", None, false, "abc");
        validate_no_quoted!("abc~def", None, false, "abc\\~def");
        validate_no_quoted!("abc~def", None, true, "abc~def");
        validate_no_quoted!("abc\\~def", None, false, "abc\\\\\\~def");
        validate_no_quoted!("abc\\~def", None, true, "abc\\\\~def");
        validate_no_quoted!("~abc", None, false, "\\~abc");
        validate_no_quoted!("~abc", None, true, "~abc");
        validate_no_quoted!("~abc|def", None, false, "\\~abc\\|def");
        validate_no_quoted!("|abc~def", None, false, "\\|abc\\~def");
        validate_no_quoted!("|abc~def", None, true, "\\|abc~def");
        validate_no_quoted!("foo\nbar", None, false, "foo\\nbar");

        // Note tildes are not expanded inside quotes, so no_tilde is ignored with a quote.
        validate_no_quoted!("abc", Some('\''), false, "abc");
        validate_no_quoted!("abc\\def", Some('\''), false, "abc\\\\def");
        validate_no_quoted!("abc'def", Some('\''), false, "abc\\'def");
        validate_no_quoted!("~abc'def", Some('\''), false, "~abc\\'def");
        validate_no_quoted!("~abc'def", Some('\''), true, "~abc\\'def");
        validate_no_quoted!("foo\nba'r", Some('\''), false, "foo'\\n'ba\\'r");
        validate_no_quoted!("foo\\\\bar", Some('\''), false, "foo\\\\\\\\bar");

        validate_no_quoted!("abc", Some('"'), false, "abc");
        validate_no_quoted!("abc\\def", Some('"'), false, "abc\\\\def");
        validate_no_quoted!("~abc'def", Some('"'), false, "~abc'def");
        validate_no_quoted!("~abc'def", Some('"'), true, "~abc'def");
        validate_no_quoted!("foo\nba'r", Some('"'), false, "foo\"\\n\"ba'r");
        validate_no_quoted!("foo\\\\bar", Some('"'), false, "foo\\\\\\\\bar");
    }

    /// The number of tests to run.
    const ESCAPE_TEST_COUNT: usize = 20_000;
    /// The average length of strings to unescape.
    const ESCAPE_TEST_LENGTH: usize = 100;

    /// Helper to convert a narrow string to a sequence of hex digits.
    fn bytes2hex(input: &[u8]) -> String {
        let mut output = String::with_capacity(input.len() * 5);
        for byte in input {
            write!(output, "0x{:2X} ", *byte).unwrap();
        }
        output
    }

    /// Test wide/narrow conversion by creating random strings and verifying that the original
    /// string comes back through double conversion.
    #[test]
    fn test_convert() {
        let seed = rand::rng().next_u64();
        let mut rng = get_seeded_rng(seed);
        let mut origin = Vec::new();

        for _ in 0..ESCAPE_TEST_COUNT {
            let length: usize = rng.random_range(0..=(2 * ESCAPE_TEST_LENGTH));
            origin.resize(length, 0);
            rng.fill_bytes(&mut origin);

            let w = bytes2wcstring(&origin[..]);
            let n = wcs2bytes(&w);
            assert_eq!(
                origin,
                n,
                "Conversion cycle of string:\n{:4} chars: {}\n\
                    produced different string:\n\
                    {:4} chars: {}\n
                    Use this seed to reproduce: {}",
                origin.len(),
                &bytes2hex(&origin),
                n.len(),
                &bytes2hex(&n),
                seed,
            );
        }
    }

    /// Verify correct behavior with embedded nulls.
    #[test]
    fn test_convert_nulls() {
        let input = L!("AAA\0BBB");
        let out_str = wcs2bytes(input);
        assert_eq!(
            input.chars().collect::<Vec<_>>(),
            std::str::from_utf8(&out_str)
                .unwrap()
                .chars()
                .collect::<Vec<_>>()
        );

        let out_wstr = bytes2wcstring(&out_str);
        assert_eq!(input, &out_wstr);
    }

    /// Verify that ASCII narrow->wide conversions are correct.
    #[test]
    fn test_convert_ascii() {
        let mut s = vec![b'\0'; 4096];
        for (i, c) in s.iter_mut().enumerate() {
            *c = u8::try_from(i % 10).unwrap() + b'0';
        }

        // Test a variety of alignments.
        for left in 0..16 {
            for right in 0..16 {
                let len = s.len() - left - right;
                let input = &s[left..left + len];
                let wide = bytes2wcstring(input);
                let narrow = wcs2bytes(&wide);
                assert_eq!(narrow, input);
            }
        }

        // Put some non-ASCII bytes in and ensure it all still works.
        for i in 0..s.len() {
            let saved = s[i];
            s[i] = 0xF7;
            assert_eq!(wcs2bytes(&bytes2wcstring(&s)), s);
            s[i] = saved;
        }
    }

    /// fish uses the private-use range to encode bytes that are not valid UTF-8.
    /// If the input decodes to these private-use codepoints,
    /// then fish should also use the direct encoding for those bytes.
    /// Verify that characters in the private use area are correctly round-tripped. See #7723.
    #[test]
    fn test_convert_private_use() {
        for c in ENCODE_DIRECT_BASE..ENCODE_DIRECT_END {
            // A `char` represents an Unicode scalar value, which takes up at most 4 bytes when encoded in UTF-8.
            // TODO MSRV(1.92?) replace 4 by `char::MAX_LEN_UTF8` once that's available in our MSRV.
            // https://doc.rust-lang.org/std/primitive.char.html#associatedconstant.MAX_LEN_UTF8
            let mut converted = [0_u8; 4];
            let s = c.encode_utf8(&mut converted).as_bytes();
            // Ask fish to decode this via bytes2wcstring.
            // bytes2wcstring should notice that the decoded form collides with its private use
            // and encode it directly.
            let ws = bytes2wcstring(s);

            // Each byte should be encoded directly, and round tripping should work.
            assert_eq!(ws.len(), s.len());
            assert_eq!(wcs2bytes(&ws), s);
        }
    }

    #[test]
    fn test_decode() {
        macro_rules! check_decode {
            ($input:expr, $expected:expr) => {{
                let encoded_input = bytes2wcstring($input);
                assert_eq!(
                    &decode_with_replacement(&encoded_input).collect::<Vec<_>>(),
                    $expected
                );
                assert_eq!(
                    decode_with_replacement(&encoded_input)
                        .rev()
                        .collect::<Vec<char>>(),
                    $expected.iter().copied().rev().collect::<Vec<char>>()
                );
                // Alternate reading direction
                let mut chars_from_front = vec![];
                let mut chars_from_back = vec![];
                let mut decoder_chars = decode_with_replacement(&encoded_input);
                loop {
                    match decoder_chars.next() {
                        Some(c) => {
                            chars_from_front.push(c);
                        }
                        None => {
                            assert_eq!(decoder_chars.next_back(), None);
                            assert_eq!(decoder_chars.next(), None);
                            break;
                        }
                    }
                    match decoder_chars.next_back() {
                        Some(c) => {
                            chars_from_back.push(c);
                        }
                        None => {
                            assert_eq!(decoder_chars.next(), None);
                            assert_eq!(decoder_chars.next_back(), None);
                            break;
                        }
                    }
                }
                chars_from_front.extend(chars_from_back.iter().copied().rev());
                assert_eq!(&chars_from_front, $expected);
            }};
        }

        check_decode!("asdf".as_bytes(), &['a', 's', 'd', 'f']);
        check_decode!("".as_bytes(), &['']);
        check_decode!("\u{f630}".as_bytes(), &['\u{f630}']);
        check_decode!(&[0xff], &['�']);
        check_decode!(&[0xef, 0x98, 0x80], &['\u{f600}']);
        check_decode!(&[0xef, 0xef, 0x98, 0x80, 0x61], &['�', '\u{f600}', 'a']);
        check_decode!(&[0x98, 0xef, 0xef, 0x80], &['�', '�', '�', '�']);
        check_decode!(&[0xff, 0xef, 0x98, 0x80], &['�', '\u{f600}']);
    }
}

#[cfg(all(nightly, feature = "benchmark"))]
#[cfg(test)]
mod bench {
    extern crate test;
    use fish_widestring::bytes2wcstring;
    use test::Bencher;

    #[bench]
    fn bench_convert_ascii(b: &mut Bencher) {
        let s: [u8; 128 * 1024] = std::array::from_fn(|i| b'0' + u8::try_from(i % 10).unwrap());
        b.bytes = u64::try_from(s.len()).unwrap();
        b.iter(|| bytes2wcstring(&s));
    }
}
