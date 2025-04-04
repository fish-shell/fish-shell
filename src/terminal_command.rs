use crate::common::{escape_string, wcs2string, EscapeStringStyle};
use crate::global_safety::RelaxedAtomicBool;
use crate::output::InfallibleWrite;
use crate::wchar::prelude::*;
use std::sync::atomic::AtomicU8;

// Text attributes

pub(crate) const EXIT_ATTRIBUTE_MODE: &[u8] = b"\x1b[m";
pub(crate) const ENTER_BOLD_MODE: &[u8] = b"\x1b[1m";
pub(crate) const ENTER_DIM_MODE: &[u8] = b"\x1b[2m";
pub(crate) const ENTER_ITALICS_MODE: &[u8] = b"\x1b[3m";
pub(crate) const ENTER_UNDERLINE_MODE: &[u8] = b"\x1b[4m";
pub(crate) const ENTER_REVERSE_MODE: &[u8] = b"\x1b[7m";
pub(crate) const EXIT_ITALICS_MODE: &[u8] = b"\x1b[23m";
pub(crate) const EXIT_UNDERLINE_MODE: &[u8] = b"\x1b[24m";

// Screen clearing

pub(crate) const CLEAR_SCREEN: &[u8] = b"\x1b[H\x1b[2J";
/// clr_eol
pub(crate) const CLEAR_TO_END_OF_LINE: &[u8] = b"\x1b[K";
/// clr_eos
pub(crate) const CLEAR_TO_END_OF_SCREEN: &[u8] = b"\x1b[J";

// Colors

pub(crate) fn palette_color(out: &mut impl InfallibleWrite, foreground: bool, idx: u8) {
    let bg = if foreground { 0 } else { 10 };
    match idx {
        0..=7 => infallible_write!(out, "\x1b[{}m", 30 + bg + idx),
        8..=15 => infallible_write!(out, "\x1b[{}m", 90 + bg + (idx - 8)),
        _ => infallible_write!(out, "\x1b[{};5;{}m", 38 + bg, idx),
    }
}

pub(crate) fn rgb_color(out: &mut impl InfallibleWrite, foreground: bool, r: u8, g: u8, b: u8) {
    // Foreground: ^[38;2;<r>;<g>;<b>m
    // Background: ^[48;2;<r>;<g>;<b>m
    infallible_write!(
        out,
        "\x1b[{};2;{};{};{}m",
        if foreground { 38 } else { 48 },
        r,
        g,
        b
    )
}

// Cursor Movement

pub(crate) const CURSOR_UP: &[u8] = b"\x1b[A";
pub(crate) const CURSOR_DOWN: &[u8] = b"\n";
pub(crate) const CURSOR_LEFT: &[u8] = b"\x08";
pub(crate) const CURSOR_RIGHT: &[u8] = b"\x1b[C";

pub(crate) enum CardinalDirection {
    Up,
    Left,
    Right,
}

pub(crate) fn cursor_move(
    out: &mut impl InfallibleWrite,
    direction: CardinalDirection,
    steps: usize,
) {
    infallible_write!(
        out,
        "\x1b[{steps}{}",
        match direction {
            CardinalDirection::Up => 'A',
            CardinalDirection::Left => 'D',
            CardinalDirection::Right => 'C',
        }
    );
}

// Commands related to querying (used for backwards-incompatible features).
pub(crate) const QUERY_PRIMARY_DEVICE_ATTRIBUTE: &[u8] = b"\x1b[0c";
pub(crate) const QUERY_XTVERSION: &[u8] = b"\x1b[>0q";

pub(crate) fn query_xtgettcap(out: &mut impl InfallibleWrite, cap: &str) {
    infallible_write!(out, "\x1bP+q{}\x1b\\", DisplayAsHex(cap));
}

pub(crate) static DECSET_ALTERNATE_SCREEN_BUFFER: &[u8] = b"\x1b[?1049h";
pub(crate) static DECRST_ALTERNATE_SCREEN_BUFFER: &[u8] = b"\x1b[?1049l";

pub(crate) static DECSET_SYNCHRONIZED_UPDATE: &[u8] = b"\x1b[?2026h";
pub(crate) static DECRST_SYNCHRONIZED_UPDATE: &[u8] = b"\x1b[?2026l";

