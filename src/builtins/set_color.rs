// Implementation of the set_color builtin.

use super::prelude::*;
use crate::color::Color;
use crate::common::str2wcstring;
use crate::terminal::{best_color, get_color_support, Outputter};
use crate::text_face::{
    parse_text_face_and_options, TextFace, TextFaceArgsAndOptions, TextFaceArgsAndOptionsResult,
    TextStyling,
};

fn print_colors(streams: &mut IoStreams, args: &[&wstr], style: TextStyling, bg: Option<Color>) {
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
            outp.set_text_face(TextFace::new(fg, bg.unwrap_or(Color::None), style));
        }
        outp.write_wstr(color_name);
        if streams.out_is_terminal() && bg.is_some() {
            // If we have a background, stop it after the color
            // or it goes to the end of the line and looks ugly.
            outp.reset_text_face(true);
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

    let TextFaceArgsAndOptions {
        wopt_index,
        bgcolor,
        style,
        print_color_mode,
    } = match parse_text_face_and_options(argv, /*is_builtin=*/ true) {
        TextFaceArgsAndOptionsResult::Ok(parsed_face) => parsed_face,
        TextFaceArgsAndOptionsResult::PrintHelp => {
            builtin_print_help(parser, streams, argv[0]);
            return Ok(SUCCESS);
        }
        TextFaceArgsAndOptionsResult::InvalidArgs => {
            // We don't error here because "-b" is the only option that requires an argument,
            // and we don't error for missing colors.
            return Err(STATUS_INVALID_ARGS);
        }
        TextFaceArgsAndOptionsResult::UnknownOption(unknown_option_index) => {
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

    let mut parse_color = |color_str| {
        Color::from_wstr(color_str).ok_or_else(|| {
            streams.err.append(wgettext_fmt!(
                "%ls: Unknown color '%ls'\n",
                argv[0],
                color_str
            ));
            STATUS_INVALID_ARGS
        })
    };

    let bg = match bgcolor {
        Some(s) => Some(parse_color(s)?),
        None => None,
    };

    if print_color_mode {
        let args = &argv[wopt_index..argc];
        print_colors(streams, args, style, bg);
        return Ok(SUCCESS);
    }

    // Remaining arguments are foreground color.
    let mut fgcolors = Vec::new();
    let mut is_reset = false;
    for (i, arg) in argv.iter().skip(wopt_index).enumerate() {
        if arg == "reset" {
            // Historical behavior: reset only applies if it's the first argument.
            if i == 0 {
                is_reset = true;
                break;
            }
        } else {
            fgcolors.push(parse_color(arg)?);
        }
    }

    // #1323: We may have multiple foreground colors. Choose the best one.
    let fg = best_color(fgcolors.into_iter(), get_color_support());

    let mut outp = Outputter::new_buffering();

    // Here's some automagic behavior: if either of foreground or background are "normal",
    // reset all colors/attributes. Same if foreground is "reset" (undocumented).
    // Note that either overwrite the attributes printed above! For "normal", this is probably wrong?
    if is_reset || [fg, bg].iter().any(|c| c.is_some_and(|c| c.is_normal())) {
        outp.reset_text_face(false);
    } else {
        outp.set_text_face(TextFace::new(Color::None, Color::None, style));
        if let Some(fg) = fg {
            if !outp.write_color(fg, true /* is_fg */) {
                // We need to do *something* or the lack of any output messes up
                // when the cartesian product here would make "foo" disappear:
                //  $ echo (set_color foo)bar
                outp.reset_text_face(true);
            }
        }
        if let Some(bg) = bg {
            outp.write_color(bg, false /* is_fg */);
        }
    }

    // Output the collected string.
    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));

    Ok(SUCCESS)
}
