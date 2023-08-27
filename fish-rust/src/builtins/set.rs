use crate::{
    env::{EnvMode, EnvVar, Environment},
    wutil::wcstoi::wcstoi_partial,
};

use super::prelude::*;
use crate::wutil::wcstoi;
use once_cell::sync::Lazy;

// PORTING: I don't know if `_` is evaluated at runtime or compile-time and whether translations are/will be embedded/loaded at runtime
// FIXME: (once localization works in Rust) These should separate `%ls: ` and the trailing `\n`, like in builtins/string
const MISMATCHED_ARGS: Lazy<&wstr> =
    Lazy::new(|| wgettext_str(L!("%ls: given %d indexes but %d values\n")));
const ARRAY_BOUNDS_ERR: Lazy<&wstr> =
    Lazy::new(|| wgettext_str(L!("%ls: array index out of bounds\n")));
const UVAR_ERR: Lazy<&wstr> = Lazy::new(|| {
    wgettext_str(L!(
        "%ls: successfully set universal '%ls'; but a global by that name shadows it\n"
    ))
});

#[derive(Debug, Clone)]
struct Options {
    print_help: bool,
    show: bool,
    local: bool,
    function: bool,
    global: bool,
    exportv: bool,
    erase: bool,
    list: bool,
    unexport: bool,
    pathvar: bool,
    unpathvar: bool,
    universal: bool,
    query: bool,
    shorten_ok: bool,
    append: bool,
    prepend: bool,
    preserve_failure_exit_status: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            print_help: false,
            show: false,
            local: false,
            function: false,
            global: false,
            exportv: false,
            erase: false,
            list: false,
            unexport: false,
            pathvar: false,
            unpathvar: false,
            universal: false,
            query: false,
            shorten_ok: true,
            append: false,
            prepend: false,
            preserve_failure_exit_status: true,
        }
    }
}

impl Options {
    fn scope(&self) -> EnvMode {
        let mut scope = EnvMode::USER;
        for (is_mode, mode) in [
            (self.local, EnvMode::LOCAL),
            (self.function, EnvMode::FUNCTION),
            (self.global, EnvMode::GLOBAL),
            (self.exportv, EnvMode::EXPORT),
            (self.unexport, EnvMode::UNEXPORT),
            (self.universal, EnvMode::UNIVERSAL),
            (self.pathvar, EnvMode::PATHVAR),
            (self.unpathvar, EnvMode::UNPATHVAR),
        ] {
            if is_mode {
                scope |= mode;
            }
        }
        scope
    }

    fn parse(
        args: &mut [WString],
        parser: &Parser,
        streams: &mut IoStreams<'_>,
    ) -> Result<(Options, usize), Option<c_int>> {
        let cmd = &args[0];

        /// Values used for long-only options.
        const PATH_ARG: char = 1 as char;
        const UNPATH_ARG: char = 2 as char;
        // Variables used for parsing the argument list. This command is atypical in using the "+"
        // (REQUIRE_ORDER) option for flag parsing. This is not typical of most fish commands. It means
        // we stop scanning for flags when the first non-flag argument is seen.
        const SHORT_OPTS: &wstr = L!("+:LSUaefghlnpqux");
        const LONG_OPTS: &[woption] = &[
            wopt(L!("export"), no_argument, 'x'),
            wopt(L!("global"), no_argument, 'g'),
            wopt(L!("function"), no_argument, 'f'),
            wopt(L!("local"), no_argument, 'l'),
            wopt(L!("erase"), no_argument, 'e'),
            wopt(L!("names"), no_argument, 'n'),
            wopt(L!("unexport"), no_argument, 'u'),
            wopt(L!("universal"), no_argument, 'U'),
            wopt(L!("long"), no_argument, 'L'),
            wopt(L!("query"), no_argument, 'q'),
            wopt(L!("show"), no_argument, 'S'),
            wopt(L!("append"), no_argument, 'a'),
            wopt(L!("prepend"), no_argument, 'p'),
            wopt(L!("path"), no_argument, PATH_ARG),
            wopt(L!("unpath"), no_argument, UNPATH_ARG),
            wopt(L!("help"), no_argument, 'h'),
        ];

        let mut opts = Self::default();

        let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, args);
        while let Some(c) = w.wgetopt_long() {
            match c {
                'a' => opts.append = true,
                'e' => {
                    opts.erase = true;
                    opts.preserve_failure_exit_status = false;
                }
                'f' => opts.function = true,
                'g' => opts.global = true,
                'h' => opts.print_help = true,
                'l' => opts.local = true,
                'n' => {
                    opts.list = true;
                    opts.preserve_failure_exit_status = true;
                }
                'p' => opts.prepend = true,
                'q' => {
                    opts.query = true;
                    opts.preserve_failure_exit_status = true;
                }
                'x' => opts.exportv = true,
                'u' => opts.unexport = true,
                PATH_ARG => opts.pathvar = true,
                UNPATH_ARG => opts.unpathvar = true,
                'U' => opts.universal = true,
                'L' => opts.shorten_ok = false,
                'S' => {
                    opts.show = true;
                    opts.preserve_failure_exit_status = false;
                }
                ':' => {
                    builtin_missing_argument(parser, streams, cmd, &args[w.woptind - 1], false);
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    let optind = w.woptind;
                    // implicit drop(w); here
                    if args[optind - 1].starts_with("-o") {
                        // TODO: translate this
                        streams.err.appendln(
                            L!("Fish does not have shell options. See `help fish-for-bash-users`.")
                        );
                        if optind < args.len() {
                            if args[optind] == "vi" {
                                // Tell the vi users how to get what they need.
                                streams
                                    .err
                                    .appendln(L!("To enable vi-mode, run `fish_vi_key_bindings`."));
                            } else if args[optind] == "ed" {
                                // This should be enough for make ed users feel at home
                                streams.err.append(L!("?\n?\n?\n"));
                            }
                        }
                    }

                    builtin_unknown_option(parser, streams, cmd, &args[optind - 1], false);
                    return Err(STATUS_INVALID_ARGS);
                }
                _ => {
                    panic!("unexpected retval from wgetopt_long");
                }
            }
        }

