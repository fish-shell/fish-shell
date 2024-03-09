use std::ops;
use std::rc::Rc;

use crate::{fallback::fish_wcwidth, wchar::prelude::*, wutil::fish_wcstoi};

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct Modifiers {
    pub control: bool,
    pub alt: bool,
    pub shift: bool,
}

impl Modifiers {
    pub(crate) fn is_some(&self) -> bool {
        self.control || self.alt || self.shift
    }
    pub(crate) fn is_none(&self) -> bool {
        !self.is_some()
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Chord {
    pub modifiers: Modifiers,
    pub codepoint: char,
}

impl From<char> for Chord {
    fn from(codepoint: char) -> Self {
        Self {
            modifiers: Modifiers::default(),
            codepoint,
        }
    }
}

impl Chord {
    pub(crate) fn canonicalize_from_terminfo(codepoint: char) -> Self {
        if codepoint == '\0' {
            // historical behavior for the `nul` name
            return control(Space);
        }
        Self {
            modifiers: Modifiers::default(),
            codepoint,
        }
    }
}

impl std::fmt::Display for Chord {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        WString::from(*self).fmt(f)
    }
}

impl printf_compat::args::ToArg<'static> for Chord {
    fn to_arg(self) -> printf_compat::args::Arg<'static> {
        printf_compat::args::Arg::BoxedStr(Rc::new(WString::from(self).into_boxed_utfstr()))
    }
}

pub(crate) fn control(codepoint: char) -> Chord {
    Chord {
        modifiers: Modifiers {
            control: true,
            ..Default::default()
        },
        codepoint,
    }
}

pub(crate) fn alt(codepoint: char) -> Chord {
    Chord {
        modifiers: Modifiers {
            alt: true,
            ..Default::default()
        },
        codepoint,
    }
}

pub(crate) fn shift(codepoint: char) -> Chord {
    Chord {
        modifiers: Modifiers {
            shift: true,
            ..Default::default()
        },
        codepoint,
    }
}

pub(crate) const Backspace: char = '\u{F500}'; // below ENCODE_DIRECT_BASE
pub(crate) const Delete: char = '\u{F501}';
pub(crate) const Escape: char = '\u{F502}';
pub(crate) const Return: char = '\u{F503}';
pub(crate) const Up: char = '\u{F504}';
pub(crate) const Down: char = '\u{F505}';
pub(crate) const Left: char = '\u{F506}';
pub(crate) const Right: char = '\u{F507}';
pub(crate) const PageUp: char = '\u{F508}';
pub(crate) const PageDown: char = '\u{F509}';
pub(crate) const Home: char = '\u{F50a}';
pub(crate) const End: char = '\u{F50b}';
pub(crate) const Insert: char = '\u{F50c}';
pub(crate) const Tab: char = '\u{F50d}';
pub(crate) const Space: char = '\u{F50e}';
pub(crate) const FocusIn: char = '\u{F50f}';
pub(crate) const FocusOut: char = '\u{F510}';
pub(crate) const Invalid: char = '\u{F511}';
pub(crate) fn function_key(n: u32) -> char {
    assert!((1..=12).contains(&n));
    char::from_u32(u32::from(Invalid) + n).unwrap()
}

const NAMED_KEYS_RANGE: ops::Range<u32> = 0xF500..0xF520;

const KEY_NAMES: &[(char, &wstr)] = &[
    ('+', L!("plus")),
    ('-', L!("minus")),
    ('[', L!("lbrack")),
    (']', L!("rbrack")),
    (Backspace, L!("backspace")),
    (Delete, L!("del")),
    (Escape, L!("esc")),
    (Return, L!("ret")),
    (Up, L!("up")),
    (Down, L!("down")),
    (Left, L!("left")),
    (Right, L!("right")),
    (PageUp, L!("pageup")),
    (PageDown, L!("pagedown")),
    (Home, L!("home")),
    (End, L!("end")),
    (Insert, L!("ins")),
    (Tab, L!("tab")),
    (Space, L!("space")),
    (FocusIn, L!("focus_in")),
    (FocusOut, L!("focus_out")),
];

