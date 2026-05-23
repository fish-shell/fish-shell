use super::prelude::*;
use crate::{
    builtins::error::Error,
    common::valid_var_name,
    env::{EnvMode, EnvStackSetResult, EnvVar, EnvVarFlags, Environment, INHERITED_VARS},
    err_fmt, err_str,
    event::{self, Event},
    expand::{expand_escape_string, expand_escape_variable},
    fds::{CowFd, FdTryFrom as _, close, dup_heightened},
    history::{History, history_id},
    parse_execution::varname_error,
    parser::ParserEnvSetMode,
    redirection::RedirectionMode,
    wutil::wcstoi::wcstoi_partial,
};
use fish_common::{EscapeFlags, EscapeStringStyle, escape, escape_string, help_section};
use fish_widestring::{ELLIPSIS_CHAR, wcs2osstring};
use std::{
    fs::File,
    io::ErrorKind,
    os::fd::{AsRawFd as _, BorrowedFd, IntoRawFd as _, OwnedFd},
};

const CLOSE_FD_ERROR: &str = "Failed to close descriptor `%s[%d]` (%s): %s";

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
struct FdMode {
    redirection_mode: RedirectionMode,
    reuse: bool,
    cloexec: bool,
}

#[derive(Debug, Clone, Eq, PartialEq)]
enum FdArg {
    /// no `--fd` option used
    None,
    /// `--fd`
    NoValue,
    /// `--fd=<value>`
    Mode(FdMode),
    /// Invalid value for  `--fd=<value>`.
    /// We don't know yet if having a value is Ok or not in the first place,
    /// so store the error here to be reported later as needed
    ValueError(WString),
}

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
    no_event: bool,
    fd: FdArg,
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
            no_event: false,
            fd: FdArg::None,
        }
    }
}

impl Options {
    fn env_mode(&self) -> EnvMode {
        let mut scope = EnvMode::empty();
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
        cmd: &wstr,
        args: &mut [&wstr],
        parser: &mut Parser,
        streams: &mut IoStreams,
    ) -> Result<Option<(Options, usize)>, ErrorCode> {
        /// Values used for long-only options.
        const PATH_ARG: char = 1 as char;
        const UNPATH_ARG: char = 2 as char;
        const NO_EVENT_ARG: char = 3 as char;
        const FD_ARG: char = 4 as char;
        // Variables used for parsing the argument list. This command is atypical in using the "+"
        // (REQUIRE_ORDER) option for flag parsing. This is not typical of most fish commands. It means
        // we stop scanning for flags when the first non-flag argument is seen.
        const SHORT_OPTS: &wstr = L!("+LSUaefghlnpqux");
        const LONG_OPTS: &[WOption] = &[
            wopt(L!("export"), NoArgument, 'x'),
            wopt(L!("global"), NoArgument, 'g'),
            wopt(L!("function"), NoArgument, 'f'),
            wopt(L!("local"), NoArgument, 'l'),
            wopt(L!("erase"), NoArgument, 'e'),
            wopt(L!("names"), NoArgument, 'n'),
            wopt(L!("unexport"), NoArgument, 'u'),
            wopt(L!("universal"), NoArgument, 'U'),
            wopt(L!("long"), NoArgument, 'L'),
            wopt(L!("query"), NoArgument, 'q'),
            wopt(L!("show"), NoArgument, 'S'),
            wopt(L!("append"), NoArgument, 'a'),
            wopt(L!("prepend"), NoArgument, 'p'),
            wopt(L!("path"), NoArgument, PATH_ARG),
            wopt(L!("unpath"), NoArgument, UNPATH_ARG),
            wopt(L!("no-event"), NoArgument, NO_EVENT_ARG),
            wopt(L!("fd"), OptionalArgument, FD_ARG),
            wopt(L!("help"), NoArgument, 'h'),
        ];

        let mut opts = Self::default();

        let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, args);
        while let Some(c) = w.next_opt() {
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
                    opts.preserve_failure_exit_status = false;
                }
                'p' => opts.prepend = true,
                'q' => {
                    opts.query = true;
                    opts.preserve_failure_exit_status = false;
                }
                'x' => opts.exportv = true,
                'u' => opts.unexport = true,
                PATH_ARG => opts.pathvar = true,
                UNPATH_ARG => opts.unpathvar = true,
                NO_EVENT_ARG => opts.no_event = true,
                'U' => opts.universal = true,
                'L' => opts.shorten_ok = false,
                'S' => {
                    opts.show = true;
                    opts.preserve_failure_exit_status = false;
                }
                FD_ARG => opts.fd = Self::parse_fd_arg(w.woptarg),
                ':' => {
                    builtin_missing_argument(
                        parser,
                        streams,
                        cmd,
                        None,
                        args[w.wopt_index - 1],
                        false,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                ';' => {
                    builtin_unexpected_argument(
                        parser,
                        streams,
                        cmd,
                        args[w.wopt_index - 1],
                        false,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    // Specifically detect `set -o` because people might be bringing over bashisms.
                    let optind = w.wopt_index;
                    // implicit drop(w); here
                    if args[optind - 1].starts_with("-o") {
                        let mut err = err_fmt!(
                            "fish does not have shell options. See `help %s`.",
                            help_section!("fish_for_bash_users")
                        );

                        if optind < args.len() {
                            err.append_assign_to_msg('\n');
                            if args[optind] == "vi" {
                                // Tell the vi users how to get what they need.
                                err.append_assign_to_msg(wgettext!(
                                    "To enable vi-mode, run `fish_vi_key_bindings`."
                                ));
                            } else if args[optind] == "ed" {
                                // This should be enough for make ed users feel at home
                                err.append_assign_to_msg(L!("?\n?\n?\n"));
                            }
                        }
                        err.finish(streams);
                    }

                    builtin_unknown_option(parser, streams, cmd, args[optind - 1], false);
                    return Err(STATUS_INVALID_ARGS);
                }
                _ => {
                    panic!("unexpected retval from WGetopter");
                }
            }
        }

        let optind = w.wopt_index;

        if opts.print_help {
            builtin_print_help(parser, streams, cmd);
            return Ok(None);
        }

        Self::validate(&opts, cmd, args, optind, parser, streams)?;

        Ok(Some((opts, optind)))
    }

