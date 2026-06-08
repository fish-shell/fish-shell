use super::{
    binding::match_key_event_to_key,
    input::{
        CharEvent, ImplicitEvent, InputEventQueuer, InputEventTrigger, KeyEvent, QueryResponse,
        QueryResultEvent, is_event_blocked_when_querying, next_input_event, stop_query,
    },
};
use crate::{
    common::shell_modes,
    fd_readable_set::Timeout,
    flog::{FloggableDisplay, flog},
    key::{
        self, Key, Modifiers, ViewportPosition, alt, canonicalize_control_char,
        canonicalize_keyed_control_char, char_to_symbol, function_key, shift,
    },
    tty_handoff::{
        SCROLL_CONTENT_UP_TERMINFO_CODE, TERMINAL_OS_NAME, XTGETTCAP_QUERY_OS_NAME, XTVERSION,
        maybe_set_kitty_keyboard_capability, maybe_set_scroll_content_up_capability,
    },
};
use fish_widestring::{WString, bytes2wcstring, encode_byte_to_char, fish_reserved_codepoint};
use std::time::Duration;

pub(crate) const LONG_READ_TIMEOUT: Duration = Duration::from_secs(10);

pub(super) fn on_byte_read<Queuer: InputEventQueuer + ?Sized>(
    queuer: &mut Queuer,
    read_byte: u8,
) -> Option<CharEvent> {
    queuer.on_byte_read(read_byte)
}

impl<T: InputEventQueuer + ?Sized> InputEventQueuerExt for T {}

trait InputEventQueuerExt: InputEventQueuer {
    fn on_byte_read(&mut self, read_byte: u8) -> Option<CharEvent> {
        let mut have_escape_prefix = false;
        let mut buffer = vec![read_byte];
        let evt = if read_byte == b'\x1b' {
            self.parse_escape_sequence(&mut buffer, &mut have_escape_prefix)
        } else {
            if self.paste_is_buffering() {
                self.paste_push_char(read_byte);
                return None;
            }
            None
        };
        if evt.as_ref().is_some_and(|key_evt| {
            key_evt
                .get_key()
                .is_some_and(|key_evt| key_evt.key.codepoint == key::INVALID)
        }) {
            return None;
        }
        let decoded_buffer = match decode_utf8_at_least_one(&mut buffer, |buffer| {
            buffer.push(
                match next_input_event(self.get_in_fd(), self.get_ioport_fd(), Timeout::Forever) {
                    InputEventTrigger::Byte(b) => b,
                    _ => 0,
                },
            );
        }) {
            Ok(decoded) => decoded,
            Err(evt) => {
                return Some(evt);
            }
        };
        let mut evt = evt
            .or_else(|| {
                let code_unit_first_byte = *buffer.last().unwrap();
                canonicalize_control_char(code_unit_first_byte)
                    .map(KeyEvent::from)
                    .map(|key_evt| CharEvent::from_key_seq(key_evt, decoded_buffer.clone()))
            })
            .unwrap_or_else(|| {
                let codepoint = decoded_buffer.chars().next_back().unwrap();
                let key_evt = KeyEvent::from_raw(codepoint);
                CharEvent::from_key_seq(key_evt, decoded_buffer)
            });
        if have_escape_prefix {
            if let Some(key) = evt.get_key_mut() {
                key.key.modifiers.alt = true;
            }
        }
        if self.is_blocked_querying() && is_event_blocked_when_querying(&evt) {
            if {
                let vintr = shell_modes().control_chars[libc::VINTR];
                vintr != 0
                    && evt.get_key().is_some_and(|key| {
                        match_key_event_to_key(&key.key, &Key::from_single_byte(vintr)).is_some()
                    })
            } {
                flog!(
                    reader,
                    "Received interrupt key, giving up waiting for response from terminal"
                );
                let ok = stop_query(self.blocking_query_mut());
                assert!(ok);
                self.get_input_data_mut().queue.clear();
                self.push_front(CharEvent::QueryResult(QueryResultEvent::Interrupted));
                return None;
            }
            flog!(
                reader,
                "Still blocked on response from terminal, deferring key event",
                evt
            );
            self.push_back(evt);
            return None;
        }
        Some(evt)
    }