impl Chord {
    pub(crate) fn underlying_codepoint(&self) -> Option<char> {
        if *self == Chord::from(Return) {
            return Some('\n');
        }
        // Tab => '\t', // todo
        if *self == Chord::from(Space) || *self == shift(Space) {
            return Some(' ');
        }
        // Escape => '\x1b',
        if self.modifiers.is_some() {
            return None;
        }
        let c = self.codepoint;
        if NAMED_KEYS_RANGE.contains(&u32::from(c)) || u32::from(c) <= 27 {
            return None;
        }
        Some(c)
    }
}

pub(crate) fn parse_keys(value: &wstr) -> Result<Vec<Chord>, WString> {
    let mut res = vec![];
    let mut i = 0;
    while i < value.len() {
        let c = value.as_char_slice()[i];
        if c != '[' {
            res.push(canonicalize(Chord::from(c)).unwrap());
            i += 1;
            continue;
        }
        let Some(end) = value[i..].chars().position(|c| c == ']') else {
            res.push(Chord::from(c));
            i += 1;
            continue;
        };
        let end = i + end;

        let delimited_name = &value[i..end + 1];
        let chord_name = &value[i + 1..end];
        let mut modifiers = Modifiers::default();
        let num_keys = chord_name.split('-').count();
        let mut components = chord_name.split('-');
        for _i in 0..num_keys - 1 {
            let modifier = components.next().unwrap();
            match modifier {
                _ if modifier == "c" => modifiers.control = true,
                _ if modifier == "a" => modifiers.alt = true,
                _ if modifier == "s" => modifiers.shift = true,
                _ => {
                    return Err(wgettext_fmt!(
                        "unknown modififer '%s' in '%s'",
                        modifier,
                        delimited_name
                    ))
                }
            }
        }
        let key_name = components.next().unwrap();
        let codepoint = KEY_NAMES
            .iter()
            .find_map(|(codepoint, name)| (name == key_name).then_some(*codepoint))
            .or_else(|| (key_name.len() == 1).then(|| key_name.as_char_slice()[0]));
        let chord = if let Some(codepoint) = codepoint {
            canonicalize(Chord {
                modifiers,
                codepoint,
            })?
        } else if codepoint.is_none() && key_name.starts_with('F') && key_name.len() <= 3 {
            let num = key_name.strip_prefix('F');
            let codepoint = match fish_wcstoi(num) {
                Ok(n) if (1..=12).contains(&n) => function_key(u32::try_from(n).unwrap()),
                _ => {
                    return Err(wgettext_fmt!(
                        "only F1 through F12 are supported, not '%s'",
                        num,
                        delimited_name
                    ));
                }
            };
            Chord {
                modifiers,
                codepoint,
            }
        } else {
            return Err(wgettext_fmt!("cannot parse key '%s'", delimited_name));
        };
        res.push(chord);
        i = end + 1;
    }
    // Historical bindings use \ek as {a-k}. Canonicalize them.
    if res.iter().any(|chord| chord.codepoint == '\x1b') {
        let mut canonical = vec![];
        let mut had_literal_escape = false;
        for mut chord in res {
            if had_literal_escape {
                had_literal_escape = false;
                if chord.modifiers.alt {
                    canonical.push(Chord::from(Escape));
                } else {
                    chord.modifiers.alt = true;
                }
            } else if chord.codepoint == '\x1b' {
                had_literal_escape = true;
                continue;
            }
            canonical.push(chord);
        }
        if had_literal_escape {
            canonical.push(Chord::from(Escape));
        }
        res = canonical;
    }
    Ok(res)
}

pub(crate) fn canonicalize(mut chord: Chord) -> Result<Chord, WString> {
    // Note escape is handled elsewhere
    if chord.codepoint == '\0' {
        // backwards compat
        chord = control(Space);
    } else if chord.codepoint == '\x09' {
        chord.codepoint = Tab;
    } else if chord.codepoint == '\x20' {
        chord.codepoint = Space;
    } else if chord.codepoint < char::from(27) {
        if chord.codepoint == '\n' {
            chord.codepoint = Return;
        } else {
            if chord.modifiers.control {
                return Err(wgettext_fmt!(
                    "Cannot add control modifier to control character '%s'",
                    char_to_symbol(chord.codepoint),
                ));
            }
            chord.modifiers.control = true;
            chord.codepoint =
                char::from_u32(u32::from(chord.codepoint) + u32::from('a') - 1).unwrap()
        }
    }
    if chord.modifiers.shift {
        if chord.codepoint.is_ascii_alphabetic() {
            // Shift + ASCII letters is just the uppercase letter.
            chord.modifiers.shift = false;
            chord.codepoint = chord.codepoint.to_ascii_uppercase();
        } else if !NAMED_KEYS_RANGE.contains(&u32::from(chord.codepoint)) {
            // Shift + any other printable character is not allowed.
            return Err(wgettext_fmt!(
                "Shift modifier only works on special keys and lowercase ASCII, not '%s'",
                char_to_symbol(chord.codepoint),
            ));
        }
    }
    Ok(chord)
}

