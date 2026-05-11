// Implementation of the set_color builtin.

use super::prelude::*;
use crate::{
    builtins::error::Error,
    err_fmt,
    screen::{is_dumb, only_grayscale},
    terminal::Outputter,
    text_face::{self, PrintColorsArgs, TextFace, TextStyling, parse_text_face_and_options},
};
use fish_color::Color;
use fish_widestring::bytes2wcstring;

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
    streams.out.append(&bytes2wcstring(contents));
}

/// set_color builtin.
pub fn set_color(
    parser: &mut Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> BuiltinResult {
    // Variables used for parsing the argument list.
    let argc = argv.len();

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if argc <= 1 {
        return Err(STATUS_CMD_ERROR);
    }

    use text_face::ParseError::*;
    use text_face::ParsedArgs::*;
    let specified_face = match parse_text_face_and_options(argv, /*is_builtin=*/ true) {
        Ok(SetFace(face)) => face,
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
        Err(err) => {
            let error = match err {
                MissingOptArg => {
                    // Either "--background" or "--underline-color" are missing an argument.
                    // Don't print an error, for consistency with "set_color".
                    // In future we change both to actually print an error.
                    return Err(STATUS_INVALID_ARGS);
                }
                UnexpectedOptArg(option_index) => {
                    err_fmt!(Error::UNEXP_OPT_ARG, argv[option_index]).full_trailer(parser)
                }
                InvalidOptArg(name, value) => {
                    err_fmt!("%s: invalid option argument: %s", name, value)
                }
                UnknownColor(arg) => {
                    err_fmt!("Unknown color '%s'", arg)
                }
                UnknownUnderlineStyle(arg) => {
                    err_fmt!("invalid underline style: %s", arg)
                }
                UnknownOption(unknown_option_index) => {
                    err_fmt!(Error::UNKNOWN_OPT, argv[unknown_option_index]).full_trailer(parser)
                }
                InvalidFgArgCombination => {
                    err_fmt!(
                        "%s: option cannot be used with a non-option argument",
                        "--foreground"
                    )
                }
                InvalidFgPrintColorCombination => {
                    err_fmt!(Error::COMBO_EXCLUSIVE, "--foreground", "--print-colors",)
                }
            };
            error.cmd(argv[0]).finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }
    };

    let mut outp = Outputter::new_buffering_no_assume_normal();

    // Note that for historical reasons "set_color normal" reset all colors/attributes. So does
    // "set_color reset" (undocumented).
    outp.set_text_face_no_magic(
        TextFace::new(
            specified_face.fg.unwrap_or(Color::None),
            specified_face.bg.unwrap_or(Color::None),
            specified_face.underline_color.unwrap_or(Color::None),
            specified_face.style,
        ),
        specified_face.reset,
    );

    if specified_face.fg.is_some() && outp.contents().is_empty() {
        assert!(is_dumb() || only_grayscale());
        // We need to do *something* or the lack of any output messes up
        // when the cartesian product here would make "foo" disappear:
        //  $ echo (set_color foo)bar
        outp.reset_text_face();
    }

    // Output the collected string.
    let contents = outp.contents();
    streams.out.append(&bytes2wcstring(contents));

    Ok(SUCCESS)
}