    fn parse_escape_sequence(
        &mut self,
        buffer: &mut Vec<u8>,
        have_escape_prefix: &mut bool,
    ) -> Option<CharEvent> {
        assert!(matches!(buffer.as_slice(), b"\x1b" | b"\x1b\x1b"));
        let recursive_invocation = buffer.len() == 2;
        let Some(next) = self.read_sequence_byte(buffer) else {
            return char_event(buffer, KeyEvent::from_raw(key::ESCAPE));
        };
        let invalid = CharEvent::from_key(KeyEvent::from_raw(key::INVALID));
        if !recursive_invocation && next == b'\x1b' {
            return match self.parse_escape_sequence(buffer, have_escape_prefix) {
                Some(mut nested_sequence) => {
                    if nested_sequence
                        .get_key()
                        .is_some_and(|key_evt| key_evt.key.codepoint == key::INVALID)
                    {
                        return char_event(buffer, KeyEvent::from_raw(key::ESCAPE));
                    }
                    if let Some(key_evt) = nested_sequence.get_key_mut() {
                        key_evt.key.modifiers.alt = true;
                    }
                    Some(nested_sequence)
                }
                _ => Some(invalid),
            };
        }
        if next == b'[' {
            // potential CSI
            return Some(self.parse_csi(buffer).unwrap_or(invalid));
        }
        if next == b'O' {
            // potential SS3
            return Some(self.parse_ss3(buffer).unwrap_or(invalid));
        }
        if !recursive_invocation {
            if next == b']' {
                // OSC
                self.parse_osc(buffer);
                return Some(invalid);
            }
            if next == b'P' {
                // potential DCS
                return Some(self.parse_dcs(buffer).unwrap_or(invalid));
            }
        }
        *have_escape_prefix = true;
        None
    }

