use super::prelude::*;
use crate::common::escape;
use crate::common::escape_string;
use crate::common::get_ellipsis_char;
use crate::common::get_ellipsis_str;
use crate::common::valid_var_name;
use crate::common::EscapeFlags;
use crate::common::EscapeStringStyle;
use crate::env::EnvStackSetResult;
use crate::env::EnvVarFlags;
use crate::env::INHERITED_VARS;
use crate::event;
use crate::event::Event;
use crate::expand::expand_escape_string;
use crate::expand::expand_escape_variable;
use crate::history::history_session_id;
use crate::history::History;
use crate::wchar_ext::WExt;
use crate::{
    env::{EnvMode, EnvVar, Environment},
    wutil::wcstoi::wcstoi_partial,
};

// FIXME: (once localization works in Rust) These should separate `%ls: ` and the trailing `\n`, like in builtins/string
const MISMATCHED_ARGS: &str = "%ls: given %d indexes but %d values\n";
const ARRAY_BOUNDS_ERR: &str = "%ls: array index out of bounds\n";
const UVAR_ERR: &str =
    "%ls: successfully set universal '%ls'; but a global by that name shadows it\n";

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
        cmd: &wstr,
        args: &mut [&wstr],
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> Result<(Options, usize), Option<c_int>> {
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
                'U' => opts.universal = true,
                'L' => opts.shorten_ok = false,
                'S' => {
                    opts.show = true;
                    opts.preserve_failure_exit_status = false;
                }
                ':' => {
                    builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], false);
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    // Specifically detect `set -o` because people might be bringing over bashisms.
                    let optind = w.woptind;
                    // implicit drop(w); here
                    if args[optind - 1].starts_with("-o") {
                        // TODO: translate this
                        streams.err.appendln(L!(
                            "Fish does not have shell options. See `help fish-for-bash-users`."
                        ));
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

                    builtin_unknown_option(parser, streams, cmd, args[optind - 1], false);
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

        Self::validate(&opts, cmd, args, optind, parser, streams)?;

        Ok((opts, optind))
    }

    fn validate(
        opts: &Self,
        cmd: &wstr,
        args: &[&wstr],
        optind: usize,
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> Result<(), Option<c_int>> {
        // Can't query and erase or list.
        if opts.query && (opts.erase || opts.list) {
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // We can't both list and erase variables.
        if opts.erase && opts.list {
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
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
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_GLOCAL, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one export status.
        if opts.exportv && opts.unexport {
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_EXPUNEXP, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Variables can only have one path status.
        if opts.pathvar && opts.unpathvar {
            streams
                .err
                .append(wgettext_fmt!(BUILTIN_ERR_PATHUNPATH, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        // Trying to erase and (un)export at the same time doesn't make sense.
        if opts.erase && (opts.exportv || opts.unexport) {
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
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
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        if args.len() == optind && opts.erase {
            streams
                .err
                .append(wgettext_fmt!(BUILTIN_ERR_MISSING, cmd, L!("--erase")));
            builtin_print_error_trailer(parser, streams.err, cmd);
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
    if opts.universal && parser.is_interactive() {
        if parser.vars().getf(dest, EnvMode::GLOBAL).is_some() {
            streams.err.append(wgettext_fmt!(UVAR_ERR, cmd, dest));
        }
    }
}

fn handle_env_return(retval: EnvStackSetResult, cmd: &wstr, key: &wstr, streams: &mut IoStreams) {
    match retval {
        EnvStackSetResult::ENV_OK => (),
        EnvStackSetResult::ENV_PERM => {
            streams.err.append(wgettext_fmt!(
                "%ls: Tried to change the read-only variable '%ls'\n",
                cmd,
                key
            ));
        }
        EnvStackSetResult::ENV_SCOPE => {
            streams.err.append(wgettext_fmt!(
                "%ls: Tried to modify the special variable '%ls' with the wrong scope\n",
                cmd,
                key
            ));
        }
        EnvStackSetResult::ENV_INVALID => {
            streams.err.append(wgettext_fmt!(
                "%ls: Tried to modify the special variable '%ls' to an invalid value\n",
                cmd,
                key
            ));
        }
        EnvStackSetResult::ENV_NOT_FOUND => {
            streams.err.append(wgettext_fmt!(
                "%ls: The variable '%ls' does not exist\n",
                cmd,
                key
            ));
        }
    }
}

/// Call vars.set. If this is a path variable, e.g. PATH, validate the elements. On error, print a
/// description of the problem to stderr.
fn env_set_reporting_errors(
    cmd: &wstr,
    key: &wstr,
    scope: EnvMode,
    list: Vec<WString>,
    streams: &mut IoStreams,
    parser: &Parser,
) -> EnvStackSetResult {
    let retval = parser.set_var_and_fire(key, scope | EnvMode::USER, list);
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
    varname: &'a wstr,
    var: Option<EnvVar>,
    indexes: Vec<usize>,
}

impl<'a> SplitVar<'a> {
    /// \return the number of elements in our variable, or 0 if missing.
    fn varsize(&self) -> usize {
        self.var.as_ref().map_or(0, |var| var.as_list().len())
    }
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
    streams: &mut IoStreams,
) -> Option<SplitVar<'a>> {
    match split_var_and_indexes_internal(arg, mode, vars) {
        Ok(split) => Some(split),
        Err(EnvArrayParseError::InvalidIndex(varname)) => {
            streams.err.append(wgettext_fmt!(
                "%ls: Invalid index starting at '%ls'\n",
                "set",
                &varname,
            ));
            None
        }
    }
}

fn split_var_and_indexes_internal<'a>(
    arg: &'a wstr,
    mode: EnvMode,
    vars: &dyn Environment,
) -> Result<SplitVar<'a>, EnvArrayParseError> {
    // PORTING: this should probably be made reusable in some way?

    let mut res = SplitVar::default();
    let open_bracket = arg.find_char('[');
    res.varname = open_bracket.map(|b| &arg[..b]).unwrap_or(arg);
    res.var = vars.getf(res.varname, mode);
    let Some(open_bracket) = open_bracket else {
        // Common case of no bracket
        return Ok(res);
    };

    // PORTING: this should return an error if there is no var,
    // we need the length of the array to validate the indexes
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
                .map_err(|_| EnvArrayParseError::InvalidIndex(res.varname.to_owned()))?;
            c = c.slice_from(consumed);
            // Skip trailing whitespace.
            while !c.is_empty() && c.char_at(0).is_whitespace() {
                c = c.slice_from(1);
            }
        }

        // Convert negative index to a positive index.
        let signed_to_unsigned = |i: isize| -> Result<usize, EnvArrayParseError> {
            match i.try_into() {
                Ok(idx) => Ok(idx),
                Err(_) => {
                    let Some(idx) = len.checked_add_signed(i).map(|i| i + 1) else {
                        // PORTING: this was not handled in C++, l_ind was just kept as an `i64`
                        // this should have a separate error
                        // also: in the case of `var[1 1]` we should probably either de-duplicate
                        // or make that a hard error.
                        // the behavior of `..` is also somewhat weird
                        return Err(EnvArrayParseError::InvalidIndex(res.varname.to_owned()));
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
                l_ind2 = wcstoi_partial(c, crate::wutil::Options::default(), &mut consumed)
                    .map_err(|_| EnvArrayParseError::InvalidIndex(res.varname.to_owned()))?;
                c = c.slice_from(consumed);
                // Skip trailing whitespace.
                while !c.is_empty() && c.char_at(0).is_whitespace() {
                    c = c.slice_from(1);
                }
            }

            let l_ind2: usize = signed_to_unsigned(l_ind2)?;
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

/// Given a list of values and 1-based indexes, return a new list with those elements removed.
/// Note this deliberately accepts both args by value, as it modifies them both.
fn erased_at_indexes(mut input: Vec<WString>, mut indexes: Vec<usize>) -> Vec<WString> {
    // Sort our indexes into *descending* order.
    indexes.sort_by_key(|&index| std::cmp::Reverse(index));

    // Remove duplicates.
    indexes.dedup();

    // Now when we walk indexes, we encounter larger indexes first.
    for idx in indexes {
        if idx > 0 && idx <= input.len() {
            // One-based indexing!
            input.remove(idx - 1);
        }
    }
    input
}

/// Print the names of all environment variables in the scope. It will include the values unless the
/// `set --names` flag was used.
fn list(opts: &Options, parser: &Parser, streams: &mut IoStreams) -> Option<c_int> {
    let names_only = opts.list;
    let mut names = parser.vars().get_names(opts.scope());
    names.sort();

    for key in names {
        let mut out = key.clone();

        if !names_only {
            let mut val = WString::new();
            if opts.shorten_ok && key == "history" {
                let history = History::with_name(&history_session_id(parser.vars()));
                for i in 1..history.size() {
                    if val.len() >= 64 {
                        break;
                    }
                    if i > 1 {
                        val.push(' ');
                    }
                    val += &expand_escape_string(history.item_at_index(i).unwrap().str())[..]
                }
            } else if let Some(var) = parser.vars().getf_unless_empty(&key, opts.scope()) {
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
                    out.push(get_ellipsis_char());
                }
            }
        }

        out.push('\n');
        streams.out.append(out);
    }

    STATUS_CMD_OK
}

fn query(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams,
    args: &[&wstr],
) -> Option<c_int> {
    let mut retval = 0;
    let scope = opts.scope();

    // No variables given, this is an error.
    // 255 is the maximum return code we allow.
    if args.is_empty() {
        return Some(255);
    }

    for arg in args {
        let Some(split) = split_var_and_indexes(arg, scope, parser.vars(), streams) else {
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
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
                if idx < 1 || idx > varsize {
                    retval += 1;
                }
            }
        }
    }

    Some(retval)
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
        wgettext!(" a path variable")
    } else {
        L!("")
    };
    let vals = var.as_list();
    streams.out.append(wgettext_fmt!(
        "$%ls: set in %ls scope, %ls,%ls with %d elements",
        var_name,
        scope_name,
        exportv,
        pathvarv,
        vals.len()
    ));
    // HACK: PWD can be set, depending on how you ask.
    // For our purposes it's read-only.
    if EnvVar::flags_for(var_name).contains(EnvVarFlags::READ_ONLY) {
        streams.out.append(wgettext!(" (read-only)\n"));
    } else {
        streams.out.push('\n');
    }

    for i in 0..vals.len() {
        if vals.len() > 100 {
            if i == 50 {
                // try to print a mid-line ellipsis because we are eliding lines not words
                streams.out.append(if u32::from(get_ellipsis_char()) > 256 {
                    L!("\u{22EF}")
                } else {
                    get_ellipsis_str()
                });
                streams.out.push('\n');
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
        streams.out.append(wgettext_fmt!(
            "$%ls[%d]: |%ls|\n",
            var_name,
            i + 1,
            &escaped_val
        ));
    }
}

/// Show mode. Show information about the named variable(s).
fn show(cmd: &wstr, parser: &Parser, streams: &mut IoStreams, args: &[&wstr]) -> Option<c_int> {
    let vars = parser.vars();
    if args.is_empty() {
        // show all vars
        let mut names = vars.get_names(EnvMode::USER);
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
                streams.out.append(wgettext_fmt!(
                    "$%ls: originally inherited as |%ls|\n",
                    name,
                    escaped_val
                ));
            }
        }
    } else {
        for arg in args.iter().copied() {
            if !valid_var_name(arg) {
                streams
                    .err
                    .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, arg));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_INVALID_ARGS;
            }

            if arg.contains('[') {
                streams.err.append(wgettext_fmt!(
                    "%ls: `set --show` does not allow slices with the var names\n",
                    cmd
                ));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_CMD_ERROR;
            }

            show_scope(arg, EnvMode::LOCAL, streams, vars);
            show_scope(arg, EnvMode::GLOBAL, streams, vars);
            show_scope(arg, EnvMode::UNIVERSAL, streams, vars);
            if let Some(inherited) = INHERITED_VARS.get().unwrap().get(arg) {
                let escaped_val = escape_string(
                    inherited,
                    EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
                );
                streams.out.append(wgettext_fmt!(
                    "$%ls: originally inherited as |%ls|\n",
                    arg,
                    escaped_val
                ));
            }
        }
    }

    STATUS_CMD_OK
}

fn erase(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,
    streams: &mut IoStreams,
    args: &[&wstr],
) -> Option<c_int> {
    let mut ret = STATUS_CMD_OK;
    let scopes = opts.scope();
    // `set -e` is allowed to be called with multiple scopes.
    for bit in (0..).take_while(|bit| 1 << bit <= EnvMode::USER.bits()) {
        let scope = scopes.intersection(EnvMode::from_bits(1 << bit).unwrap());
        if scope.bits() == 0 || (scope == EnvMode::USER && scopes != EnvMode::USER) {
            continue;
        }
        for arg in args {
            let Some(split) = split_var_and_indexes(arg, scope, parser.vars(), streams) else {
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_CMD_ERROR;
            };

            if !valid_var_name(split.varname) {
                streams
                    .err
                    .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, split.varname));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_INVALID_ARGS;
            }
            let retval;
            if split.indexes.is_empty() {
                // unset the var
                retval = parser.vars().remove(split.varname, scope);
                // When a non-existent-variable is unset, return ENV_NOT_FOUND as $status
                // but do not emit any errors at the console as a compromise between user
                // friendliness and correctness.
                if retval != EnvStackSetResult::ENV_NOT_FOUND {
                    handle_env_return(retval, cmd, split.varname, streams);
                }
                if retval == EnvStackSetResult::ENV_OK {
                    event::fire(parser, Event::variable_erase(split.varname.to_owned()));
                }
            } else {
                // remove just the specified indexes of the var
                let Some(var) = split.var else {
                    return STATUS_CMD_ERROR;
                };
                let result = erased_at_indexes(var.as_list().to_owned(), split.indexes);
                retval =
                    env_set_reporting_errors(cmd, split.varname, scope, result, streams, parser);
            }

            // Set $status to the last error value.
            // This is cheesy, but I don't expect this to be checked often.
            if retval != EnvStackSetResult::ENV_OK {
                ret = env_result_to_status(retval);
            }
        }
    }
    ret
}

fn env_result_to_status(retval: EnvStackSetResult) -> Option<c_int> {
    Some(match retval {
        EnvStackSetResult::ENV_OK => 0,
        EnvStackSetResult::ENV_PERM => 1,
        EnvStackSetResult::ENV_SCOPE => 2,
        EnvStackSetResult::ENV_INVALID => 3,
        EnvStackSetResult::ENV_NOT_FOUND => 4,
    })
}

/// Return a list of new values for the variable \p varname, respecting the \p opts.
/// This handles the simple case where there are no indexes.
fn new_var_values(
    varname: &wstr,
    opts: &Options,
    argv: &[&wstr],
    vars: &dyn Environment,
) -> Vec<WString> {
    let mut result = vec![];
    if !opts.prepend && !opts.append {
        // Not prepending or appending.
        result.extend(argv.iter().copied().map(|s| s.to_owned()));
    } else {
        // Note: when prepending or appending, we always use default scoping when fetching existing
        // values. For example:
        //   set --global var 1 2
        //   set --local --append var 3 4
        // This starts with the existing global variable, appends to it, and sets it locally.
        // So do not use the given variable: we must re-fetch it.
        // TODO: this races under concurrent execution.
        if let Some(existing) = vars.get(varname) {
            result = existing.as_list().to_owned();
        }

        if opts.prepend {
            result.splice(0..0, argv.iter().copied().map(|s| s.to_owned()));
        }

        if opts.append {
            result.extend(argv.iter().copied().map(|s| s.to_owned()));
        }
    }
    result
}

/// This handles the more difficult case of setting individual slices of a var.
fn new_var_values_by_index(split: &SplitVar, argv: &[&wstr]) -> Vec<WString> {
    assert!(
        argv.len() == split.indexes.len(),
        "Must have the same number of indexes as arguments"
    );

    // Inherit any existing values.
    // Note unlike the append/prepend case, we start with a variable in the same scope as we are
    // setting.
    let mut result = vec![];
    if let Some(var) = split.var.as_ref() {
        result = var.as_list().to_owned();
    }

    // For each (index, argument) pair, set the element in our \p result to the replacement string.
    // Extend the list with empty strings as needed. The indexes are 1-based.
    for (i, arg) in argv.iter().copied().enumerate() {
        let lidx = split.indexes[i];
        assert!(lidx >= 1, "idx should have been verified in range already");
        // Convert from 1-based to 0-based.
        let idx = lidx - 1;
        // Extend as needed with empty strings.
        if idx >= result.len() {
            result.resize(idx + 1, WString::new());
        }
        result[idx] = arg.into();
    }
    result
}

/// Set a variable.
fn set_internal(
    cmd: &wstr,
    opts: &Options,
    parser: &Parser,

    streams: &mut IoStreams,
    argv: &[&wstr],
) -> Option<c_int> {
    if argv.is_empty() {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    let scope = opts.scope();
    let var_expr = argv[0];
    let argv = &argv[1..];

    let Some(split) = split_var_and_indexes(var_expr, scope, parser.vars(), streams) else {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    };

    // Is the variable valid?
    if !valid_var_name(split.varname) {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, split.varname));
        if let Some(pos) = split.varname.chars().position(|c| c == '=') {
            streams.err.append(wgettext_fmt!(
                "%ls: Did you mean `set %ls %ls`?",
                cmd,
                &escape(&split.varname[..pos]),
                &escape(&split.varname[pos + 1..])
            ));
        }
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Setting with explicit indexes like `set foo[3] ...` has additional error handling.
    if !split.indexes.is_empty() {
        // Indexes must be > 0. (Note split_var_and_indexes negates negative values).

        // Append and prepend are disallowed.
        if opts.append || opts.prepend {
            streams.err.append(wgettext_fmt!(
                "%ls: Cannot use --append or --prepend when assigning to a slice",
                cmd
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        // Argument count and index count must agree.
        if split.indexes.len() != argv.len() {
            streams.err.append(wgettext_fmt!(
                MISMATCHED_ARGS,
                cmd,
                split.indexes.len(),
                argv.len()
            ));
            return STATUS_INVALID_ARGS;
        }
    }

    let new_values = if split.indexes.is_empty() {
        // Handle the simple, common, case. Set the var to the specified values.
        new_var_values(split.varname, opts, argv, parser.vars())
    } else {
        // Handle the uncommon case of setting specific slices of a var.
        new_var_values_by_index(&split, argv)
    };

    // Set the value back in the variable stack and fire any events.
    let retval = env_set_reporting_errors(cmd, split.varname, scope, new_values, streams, parser);

    if retval == EnvStackSetResult::ENV_OK {
        warn_if_uvar_shadows_global(cmd, opts, split.varname, streams, parser);
    }
    env_result_to_status(retval)
}

/// The set builtin creates, updates, and erases (removes, deletes) variables.
pub fn set(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let cmd = args[0];
    let (opts, optind) = match Options::parse(cmd, args, parser, streams) {
        Ok(res) => res,
        Err(err) => return err,
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
    } else if args.is_empty() {
        list(&opts, parser, streams)
    } else {
        set_internal(cmd, &opts, parser, streams, args)
    };

    if retval == STATUS_CMD_OK && opts.preserve_failure_exit_status {
        return None;
    }

    return retval;
}