pub(crate) const QUERY_SYNCHRONIZED_OUTPUT: &[u8] = b"\x1b[?2026$p";
pub(crate) static SYNCHRONIZED_OUTPUT_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

// Keyboard protocols

pub(crate) static KITTY_KEYBOARD_SUPPORTED: AtomicU8 = AtomicU8::new(Capability::Unknown as _);
pub(crate) static KITTY_KEYBOARD_PROGRESSIVE_ENHANCEMENTS_ENABLE: &[u8] = b"\x1b[=5u";
pub(crate) static KITTY_KEYBOARD_PROGRESSIVE_ENHANCEMENTS_DISABLE: &[u8] = b"\x1b[=0u";
pub(crate) const QUERY_KITTY_KEYBOARD_PROGRESSIVE_ENHANCEMENTS: &[u8] = b"\x1b[?u";

pub(crate) const MODIFY_OTHER_KEYS_DISABLE: &[u8] = b"\x1b[>4;0m";
// Enable level 1 only; level 2 has issues with shifted keys.
pub(crate) const MODIFY_OTHER_KEYS_ENABLE: &[u8] = b"\x1b[>4;1m";

pub(crate) const APPLICATION_KEYPAD_MODE: &[u8] = b"\x1b=";
pub(crate) const NO_APPLICATION_KEYPAD_MODE: &[u8] = b"\x1b>";

// OSC sequences
//
// Note that OSC 7 and OSC 52 are written from fish script.

pub(crate) fn osc_0_window_title(out: &mut impl InfallibleWrite, title: &[WString]) {
    out.infallible_write(b"\x1b]0;");
    for title_line in title {
        out.infallible_write(&wcs2string(title_line));
    }
    out.infallible_write(b"\x07"); // BEL
}

pub(crate) const OSC_133_PROMPT_START: &[u8] = b"\x1b]133;A;click_events=1\x07";

pub(crate) fn osc_133_command_start(out: &mut impl InfallibleWrite, command: &wstr) {
    infallible_write!(
        out,
        "\x1b]133;C;cmdline_url={}\x07",
        escape_string(command, EscapeStringStyle::Url),
    );
}

pub(crate) fn osc_133_command_finished(out: &mut impl InfallibleWrite, exit_status: libc::c_int) {
    infallible_write!(out, "\x1b]133;D;{}\x07", exit_status);
}

// Other terminal features

pub(crate) const QUERY_CURSOR_POSITION: &[u8] = b"\x1b[6n";

pub(crate) static SCROLL_FORWARD_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
pub(crate) static SCROLL_FORWARD_TERMINFO_NAME: &str = "indn";
pub(crate) const SCROLL_FORWARD_SEQUENCE_DESCS: [&[u8]; 2] = [b"\x1b[%p1%dS", b"\\E[%p1%dS"];
pub(crate) fn scroll_forward(out: &mut impl InfallibleWrite, lines: usize) {
    assert!(SCROLL_FORWARD_SUPPORTED.load());
    infallible_write!(out, "\x1b[{}S", lines)
}
pub(crate) const DECSET_SHOW_CURSOR: &[u8] = b"\x1b[?25h";

pub(crate) const DECRST_MOUSE_TRACKING: &[u8] = b"\x1b[?1000l";

pub(crate) const DECSET_FOCUS_REPORTING: &[u8] = b"\x1b[?1004h";
pub(crate) const DECRST_FOCUS_REPORTING: &[u8] = b"\x1b[?1004l";

pub(crate) const DECSET_BRACKETED_PASTE: &[u8] = b"\x1b[?2004h";
pub(crate) const DECRST_BRACKETED_PASTE: &[u8] = b"\x1b[?2004l";

// Helpers

#[repr(u8)]
pub(crate) enum Capability {
    Unknown,
    Supported,
    NotSupported,
}

struct DisplayAsHex<'a>(&'a str);

impl<'a> std::fmt::Display for DisplayAsHex<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for byte in self.0.bytes() {
            write!(f, "{:x}", byte)?;
        }
        Ok(())
    }
}