    fn parse_csi(&mut self, buffer: &mut Vec<u8>) -> Option<CharEvent> {
        // The maximum number of CSI parameters is defined by NPAR, nominally 16.
        let mut params = [[0_u32; 4]; 16];
        let Some(mut c) = self.read_sequence_byte(buffer) else {
            return char_event(buffer, KeyEvent::from(alt('[')));
        };
        let mut next_char = |zelf: &mut Self| zelf.read_sequence_byte(buffer).unwrap_or(0xff);
        let private_mode;
        match c {
            b'[' => {
                // Illegal CSI command.
                let key_evt = KeyEvent::new(
                    Modifiers::default(),
                    function_key(match next_char(self) {
                        b'A' => 1,
                        b'B' => 2,
                        b'C' => 3,
                        b'D' => 4,
                        b'E' => 5,
                        _ => return invalid_sequence(buffer),
                    }),
                );
                return char_event(buffer, key_evt);
            }
            b'?' | b'<' | b'=' | b'>' => {
                private_mode = Some(c);
                c = next_char(self);
            }
            _ => {
                private_mode = None;
            }
        }
        let mut count = 0;
        let mut subcount = 0;
        while count < 16 && (0x30..=0x3f).contains(&c) {
            if c.is_ascii_digit() {
                // Return None on invalid ascii numeric CSI parameter exceeding u32 bounds
                match params[count][subcount]
                    .checked_mul(10)
                    .and_then(|result| result.checked_add(u32::from(c - b'0')))
                {
                    Some(c) => params[count][subcount] = c,
                    None => return invalid_sequence(buffer),
                }
            } else if c == b':' && subcount < 3 {
                subcount += 1;
            } else if c == b';' {
                count += 1;
                subcount = 0;
            } else {
                // Unexpected character or unrecognized CSI
                return invalid_sequence(buffer);
            }
            c = next_char(self);
        }
        if c != b'$' && !(0x40..=0x7e).contains(&c) {
            return invalid_sequence(buffer);
        }

        let kitty_key = |key: char, shifted_key: Option<char>, base_layout_key: Option<char>| {
            let mask = params[1][0].saturating_sub(1);
            let (mut modifiers, caps_lock) = parse_mask(mask);

            // An event like "capslock-shift-=" should have a shifted codepoint ("+") to enable
            // fish to match "bind +".
            //
            // With letters that are affected by capslock, capslock and shift cancel each
            // other out ("capslock-shift-ä"), unless there is another modifier to imply that
            // capslock should be ignored.
            //
            // So if shift is the only modifier, we should consume it, but not if the event is
            // something like "capslock-shift-delete" because delete is not affected by capslock.
            //
            // Normally, we could consume shift by translating to the shifted key.
            // While capslock is on however, we don't get a shifted key, see
            // https://github.com/kovidgoyal/kitty/issues/8493.
            //
            // Do it by trying to find out ourselves whether the key is affected by capslock.
            //
            // Alternatively, we could relax our exact matching semantics, and make "bind ä"
            // match the "shift-ä" event, as suggested in the kitty issue.
            if caps_lock && modifiers == Modifiers::SHIFT && !key.to_uppercase().eq(Some(key)) {
                modifiers.shift = false;
            }
            KeyEvent::new_with(modifiers, key, shifted_key, base_layout_key)
        };
        let masked_key = |key: char| kitty_key(key, None, None);

        let key = match c {
            b'$' => {
                if next_char(self) == b'y' {
                    // DECRPM/DECRQM
                    return None;
                }
                match params[0][0] {
                    23 | 24 => KeyEvent::from(shift(
                        char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(), // rxvt style
                    )),
                    _ => return None,
                }
            }
            b'A' => masked_key(key::UP),
            b'B' => masked_key(key::DOWN),
            b'C' => masked_key(key::RIGHT),
            b'D' => masked_key(key::LEFT),
            b'E' => masked_key('5'),       // Numeric keypad
            b'F' => masked_key(key::END),  // PC/xterm style
            b'H' => masked_key(key::HOME), // PC/xterm style
            b'M' | b'm' => {
                flog!(reader, "mouse event");
                // Generic X10 or modified VT200 sequence, or extended (SGR/1006) mouse
                // reporting mode, with semicolon-separated parameters for button code, Px,
                // and Py, ending with 'M' for button press or 'm' for button release.
                let sgr = private_mode == Some(b'<');
                if !sgr && c == b'm' {
                    return None;
                }
                let Some(button) = (if sgr {
                    Some(params[0][0])
                } else {
                    u32::from(next_char(self)).checked_sub(32)
                }) else {
                    return invalid_sequence(buffer);
                };
                let mut convert = |param| {
                    (if sgr {
                        Some(param)
                    } else {
                        u32::from(next_char(self)).checked_sub(32)
                    })
                    .and_then(|coord| coord.checked_sub(1))
                    .and_then(|coord| usize::try_from(coord).ok())
                };
                let Some(x) = convert(params[1][0]) else {
                    return invalid_sequence(buffer);
                };
                let Some(y) = convert(params[2][0]) else {
                    return invalid_sequence(buffer);
                };
                let position = ViewportPosition { x, y };
                let (modifiers, _caps_lock) = parse_mask((button >> 2) & 0x07);
                let code = button & 0x43;
                if code != 0 || c != b'M' || modifiers.is_some() {
                    return None;
                }
                return Some(CharEvent::Implicit(ImplicitEvent::MouseLeft(position)));
            }
            b't' => {
                flog!(reader, "mouse event");
                // VT200 button released in mouse highlighting mode at valid text location. 5 chars.
                let _ = next_char(self);
                let _ = next_char(self);
                return None;
            }
            b'T' => {
                flog!(reader, "mouse event");
                // VT200 button released in mouse highlighting mode past end-of-line. 9 characters.
                for _ in 0..6 {
                    let _ = next_char(self);
                }
                return None;
            }
            b'P' => masked_key(function_key(1)),
            b'Q' => masked_key(function_key(2)),
            b'R' => {
                let Some(y) = params[0][0]
                    .checked_sub(1)
                    .and_then(|y| usize::try_from(y).ok())
                else {
                    return invalid_sequence(buffer);
                };
                let Some(x) = params[1][0]
                    .checked_sub(1)
                    .and_then(|x| usize::try_from(x).ok())
                else {
                    return invalid_sequence(buffer);
                };
                flog!(reader, "Received cursor position report y:", y, "x:", x);
                let cursor_pos = ViewportPosition { x, y };
                return Some(query_response(QueryResponse::CursorPosition(cursor_pos)));
            }
            b'S' => masked_key(function_key(4)),
            b'~' => match params[0][0] {
                1 => masked_key(key::HOME), // VT220/tmux style
                2 => masked_key(key::INSERT),
                3 => masked_key(key::DELETE),
                4 => masked_key(key::END), // VT220/tmux style
                5 => masked_key(key::PAGE_UP),
                6 => masked_key(key::PAGE_DOWN),
                7 => masked_key(key::HOME), // rxvt style
                8 => masked_key(key::END),  // rxvt style
                11..=15 => masked_key(
                    char::from_u32(u32::from(function_key(1)) + params[0][0] - 11).unwrap(),
                ),
                17..=21 => masked_key(
                    char::from_u32(u32::from(function_key(6)) + params[0][0] - 17).unwrap(),
                ),
                23 | 24 => masked_key(
                    char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(),
                ),
                25 | 26 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(3)) + params[0][0] - 25).unwrap(),
                )), // rxvt style
                27 => {
                    let Some(key) = char::from_u32(params[2][0]) else {
                        return invalid_sequence(buffer);
                    };
                    masked_key(canonicalize_keyed_control_char(key))
                }
                28 | 29 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(5)) + params[0][0] - 28).unwrap(),
                )), // rxvt style
                31 | 32 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(7)) + params[0][0] - 31).unwrap(),
                )), // rxvt style
                33 | 34 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(9)) + params[0][0] - 33).unwrap(),
                )), // rxvt style
                200 => {
                    self.paste_start_buffering();
                    return None;
                }
                201 => {
                    self.paste_commit();
                    return None;
                }
                _ => return None,
            },
            b'c' if private_mode == Some(b'?') => {
                flog!(reader, "Received Primary Device Attribute response");
                return Some(query_response(QueryResponse::PrimaryDeviceAttribute));
            }
            b'n' if private_mode == Some(b'?') && params[0] == [997, 0, 0, 0] => {
                match params[1] {
                    [1, 0, 0, 0] | [2, 0, 0, 0] => (),
                    _ => return None,
                }
                flog!(reader, "Received color theme change");
                return Some(CharEvent::Implicit(ImplicitEvent::NewColorTheme));
            }
            b'u' => {
                if private_mode == Some(b'?') {
                    maybe_set_kitty_keyboard_capability();
                    return None;
                }

                // Treat numpad keys the same as their non-numpad counterparts. Could add a numpad modifier here.
                let key = match params[0][0] {
                    57361 => key::PRINT_SCREEN,
                    57363 => key::MENU,
                    57399 => '0',
                    57400 => '1',
                    57401 => '2',
                    57402 => '3',
                    57403 => '4',
                    57404 => '5',
                    57405 => '6',
                    57406 => '7',
                    57407 => '8',
                    57408 => '9',
                    57409 => '.',
                    57410 => '/',
                    57411 => '*',
                    57412 => '-',
                    57413 => '+',
                    57414 => key::ENTER,
                    57415 => '=',
                    57417 => key::LEFT,
                    57418 => key::RIGHT,
                    57419 => key::UP,
                    57420 => key::DOWN,
                    57421 => key::PAGE_UP,
                    57422 => key::PAGE_DOWN,
                    57423 => key::HOME,
                    57424 => key::END,
                    57425 => key::INSERT,
                    57426 => key::DELETE,
                    cp => {
                        let Some(key) = char::from_u32(cp) else {
                            return invalid_sequence(buffer);
                        };
                        canonicalize_keyed_control_char(key)
                    }
                };
                let Some(shifted_key) = char::from_u32(params[0][1]) else {
                    return invalid_sequence(buffer);
                };
                let Some(base_layout_key) = char::from_u32(params[0][2]) else {
                    return invalid_sequence(buffer);
                };
                kitty_key(
                    key,
                    Some(canonicalize_keyed_control_char(shifted_key)),
                    Some(base_layout_key),
                )
            }
            b'Z' => KeyEvent::from(shift(key::TAB)),
            b'I' => {
                return Some(CharEvent::Implicit(ImplicitEvent::FocusIn));
            }
            b'O' => {
                return Some(CharEvent::Implicit(ImplicitEvent::FocusOut));
            }
            _ => return None,
        };
        char_event(buffer, key)
    }

    fn parse_ss3(&mut self, buffer: &mut Vec<u8>) -> Option<CharEvent> {
        let mut raw_mask = 0;
        let Some(mut code) = self.read_sequence_byte(buffer) else {
            return char_event(buffer, KeyEvent::from(alt('O')));
        };
        while code.is_ascii_digit() {
            raw_mask = raw_mask * 10 + u32::from(code - b'0');
            code = self.read_sequence_byte(buffer).unwrap_or(0xff);
        }
        let (modifiers, _caps_lock) = parse_mask(raw_mask.saturating_sub(1));
        #[rustfmt::skip]
        let key = match code {
            b' ' => KeyEvent::new(modifiers, key::SPACE),
            b'A' => KeyEvent::new(modifiers, key::UP),
            b'B' => KeyEvent::new(modifiers, key::DOWN),
            b'C' => KeyEvent::new(modifiers, key::RIGHT),
            b'D' => KeyEvent::new(modifiers, key::LEFT),
            b'F' => KeyEvent::new(modifiers, key::END),
            b'H' => KeyEvent::new(modifiers, key::HOME),
            b'I' => KeyEvent::new(modifiers, key::TAB),
            b'M' => KeyEvent::new(modifiers, key::ENTER),
            b'P' => KeyEvent::new(modifiers, function_key(1)),
            b'Q' => KeyEvent::new(modifiers, function_key(2)),
            b'R' => KeyEvent::new(modifiers, function_key(3)),
            b'S' => KeyEvent::new(modifiers, function_key(4)),
            b'X' => KeyEvent::new(modifiers, '='),
            b'j' => KeyEvent::new(modifiers, '*'),
            b'k' => KeyEvent::new(modifiers, '+'),
            b'l' => KeyEvent::new(modifiers, ','),
            b'm' => KeyEvent::new(modifiers, '-'),
            b'n' => KeyEvent::new(modifiers, '.'),
            b'o' => KeyEvent::new(modifiers, '/'),
            b'p' => KeyEvent::new(modifiers, '0'),
            b'q' => KeyEvent::new(modifiers, '1'),
            b'r' => KeyEvent::new(modifiers, '2'),
            b's' => KeyEvent::new(modifiers, '3'),
            b't' => KeyEvent::new(modifiers, '4'),
            b'u' => KeyEvent::new(modifiers, '5'),
            b'v' => KeyEvent::new(modifiers, '6'),
            b'w' => KeyEvent::new(modifiers, '7'),
            b'x' => KeyEvent::new(modifiers, '8'),
            b'y' => KeyEvent::new(modifiers, '9'),
            _ => return None,
        };
        char_event(buffer, key)
    }

    fn read_until_sequence_terminator(
        &mut self,
        buffer: &mut Vec<u8>,
        allow_bel: bool,
    ) -> Option<()> {
        let mut escape = false;
        loop {
            let b = self.read_sequence_byte(buffer)?;
            if allow_bel && b == b'\x07' {
                buffer.pop();
                return Some(());
            }
            if escape && b == b'\\' {
                buffer.pop();
                buffer.pop();
                return Some(());
            }
            escape = b == b'\x1b';
        }
    }

    fn parse_xtversion(&mut self, buffer: &mut Vec<u8>) -> Option<()> {
        assert_eq!(buffer, b"\x1bP>");
        self.read_until_sequence_terminator(buffer, false)?;
        if buffer.get(3)? != &b'|' {
            return None;
        }
        XTVERSION.get_or_init(|| {
            let xtversion = bytes2wcstring(&buffer[4..buffer.len()]);
            flog!(
                reader,
                format!("Received XTVERSION response: {}", xtversion)
            );
            xtversion
        });
        None
    }

    fn parse_osc(&mut self, buffer: &mut Vec<u8>) -> Option<CharEvent> {
        let osc_prefix = b"\x1b]";
        assert_eq!(buffer, osc_prefix);
        self.read_until_sequence_terminator(buffer, /*allow_bel=*/ true)?;
        let buffer = &buffer[osc_prefix.len()..];
        let buffer = buffer.strip_prefix(b"11;")?;
        let c = xterm_color::Color::parse(buffer).ok()?;
        flog!(reader, format!("Received background color {c:?}"));
        Some(query_response(QueryResponse::BackgroundColor(c)))
    }

    fn parse_dcs(&mut self, buffer: &mut Vec<u8>) -> Option<CharEvent> {
        assert_eq!(buffer, b"\x1bP");
        let Some(success) = self.read_sequence_byte(buffer) else {
            return char_event(buffer, KeyEvent::from(alt('P')));
        };
        let success = match success {
            b'0' => false,
            b'1' => true,
            b'>' => {
                self.parse_xtversion(buffer);
                return None;
            }
            _ => return None,
        };
        if self.read_sequence_byte(buffer)? != b'+' {
            return None;
        }
        if self.read_sequence_byte(buffer)? != b'r' {
            return None;
        }
        self.read_until_sequence_terminator(buffer, false)?;
        // \e P 1 r + Pn ST
        // \e P 0 r + msg ST
        let buffer = &buffer[5..];
        if !success {
            flog!(
                reader,
                format!(
                    "Received XTGETTCAP failure response: {}",
                    bytes2wcstring(&parse_hex(buffer)?),
                )
            );
            return None;
        }
        let mut buffer = buffer.splitn(2, |&c| c == b'=');
        let key = buffer.next().unwrap();
        let key = parse_hex(key)?;
        let value = if let Some(value) = buffer.next() {
            let value = parse_hex(value)?;
            flog!(
                reader,
                format!(
                    "Received XTGETTCAP response: {}={:?}",
                    bytes2wcstring(&key),
                    bytes2wcstring(&value)
                )
            );
            Some(value)
        } else {
            flog!(
                reader,
                format!("Received XTGETTCAP response: {}", bytes2wcstring(&key))
            );
            None
        };
        if key == SCROLL_CONTENT_UP_TERMINFO_CODE.as_bytes() {
            maybe_set_scroll_content_up_capability();
        } else if key == XTGETTCAP_QUERY_OS_NAME.as_bytes() {
            if let Some(value) = value {
                TERMINAL_OS_NAME.get_or_init(|| Some(bytes2wcstring(&value)));
            }
        }
        None
    }
}

