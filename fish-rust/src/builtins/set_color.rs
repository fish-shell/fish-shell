// Implementation of the set_color builtin.

use super::shared::{
    builtin_print_help, builtin_unknown_option, io_streams_t, STATUS_CMD_ERROR, STATUS_CMD_OK,
    STATUS_INVALID_ARGS,
};
use crate::color::RgbColor;
use crate::common::str2wcstring;
use crate::curses::{self, tparm0, Term};
use crate::ffi::parser_t;
use crate::output::{self, writembs_nofail, Outputter};
use crate::wchar::{wstr, L};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::wgettext_fmt;
use libc::c_int;
use std::ffi::CString;

// Helper to make curses::tparm0 more convenient.
fn tparm(s: &Option<CString>) -> Option<CString> {
    match s {
        None => None,
        Some(s) => tparm0(s),
    }
}

#[allow(clippy::too_many_arguments)]
fn print_modifiers(
    outp: &mut Outputter,
    term: &Term,
    bold: bool,
    underline: bool,
    italics: bool,
    dim: bool,
    reverse: bool,
    bg: RgbColor,
) {
    let Term {
        enter_bold_mode,
        enter_underline_mode,
        enter_italics_mode,
        enter_dim_mode,
        enter_reverse_mode,
        enter_standout_mode,
        exit_attribute_mode,
        ..
    } = term;
    if bold && enter_bold_mode.is_some() {
        writembs_nofail!(outp, tparm(enter_bold_mode));
    }

    if underline && enter_underline_mode.is_some() {
        writembs_nofail!(outp, enter_underline_mode);
    }

    if italics && enter_italics_mode.is_some() {
        writembs_nofail!(outp, enter_italics_mode);
    }

    if dim && enter_dim_mode.is_some() {
        writembs_nofail!(outp, enter_dim_mode);
    }

    if reverse && enter_reverse_mode.is_some() {
        writembs_nofail!(outp, enter_reverse_mode);
    } else if reverse && enter_standout_mode.is_some() {
        writembs_nofail!(outp, enter_standout_mode);
    }
    if !bg.is_none() && bg.is_normal() {
        writembs_nofail!(outp, tparm(exit_attribute_mode));
    }
}

#[allow(clippy::too_many_arguments)]
fn print_colors(
    streams: &mut io_streams_t,
    args: &[&wstr],
    bold: bool,
    underline: bool,
    italics: bool,
    dim: bool,
    reverse: bool,
    bg: RgbColor,
) {
    let outp = &mut output::Outputter::new_buffering();

    // Rebind args to named_colors if there are no args.
    let named_colors;
    let args = if !args.is_empty() {
        args
    } else {
        named_colors = RgbColor::named_color_names();
        &named_colors
    };

    let term = curses::term();
    for color_name in args {
        // Safety: isatty cannot fail.
        if !streams.out_is_redirected && unsafe { libc::isatty(libc::STDOUT_FILENO) == 1 } {
            if let Some(term) = term.as_ref() {
                print_modifiers(outp, term, bold, underline, italics, dim, reverse, bg);
            }
            let color = RgbColor::from_wstr(color_name).unwrap_or(RgbColor::NONE);
            outp.set_color(color, RgbColor::NONE);
            if !bg.is_none() {
                outp.write_color(bg, false /* not is_fg */);
            }
        }
        outp.write_wstr(color_name);
        if !bg.is_none() {
            // If we have a background, stop it after the color
            // or it goes to the end of the line and looks ugly.
            if let Some(term) = term.as_ref() {
                writembs_nofail!(outp, tparm(&term.exit_attribute_mode));
            }
        }
        outp.writech('\n');
    } // conveniently, 'normal' is always the last color so we don't need to reset here

    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));
}

const short_options: &wstr = L!(":b:hoidrcu");
const long_options: &[woption] = &[
    wopt(L!("background"), woption_argument_t::required_argument, 'b'),
    wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    wopt(L!("bold"), woption_argument_t::no_argument, 'o'),
    wopt(L!("underline"), woption_argument_t::no_argument, 'u'),
    wopt(L!("italics"), woption_argument_t::no_argument, 'i'),
    wopt(L!("dim"), woption_argument_t::no_argument, 'd'),
    wopt(L!("reverse"), woption_argument_t::no_argument, 'r'),
    wopt(L!("print-colors"), woption_argument_t::no_argument, 'c'),
];

