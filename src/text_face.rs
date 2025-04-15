use crate::color::Color;
use crate::terminal::{best_color, get_color_support};
use crate::wchar::prelude::*;
use crate::wgetopt::{wopt, ArgType, WGetopter, WOption};

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub(crate) struct TextStyling {
    pub(crate) bold: bool,
    pub(crate) underline: bool,
    pub(crate) italics: bool,
    pub(crate) dim: bool,
    pub(crate) reverse: bool,
}

impl TextStyling {
    pub(crate) const fn default() -> Self {
        Self {
            bold: false,
            underline: false,
            italics: false,
            dim: false,
            reverse: false,
        }
    }
    pub(crate) fn is_empty(&self) -> bool {
        *self == Self::default()
    }
    pub(crate) fn union(self, other: Self) -> Self {
        Self {
            bold: self.is_bold() || other.is_bold(),
            underline: self.is_underline() || other.is_underline(),
            italics: self.is_italics() || other.is_italics(),
            dim: self.is_dim() || other.is_dim(),
            reverse: self.is_reverse() || other.is_reverse(),
        }
    }
    pub(crate) fn difference(self, other: Self) -> Self {
        Self {
            bold: self.is_bold() && !other.is_bold(),
            underline: self.is_underline() && !other.is_underline(),
            italics: self.is_italics() && !other.is_italics(),
            dim: self.is_dim() && !other.is_dim(),
            reverse: self.is_reverse() && !other.is_reverse(),
        }
    }

    /// Returns whether the text face is bold.
    pub const fn is_bold(self) -> bool {
        self.bold
    }

    /// Returns whether the text face is underlined.
    pub const fn is_underline(self) -> bool {
        self.underline
    }

    /// Set whether the text face is underline.
    pub fn inject_underline(&mut self, underline: bool) {
        self.underline = underline;
    }

    /// Returns whether the text face is italics.
    pub const fn is_italics(self) -> bool {
        self.italics
    }

    /// Returns whether the text face is dim.
    pub const fn is_dim(self) -> bool {
        self.dim
    }

    /// Returns whether the text face has reverse foreground/background colors.
    pub const fn is_reverse(self) -> bool {
        self.reverse
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct TextFace {
    pub(crate) fg: Color,
    pub(crate) bg: Color,
    pub(crate) style: TextStyling,
}

impl TextFace {
    pub const fn default() -> Self {
        Self {
            fg: Color::Normal,
            bg: Color::Normal,
            style: TextStyling::default(),
        }
    }

    pub fn new(fg: Color, bg: Color, style: TextStyling) -> Self {
        Self { fg, bg, style }
    }
}

pub(crate) struct SpecifiedTextFace {
    pub(crate) fg: Option<Color>,
    pub(crate) bg: Option<Color>,
    pub(crate) style: TextStyling,
}

pub(crate) fn parse_text_face(arguments: &[WString]) -> SpecifiedTextFace {
    let mut argv: Vec<&wstr> = Some(L!(""))
        .into_iter()
        .chain(arguments.iter().map(|s| s.as_utfstr()))
        .collect();
    let TextFaceArgsAndOptions {
        wopt_index,
        bgcolor,
        style,
        print_color_mode,
    } = match parse_text_face_and_options(&mut argv, /*is_builtin=*/ false) {
        TextFaceArgsAndOptionsResult::Ok(parsed_text_faces) => parsed_text_faces,
        TextFaceArgsAndOptionsResult::PrintHelp
        | TextFaceArgsAndOptionsResult::InvalidArgs
        | TextFaceArgsAndOptionsResult::UnknownOption(_) => unreachable!(),
    };
    assert!(!print_color_mode);
    let fg = best_color(
        argv[wopt_index..]
            .iter()
            .filter_map(|&color| Color::from_wstr(color)),
        get_color_support(),
    );
    let bg = bgcolor.and_then(Color::from_wstr);
    assert!(fg.map_or(true, |fg| !fg.is_none()));
    assert!(bg.map_or(true, |bg| !bg.is_none()));
    SpecifiedTextFace { fg, bg, style }
}

pub(crate) struct TextFaceArgsAndOptions<'a> {
    pub(crate) wopt_index: usize,
    pub(crate) bgcolor: Option<&'a wstr>,
    pub(crate) style: TextStyling,
    pub(crate) print_color_mode: bool,
}

pub(crate) enum TextFaceArgsAndOptionsResult<'a> {
    Ok(TextFaceArgsAndOptions<'a>),
    PrintHelp,
    InvalidArgs,
    UnknownOption(usize),
}

pub(crate) fn parse_text_face_and_options<'a>(
    argv: &mut [&'a wstr],
    is_builtin: bool,
) -> TextFaceArgsAndOptionsResult<'a> {
    let builtin_extra_args = if is_builtin { 0 } else { "hc".len() };
    let short_options = L!(":b:oidruch");
    let short_options = &short_options[..short_options.len() - builtin_extra_args];
    let long_options: &[WOption] = &[
        wopt(L!("background"), ArgType::RequiredArgument, 'b'),
        wopt(L!("bold"), ArgType::NoArgument, 'o'),
        wopt(L!("underline"), ArgType::NoArgument, 'u'),
        wopt(L!("italics"), ArgType::NoArgument, 'i'),
        wopt(L!("dim"), ArgType::NoArgument, 'd'),
        wopt(L!("reverse"), ArgType::NoArgument, 'r'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("print-colors"), ArgType::NoArgument, 'c'),
    ];
    let long_options = &long_options[..long_options.len() - builtin_extra_args];

    let mut bgcolor = None;
    let mut style = TextStyling::default();
    let mut print_color_mode = false;

    let mut w = WGetopter::new(short_options, long_options, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'b' => {
                assert!(w.woptarg.is_some(), "Arg should have been set");
                bgcolor = w.woptarg;
            }
            'h' => {
                if is_builtin {
                    return TextFaceArgsAndOptionsResult::PrintHelp;
                }
            }
            'o' => style.bold = true,
            'i' => style.italics = true,
            'd' => style.dim = true,
            'r' => style.reverse = true,
            'u' => style.underline = true,
            'c' => print_color_mode = true,
            ':' => {
                if is_builtin {
                    return TextFaceArgsAndOptionsResult::InvalidArgs;
                }
            }
            '?' => {
                if is_builtin {
                    return TextFaceArgsAndOptionsResult::UnknownOption(w.wopt_index - 1);
                }
            }
            _ => unreachable!("unexpected retval from WGetopter"),
        }
    }
    TextFaceArgsAndOptionsResult::Ok(TextFaceArgsAndOptions {
        wopt_index: w.wopt_index,
        bgcolor,
        style,
        print_color_mode,
    })
}