fn decode_utf8_at_least_one(
    buffer: &mut Vec<u8>,
    on_incomplete: impl Fn(&mut Vec<u8>),
) -> Result<WString, CharEvent> {
    let mut decoded_input = WString::new();
    loop {
        match decode_utf8(&mut decoded_input, InvalidPolicy::Error, buffer) {
            DecodeState::Incomplete => {
                on_incomplete(buffer);
            }
            DecodeState::Complete => {
                return Ok(decoded_input);
            }
            DecodeState::Error => {
                return Err(CharEvent::from_check_exit());
            }
        }
    }
}

fn char_event(buffer: &mut Vec<u8>, key_evt: KeyEvent) -> Option<CharEvent> {
    Some({
        match decode_utf8_at_least_one(buffer, |_buffer| panic!()) {
            Ok(decoded_buffer) => CharEvent::from_key_seq(key_evt, decoded_buffer),
            Err(evt) => evt,
        }
    })
}

fn query_response(response: QueryResponse) -> CharEvent {
    CharEvent::QueryResult(QueryResultEvent::Response(response))
}

fn parse_mask(mask: u32) -> (Modifiers, bool) {
    let modifiers = Modifiers {
        ctrl: (mask & 4) != 0,
        alt: (mask & 2) != 0,
        shift: (mask & 1) != 0,
        sup: (mask & 8) != 0,
    };
    let caps_lock = (mask & 64) != 0;
    (modifiers, caps_lock)
}