/// set_color builtin.
pub fn set_color(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    argv: &mut [&wstr],
) -> Option<c_int> {
    // Variables used for parsing the argument list.
    let argc = argv.len();

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if argc <= 1 {
        return STATUS_CMD_ERROR;
    }

    let mut bgcolor = None;
    let mut bold = false;
    let mut underline = false;
    let mut italics = false;
    let mut dim = false;
    let mut reverse = false;
    let mut print = false;

    let mut w = wgetopter_t::new(short_options, long_options, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'b' => {
                assert!(w.woptarg.is_some(), "Arg should have been set");
                bgcolor = w.woptarg;
            }
            'h' => {
                builtin_print_help(parser, streams, argv[0]);
                return STATUS_CMD_OK;
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
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(
                    parser,
                    streams,
                    L!("set_color"),
                    argv[w.woptind - 1],
                    true, /* print_hints */
                );
                return STATUS_INVALID_ARGS;
            }
            _ => unreachable!("unexpected retval from wgeopter_t::wgetopt_long"),
        }
    }
    // We want to reclaim argv so grab woptind now.
    let mut woptind = w.woptind;

    let mut bg = RgbColor::from_wstr(bgcolor.unwrap_or(L!(""))).unwrap_or(RgbColor::NONE);
    if bgcolor.is_some() && bg.is_none() {
        streams.err.append(wgettext_fmt!(
            "%ls: Unknown color '%ls'\n",
            argv[0],
            bgcolor.unwrap()
        ));
        return STATUS_INVALID_ARGS;
    }

    if print {
        // Hack: Explicitly setting a background of "normal" crashes
        // for --print-colors. Because it's not interesting in terms of display,
        // just skip it.
        if bgcolor.is_some() && bg.is_special() {
            bg = RgbColor::from_wstr(L!("")).unwrap_or(RgbColor::NONE);
        }
        let args = &argv[woptind..argc];
        print_colors(streams, args, bold, underline, italics, dim, reverse, bg);
        return STATUS_CMD_OK;
    }

    // Remaining arguments are foreground color.
    let mut fgcolors = Vec::new();
    while woptind < argc {
        let fg = RgbColor::from_wstr(argv[woptind]).unwrap_or(RgbColor::NONE);
        if fg.is_none() {
            streams.err.append(wgettext_fmt!(
                "%ls: Unknown color '%ls'\n",
                argv[0],
                argv[woptind]
            ));
            return STATUS_INVALID_ARGS;
        };
        fgcolors.push(fg);
        woptind += 1;
    }

    // #1323: We may have multiple foreground colors. Choose the best one. If we had no foreground
    // color, we'll get none(); if we have at least one we expect not-none.
    let fg = output::best_color(&fgcolors, output::get_color_support());
    assert!(fgcolors.is_empty() || !fg.is_none());

    // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
    // just give up...
    let Some(term) = curses::term() else {
        return STATUS_CMD_ERROR;
    };
    let Some(exit_attribute_mode) = term.exit_attribute_mode.as_ref() else {
        return STATUS_CMD_ERROR;
    };
    let outp = &mut output::Outputter::new_buffering();
    print_modifiers(outp, &term, bold, underline, italics, dim, reverse, bg);
    if bgcolor.is_some() && bg.is_normal() {
        writembs_nofail!(outp, tparm0(exit_attribute_mode));
    }

    if !fg.is_none() {
        if fg.is_normal() || fg.is_reset() {
            writembs_nofail!(outp, tparm0(exit_attribute_mode));
        } else if !outp.write_color(fg, true /* is_fg */) {
            // We need to do *something* or the lack of any output messes up
            // when the cartesian product here would make "foo" disappear:
            //  $ echo (set_color foo)bar
            outp.set_color(RgbColor::RESET, RgbColor::NONE);
        }
    }
    if bgcolor.is_some() && !bg.is_normal() && !bg.is_reset() {
        outp.write_color(bg, false /* is_fg */);
    }

    // Output the collected string.
    let contents = outp.contents();
    streams.out.append(str2wcstring(contents));

    STATUS_CMD_OK
}
