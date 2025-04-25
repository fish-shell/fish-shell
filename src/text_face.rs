use crate::color::Color;
use crate::terminal::{best_color, get_color_support};
use crate::wchar::prelude::*;
use crate::wgetopt::{wopt, ArgType, WGetopter, WOption};

trait StyleSet {
    fn union_prefer_right(self, other: Self) -> Self;
    fn difference_prefer_empty(self, other: Self) -> Self;
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub(crate) enum UnderlineStyle {
    Single,
    Curly,
}

impl StyleSet for Option<UnderlineStyle> {
    fn union_prefer_right(self, other: Self) -> Self {
        other.or(self)
    }

    fn difference_prefer_empty(self, other: Self) -> Self {
        if other.is_some() {
            return None;
        }
        self
    }
}

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub(crate) struct TextStyling {
    pub(crate) bold: bool,
    pub(crate) underline_style: Option<UnderlineStyle>,
    pub(crate) italics: bool,
    pub(crate) dim: bool,
    pub(crate) reverse: bool,
}

impl TextStyling {
    pub(crate) const fn default() -> Self {
        Self {
            bold: false,
            underline_style: None,
            italics: false,
            dim: false,
            reverse: false,
        }
    }
    pub(crate) fn is_empty(&self) -> bool {
        *self == Self::default()
    }
    pub(crate) fn union_prefer_right(self, other: Self) -> Self {
        Self {
            bold: self.is_bold() || other.is_bold(),
            underline_style: self
                .underline_style
                .union_prefer_right(other.underline_style),
            italics: self.is_italics() || other.is_italics(),
            dim: self.is_dim() || other.is_dim(),
            reverse: self.is_reverse() || other.is_reverse(),
        }
    }
    pub(crate) fn difference_prefer_empty(self, other: Self) -> Self {
        Self {
            bold: self.is_bold() && !other.is_bold(),
            underline_style: self
                .underline_style
                .difference_prefer_empty(other.underline_style),
            italics: self.is_italics() && !other.is_italics(),
            dim: self.is_dim() && !other.is_dim(),
            reverse: self.is_reverse() && !other.is_reverse(),
        }
    }

    /// Returns whether the text face is bold.
    pub const fn is_bold(self) -> bool {
        self.bold
    }

    #[cfg(test)]
    pub const fn underline_style(self) -> Option<UnderlineStyle> {
        self.underline_style
    }

    /// Set the given underline style.
    pub fn inject_underline(&mut self, underline: UnderlineStyle) {
        self.underline_style = Some(underline);
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
    pub(crate) underline_color: Color,
    pub(crate) style: TextStyling,
}

impl TextFace {
    pub const fn default() -> Self {
        Self {
            fg: Color::Normal,
            bg: Color::Normal,
            underline_color: Color::None,
            style: TextStyling::default(),
        }
    }

