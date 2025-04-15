use std::cmp::Ordering;

use crate::wchar::prelude::*;

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

/// A type that represents a color.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Color {
    // TODO: remove this? Users should probably use `Option<RgbColor>` instead
    None,
    Named { idx: u8 },
    Rgb(Color24),
    Normal,
}

impl Color {
    /// The color white
    pub const WHITE: Self = Self::Named { idx: 7 };

    /// The color black
    pub const BLACK: Self = Self::Named { idx: 0 };

    /// Parse a color from a string.
    pub fn from_wstr(s: &wstr) -> Option<Self> {
        Self::try_parse_special(s)
            .or_else(|| Self::try_parse_named(s))
            .or_else(|| Self::try_parse_rgb(s))
    }

    /// Create an RGB color.
    pub fn from_rgb(r: u8, g: u8, b: u8) -> Self {
        Self::Rgb(Color24 { r, g, b })
    }

    /// Returns whether the color is the normal special color.
    pub const fn is_normal(self) -> bool {
        matches!(self, Self::Normal)
    }

    /// Returns whether the color is the none special color.
    pub const fn is_none(self) -> bool {
        matches!(self, Self::None)
    }

    /// Returns whether the color is a named color (like "magenta").
    pub const fn is_named(self) -> bool {
        matches!(self, Self::Named { .. })
    }

    /// Returns whether the color is specified via RGB components.
    pub const fn is_rgb(self) -> bool {
        matches!(self, Self::Rgb(_))
    }

    /// Returns whether the color is special, that is, not rgb or named.
    pub const fn is_special(self) -> bool {
        !self.is_named() && !self.is_rgb()
    }

    pub fn is_grayscale(&self) -> bool {
        match self {
            Self::None => true,
            Self::Named { idx } => [0, 7, 8, 15, 16].contains(idx) || (232..=255).contains(idx),
            Self::Rgb(rgb) => rgb.r == rgb.g && rgb.r == rgb.b,
            Self::Normal => true,
        }
    }

    /// Returns the name index for the given color. Requires that the color be named or RGB.
    pub fn to_name_index(self) -> u8 {
        // TODO: This should look for the nearest color.
        match self {
            Self::Named { idx } => idx,
            Self::Rgb(c) => term16_color_for_rgb(c),
            Self::None | Self::Normal => {
                panic!("to_name_index() called on Color that's not named or RGB")
            }
        }
    }

    /// Returns the term256 index for the given color. Requires that the color be RGB.
    pub fn to_term256_index(self) -> u8 {
        let Self::Rgb(c) = self else {
            panic!("Tried to get term256 index of non-RGB color");
        };

        term256_color_for_rgb(c)
    }

    /// Returns the 24 bit color for the given color. Requires that the color be RGB.
    pub const fn to_color24(self) -> Color24 {
        let Self::Rgb(c) = self else {
            panic!("Tried to get color24 of non-RGB color");
        };

        c
    }

    /// Returns the names of all named colors.
    pub fn named_color_names() -> Vec<&'static wstr> {
        // We don't use all the NAMED_COLORS but we also need room for one more.
        let mut v = Vec::with_capacity(NAMED_COLORS.len());
        v.extend(
            NAMED_COLORS
                .iter()
                .filter_map(|&NamedColor { name, hidden, .. }| (!hidden).then_some(name)),
        );