        let optind = w.woptind;

        if opts.print_help {
            builtin_print_help(parser, streams, cmd);
            return Err(STATUS_CMD_OK);
        }

        Self::validate(&opts, args, parser, streams)?;

        Ok((opts, optind))
    }

    fn validate(
        opts: &Self,
        args: &[WString],
        parser: &Parser,
        streams: &mut IoStreams<'_>,
    ) -> Result<(), Option<c_int>> {
        let cmd = &args[0];
        // Can't query and erase or list.
        if opts.query && (opts.erase || opts.list) {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // We can't both list and erase variables.
        if opts.erase && opts.list {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one scope...
        if [opts.local, opts.function, opts.global, opts.universal]
            .into_iter()
            .filter(|b| *b)
            .count()
            > 1
            // ..unless we are erasing a variable, in which case we can erase from several in one go.
            && !opts.erase
        {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one export status.
        if opts.exportv && opts.unexport {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one path status.
        if opts.pathvar && opts.unpathvar {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Trying to erase and (un)export at the same time doesn't make sense.
        if opts.erase && (opts.exportv || opts.unexport) {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // The --show flag cannot be combined with any other flag.
        if opts.show
            && (opts.local
                || opts.function
                || opts.global
                || opts.erase
                || opts.list
                || opts.exportv
                || opts.universal)
        {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        if args.len() == 0 && opts.erase {
            streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        Ok(())
    }
}

// Check if we are setting a uvar and a global of the same name exists. See
// https://github.com/fish-shell/fish-shell/issues/806

// PORTING: maybe just add `thiserror`?
enum EnvArrayParseError {
    InvalidIndex(WString),
}

impl std::fmt::Display for EnvArrayParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(
            f,
            "{}",
            match self {
                EnvArrayParseError::InvalidIndex(varname) =>
                    wgettext_fmt!("%ls: Invalid index starting at '%ls'\n", "set", varname)
                        .to_string(),
            }
        )
    }
}

#[derive(Debug, Default)]
struct SplitVar<'a> {
    name: &'a wstr,
    var: Option<EnvVar>,
    indexes: Vec<usize>,
}

/// Extract indexes from an argument of the form `var_name[index1 index2...]`.
/// The argument \p arg is split into a variable name and list of indexes, which is returned by
/// reference. Indexes are "expanded" in the sense that range expressions .. and negative values are
/// handled.
///
/// Returns:
///   a split var on success, none() on error, in which case an error will have been printed.
///   If no index is found, this leaves indexes empty.
fn split_var_and_indexes<'a>(
    arg: &'a wstr,
    mode: EnvMode,
    vars: &dyn Environment,
) -> Result<SplitVar<'a>, EnvArrayParseError> {
    // PORTING: this should probably be made reusable in some way?

    let mut res = SplitVar::default();
    let open_bracket = arg.find_char('[');
    res.name = open_bracket.map(|b| arg.slice_from(b)).unwrap_or(arg);
    res.var = vars.get(res.name);
    let Some(open_bracket) = open_bracket else {
        // Common case of no bracket
        return Ok(res);
    };
    
    // REMINDER: we are parsing 1-based indexes
    // PORTING: this should return an error if there is no var,
    // we need the length of the array to validate the indexes
    let len = res.var.as_ref().map(|v| v.as_list().len()).unwrap_or_default();
    let mut c = arg.slice_from(open_bracket + 1);

    while c.char_at(0) != ']' {
        let mut consumed = 0;
        let l_ind: isize;
        if res.indexes.is_empty() && c.char_at(0) == '.' && c.char_at(1) == '.' {
            // If we are at the first index expression, a missing start-index means the range starts
            // at the first item.
            l_ind = 1; // first index
        } else {
            l_ind = wcstoi_partial(c, crate::wutil::Options::default(), &mut consumed)
                .map_err(|_| EnvArrayParseError::InvalidIndex(res.name.to_owned()))?;
            c = c.slice_from(consumed);
        }

        // Convert negative index to a positive index.
        let signed_to_unsigned = |i: isize| -> Result<usize, EnvArrayParseError> {
            match i.try_into() {
                Ok(idx) => Ok(idx),
                Err(_) => {
                    let Some(idx) = len.checked_add_signed(l_ind).map(|i| i + 1) else {
                        // PORTING: this was not handled in C++, l_ind was just kept as an `i64`
                        // this should have a separate error
                        // also: in the case of `var[1 1]` we should probably either de-duplicate 
                        // or make that a hard error.
                        // the behavior of `..` is also somewhat weird
                        return Err(EnvArrayParseError::InvalidIndex(res.name.to_owned()));
                    };
                    Ok(idx)
                }
            }
        };

        let l_ind: usize = signed_to_unsigned(l_ind)?;

        if c.char_at(0) == '.' && c.char_at(1) == '.' {
            // If we are at the last index range expression, a missing end-index means the range
            // spans until the last item.
            c = c.slice_from(2);
            let l_ind2: isize;
            // If we are at the last index range expression, a missing end-index means the range
            // spans until the last item.
            if res.indexes.is_empty() && c.char_at(0) == ']' {
                l_ind2 = -1;
            } else {
                // PORTING: this needs an error-message
                l_ind2 = wcstoi_partial(c, crate::wutil::Options::default(), &mut consumed)
                    .map_err(|_| EnvArrayParseError::InvalidIndex(res.name.to_owned()))?;
                c = c.slice_from(consumed);
            }

            let l_ind2: usize = signed_to_unsigned(l_ind2)?;
            if l_ind < l_ind2  {
                res.indexes.extend(l_ind..=l_ind2);
            } else {
                res.indexes.extend(l_ind2..=l_ind);
            }
        } else {
            res.indexes.push(l_ind);
        }
    }

    Ok(res)
}

fn query(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    todo!()
}

fn erase(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    todo!()
}

fn list(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    use std::pin::Pin;

    let names_only = opts.list;
    let mut names = parser.vars().get_names(opts.scope());
    names.sort();

    for key in names {
        let mut out = key.clone();
        
        if !names_only {
            let mut val = WString::new();
            if opts.shorten_ok && key == "history" {
                todo!("history")
            }
        }
    }

    STATUS_CMD_OK
}

fn show(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    todo!()
}

fn set_internal(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    todo!()
}

/// The block builtin, used for temporarily blocking events.
pub fn set(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {

    let cmd = &args[0];

    let opts = match Options::parse(args, parser, streams) {
        Ok((opts, _)) => opts,
        Err(err) => return err,
    };

    let retval = if opts.query {
        query(cmd, &opts, parser, streams, args)
    } else if opts.erase {
        erase(cmd, &opts, parser, streams, args)
    } else if opts.list {
        list(cmd, &opts, parser, streams, args)
    } else if opts.show {
        show(cmd, &opts, parser, streams, args)
    } else if args.len() == 0 {
        list(cmd, &opts, parser, streams, args)
    } else {
        set_internal(cmd, &opts, parser, streams, args)
    };

    if retval == STATUS_CMD_OK && opts.preserve_failure_exit_status {
        return None;
    }

    return retval;
}