    pub fn new(fg: Color, bg: Color, underline_color: Color, style: TextStyling) -> Self {
        Self {
            fg,
            bg,
            underline_color,
            style,
        }
    }
}

#[derive(Default)]
pub(crate) struct SpecifiedTextFace {
    pub(crate) fg: Option<Color>,
    pub(crate) bg: Option<Color>,
    pub(crate) underline_color: Option<Color>,
    pub(crate) style: TextStyling,
}

pub(crate) fn parse_text_face(arguments: &[WString]) -> SpecifiedTextFace {
    let mut argv: Vec<&wstr> = Some(L!(""))
        .into_iter()
        .chain(arguments.iter().map(|s| s.as_utfstr()))
        .collect();
    use ParsedArgs::*;
    match parse_text_face_and_options(&mut argv, /*is_builtin=*/ false) {
        Ok(SetFace(specified_face)) => specified_face,
        Ok(ResetFace) | Ok(PrintColors(_)) | Ok(PrintHelp) | Err(_) => unreachable!(),
    }
}

pub(crate) struct PrintColorsArgs<'argarray, 'args> {
    pub(crate) fg_args: &'argarray [&'args wstr],
    pub(crate) bg: Option<Color>,
    pub(crate) underline_color: Option<Color>,
    pub(crate) style: TextStyling,
}

pub(crate) enum ParsedArgs<'argarray, 'args> {
    SetFace(SpecifiedTextFace),
    ResetFace,
    PrintHelp,
    PrintColors(PrintColorsArgs<'argarray, 'args>),
}

pub(crate) enum ParseError<'args> {
    MissingOptArg,
    UnknownColor(&'args wstr),
    UnknownUnderlineStyle(&'args wstr),
    UnknownOption(usize),
}

pub(crate) fn parse_text_face_and_options<'argarray, 'args>(
    argv: &'argarray mut [&'args wstr],
    is_builtin: bool,
) -> Result<ParsedArgs<'argarray, 'args>, ParseError<'args>> {
    let builtin_extra_args = if is_builtin { 0 } else { "hc".len() };
    let short_options = L!(":b:oidru::ch");
    let short_options = &short_options[..short_options.len() - builtin_extra_args];
    let long_options: &[WOption] = &[
        wopt(L!("background"), ArgType::RequiredArgument, 'b'),
        wopt(L!("underline-color"), ArgType::RequiredArgument, '\x02'),
        wopt(L!("bold"), ArgType::NoArgument, 'o'),
        wopt(L!("underline"), ArgType::OptionalArgument, 'u'),
        wopt(L!("italics"), ArgType::NoArgument, 'i'),
        wopt(L!("dim"), ArgType::NoArgument, 'd'),
        wopt(L!("reverse"), ArgType::NoArgument, 'r'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("print-colors"), ArgType::NoArgument, 'c'),
    ];
    let long_options = &long_options[..long_options.len() - builtin_extra_args];

    use ParseError::*;
    use ParsedArgs::*;

    let parse_color = |color_str| match Color::from_wstr(color_str) {
        color @ Some(_) => Ok(color),
        None => {
            if is_builtin {
                Err(UnknownColor(color_str))
            } else {
                Ok(None)
            }
        }
    };

    let mut face = SpecifiedTextFace::default();
    let mut print_color_mode = false;

    let mut w = WGetopter::new(short_options, long_options, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'b' => {
                face.bg = parse_color(w.woptarg.unwrap())?;
            }
            '\x02' => {
                face.underline_color = parse_color(w.woptarg.unwrap())?;
            }
            'h' => {
                assert!(is_builtin);
                return Ok(PrintHelp);
            }
            'o' => face.style.bold = true,
            'i' => face.style.italics = true,
            'd' => face.style.dim = true,
            'r' => face.style.reverse = true,
            'u' => {
                let arg = w.woptarg.unwrap_or(L!("single"));
                if arg == "single" {
                    face.style.underline_style = Some(UnderlineStyle::Single);
                } else if arg == "curly" {
                    face.style.underline_style = Some(UnderlineStyle::Curly);
                } else if is_builtin {
                    return Err(UnknownUnderlineStyle(arg));
                }
            }
            'c' => {
                assert!(is_builtin);
                print_color_mode = true;
            }
            ':' => {
                if is_builtin {
                    return Err(MissingOptArg);
                }
            }
            '?' => {
                if is_builtin {
                    return Err(UnknownOption(w.wopt_index - 1));
                }
            }
            _ => unreachable!("unexpected retval from WGetopter"),
        }
    }

    let fg_args = &w.argv[w.wopt_index..];

    if print_color_mode {
        return Ok(PrintColors(PrintColorsArgs {
            fg_args,
            bg: face.bg,
            underline_color: face.underline_color,
            style: face.style,
        }));
    }

    // Historical behavior: reset only applies if it's the first argument.
    if is_builtin && fg_args.get(0).is_some_and(|fg| fg == "reset") {
        return Ok(ResetFace);
    }

    let mut fg_colors = Vec::with_capacity(fg_args.len());
    for fg in fg_args {
        if is_builtin && fg == "reset" {
            continue;
        }
        if let Some(fg) = parse_color(fg)? {
            fg_colors.push(fg);
        }
    }
    // #1323: We may have multiple foreground colors. Choose the best one.
    face.fg = best_color(fg_colors.into_iter(), get_color_support());
    Ok(SetFace(face))
}