    fn parse_fd_arg(mode: Option<&wstr>) -> FdArg {
        let Some(mode) = mode else {
            return FdArg::NoValue;
        };

        let remove_if = |vec: &mut Vec<&wstr>, key| {
            if let Some(idx) = vec.iter().position(|s| s == key) {
                vec.remove(idx);
                true
            } else {
                false
            }
        };
        let mut opt_args = mode
            .split(',')
            .filter(|s| !s.is_empty())
            .collect::<Vec<_>>();
        opt_args.sort_unstable();
        opt_args.dedup();
        let ro = remove_if(&mut opt_args, L!("ro"));
        let wo = remove_if(&mut opt_args, L!("wo"));
        let r#try = remove_if(&mut opt_args, L!("try"));
        let append = remove_if(&mut opt_args, L!("append"));
        let reuse = remove_if(&mut opt_args, L!("reuse"));
        let cloexec = remove_if(&mut opt_args, L!("cloexec"));

        if !opt_args.is_empty() {
            return FdArg::ValueError(wgettext_fmt!(
                Error::INVALID_OPT_ARGS,
                "--fd",
                opt_args.iter().map(|s| sprintf!("`%s`", s)).join(L!(", "))
            ));
        }

        let redirection_mode = match (ro, wo, r#try, append) {
            (false, true, false, false) => RedirectionMode::Overwrite,
            (false, _, false, true) => RedirectionMode::Append,
            (true, false, false, false) => RedirectionMode::Input,
            (true, false, true, false) => RedirectionMode::TryInput,
            (false, true, true, false) => RedirectionMode::NoClob,
            (false, false, _, false) => {
                return FdArg::ValueError(wgettext_fmt!("--fd: requires `ro`, `wo` or `append`"));
            }
            (true, true, _, _) => {
                return FdArg::ValueError(wgettext_fmt!(
                    Error::INVALID_OPT_ARG_COMBO_2,
                    "--fd",
                    "ro",
                    "wo"
                ));
            }
            (true, false, _, true) => {
                return FdArg::ValueError(wgettext_fmt!(
                    Error::INVALID_OPT_ARG_COMBO_2,
                    "--fd",
                    "ro",
                    "append"
                ));
            }
            (false, _, true, true) => {
                return FdArg::ValueError(wgettext_fmt!(
                    Error::INVALID_OPT_ARG_COMBO_2,
                    "--fd",
                    "try",
                    "append"
                ));
            }
        };

        FdArg::Mode(FdMode {
            redirection_mode,
            reuse,
            cloexec,
        })
    }

