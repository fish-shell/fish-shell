use bitflags::bitflags;

use crate::color::Color;
use crate::terminal::{best_color, get_color_support};
use crate::wchar::prelude::*;
use crate::wgetopt::{wopt, ArgType, WGetopter, WOption};

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

impl TextFace {
    pub const fn default() -> Self {
        Self {
            fg: Color::Normal,
            bg: Color::Normal,
            style: TextStyling::empty(),
        }
    }

    pub fn new(fg: Color, bg: Color, style: TextStyling) -> Self {
        Self { fg, bg, style }
    }
    /// Returns whether the text face is bold.
    pub const fn is_bold(self) -> bool {
        self.style.contains(TextStyling::BOLD)
    }

    /// Returns whether the text face is underlined.
    pub const fn is_underline(self) -> bool {
        self.style.contains(TextStyling::UNDERLINE)
    }

    /// Set whether the text face is underline.
    pub fn inject_underline(&mut self, underline: bool) {
        self.style.set(TextStyling::UNDERLINE, underline)
    }

    /// Returns whether the text face is italics.
    pub const fn is_italics(self) -> bool {
        self.style.contains(TextStyling::ITALICS)
    }

    /// Returns whether the text face is dim.
    pub const fn is_dim(self) -> bool {
        self.style.contains(TextStyling::DIM)
    }

    /// Returns whether the text face has reverse foreground/background colors.
    pub const fn is_reverse(self) -> bool {
        self.style.contains(TextStyling::REVERSE)
    }
}

pub fn parse_text_face(arguments: &[WString], is_background: bool) -> (Option<Color>, TextStyling) {
    let mut argv: Vec<&wstr> = Some(L!(""))
        .into_iter()
        .chain(arguments.iter().map(|s| s.as_utfstr()))
        .collect();
    let TextFaceArgsAndOptions {
        wopt_index,
        bgcolor,
        mut style,
        print_color_mode,
    } = match parse_text_face_and_options(&mut argv, /*is_builtin=*/ false) {
        TextFaceArgsAndOptionsResult::Ok(parsed_text_faces) => parsed_text_faces,
        TextFaceArgsAndOptionsResult::PrintHelp
        | TextFaceArgsAndOptionsResult::InvalidArgs
        | TextFaceArgsAndOptionsResult::UnknownOption(_) => unreachable!(),
    };
    assert!(!print_color_mode);
    let color;
    if is_background {
        color = bgcolor.and_then(Color::from_wstr);
        // We ignore styles for dedicated background roles but reverse should be meaningful in
        // either context
        style &= TextStyling::REVERSE;
    } else {
        color = best_color(
            argv[wopt_index..]
                .iter()
                .filter_map(|&color| Color::from_wstr(color)),
            get_color_support(),
        );
    }
    assert!(color.map_or(true, |color| !color.is_none()));
    (color, style)
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
    let mut style = TextStyling::empty();
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
            'o' => style |= TextStyling::BOLD,
            'i' => style |= TextStyling::ITALICS,
            'd' => style |= TextStyling::DIM,
            'r' => style |= TextStyling::REVERSE,
            'u' => style |= TextStyling::UNDERLINE,
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
