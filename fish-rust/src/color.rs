use std::{array, cmp::Ordering};

use crate::{
    wchar::{widestrs, wstr, WExt, WString, L},
    wutil::sprintf,
};

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct Color24 {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl Color24 {
    fn from_bits(bits: u32) -> Self {
        assert_eq!(bits >> 24, 0, "from_bits() called with non-zero high byte");

        Self {
            r: (bits >> 16) as u8,
            g: (bits >> 8) as u8,
            b: bits as u8,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Type {
    // TODO: remove this? Users should probably use `Option<RgbColor>` instead
    None,
    Named { idx: u8 },
    Rgb(Color24),
    Normal,
    Reset,
}

#[derive(Debug, Default, Copy, Clone, PartialEq, Eq)]
pub struct Flags {
    pub bold: bool,
    pub underline: bool,
    pub italics: bool,
    pub dim: bool,
    pub reverse: bool,
}

impl Flags {
    // const eval workaround
    const DEFAULT: Self = Flags {
        bold: false,
        underline: false,
        italics: false,
        dim: false,
        reverse: false,
    };
}

/// A type that represents a color.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct RgbColor {
    pub typ: Type,
    pub flags: Flags,
}

impl RgbColor {
    /// The color white
    pub const WHITE: Self = Self {
        typ: Type::Named { idx: 7 },
        flags: Flags::DEFAULT,
    };

    /// The color black
    pub const BLACK: Self = Self {
        typ: Type::Named { idx: 0 },
        flags: Flags::DEFAULT,
    };

    /// The reset special color.
    pub const RESET: Self = Self {
        typ: Type::Reset,
        flags: Flags::DEFAULT,
    };

    /// The normal special color.
    pub const NORMAL: Self = Self {
        typ: Type::Normal,
        flags: Flags::DEFAULT,
    };

    /// The none special color.
    pub const NONE: Self = Self {
        typ: Type::None,
        flags: Flags::DEFAULT,
    };

    /// Parse a color from a string.
    pub fn from_wstr(s: &wstr) -> Option<Self> {
        Self::try_parse_special(s)
            .or_else(|| Self::try_parse_named(s))
            .or_else(|| Self::try_parse_rgb(s))
    }

    /// Returns whether the color is the normal special color.
    pub const fn is_normal(self) -> bool {
        matches!(self.typ, Type::Normal)
    }

    /// Returns whether the color is the reset special color.
    pub const fn is_reset(self) -> bool {
        matches!(self.typ, Type::Reset)
    }

    /// Returns whether the color is the none special color.
    pub const fn is_none(self) -> bool {
        matches!(self.typ, Type::None)
    }

    /// Returns whether the color is a named color (like "magenta").
    pub const fn is_named(self) -> bool {
        matches!(self.typ, Type::Named { .. })
    }

    /// Returns whether the color is specified via RGB components.
    pub const fn is_rgb(self) -> bool {
        matches!(self.typ, Type::Rgb(_))
    }

    /// Returns whether the color is special, that is, not rgb or named.
    pub const fn is_special(self) -> bool {
        !self.is_named() && !self.is_rgb()
    }

    /// Returns a description of the color.
    #[widestrs]
    pub fn description(self) -> WString {
        match self.typ {
            Type::None => WString::from_str("none"),
            Type::Named { idx } => {
                sprintf!("named(%d, %ls)"L, idx, name_for_color_idx(idx).unwrap())
            }
            Type::Rgb(c) => {
                sprintf!("rgb(0x%02x%02x%02x"L, c.r, c.g, c.b)
            }
            Type::Normal => WString::from_str("normal"),
            Type::Reset => WString::from_str("reset"),
        }
    }

    /// Returns the name index for the given color. Requires that the color be named or RGB.
    pub fn to_name_index(self) -> u8 {
        // TODO: This should look for the nearest color.
        match self.typ {
            Type::Named { idx } => idx,
            Type::Rgb(c) => term16_color_for_rgb(c),
            Type::None | Type::Normal | Type::Reset => {
                panic!("to_name_index() called on Color that's not named or RGB")
            }
        }
    }

    /// Returns the term256 index for the given color. Requires that the color be RGB.
    pub fn to_term256_index(self) -> u8 {
        let Type::Rgb(c) = self.typ else {
            panic!("Tried to get term256 index of non-RGB color");
        };

        term256_color_for_rgb(c)
    }

    /// Returns the 24 bit color for the given color. Requires that the color be RGB.
    pub const fn to_color24(self) -> Color24 {
        let Type::Rgb(c) = self.typ else {
            panic!("Tried to get color24 of non-RGB color");
        };

        c
    }

    /// Returns the names of all named colors.
    pub fn named_color_names() -> Vec<&'static wstr> {
        let mut v: Vec<_> = NAMED_COLORS
            .iter()
            .filter_map(|&NamedColor { name, hidden, .. }| (!hidden).then_some(name))
            .collect();

        // "normal" isn't really a color and does not have a color palette index or
        // RGB value. Therefore, it does not appear in the NAMED_COLORS table.
        // However, it is a legitimate color name for the "set_color" command so
        // include it in the publicly known list of colors. This is primarily so it
        // appears in the output of "set_color --print-colors".
        v.push(L!("normal"));
        v
    }

    /// Try parsing a special color name like "normal".
    #[widestrs]
    fn try_parse_special(special: &wstr) -> Option<Self> {
        // TODO: this is a very hot function, may need optimization by e.g. comparing length first,
        // depending on how well inlining of `simple_icase_compare` works
        let typ = if simple_icase_compare(special, "normal"L) == Ordering::Equal {
            Type::Normal
        } else if simple_icase_compare(special, "reset"L) == Ordering::Equal {
            Type::Reset
        } else {
            return None;
        };

        Some(Self {
            typ,
            flags: Flags::default(),
        })
    }

    /// Try parsing an rgb color like "#F0A030".
    ///
    /// We support the following style of rgb formats (case insensitive):
    ///
    /// - `#FA3`
    /// - `#F3A035`
    /// - `FA3`
    /// - `F3A035`

    fn try_parse_rgb(mut s: &wstr) -> Option<Self> {
        // Skip any leading #.
        if s.chars().next()? == '#' {
            s = &s[1..];
        }

        let hex_digit = |i| {
            s.char_at(i)
                .to_digit(16)
                .map(|n| n.try_into().expect("hex digit should always be < 256"))
        };

        // TODO: `array::try_from_fn()`: https://github.com/rust-lang/rust/issues/89379
        let rgb: [_; 3] = if s.len() == 3 {
            // Format: FA3
            array::from_fn(hex_digit)
        } else if s.len() == 6 {
            // Format: F3A035
            array::from_fn(|i| {
                let hi = hex_digit(2 * i)?;
                let lo = hex_digit(2 * i + 1)?;

                Some(hi * 16 + lo)
            })
        } else {
            return None;
        };

        Some(Self {
            typ: Type::Rgb(Color24 {
                r: rgb[0]?,
                g: rgb[1]?,
                b: rgb[2]?,
            }),
            flags: Flags::default(),
        })
    }

    /// Try parsing an explicit color name like "magenta".
    fn try_parse_named(name: &wstr) -> Option<Self> {
        let i = NAMED_COLORS
            .binary_search_by(|c| simple_icase_compare(c.name, name))
            .ok()?;

        Some(Self {
            typ: Type::Named {
                idx: NAMED_COLORS[i].idx,
            },
            flags: Flags::default(),
        })
    }
}

/// Compare wide strings with simple ASCII canonicalization.
#[inline(always)]
fn simple_icase_compare(s1: &wstr, s2: &wstr) -> Ordering {
    let c1 = s1.chars().map(|c| c.to_ascii_lowercase());
    let c2 = s2.chars().map(|c| c.to_ascii_lowercase());

    c1.cmp(c2)
}

struct NamedColor {
    name: &'static wstr,
    idx: u8,
    rgb: [u8; 3],
    hidden: bool,
}

#[widestrs]
#[rustfmt::skip]
const NAMED_COLORS: &[NamedColor] = &[
    // Keep this sorted alphabetically
    NamedColor {name: "black"L, idx: 0, rgb: [0x00, 0x00, 0x00], hidden: false},
    NamedColor {name: "blue"L, idx: 4, rgb: [0x00, 0x00, 0x80], hidden: false},
    NamedColor {name: "brblack"L, idx: 8, rgb: [0x80, 0x80, 0x80], hidden: false},
    NamedColor {name: "brblue"L, idx: 12, rgb: [0x00, 0x00, 0xFF], hidden: false},
    NamedColor {name: "brbrown"L, idx: 11, rgb: [0xFF, 0xFF, 0x00], hidden: true},
    NamedColor {name: "brcyan"L, idx: 14, rgb: [0x00, 0xFF, 0xFF], hidden: false},
    NamedColor {name: "brgreen"L, idx: 10, rgb: [0x00, 0xFF, 0x00], hidden: false},
    NamedColor {name: "brgrey"L, idx: 8, rgb: [0x55, 0x55, 0x55], hidden: true},
    NamedColor {name: "brmagenta"L, idx: 13, rgb: [0xFF, 0x00, 0xFF], hidden: false},
    NamedColor {name: "brown"L, idx: 3, rgb: [0x72, 0x50, 0x00], hidden: true},
    NamedColor {name: "brpurple"L, idx: 13, rgb: [0xFF, 0x00, 0xFF], hidden: true},
    NamedColor {name: "brred"L, idx: 9, rgb: [0xFF, 0x00, 0x00], hidden: false},
    NamedColor {name: "brwhite"L, idx: 15, rgb: [0xFF, 0xFF, 0xFF], hidden: false},
    NamedColor {name: "bryellow"L, idx: 11, rgb: [0xFF, 0xFF, 0x00], hidden: false},
    NamedColor {name: "cyan"L, idx: 6, rgb: [0x00, 0x80, 0x80], hidden: false},
    NamedColor {name: "green"L, idx: 2, rgb: [0x00, 0x80, 0x00], hidden: false},
    NamedColor {name: "grey"L, idx: 7, rgb: [0xE5, 0xE5, 0xE5], hidden: true},
    NamedColor {name: "magenta"L, idx: 5, rgb: [0x80, 0x00, 0x80], hidden: false},
    NamedColor {name: "purple"L, idx: 5, rgb: [0x80, 0x00, 0x80], hidden: true},
    NamedColor {name: "red"L, idx: 1, rgb: [0x80, 0x00, 0x00], hidden: false},
    NamedColor {name: "white"L, idx: 7, rgb: [0xC0, 0xC0, 0xC0], hidden: false},
    NamedColor {name: "yellow"L, idx: 3, rgb: [0x80, 0x80, 0x00], hidden: false},
];

assert_sorted_by_name!(NAMED_COLORS);

fn convert_color(color: Color24, colors: &[u32]) -> usize {
    fn squared_difference(a: u8, b: u8) -> u16 {
        u16::from(a.abs_diff(b)).pow(2)
    }

    colors
        .iter()
        .enumerate()
        .min_by_key(|&(_i, c)| {
            let Color24 { r, g, b } = Color24::from_bits(*c);

            squared_difference(r, color.r)
                + squared_difference(g, color.g)
                + squared_difference(b, color.b)
        })
        .expect("convert_color() called with empty color list")
        .0
}

fn name_for_color_idx(target_idx: u8) -> Option<&'static wstr> {
    NAMED_COLORS
        .iter()
        .find_map(|&NamedColor { name, idx, .. }| (idx == target_idx).then_some(name))
}

fn term16_color_for_rgb(color: Color24) -> u8 {
    const COLORS: &[u32] = &[
        0x000000, // Black
        0x800000, // Red
        0x008000, // Green
        0x808000, // Yellow
        0x000080, // Blue
        0x800080, // Magenta
        0x008080, // Cyan
        0xc0c0c0, // White
        0x808080, // Bright Black
        0xFF0000, // Bright Red
        0x00FF00, // Bright Green
        0xFFFF00, // Bright Yellow
        0x0000FF, // Bright Blue
        0xFF00FF, // Bright Magenta
        0x00FFFF, // Bright Cyan
        0xFFFFFF, // Bright White
    ];

    convert_color(color, COLORS).try_into().unwrap()
}

fn term256_color_for_rgb(color: Color24) -> u8 {
    const COLORS: &[u32] = &[
        0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f, 0x005f87,
        0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af, 0x0087d7, 0x0087ff,
        0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff, 0x00d700, 0x00d75f, 0x00d787,
        0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f, 0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff,
        0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af, 0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87,
        0x5f5faf, 0x5f5fd7, 0x5f5fff, 0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff,
        0x5faf00, 0x5faf5f, 0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787,
        0x5fd7af, 0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
        0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f, 0x875f87,
        0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af, 0x8787d7, 0x8787ff,
        0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff, 0x87d700, 0x87d75f, 0x87d787,
        0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f, 0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff,
        0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af, 0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87,
        0xaf5faf, 0xaf5fd7, 0xaf5fff, 0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff,
        0xafaf00, 0xafaf5f, 0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787,
        0xafd7af, 0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,
        0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f, 0xd75f87,
        0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af, 0xd787d7, 0xd787ff,
        0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff, 0xd7d700, 0xd7d75f, 0xd7d787,
        0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f, 0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff,
        0xff0000, 0xff005f, 0xff0087, 0xff00af, 0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87,
        0xff5faf, 0xff5fd7, 0xff5fff, 0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff,
        0xffaf00, 0xffaf5f, 0xffaf87, 0xffafaf, 0xffafd7, 0xffafff, 0xffd700, 0xffd75f, 0xffd787,
        0xffd7af, 0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
        0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e, 0x585858,
        0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e, 0xa8a8a8, 0xb2b2b2,
        0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee,
    ];

    (16 + convert_color(color, COLORS)).try_into().unwrap()
}

#[cfg(test)]
mod tests {
    use crate::{color::RgbColor, wchar::widestrs};

    #[test]
    #[widestrs]
    fn parse() {
        assert!(RgbColor::from_wstr("#FF00A0"L).unwrap().is_rgb());
        assert!(RgbColor::from_wstr("FF00A0"L).unwrap().is_rgb());
        assert!(RgbColor::from_wstr("#F30"L).unwrap().is_rgb());
        assert!(RgbColor::from_wstr("F30"L).unwrap().is_rgb());
        assert!(RgbColor::from_wstr("f30"L).unwrap().is_rgb());
        assert!(RgbColor::from_wstr("#FF30a5"L).unwrap().is_rgb());
        assert!(RgbColor::from_wstr("3f30"L).is_none());
        assert!(RgbColor::from_wstr("##f30"L).is_none());
        assert!(RgbColor::from_wstr("magenta"L).unwrap().is_named());
        assert!(RgbColor::from_wstr("MaGeNTa"L).unwrap().is_named());
        assert!(RgbColor::from_wstr("mooganta"L).is_none());
    }
}