    fn validate(
        opts: &Self,
        cmd: &wstr,
        args: &[&wstr],
        optind: usize,
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> Result<(), ErrorCode> {
        // Can't query and erase or list.
        if opts.query && (opts.erase || opts.list) {
            err_str!(Error::INVALID_OPT_COMBO)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        // We can't both list and erase variables.
        if opts.erase && opts.list {
            err_str!(Error::INVALID_OPT_COMBO)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
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
            err_str!(Error::MULTIPLE_SCOPES)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one export status.
        if opts.exportv && opts.unexport {
            err_str!(Error::EXPORT_UNEXPORT)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one path status.
        if opts.pathvar && opts.unpathvar {
            err_str!(Error::PATH_UNPATH)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        // Trying to erase and (un)export at the same time doesn't make sense.
        if opts.erase && (opts.exportv || opts.unexport) {
            err_str!(Error::INVALID_OPT_COMBO)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
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
            err_str!(Error::INVALID_OPT_COMBO)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        // --fd and its value are only valid in specific contexts
        if opts.query || opts.list || opts.show {
            if opts.fd != FdArg::None {
                err_str!(Error::INVALID_OPT_COMBO)
                    .cmd(cmd)
                    .full_trailer(parser)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
        } else if opts.erase {
            // The fact that there is a value is the error, it doesn't matter
            // if that value itself is valid or not
            if matches!(opts.fd, FdArg::Mode(_) | FdArg::ValueError(_)) {
                err_str!("--fd: option does not take an argument when erasing a variable")
                    .cmd(cmd)
                    .full_trailer(parser)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
        } else {
            if opts.fd == FdArg::NoValue {
                err_str!("--fd: option requires an argument when setting a variable")
                    .cmd(cmd)
                    .full_trailer(parser)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
            if let FdArg::ValueError(err) = &opts.fd {
                err_raw!(err).cmd(cmd).full_trailer(parser).finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
        }

        if args.len() == optind && opts.erase {
            err_fmt!(Error::MISSING_OPT_ARG, L!("--erase"))
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        Ok(())
    }
}

// Check if we are setting a uvar and a global of the same name exists. See
// https://github.com/fish-shell/fish-shell/issues/806
fn warn_if_uvar_shadows_global(
    cmd: &wstr,
    opts: &Options,
    dest: &wstr,
    streams: &mut IoStreams,
    parser: &Parser,
) {
    if opts.universal
        && parser.is_interactive()
        && parser.vars().getf(dest, EnvMode::GLOBAL).is_some()
    {
        err_fmt!(
            "successfully set universal '%s'; but a global by that name shadows it",
            dest
        )
        .cmd(cmd)
        .finish(streams);
    }
}

fn handle_env_return(retval: EnvStackSetResult, cmd: &wstr, key: &wstr, streams: &mut IoStreams) {
    match retval {
        EnvStackSetResult::Ok => (),
        EnvStackSetResult::Perm => {
            err_fmt!("Tried to change the read-only variable '%s'", key)
                .cmd(cmd)
                .finish(streams);
        }
        EnvStackSetResult::Scope => {
            err_fmt!(
                "Tried to modify the special variable '%s' with the wrong scope",
                key
            )
            .cmd(cmd)
            .finish(streams);
        }
        EnvStackSetResult::Invalid => {
            err_fmt!(
                "Tried to modify the special variable '%s' to an invalid value",
                key
            )
            .cmd(cmd)
            .finish(streams);
        }
        EnvStackSetResult::NotFound => {
            // Only variable deletion can return a `NotFound` error, but that case is explicitly silenced
            unreachable!("variable not found");
        }
    }
}

/// Call vars.set. If this is a path variable, e.g. PATH, validate the elements. On error, print a
/// description of the problem to stderr.
fn env_set_reporting_errors(
    cmd: &wstr,
    opts: &Options,
    key: &wstr,
    mode: EnvMode,
    list: Vec<WString>,
    streams: &mut IoStreams,
    parser: &mut Parser,
) -> EnvStackSetResult {
    let mode = ParserEnvSetMode::user(mode);
    let retval = if opts.no_event {
        parser.set_var(key, mode, list)
    } else {
        parser.set_var_and_fire(key, mode, list)
    };
    // If this returned OK, the parser already fired the event.
    handle_env_return(retval, cmd, key, streams);
    retval
}

// PORTING: maybe just add `thiserror`?
enum EnvArrayParseError {
    InvalidIndex(WString),
}

impl std::fmt::Display for EnvArrayParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(
            f,
            "{}\n",
            match self {
                EnvArrayParseError::InvalidIndex(varname) =>
                    err_fmt!("Invalid index starting at '%s'", varname)
                        .cmd(L!("set"))
                        .to_string(),
            }
        )
    }
}

#[derive(Debug, Default)]
struct SplitVar<'a> {
    varname: &'a wstr,
    var: Option<EnvVar>,
    indexes: Vec<isize>,
}

impl<'a> SplitVar<'a> {
    /// Return the number of elements in our variable, or 0 if missing.
    fn varsize(&self) -> usize {
        self.var.as_ref().map_or(0, |var| var.as_list().len())
    }
}

/// Extract indexes from an argument of the form `var_name[index1 index2...]`.
/// The argument `arg` is split into a variable name and list of indexes, which is returned by
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
    streams: &mut IoStreams,
) -> Option<SplitVar<'a>> {
    match split_var_and_indexes_internal(arg, mode, vars) {
        Ok(split) => Some(split),
        Err(env_array_parse_error) => {
            streams.err.append(&format!("{env_array_parse_error}"));
            None
        }
    }
}

fn split_var_and_indexes_internal<'a>(
    arg: &'a wstr,
    mode: EnvMode,
    vars: &dyn Environment,
) -> Result<SplitVar<'a>, EnvArrayParseError> {
    let mut res = SplitVar::default();
    let open_bracket = arg.find_char('[');
    res.varname = open_bracket.map_or(arg, |b| &arg[..b]);
    res.var = vars.getf(res.varname, mode);
    let Some(open_bracket) = open_bracket else {
        // Common case of no bracket
        return Ok(res);
    };

    // We need the length of the array to validate the indexes.
    let len = res
        .var
        .as_ref()
        .map(|v| v.as_list().len())
        .unwrap_or_default();
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
                .map_err(|_| EnvArrayParseError::InvalidIndex(c.slice_from(consumed).to_owned()))?;
            c = c.slice_from(consumed);
            // Skip trailing whitespace.
            while !c.is_empty() && c.char_at(0).is_whitespace() {
                c = c.slice_from(1);
            }
        }

        // Convert negative index to a positive index.
        let convert_negative_index = |i: isize| -> isize {
            if i >= 0 {
                i
            } else {
                isize::try_from(len).unwrap() + i + 1
            }
        };

        let l_ind = convert_negative_index(l_ind);

        if c.char_at(0) == '.' && c.char_at(1) == '.' {
            // If we are at the last index range expression, a missing end-index means the range
            // spans until the last item.
            c = c.slice_from(2);
            let l_ind2: isize;
            // If we are at the last index range expression, a missing end-index means the range
            // spans until the last item.
            let tmp = consumed;
            if res.indexes.is_empty() && c.char_at(0) == ']' {
                l_ind2 = -1;
            } else {
                l_ind2 = wcstoi_partial(c, crate::wutil::Options::default(), &mut consumed)
                    .map_err(|_| {
                        EnvArrayParseError::InvalidIndex(c.slice_from(consumed - tmp).to_owned())
                    })?;
                c = c.slice_from(consumed);
                // Skip trailing whitespace.
                while !c.is_empty() && c.char_at(0).is_whitespace() {
                    c = c.slice_from(1);
                }
            }

            let l_ind2 = convert_negative_index(l_ind2);
            if l_ind < l_ind2 {
                res.indexes.extend(l_ind..=l_ind2);
            } else {
                res.indexes.extend((l_ind2..=l_ind).rev());
            }
        } else {
            res.indexes.push(l_ind);
        }
    }

    Ok(res)
}

fn maybe_close_fd_var(fd_str: &wstr) -> std::io::Result<()> {
    if let Ok(fd) = BorrowedFd::fd_try_from(fd_str) {
        if fd.as_raw_fd() <= 2 {
            // stdin/ou/err are always assumed to exists, so to avoid errors
            // (and worse, targeting some random file), redirect from `/dev/null`
            // instead
            let file = File::options().read(true).write(true).open("/dev/null")?;
            unsafe { nix::unistd::dup2_raw(file, fd.as_raw_fd()) }?;
        } else {
            close(fd)?;
        }
    }
    Ok(())
}

/// Given a list of values and 1-based indexes, return a new list with those elements removed.
/// Note this deliberately accepts both args by value, as it modifies them both.
fn erased_at_indexes(
    var: &wstr,
    mut input: Vec<WString>,
    mut indexes: Vec<isize>,
    close_fd: bool,
) -> Vec<WString> {
    // Sort our indexes into *descending* order.
    indexes.sort_by_key(|&index| std::cmp::Reverse(index));

    // Remove duplicates.
    indexes.dedup();

    // Now when we walk indexes, we encounter larger indexes first.
    for idx in indexes {
        let Ok(idx) = usize::try_from(idx) else {
            continue;
        };
        if idx > 0 && idx <= input.len() {
            // One-based indexing!
            let str = input.remove(idx - 1);
            if close_fd {
                if let Err(err) = maybe_close_fd_var(&str) {
                    flogf!(warning, CLOSE_FD_ERROR, var, idx, str, err.to_string());
                }
            }
        }
    }
    input
}

/// Print the names of all environment variables in the scope. It will include the values unless the
/// `set --names` flag was used.
fn list(opts: &Options, parser: &Parser, streams: &mut IoStreams) -> BuiltinResult {
    let names_only = opts.list;
    let mut names = parser.vars().get_names(opts.env_mode());
    names.sort();

    for key in names {
        let mut out = key.clone();

        if !names_only {
            let mut val = WString::new();
            if opts.shorten_ok && key == "history" {
                let history = History::new(history_id(parser.vars()));
                for i in 1..history.size() {
                    if val.len() >= 64 {
                        break;
                    }
                    if i > 1 {
                        val.push(' ');
                    }
                    val += &expand_escape_string(history.item_at_index(i).unwrap().str())[..];
                }
            } else if let Some(var) = parser.vars().getf_unless_empty(&key, opts.env_mode()) {
                val = expand_escape_variable(&var);
            }
            if !val.is_empty() {
                let mut shorten = false;
                if opts.shorten_ok && val.len() > 64 {
                    shorten = true;
                    val.truncate(60);
                }
                out.push(' ');
                out.push_utfstr(&val);

                if shorten {
                    out.push(ELLIPSIS_CHAR);
                }
            }
        }

        out.push('\n');
        streams.out.append(&out);
    }

    Ok(SUCCESS)
}

fn query(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams,
    args: &[&wstr],
) -> BuiltinResult {
    let mut retval = 0;
    let mode = opts.env_mode();

    // No variables given, this is an error.
    // 255 is the maximum return code we allow.
    if args.is_empty() {
        return Err(STATUS_NO_VARIABLES_GIVEN);
    }

    for arg in args {
        let Some(split) = split_var_and_indexes(arg, mode, parser.vars(), streams) else {
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_CMD_ERROR);
        };

        if split.indexes.is_empty() {
            // No indexes, just increment if our variable is missing.
            if split.var.is_none() {
                retval += 1;
            }
        } else {
            // Increment for every index out of range.
            let varsize = split.varsize();
            for idx in split.indexes {
                if idx < 1 || usize::try_from(idx).unwrap() > varsize {
                    retval += 1;
                }
            }
        }
    }

    BuiltinResult::from_dynamic(retval)
}

fn show_scope(var_name: &wstr, scope: EnvMode, streams: &mut IoStreams, vars: &dyn Environment) {
    let scope_name = match scope {
        EnvMode::LOCAL => L!("local"),
        EnvMode::GLOBAL => L!("global"),
        EnvMode::UNIVERSAL => L!("universal"),
        _ => panic!("invalid scope"),
    };
    let Some(var) = vars.getf(var_name, scope) else {
        return;
    };

    let exportv = if var.exports() {
        wgettext!("exported")
    } else {
        wgettext!("unexported")
    };
    let pathvarv = if var.is_pathvar() {
        let mut pathvarv = L!(" ").to_owned();
        pathvarv.push_utfstr(wgettext!("a path variable"));
        pathvarv
    } else {
        L!("").to_owned()
    };
    let vals = var.as_list();
    streams.out.append(&wgettext_fmt!(
        "$%s: set in %s scope, %s,%s with %d elements",
        var_name,
        scope_name,
        exportv,
        pathvarv,
        vals.len()
    ));
    // HACK: PWD can be set, depending on how you ask.
    // For our purposes it's read-only.
    if EnvVar::flags_for(var_name).contains(EnvVarFlags::READ_ONLY) {
        streams
            .out
            .appendln(" ".chars().chain(wgettext!("(read-only)").chars()));
    } else {
        streams.out.append('\n');
    }

    for i in 0..vals.len() {
        if vals.len() > 100 {
            if i == 50 {
                // print a mid-line ellipsis because we are eliding lines not words
                streams.out.appendln(L!("\u{22EF}")); // ⋯
            }
            if i >= 50 && i < vals.len() - 50 {
                continue;
            }
        }
        let value = &vals[i];
        let escaped_val = escape_string(
            value,
            EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
        );
        streams
            .out
            .append(&sprintf!("$%s[%d]: |%s|\n", var_name, i + 1, &escaped_val));
    }
}

/// Show mode. Show information about the named variable(s).
fn show(cmd: &wstr, parser: &Parser, streams: &mut IoStreams, args: &[&wstr]) -> BuiltinResult {
    localizable_consts! {
        ORIGINALLY_INHERITED_AS
        "$%s: originally inherited as |%s|"
    }
    let vars = parser.vars();
    if args.is_empty() {
        // show all vars
        let mut names = vars.get_names(EnvMode::empty());
        names.sort();
        for name in names {
            if name == "history" {
                continue;
            }
            show_scope(&name, EnvMode::LOCAL, streams, vars);
            show_scope(&name, EnvMode::GLOBAL, streams, vars);
            show_scope(&name, EnvMode::UNIVERSAL, streams, vars);

            // Show the originally imported value as a debugging aid.
            if let Some(inherited) = INHERITED_VARS.get().unwrap().get(&name) {
                let escaped_val = escape_string(
                    inherited,
                    EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
                );
                streams
                    .out
                    .appendln(&wgettext_fmt!(ORIGINALLY_INHERITED_AS, name, escaped_val));
            }
        }
    } else {
        for arg in args.iter().copied() {
            let bracket = arg.find(L!("["));
            let arg = if let Some(idx) = bracket {
                &arg[..idx]
            } else {
                arg
            };

            if !valid_var_name(arg) {
                varname_error(cmd, arg).full_trailer(parser).finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }

            if bracket.is_some() {
                err_str!("`set --show` does not allow slices with the var names")
                    .cmd(cmd)
                    .full_trailer(parser)
                    .finish(streams);
                return Err(STATUS_CMD_ERROR);
            }

            show_scope(arg, EnvMode::LOCAL, streams, vars);
            show_scope(arg, EnvMode::GLOBAL, streams, vars);
            show_scope(arg, EnvMode::UNIVERSAL, streams, vars);
            if let Some(inherited) = INHERITED_VARS.get().unwrap().get(arg) {
                let escaped_val = escape_string(
                    inherited,
                    EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
                );
                streams
                    .out
                    .appendln(&wgettext_fmt!(ORIGINALLY_INHERITED_AS, arg, escaped_val));
            }
        }
    }

    Ok(SUCCESS)
}

fn erase(
    cmd: &wstr,
    opts: &Options,
    parser: &mut Parser,
    streams: &mut IoStreams,
    args: &[&wstr],
) -> BuiltinResult {
    let mut ret = Ok(SUCCESS);
    let close_fd = opts.fd == FdArg::NoValue;

    let mut erase_with_mode = |mode| {
        for arg in args {
            let Some(split) = split_var_and_indexes(arg, mode, parser.vars(), streams) else {
                builtin_print_error_trailer(parser, streams.err, cmd);
                return Err(STATUS_CMD_ERROR);
            };

            if !valid_var_name(split.varname) {
                varname_error(cmd, split.varname)
                    .full_trailer(parser)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
            let retval;
            if split.indexes.is_empty() {
                // With --fd, try to close the file descriptors when the values are numbers
                if close_fd {
                    if let Some(var) = parser.vars().get(split.varname) {
                        // `rev()` to be consistent with split indexes when reporting errors
                        var.as_list()
                            .iter()
                            .enumerate()
                            .rev()
                            .for_each(|(idx, fd_str)| {
                                if let Err(err) = maybe_close_fd_var(fd_str) {
                                    flogf!(
                                        warning,
                                        CLOSE_FD_ERROR,
                                        split.varname,
                                        idx + 1,
                                        fd_str,
                                        err.to_string()
                                    );
                                }
                            });
                    }
                }
                // unset the var
                retval = parser.remove_var(split.varname, ParserEnvSetMode::new(mode));
                // When a non-existent-variable is unset, return NotFound as $status
                // but do not emit any errors at the console as a compromise between user
                // friendliness and correctness.
                if retval != EnvStackSetResult::NotFound {
                    handle_env_return(retval, cmd, split.varname, streams);
                }
                if retval == EnvStackSetResult::Ok && !opts.no_event {
                    event::fire(parser, Event::variable_erase(split.varname.to_owned()));
                }
            } else {
                // remove just the specified indexes of the var
                let Some(var) = split.var else {
                    return Err(STATUS_CMD_ERROR);
                };
                let result = erased_at_indexes(
                    split.varname,
                    var.as_list().to_owned(),
                    split.indexes,
                    close_fd,
                );
                retval = env_set_reporting_errors(
                    cmd,
                    opts,
                    split.varname,
                    mode,
                    result,
                    streams,
                    parser,
                );
            }

            // Set $status to the last error value.
            // This is cheesy, but I don't expect this to be checked often.
            if retval != EnvStackSetResult::Ok {
                ret = retval.into();
            }
        }
        Ok(())
    };
    // `set -e` is allowed to be called with multiple scopes.
    let mode = opts.env_mode();
    let any_scope = EnvMode::ANY_SCOPE;
    let scopes = mode.intersection(any_scope);
    if scopes.is_empty() {
        erase_with_mode(mode)?;
    } else {
        // Historical behavior is to go from inner to outer, which may be relevant for scopes that
        // collide with the function scope (i.e. local and global).
        assert!(is_subsequence(
            scopes.iter(),
            [
                EnvMode::LOCAL,
                EnvMode::FUNCTION,
                EnvMode::GLOBAL,
                EnvMode::UNIVERSAL
            ]
            .into_iter()
        ));
        for scope in scopes.iter() {
            let other_scopes = any_scope - scope;
            erase_with_mode(mode - other_scopes)?;
        }
    }
    ret
}

fn is_subsequence<T: Eq>(
    mut lhs: impl Iterator<Item = T>,
    mut rhs: impl Iterator<Item = T>,
) -> bool {
    lhs.all(|l| {
        for r in rhs.by_ref() {
            if r == l {
                return true;
            }
        }
        false
    })
}

/// Convert `arg`, a path or a file descriptor, into a new file descriptor
/// The descriptor will either be heightened or use the same fd number as
/// `target_fd`.
/// The error can be `None` if we failed to create the file descriptor but it
/// was somewhat expected (e.g. with `RedirectionMode::NoClob` when `args` is
/// a path to an existing file)
fn arg_to_fd(
    arg: &wstr,
    mode: RedirectionMode,
    cloexec: bool,
    target_fd: Option<BorrowedFd>,
) -> Result<OwnedFd, Option<WString>> {
    let fd = if let Ok(fd) = BorrowedFd::fd_try_from(arg) {
        // Number => expect an fd we need to duplicate
        CowFd::Borrowed(fd)
    } else {
        // Not a number => expect a path we can open
        let options = mode.options().expect("invalid redirection mode");
        // Not using wopen_cloexec because we don't want to heightenize yet
        match options.open(wcs2osstring(arg)) {
            Ok(fd) => CowFd::Owned(std::os::fd::OwnedFd::from(fd)),
            Err(err) => match mode {
                RedirectionMode::TryInput => match options.open("/dev/null") {
                    Ok(fd) => CowFd::Owned(std::os::fd::OwnedFd::from(fd)),
                    Err(err) => {
                        return Err(Some(wgettext_fmt!(
                            "Failed to open `%s`: %s",
                            "/dev/null",
                            err.to_string()
                        )));
                    }
                },
                RedirectionMode::NoClob if err.kind() == ErrorKind::AlreadyExists => {
                    flogf!(warning, "`%s`: Already exists", arg);
                    return Err(None);
                }
                _ => {
                    return Err(Some(wgettext_fmt!(
                        "Failed to open `%s`: %s",
                        arg,
                        err.to_string()
                    )));
                }
            },
        }
    };
    match target_fd {
        None => match dup_heightened(fd, cloexec) {
            Ok(fd) => Ok(fd),
            Err(err) => Err(Some(wgettext_fmt!(
                "Failed to duplicate descriptor %s: %s",
                arg,
                err.to_string()
            ))),
        },
        Some(target_fd) => {
            unsafe { nix::unistd::dup2_raw(fd, target_fd.as_raw_fd()) }.map_err(|err| {
                Some(wgettext_fmt!(
                    "Failed to duplicate descriptor: %s",
                    err.to_string()
                ))
            })
        }
    }
}

fn print_fd_error_fd(err: Option<WString>) {
    if let Some(err) = err {
        flog!(warning, err);
    }
}

/// Return a list of new values for the variable `varname`, respecting the `opts`.
/// This handles the simple case where there are no indexes.
fn new_var_values(
    opts: &Options,
    varname: &wstr,
    argv: &[&wstr],
    vars: &dyn Environment,
) -> Vec<WString> {
    let fd_list = if let FdArg::Mode(mode) = opts.fd {
        argv.iter()
            .copied()
            .map(|s| {
                arg_to_fd(s, mode.redirection_mode, mode.cloexec, None).map_or_else(
                    |err| {
                        print_fd_error_fd(err);
                        WString::new()
                    },
                    |fd| fd.into_raw_fd().to_wstring(),
                )
            })
            .collect()
    } else {
        vec![]
    };

    let mut result = vec![];
    if !opts.prepend && !opts.append {
        // Not prepending or appending.
        if fd_list.is_empty() {
            result.extend(argv.iter().copied().map(|s| s.to_owned()));
        } else {
            result.extend(fd_list);
        }
    } else {
        // Note: when prepending or appending, we always use default scoping when fetching existing
        // values. For example:
        //   set --global var 1 2
        //   set --local --append var 3 4
        // This starts with the existing global variable, appends to it, and sets it locally.
        // So do not use the given variable: we must re-fetch it.
        // TODO: this races under concurrent execution.
        if let Some(existing) = vars.get(varname) {
            existing.as_list().clone_into(&mut result);
        }

        if opts.prepend {
            if fd_list.is_empty() {
                result.splice(0..0, argv.iter().copied().map(|arg| arg.to_owned()));
            } else {
                // We could move instead of clone when not also appending, but
                // the Rust compiler is not smart enough yet to not complain
                // about a moved `fd_list` regardless
                result.splice(0..0, fd_list.iter().cloned());
            }
        }

        if opts.append {
            if fd_list.is_empty() {
                result.extend(argv.iter().copied().map(|s| s.to_owned()));
            } else {
                result.extend(fd_list);
            }
        }
    }
    result
}

/// This handles the more difficult case of setting individual slices of a var.
fn new_var_values_by_index(opts: &Options, split: &SplitVar, argv: &[&wstr]) -> Vec<WString> {
    assert_eq!(
        argv.len(),
        split.indexes.len(),
        "Must have the same number of indexes as arguments"
    );

    // Inherit any existing values.
    // Note unlike the append/prepend case, we start with a variable in the same scope as we are
    // setting.
    let mut result = split
        .var
        .as_ref()
        .map(EnvVar::as_list)
        .unwrap_or_default()
        .to_owned();

    // For each (index, argument) pair, set the element in our `result` to the replacement string.
    // Extend the list with empty strings as needed. The indexes are 1-based.
    for (i, arg) in argv.iter().copied().enumerate() {
        let lidx = usize::try_from(split.indexes[i]).unwrap();
        assert!(lidx >= 1, "idx should have been verified in range already");
        // Convert from 1-based to 0-based.
        let idx = lidx - 1;
        // Extend as needed with empty strings.
        if idx >= result.len() {
            result.resize(idx + 1, WString::new());
        }

        if let FdArg::Mode(mode) = opts.fd {
            let fd_target = if mode.reuse {
                BorrowedFd::fd_try_from(&result[idx]).ok()
            } else {
                None
            };
            if let Some(fd_target) = fd_target {
                match arg_to_fd(arg, mode.redirection_mode, mode.cloexec, Some(fd_target)) {
                    Ok(fd) => {
                        // We don't want the newly created `OwnedFd` to
                        // close the socket when done.
                        let _ = fd.into_raw_fd();

                        // `result[idx]` value is unchanged
                    }
                    Err(err) => {
                        print_fd_error_fd(err);
                        result[idx].clear();
                    }
                }
            } else {
                result[idx] = arg_to_fd(arg, mode.redirection_mode, mode.cloexec, None)
                    .map_or_else(
                        |err| {
                            print_fd_error_fd(err);
                            WString::new()
                        },
                        |fd| fd.into_raw_fd().to_wstring(),
                    );
            }
        } else {
            result[idx] = arg.into();
        }
    }
    result
}

/// Set a variable.
fn set_internal(
    cmd: &wstr,
    opts: &Options,
    parser: &mut Parser,
    streams: &mut IoStreams,
    argv: &[&wstr],
) -> BuiltinResult {
    if argv.is_empty() {
        err_fmt!(Error::MIN_ARG_COUNT, 1, 0)
            .cmd(cmd)
            .full_trailer(parser)
            .finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    let mode = opts.env_mode();
    let var_expr = argv[0];
    let argv = &argv[1..];

    let Some(split) = split_var_and_indexes(var_expr, mode, parser.vars(), streams) else {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    };

    // Is the variable valid?
    if !valid_var_name(split.varname) {
        let mut err = varname_error(cmd, split.varname);
        if let Some(pos) = split.varname.chars().position(|c| c == '=') {
            err.append_assign_to_msg('\n');
            let extra = err_fmt!(
                "Did you mean `set %s %s`?",
                &escape(&split.varname[..pos]),
                &escape(&split.varname[pos + 1..])
            )
            .cmd(cmd)
            .to_string();
            err.append_assign_to_msg(&extra);
        }
        err.full_trailer(parser).finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    // Setting with explicit indexes like `set foo[3] ...` has additional error handling.
    #[allow(clippy::collapsible_else_if)]
    if !split.indexes.is_empty() {
        // Indexes must be > 0. (Note split_var_and_indexes negates negative values).
        for ind in &split.indexes {
            if *ind <= 0 {
                err_str!("array index out of bounds")
                    .cmd(cmd)
                    .full_trailer(parser)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
        }
        // Append and prepend are disallowed.
        if opts.append || opts.prepend {
            err_str!("Cannot use --append or --prepend when assigning to a slice")
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        // Argument count and index count must agree.
        if split.indexes.len() != argv.len() {
            err_fmt!(
                "given %d indexes but %d values",
                split.indexes.len(),
                argv.len()
            )
            .cmd(cmd)
            .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }
    } else {
        if matches!(opts.fd, FdArg::Mode(mode) if mode.reuse) {
            err_str!("--fd=reuse: can only be used when assigning to a slice",)
                .cmd(cmd)
                .full_trailer(parser)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }
    }

    let new_values = if split.indexes.is_empty() {
        // Handle the simple, common, case. Set the var to the specified values.
        new_var_values(opts, split.varname, argv, parser.vars())
    } else {
        // Handle the uncommon case of setting specific slices of a var.
        new_var_values_by_index(opts, &split, argv)
    };

    // Set the value back in the variable stack and fire any events.
    let retval =
        env_set_reporting_errors(cmd, opts, split.varname, mode, new_values, streams, parser);

    if retval == EnvStackSetResult::Ok {
        warn_if_uvar_shadows_global(cmd, opts, split.varname, streams, parser);
    }

    retval.into()
}

/// The set builtin creates, updates, and erases (removes, deletes) variables.
pub fn set(parser: &mut Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let cmd = args[0];
    let (opts, optind) = match Options::parse(cmd, args, parser, streams)? {
        Some((opts, optind)) => (opts, optind),
        None => return Ok(SUCCESS),
    };

    let args = &args[optind..];

    let retval = if opts.query {
        query(cmd, &opts, parser, streams, args)
    } else if opts.erase {
        erase(cmd, &opts, parser, streams, args)
    } else if opts.list {
        list(&opts, parser, streams)
    } else if opts.show {
        show(cmd, parser, streams, args)
    } else if args.is_empty() && !(opts.append || opts.prepend) {
        list(&opts, parser, streams)
    } else {
        set_internal(cmd, &opts, parser, streams, args)
    };

    if retval.is_ok() && opts.preserve_failure_exit_status {
        return Ok(Success {
            preserve_failure_exit_status: true,
        });
    }

    retval
}
