// Implementation of the set_color builtin.

use super::prelude::*;
use crate::color::Color;
use crate::common::str2wcstring;
use crate::terminal::TerminalCommand::{
    EnterBoldMode, EnterDimMode, EnterItalicsMode, EnterReverseMode, EnterStandoutMode,
    EnterUnderlineMode, ExitAttributeMode,
};
use crate::terminal::{best_color, get_color_support, Output, Outputter};

#[allow(clippy::too_many_arguments)]
fn print_modifiers(
    outp: &mut Outputter,
    bold: bool,
    underline: bool,
    italics: bool,
    dim: bool,
    reverse: bool,
    bg: Color,
) {
    if bold {
        outp.write_command(EnterBoldMode);
    }

    if underline {
        outp.write_command(EnterUnderlineMode);
    }

    if italics {
        outp.write_command(EnterItalicsMode);
    }

    if dim {
        outp.write_command(EnterDimMode);
    }

    #[allow(clippy::collapsible_if)]
    if reverse {
        if !outp.write_command(EnterReverseMode) {
            outp.write_command(EnterStandoutMode);
        }
    }
    if !bg.is_none() && bg.is_normal() {
        outp.write_command(ExitAttributeMode);
    }
}

#[allow(clippy::too_many_arguments)]
fn print_colors(
    streams: &mut IoStreams,
    args: &[&wstr],
    bold: bool,
    underline: bool,
    italics: bool,
    dim: bool,
    reverse: bool,
    bg: Color,
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
            print_modifiers(outp, bold, underline, italics, dim, reverse, bg);
            let color = Color::from_wstr(color_name).unwrap_or(Color::NONE);
            outp.set_color(color, Color::NONE);
            if !bg.is_none() {
                outp.write_color(bg, false /* not is_fg */);
            }
        }
        outp.write_wstr(color_name);
        if !bg.is_none() {
            // If we have a background, stop it after the color
            // or it goes to the end of the line and looks ugly.
            outp.write_command(ExitAttributeMode);
        }
        outp.writech('\n');
    } // conveniently, 'normal' is always the last color so we don't need to reset here

    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));
}

const SHORT_OPTIONS: &wstr = L!(":b:hoidrcu");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("background"), ArgType::RequiredArgument, 'b'),
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("bold"), ArgType::NoArgument, 'o'),
    wopt(L!("underline"), ArgType::NoArgument, 'u'),
    wopt(L!("italics"), ArgType::NoArgument, 'i'),
    wopt(L!("dim"), ArgType::NoArgument, 'd'),
    wopt(L!("reverse"), ArgType::NoArgument, 'r'),
    wopt(L!("print-colors"), ArgType::NoArgument, 'c'),
];

/// set_color builtin.
pub fn set_color(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    // Variables used for parsing the argument list.
    let argc = argv.len();

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if argc <= 1 {
        return Err(STATUS_CMD_ERROR);
    }

    let mut bgcolor = None;
    let mut bold = false;
    let mut underline = false;
    let mut italics = false;
    let mut dim = false;
    let mut reverse = false;
    let mut print = false;

    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'b' => {
                assert!(w.woptarg.is_some(), "Arg should have been set");
                bgcolor = w.woptarg;
            }
            'h' => {
                builtin_print_help(parser, streams, argv[0]);
                return Ok(SUCCESS);
            }
            'o' => bold = true,
            'i' => italics = true,
            'd' => dim = true,
            'r' => reverse = true,
            'u' => underline = true,
            'c' => print = true,
            ':' => {
                // We don't error here because "-b" is the only option that requires an argument,
                // and we don't error for missing colors.
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(
                    parser,
                    streams,
                    L!("set_color"),
                    argv[w.wopt_index - 1],
                    true, /* print_hints */
                );
                return Err(STATUS_INVALID_ARGS);
            }
            _ => unreachable!("unexpected retval from WGetopter"),
        }
    }
    // We want to reclaim argv so grab wopt_index now.
    let mut wopt_index = w.wopt_index;

    let mut bg = Color::from_wstr(bgcolor.unwrap_or(L!(""))).unwrap_or(Color::NONE);
    if bgcolor.is_some() && bg.is_none() {
        streams.err.append(wgettext_fmt!(
            "%ls: Unknown color '%ls'\n",
            argv[0],
            bgcolor.unwrap()
        ));
        return Err(STATUS_INVALID_ARGS);
    }

    if print {
        // Hack: Explicitly setting a background of "normal" crashes
        // for --print-colors. Because it's not interesting in terms of display,
        // just skip it.
        if bgcolor.is_some() && bg.is_special() {
            bg = Color::from_wstr(L!("")).unwrap_or(Color::NONE);
        }
        let args = &argv[wopt_index..argc];
        print_colors(streams, args, bold, underline, italics, dim, reverse, bg);
        return Ok(SUCCESS);
    }

    // Remaining arguments are foreground color.
    let mut fgcolors = Vec::new();
    while wopt_index < argc {
        let fg = Color::from_wstr(argv[wopt_index]).unwrap_or(Color::NONE);
        if fg.is_none() {
            streams.err.append(wgettext_fmt!(
                "%ls: Unknown color '%ls'\n",
                argv[0],
                argv[wopt_index]
            ));
            return Err(STATUS_INVALID_ARGS);
        };
        fgcolors.push(fg);
        wopt_index += 1;
    }

    // #1323: We may have multiple foreground colors. Choose the best one. If we had no foreground
    // color, we'll get none(); if we have at least one we expect not-none.
    let fg = best_color(&fgcolors, get_color_support());
    assert!(fgcolors.is_empty() || !fg.is_none());

    let outp = &mut Outputter::new_buffering();
    print_modifiers(outp, bold, underline, italics, dim, reverse, bg);
    if bgcolor.is_some() && bg.is_normal() {
        outp.write_command(ExitAttributeMode);
    }

    if !fg.is_none() {
        if fg.is_normal() || fg.is_reset() {
            outp.write_command(ExitAttributeMode);
        } else if !outp.write_color(fg, true /* is_fg */) {
            // We need to do *something* or the lack of any output messes up
            // when the cartesian product here would make "foo" disappear:
            //  $ echo (set_color foo)bar
            outp.set_color(Color::RESET, Color::NONE);
        }
    }
    if bgcolor.is_some() && !bg.is_normal() && !bg.is_reset() {
        outp.write_color(bg, false /* is_fg */);
    }

    // Output the collected string.
    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));

    Ok(SUCCESS)
}