pub(crate) enum DecodeState {
    Incomplete,
    Complete,
    Error,
}

#[derive(Eq, PartialEq)]
pub(crate) enum InvalidPolicy {
    Error,
    Passthrough,
}

/// Decode the UTF-8-encoded `buffer`.
/// On success, the result is appended to `out_seq` and [`DecodeState::Complete`] is returned.
/// [`DecodeState::Incomplete`] is returned if the buffer contains valid UTF-8
/// with the exception of the last bytes,
/// where the last 1 to 3 bytes are a prefix of the encoding of a valid char,
/// which can happen if input is read incrementally.
/// In this case `out_seq` will not be modified.
/// If other errors occur, the behavior depends on `invalid_policy`.
/// For [`InvalidPolicy::Error`], [`DecodeState::Error`] will be returned, without modifying
/// `out_seq`.
/// For [`InvalidPolicy::Passthrough`], [`DecodeState::Complete`] will be returned
/// and `out_seq` will have the individual bytes of `buffer` appended to it, each encoded using our
/// PUA encoding scheme.
pub(crate) fn decode_utf8(
    out_seq: &mut WString,
    invalid_policy: InvalidPolicy,
    buffer: &[u8],
) -> DecodeState {
    use DecodeState::*;
    match std::str::from_utf8(buffer) {
        Ok(parsed_str) => {
            for c in parsed_str.chars() {
                if !fish_reserved_codepoint(c) {
                    out_seq.push(c);
                }
            }
            Complete
        }
        Err(e) => match e.error_len() {
            Some(_) => match invalid_policy {
                InvalidPolicy::Error => {
                    flog!(reader, "Illegal input encoding");
                    Error
                }
                InvalidPolicy::Passthrough => {
                    for &b in buffer {
                        out_seq.push(encode_byte_to_char(b));
                    }
                    Complete
                }
            },
            None => Incomplete,
        },
    }
}