        // "normal" isn't really a color and does not have a color palette index or
        // RGB value. Therefore, it does not appear in the NAMED_COLORS table.
        // However, it is a legitimate color name for the "set_color" command so
        // include it in the publicly known list of colors. This is primarily so it
        // appears in the output of "set_color --print-colors".
        v.push(L!("normal"));
        v
    }

    /// Try parsing a special color name like "normal".
    fn try_parse_special(special: &wstr) -> Option<Self> {
        if simple_icase_compare(special, L!("normal")) == Ordering::Equal {
            Some(Self::Normal)
        } else {
            None
        }
    }

    /// Try parsing an rgb color like "#F0A030".
    ///
    /// We support the following style of rgb formats (case insensitive):
    ///
    /// - `#FA3`
    /// - `#F3A035`
    /// - `FA3`
    /// - `F3A035`
    ///
    /// Parses input in the form of `#RGB` or `#RRGGBB` with an optional single leading `#` into
    /// an instance of [`RgbColor`].
    ///
    /// Returns `None` if the input contains invalid hexadecimal characters or is not in the
    /// expected `#RGB` or `#RRGGBB` formats.
    fn try_parse_rgb(mut s: &wstr) -> Option<Self> {
        // Skip one leading #
        if s.chars().next()? == '#' {
            s = &s[1..];
        }

        let mut hex = s.chars().map_while(|c| c.to_digit(16).map(|b| b as u8));

        let (r, g, b) = if s.len() == 3 {
            // Expected format: FA3
            (
                hex.next().map(|d| d * 16 + d)?,
                hex.next().map(|d| d * 16 + d)?,
                hex.next().map(|d| d * 16 + d)?,
            )
        } else if s.len() == 6 {
            // Expected format: F3A035
            (
                hex.next()? * 16 + hex.next()?,
                hex.next()? * 16 + hex.next()?,
                hex.next()? * 16 + hex.next()?,
            )
        } else {
            return None;
        };
        Some(Color::from_rgb(r, g, b))
    }

    /// Try parsing an explicit color name like "magenta".
    fn try_parse_named(name: &wstr) -> Option<Self> {
        let i = NAMED_COLORS
            .binary_search_by(|c| simple_icase_compare(c.name, name))
            .ok()?;

        Some(Self::Named {
            idx: NAMED_COLORS[i].idx,
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
    _rgb: [u8; 3],
    hidden: bool,
}

#[rustfmt::skip]
const NAMED_COLORS: &[NamedColor] = &[
    // Keep this sorted alphabetically
    NamedColor {name: L!("black"), idx: 0, _rgb: [0x00, 0x00, 0x00], hidden: false},
    NamedColor {name: L!("blue"), idx: 4, _rgb: [0x00, 0x00, 0x80], hidden: false},
    NamedColor {name: L!("brblack"), idx: 8, _rgb: [0x80, 0x80, 0x80], hidden: false},
    NamedColor {name: L!("brblue"), idx: 12, _rgb: [0x00, 0x00, 0xFF], hidden: false},
    NamedColor {name: L!("brbrown"), idx: 11, _rgb: [0xFF, 0xFF, 0x00], hidden: true},
    NamedColor {name: L!("brcyan"), idx: 14, _rgb: [0x00, 0xFF, 0xFF], hidden: false},
    NamedColor {name: L!("brgreen"), idx: 10, _rgb: [0x00, 0xFF, 0x00], hidden: false},
    NamedColor {name: L!("brgrey"), idx: 8, _rgb: [0x55, 0x55, 0x55], hidden: true},
    NamedColor {name: L!("brmagenta"), idx: 13, _rgb: [0xFF, 0x00, 0xFF], hidden: false},
    NamedColor {name: L!("brown"), idx: 3, _rgb: [0x72, 0x50, 0x00], hidden: true},
    NamedColor {name: L!("brpurple"), idx: 13, _rgb: [0xFF, 0x00, 0xFF], hidden: true},
    NamedColor {name: L!("brred"), idx: 9, _rgb: [0xFF, 0x00, 0x00], hidden: false},
    NamedColor {name: L!("brwhite"), idx: 15, _rgb: [0xFF, 0xFF, 0xFF], hidden: false},
    NamedColor {name: L!("bryellow"), idx: 11, _rgb: [0xFF, 0xFF, 0x00], hidden: false},
    NamedColor {name: L!("cyan"), idx: 6, _rgb: [0x00, 0x80, 0x80], hidden: false},
    NamedColor {name: L!("green"), idx: 2, _rgb: [0x00, 0x80, 0x00], hidden: false},
    NamedColor {name: L!("grey"), idx: 7, _rgb: [0xE5, 0xE5, 0xE5], hidden: true},
    NamedColor {name: L!("magenta"), idx: 5, _rgb: [0x80, 0x00, 0x80], hidden: false},
    NamedColor {name: L!("purple"), idx: 5, _rgb: [0x80, 0x00, 0x80], hidden: true},
    NamedColor {name: L!("red"), idx: 1, _rgb: [0x80, 0x00, 0x00], hidden: false},
    NamedColor {name: L!("white"), idx: 7, _rgb: [0xC0, 0xC0, 0xC0], hidden: false},
    NamedColor {name: L!("yellow"), idx: 3, _rgb: [0x80, 0x80, 0x00], hidden: false},
];

assert_sorted_by_name!(NAMED_COLORS);

fn convert_color(color: Color24, colors: &[u32]) -> usize {
    fn squared_difference(a: u8, b: u8) -> u32 {
        let a = u32::from(a);
        let b = u32::from(b);
        a.abs_diff(b).pow(2)
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
    use crate::color::{Color, Color24};
    use crate::wchar::prelude::*;

    #[test]
    fn parse() {
        assert!(Color::from_wstr(L!("#FF00A0")).unwrap().is_rgb());
        assert!(Color::from_wstr(L!("FF00A0")).unwrap().is_rgb());
        assert!(Color::from_wstr(L!("#F30")).unwrap().is_rgb());
        assert!(Color::from_wstr(L!("F30")).unwrap().is_rgb());
        assert!(Color::from_wstr(L!("f30")).unwrap().is_rgb());
        assert!(Color::from_wstr(L!("#FF30a5")).unwrap().is_rgb());
        assert!(Color::from_wstr(L!("3f30")).is_none());
        assert!(Color::from_wstr(L!("##f30")).is_none());
        assert!(Color::from_wstr(L!("magenta")).unwrap().is_named());
        assert!(Color::from_wstr(L!("MaGeNTa")).unwrap().is_named());
        assert!(Color::from_wstr(L!("mooganta")).is_none());
    }

    #[test]
    fn parse_rgb() {
        assert!(Color::from_wstr(L!("##FF00A0")).is_none());
        assert!(Color::from_wstr(L!("#FF00A0")) == Some(Color::from_rgb(0xff, 0x00, 0xa0)));
        assert!(Color::from_wstr(L!("FF00A0")) == Some(Color::from_rgb(0xff, 0x00, 0xa0)));
        assert!(Color::from_wstr(L!("FAF")) == Some(Color::from_rgb(0xff, 0xaa, 0xff)));
    }

    // Regression test for multiplicative overflow in convert_color.
    #[test]
    fn test_term16_color_for_rgb() {
        for c in 0..=u8::MAX {
            let color = Color::Rgb(Color24 { r: c, g: c, b: c });
            let _ = color.to_name_index();
        }
    }

    #[test]
    fn parse_short_hex_with_hash() {
        assert_eq!(
            Color::try_parse_rgb(L!("#F3A")),
            Some(Color::from_rgb(0xFF, 0x33, 0xAA))
        );
    }

    #[test]
    fn parse_long_hex_with_hash() {
        assert_eq!(
            Color::try_parse_rgb(L!("#F3A035")),
            Some(Color::from_rgb(0xF3, 0xA0, 0x35))
        );
    }

    #[test]
    fn parse_short_hex_without_hash() {
        assert_eq!(
            Color::try_parse_rgb(L!("F3A")),
            Some(Color::from_rgb(0xFF, 0x33, 0xAA))
        );
    }

    #[test]
    fn parse_long_hex_without_hash() {
        assert_eq!(
            Color::try_parse_rgb(L!("F3A035")),
            Some(Color::from_rgb(0xF3, 0xA0, 0x35))
        );
    }

    #[test]
    fn invalid_hex_length() {
        assert_eq!(Color::try_parse_rgb(L!("#F3A03")), None);
        assert_eq!(Color::try_parse_rgb(L!("F3A0")), None);
    }

    #[test]
    fn invalid_hex_character() {
        assert_eq!(Color::try_parse_rgb(L!("#GFA")), None);
        assert_eq!(Color::try_parse_rgb(L!("F3G035")), None);
    }

    #[test]
    fn invalid_hash_combinations() {
        assert_eq!(Color::try_parse_rgb(L!("##F3A")), None);
        assert_eq!(Color::try_parse_rgb(L!("###F3A035")), None);
        assert_eq!(Color::try_parse_rgb(L!("F3A#")), None);
        assert_eq!(Color::try_parse_rgb(L!("#F#3A")), None);
    }
}