impl From<Chord> for WString {
    fn from(c: Chord) -> Self {
        let name = KEY_NAMES
            .iter()
            .find_map(|&(codepoint, name)| (codepoint == c.codepoint).then(|| name.to_owned()))
            .or_else(|| {
                (function_key(1)..=function_key(12))
                    .contains(&c.codepoint)
                    .then(|| {
                        sprintf!(
                            "F%d",
                            u32::from(c.codepoint) - u32::from(function_key(1)) + 1
                        )
                    })
            });
        let is_named = name.is_some();
        let mut res = name.unwrap_or_else(|| char_to_symbol(c.codepoint));

        if c.modifiers.shift {
            res.insert_utfstr(0, L!("s-"));
        }
        if c.modifiers.alt {
            res.insert_utfstr(0, L!("a-"));
        }
        if c.modifiers.control {
            res.insert_utfstr(0, L!("c-"));
        }

        if is_named || c.modifiers.is_some() {
            sprintf!("[%s]", res)
        } else {
            res
        }
    }
}

/// Return true if the character must be escaped when used in the sequence of chars to be bound in
/// a `bind` command.
fn must_escape(c: char) -> bool {
    "[]()<>{}*\\?$#;&|'\"".contains(c)
}

fn ctrl_to_symbol(buf: &mut WString, c: char) {
    let ctrl_symbolic_names: [&wstr; 32] = {
        std::array::from_fn(|i| match i {
            8 => L!("\\b"),
            9 => L!("\\t"),
            10 => L!("\\n"),
            13 => L!("\\r"),
            27 => L!("\\e"),
            28 => L!("\\x1c"),
            _ => L!(""),
        })
    };

    let c = u8::try_from(c).unwrap();
    let cu = usize::from(c);

    if !ctrl_symbolic_names[cu].is_empty() {
        sprintf!(=> buf, "%s", ctrl_symbolic_names[cu]);
    } else {
        sprintf!(=> buf, "\\c%c", char::from(c + 0x40));
    }
}

fn space_to_symbol(buf: &mut WString, c: char) {
    sprintf!(=> buf, "\\x%X", u32::from(c));
}

fn del_to_symbol(buf: &mut WString, c: char) {
    sprintf!(=> buf, "\\x%X", u32::from(c));
}

fn ascii_printable_to_symbol(buf: &mut WString, c: char) {
    if must_escape(c) {
        sprintf!(=> buf, "\\%c", c);
    } else {
        sprintf!(=> buf, "%c", c);
    }
}

/// Convert a wide-char to a symbol that can be used in our output.
fn char_to_symbol(c: char) -> WString {
    let mut buff = WString::new();
    let buf = &mut buff;
    if c == '\x1b' {
        // Escape
        buf.push_str("\\e");
    } else if c < ' ' {
        // ASCII control character
        ctrl_to_symbol(buf, c);
    } else if c == ' ' {
        // the "space" character
        space_to_symbol(buf, c);
    } else if c == '\x7F' {
        // the "del" character
        del_to_symbol(buf, c);
    } else if c < '\u{80}' {
        // ASCII characters that are not control characters
        ascii_printable_to_symbol(buf, c);
    } else if fish_wcwidth(c) > 0 {
        sprintf!(=> buf, "%lc", c);
    } else if c <= '\u{FFFF}' {
        // BMP Unicode chararacter
        sprintf!(=> buf, "\\u%04X", u32::from(c));
    } else {
        sprintf!(=> buf, "\\U%06X", u32::from(c));
    }
    buff
}