fn invalid_sequence(buffer: &[u8]) -> Option<CharEvent> {
    flog!(
        reader,
        "Error: invalid escape sequence: ",
        DisplayBytes(buffer)
    );
    None
}

pub(super) struct DisplayBytes<'a>(pub(super) &'a [u8]);

impl<'a> std::fmt::Display for DisplayBytes<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (i, &c) in self.0.iter().enumerate() {
            if i != 0 {
                write!(f, " ")?;
            }
            write!(f, "{}", char_to_symbol(char::from(c), i == 0))?;
        }
        Ok(())
    }
}
impl<'a> FloggableDisplay for DisplayBytes<'a> {}

fn parse_hex(hex: &[u8]) -> Option<Vec<u8>> {
    if hex.len() % 2 != 0 {
        return None;
    }
    let mut result = vec![0; hex.len() / 2];
    parse_hex_into(&mut result, hex)?;
    Some(result)
}
fn parse_hex_into(out: &mut [u8], hex: &[u8]) -> Option<()> {
    assert_eq!(out.len() * 2, hex.len());
    let mut i = 0;
    while i < hex.len() {
        let d1 = char::from(hex[i]).to_digit(16)?;
        let d2 = char::from(hex[i + 1]).to_digit(16)?;
        let decoded = u8::try_from(16 * d1 + d2).unwrap();
        out[i / 2] = decoded;
        i += 2;
    }
    Some(())
}

#[cfg(test)]
mod tests {
    use super::parse_hex;

    #[test]
    fn test_parse_hex() {
        assert_eq!(parse_hex(b"3d"), Some(vec![61]));
    }
}
