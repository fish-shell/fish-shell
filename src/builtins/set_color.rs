// Implementation of the set_color builtin.

use super::prelude::*;
use crate::color::Color;
use crate::common::str2wcstring;
use crate::terminal::TerminalCommand::DefaultUnderlineColor;
use crate::terminal::{Output, Outputter, Paintable};
use crate::text_face::{
    self, parse_text_face_and_options, PrintColorsArgs, SpecifiedTextFace, TextFace, TextStyling,
};

fn print_colors(
    streams: &mut IoStreams,
    args: &[&wstr],
    style: TextStyling,
    bg: Option<Color>,
    underline_color: Option<Color>,
) {
    let outp = &mut Outputter::new_buffering();

    // Rebind args to named_colors if there are no args.
    let named_colors;
    let args = if !args.is_empty() {
        args
    } else {
        named_colors = Color::named_color_names();
        &named_colors
    };

    for color_name in args {
        if streams.out_is_terminal() {
            let fg = Color::from_wstr(color_name).unwrap_or(Color::None);
            outp.set_text_face(TextFace::new(
                fg,
                bg.unwrap_or(Color::None),
                underline_color.unwrap_or(Color::None),
                style,
            ));
        }
        outp.write_wstr(color_name);
        if streams.out_is_terminal() && bg.is_some() {
            // If we have a background, stop it after the color
            // or it goes to the end of the line and looks ugly.
            outp.reset_text_face();
        }
        outp.writech('\n');
    } // conveniently, 'normal' is always the last color so we don't need to reset here

    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));
}

/// set_color builtin.
pub fn set_color(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    // Variables used for parsing the argument list.
    let argc = argv.len();

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if argc <= 1 {
        return Err(STATUS_CMD_ERROR);
    }

    use text_face::ParseError::*;
    use text_face::ParsedArgs::*;
    let (specified_face, is_reset) =
        match parse_text_face_and_options(argv, /*is_builtin=*/ true) {
            Ok(SetFace(face)) => (face, false),
            Ok(ResetFace) => (SpecifiedTextFace::default(), true),
            Ok(PrintColors(PrintColorsArgs {
                fg_args,
                bg,
                underline_color,
                style,
            })) => {
                print_colors(streams, fg_args, style, bg, underline_color);
                return Ok(SUCCESS);
            }
            Ok(PrintHelp) => {
                builtin_print_help(parser, streams, argv[0]);
                return Ok(SUCCESS);
            }
            Err(MissingOptArg) => {
                // Either "--background" or "--underline-color" are missing an argument.
                // Don't print an error, for consistency with "set_color".
                // In future we change both to actually print an error.
                return Err(STATUS_INVALID_ARGS);
            }
            Err(UnknownColor(arg)) => {
                streams
                    .err
                    .append(wgettext_fmt!("%ls: Unknown color '%ls'\n", argv[0], arg));
                return Err(STATUS_INVALID_ARGS);
            }
            Err(UnknownUnderlineStyle(arg)) => {
                streams.err.append(wgettext_fmt!(
                    "%ls: invalid underline style: %ls\n",
                    argv[0],
                    arg
                ));
                return Err(STATUS_INVALID_ARGS);
            }
            Err(UnknownOption(unknown_option_index)) => {
                builtin_unknown_option(
                    parser,
                    streams,
                    L!("set_color"),
                    argv[unknown_option_index],
                    true, /* print_hints */
                );
                return Err(STATUS_INVALID_ARGS);
            }
        };

    let mut outp = Outputter::new_buffering();

    let fg = specified_face.fg;
    let bg = specified_face.bg;
    let underline_color = specified_face.underline_color;
    let style = specified_face.style;

    // Here's some automagic behavior: if either of foreground or background are "normal",
    // reset all colors/attributes. Same if foreground is "reset" (undocumented).
    if is_reset || [fg, bg].iter().any(|c| c.is_some_and(|c| c.is_normal())) {
        outp.reset_text_face();
    }

    // Historically we have not used set_text_face() for colors here.
    // Doing so would add two magic behaviors:
    // - if fg and bg are equal, it makes one of them white
    // - if bg is not normal, it makes the foreground bold
    // The first one seems fine but the second one not really.
    outp.set_text_face(TextFace::new(Color::None, Color::None, Color::None, style));
    if let Some(fg) = fg {
        if !fg.is_normal() && !outp.write_color(Paintable::Foreground, fg) {
            // We need to do *something* or the lack of any output messes up
            // when the cartesian product here would make "foo" disappear:
            //  $ echo (set_color foo)bar
            outp.reset_text_face();
        }
    }
    if let Some(bg) = bg {
        if !bg.is_normal() {
            outp.write_color(Paintable::Background, bg);
        }
    }
    if let Some(underline_color) = underline_color {
        if underline_color.is_normal() {
            outp.write_command(DefaultUnderlineColor);
        } else {
            outp.write_color(Paintable::Underline, underline_color);
        }
    }

    // Output the collected string.
    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));

    Ok(SUCCESS)
}
