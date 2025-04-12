use bitflags::bitflags;

use crate::color::Color;

bitflags! {
    #[derive(Debug, Default, Copy, Clone, PartialEq, Eq)]
    pub struct TextStyling: u8 {
        const BOLD = 1<<0;
        const UNDERLINE = 1<<1;
        const ITALICS = 1<<2;
        const DIM = 1<<3;
        const REVERSE = 1<<4;
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct TextFace {
    pub(crate) fg: Color,
    pub(crate) bg: Color,
    pub(crate) style: TextStyling,
}

impl Default for TextFace {
    fn default() -> Self {
        Self {
            fg: Color::Normal,
            bg: Color::Normal,
            style: TextStyling::empty(),
        }
    }
}

impl TextFace {
    pub fn new(fg: Color, bg: Color, style: TextStyling) -> Self {
        Self { fg, bg, style }
    }
    /// Returns whether the text face is bold.
    pub const fn is_bold(self) -> bool {
        self.style.contains(TextStyling::BOLD)
    }

    /// Set whether the text face is bold.
    pub fn set_bold(&mut self, bold: bool) {
        self.style.set(TextStyling::BOLD, bold)
    }

    /// Returns whether the text face is underlined.
    pub const fn is_underline(self) -> bool {
        self.style.contains(TextStyling::UNDERLINE)
    }

    /// Set whether the text face is underline.
    pub fn set_underline(&mut self, underline: bool) {
        self.style.set(TextStyling::UNDERLINE, underline)
    }

    /// Returns whether the text face is italics.
    pub const fn is_italics(self) -> bool {
        self.style.contains(TextStyling::ITALICS)
    }

    /// Set whether the text face is italics.
    pub fn set_italics(&mut self, italics: bool) {
        self.style.set(TextStyling::ITALICS, italics)
    }

    /// Returns whether the text face is dim.
    pub const fn is_dim(self) -> bool {
        self.style.contains(TextStyling::DIM)
    }

    /// Set whether the text face is dim.
    pub fn set_dim(&mut self, dim: bool) {
        self.style.set(TextStyling::DIM, dim)
    }

    /// Returns whether the text face has reverse foreground/background colors.
    pub const fn is_reverse(self) -> bool {
        self.style.contains(TextStyling::REVERSE)
    }

    /// Set whether the text face has reverse foreground/background colors.
    pub fn set_reverse(&mut self, reverse: bool) {
        self.style.set(TextStyling::REVERSE, reverse)
    }
}
