use super::prelude::*;
use crate::abbrs::{self, Abbreviation, Position};
use crate::common::{escape, escape_string, valid_func_name, EscapeStringStyle};
use crate::env::status::{ENV_NOT_FOUND, ENV_OK};
use crate::env::{EnvMode, EnvStackSetResult};
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::re::{regex_make_anchored, to_boxed_chars};
use pcre2::utf32::{Regex, RegexBuilder};

const CMD: &wstr = L!("abbr");

#[derive(Default, Debug)]
struct Options {
    add: bool,
    rename: bool,
    show: bool,
    list: bool,
    erase: bool,
    query: bool,
    function: Option<WString>,
    regex_pattern: Option<WString>,
    position: Option<Position>,
    set_cursor_marker: Option<WString>,
    args: Vec<WString>,
}

impl Options {
    fn validate(&mut self, streams: &mut IoStreams<'_>) -> bool {
        // Duplicate options?
        let mut cmds = vec![];
        if self.add {
            cmds.push(L!("add"))
        };
        if self.rename {
            cmds.push(L!("rename"))
        };
        if self.show {
            cmds.push(L!("show"))
        };
        if self.list {
            cmds.push(L!("list"))
        };
        if self.erase {
            cmds.push(L!("erase"))
        };
        if self.query {
            cmds.push(L!("query"))
        };

        if cmds.len() > 1 {
            streams.err.append(&wgettext_fmt!(
                "%ls: Cannot combine options %ls\n",
                CMD,
                join(&cmds, L!(", "))
            ));
            return false;
        }

        // If run with no options, treat it like --add if we have arguments,
        // or --show if we do not have any arguments.
        if cmds.is_empty() {
            self.show = self.args.is_empty();
            self.add = !self.args.is_empty();
        }

        if !self.add && self.position.is_some() {
            streams.err.append(&wgettext_fmt!(
                "%ls: --position option requires --add\n",
                CMD
            ));
            return false;
        }
        if !self.add && self.regex_pattern.is_some() {
            streams
                .err
                .append(&wgettext_fmt!("%ls: --regex option requires --add\n", CMD));
            return false;
        }
        if !self.add && self.function.is_some() {
            streams.err.append(&wgettext_fmt!(
                "%ls: --function option requires --add\n",
                CMD
            ));
            return false;
        }
        if !self.add && self.set_cursor_marker.is_some() {
            streams.err.append(&wgettext_fmt!(
                "%ls: --set-cursor option requires --add\n",
                CMD
            ));
            return false;
        }
        if self
            .set_cursor_marker
            .as_ref()
            .map(|m| m.is_empty())
            .unwrap_or(false)
        {
            streams.err.append(&wgettext_fmt!(
                "%ls: --set-cursor argument cannot be empty\n",
                CMD
            ));
            return false;
        }

        return true;
    }
}

fn join(list: &[&wstr], sep: &wstr) -> WString {
    let mut result = WString::new();
    let mut iter = list.iter();

    let first = match iter.next() {
        Some(first) => first,
        None => return result,
    };
    result.push_utfstr(first);

    for s in iter {
        result.push_utfstr(sep);
        result.push_utfstr(s);
    }
    result
}

// Print abbreviations in a fish-script friendly way.
fn abbr_show(streams: &mut IoStreams<'_>) -> Option<c_int> {
    let style = EscapeStringStyle::Script(Default::default());

    abbrs::with_abbrs(|abbrs| {
        let mut result = WString::new();
        for abbr in abbrs.list() {
            result.clear();
            let mut add_arg = |arg: &wstr| {
                if !result.is_empty() {
                    result.push_str(" ");
                }
                result.push_utfstr(arg);
            };

            add_arg(L!("abbr -a"));
            if abbr.is_regex() {
                add_arg(L!("--regex"));
                add_arg(&escape_string(&abbr.key, style));
            }
            if abbr.position != Position::Command {
                add_arg(L!("--position"));
                add_arg(L!("anywhere"));
            }
            if let Some(ref set_cursor_marker) = abbr.set_cursor_marker {
                add_arg(L!("--set-cursor="));
                add_arg(&escape_string(set_cursor_marker, style));
            }
            if abbr.replacement_is_function {
                add_arg(L!("--function"));
                add_arg(&escape_string(&abbr.replacement, style));
            }
            add_arg(L!("--"));
            // Literal abbreviations have the name and key as the same.
            // Regex abbreviations have a pattern separate from the name.
            add_arg(&escape_string(&abbr.name, style));
            if !abbr.replacement_is_function {
                add_arg(&escape_string(&abbr.replacement, style));
            }
            if abbr.from_universal {
                add_arg(L!("# imported from a universal variable, see `help abbr`"));
            }
            result.push('\n');
            streams.out.append(&result);
        }
    });

    return STATUS_CMD_OK;
}

// Print the list of abbreviation names.
fn abbr_list(opts: &Options, streams: &mut IoStreams<'_>) -> Option<c_int> {
    const subcmd: &wstr = L!("--list");
    if !opts.args.is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Unexpected argument -- '%ls'\n",
            CMD,
            subcmd,
            &opts.args[0]
        ));
        return STATUS_INVALID_ARGS;
    }
    abbrs::with_abbrs(|abbrs| {
        for abbr in abbrs.list() {
            streams.out.append(&abbr.name);
            streams.out.push('\n');
        }
    });

    return STATUS_CMD_OK;
}

// Rename an abbreviation, deleting any existing one with the given name.
fn abbr_rename(opts: &Options, streams: &mut IoStreams<'_>) -> Option<c_int> {
    const subcmd: &wstr = L!("--rename");

    if opts.args.len() != 2 {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Requires exactly two arguments\n",
            CMD,
            subcmd
        ));
        return STATUS_INVALID_ARGS;
    }
    let old_name = &opts.args[0];
    let new_name = &opts.args[1];
    if old_name.is_empty() || new_name.is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Name cannot be empty\n",
            CMD,
            subcmd
        ));
        return STATUS_INVALID_ARGS;
    }

    if contains_whitespace(new_name) {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Abbreviation '%ls' cannot have spaces in the word\n",
            CMD,
            subcmd,
            new_name.as_utfstr()
        ));
        return STATUS_INVALID_ARGS;
    }
    abbrs::with_abbrs_mut(|abbrs| -> Option<c_int> {
        if !abbrs.has_name(old_name) {
            streams.err.append(&wgettext_fmt!(
                "%ls %ls: No abbreviation named %ls\n",
                CMD,
                subcmd,
                old_name.as_utfstr()
            ));
            return STATUS_CMD_ERROR;
        }
        if abbrs.has_name(new_name) {
            streams.err.append(&wgettext_fmt!(
                "%ls %ls: Abbreviation %ls already exists, cannot rename %ls\n",
                CMD,
                subcmd,
                new_name.as_utfstr(),
                old_name.as_utfstr()
            ));
            return STATUS_INVALID_ARGS;
        }
        abbrs.rename(old_name, new_name);
        STATUS_CMD_OK
    })
}

fn contains_whitespace(val: &wstr) -> bool {
    val.chars().any(char::is_whitespace)
}

// Test if any args is an abbreviation.
fn abbr_query(opts: &Options) -> Option<c_int> {
    // Return success if any of our args matches an abbreviation.
    abbrs::with_abbrs(|abbrs| {
        for arg in opts.args.iter() {
            if abbrs.has_name(arg) {
                return STATUS_CMD_OK;
            }
        }
        return STATUS_CMD_ERROR;
    })
}

// Add a named abbreviation.
fn abbr_add(opts: &Options, streams: &mut IoStreams<'_>) -> Option<c_int> {
    const subcmd: &wstr = L!("--add");

    if opts.args.len() < 2 && opts.function.is_none() {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Requires at least two arguments\n",
            CMD,
            subcmd
        ));
        return STATUS_INVALID_ARGS;
    }

    if opts.args.is_empty() || opts.args[0].is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Name cannot be empty\n",
            CMD,
            subcmd
        ));
        return STATUS_INVALID_ARGS;
    }
    let name = &opts.args[0];
    if name.chars().any(|c| c.is_whitespace()) {
        streams.err.append(&wgettext_fmt!(
            "%ls %ls: Abbreviation '%ls' cannot have spaces in the word\n",
            CMD,
            subcmd,
            name.as_utfstr()
        ));
        return STATUS_INVALID_ARGS;
    }

    let key: &wstr;
    let regex: Option<Regex>;
    if let Some(regex_pattern) = &opts.regex_pattern {
        // Compile the regex as given; if that succeeds then wrap it in our ^$ so it matches the
        // entire token.
        // We have historically disabled the "(*UTF)" sequence.
        let mut builder = RegexBuilder::new();
        builder.caseless(false).never_utf(true);

        let result = builder.build(to_boxed_chars(regex_pattern));

        if let Err(error) = result {
            streams.err.append(&wgettext_fmt!(
                "%ls: Regular expression compile error: %ls\n",
                CMD,
                error.error_message(),
            ));
            if let Some(offset) = error.offset() {
                streams
                    .err
                    .append(&wgettext_fmt!("%ls: %ls\n", CMD, regex_pattern.as_utfstr()));
                streams
                    .err
                    .append(&wgettext_fmt!("%ls: %*ls\n", CMD, offset, "^"));
            }
            return STATUS_INVALID_ARGS;
        }
        let anchored = regex_make_anchored(regex_pattern);
        let re = builder
            .build(to_boxed_chars(&anchored))
            .expect("Anchored compilation should have succeeded");

        key = regex_pattern;
        regex = Some(re);
    } else {
        // The name plays double-duty as the token to replace.
        key = name;
        regex = None;
    };

    if opts.function.is_some() && opts.args.len() > 1 {
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, L!("abbr")));
        return STATUS_INVALID_ARGS;
    }
    let replacement = if let Some(ref function) = opts.function {
        // Abbreviation function names disallow spaces.
        // This is to prevent accidental usage of e.g. `--function 'string replace'`
        if !valid_func_name(function) || contains_whitespace(function) {
            streams.err.append(&wgettext_fmt!(
                "%ls: Invalid function name: %ls\n",
                CMD,
                function.as_utfstr()
            ));
            return STATUS_INVALID_ARGS;
        }
        function.clone()
    } else {
        let mut replacement = WString::new();
        for iter in opts.args.iter().skip(1) {
            if !replacement.is_empty() {
                replacement.push(' ')
            };
            replacement.push_utfstr(iter);
        }
        replacement
    };

    let position = opts.position.unwrap_or(Position::Command);

    // Note historically we have allowed overwriting existing abbreviations.
    abbrs::with_abbrs_mut(move |abbrs| {
        abbrs.add(Abbreviation {
            name: name.clone(),
            key: key.to_owned(),
            regex,
            replacement,
            replacement_is_function: opts.function.is_some(),
            position,
            set_cursor_marker: opts.set_cursor_marker.clone(),
            from_universal: false,
        })
    });

    return STATUS_CMD_OK;
}

// Erase the named abbreviations.
fn abbr_erase(opts: &Options, parser: &Parser) -> Option<c_int> {
    if opts.args.is_empty() {
        // This has historically been a silent failure.
        return STATUS_CMD_ERROR;
    }

    // Erase each. If any is not found, return ENV_NOT_FOUND which is historical.
    abbrs::with_abbrs_mut(|abbrs| -> Option<c_int> {
        let mut result = STATUS_CMD_OK;
        for arg in &opts.args {
            if !abbrs.erase(arg) {
                result = Some(ENV_NOT_FOUND);
            }
            // Erase the old uvar - this makes `abbr -e` work.
            let esc_src = escape(arg);
            if !esc_src.is_empty() {
                let var_name = WString::from_str("_fish_abbr_") + esc_src.as_utfstr();
                let ret = parser.vars().remove(&var_name, EnvMode::UNIVERSAL.into());

                if ret == EnvStackSetResult::ENV_OK {
                    result = STATUS_CMD_OK
                };
            }
        }
        result
    })
}

pub fn abbr(parser: &Parser, streams: &mut IoStreams<'_>, argv: &mut [WString]) -> Option<c_int> {
    let mut argv_read = Vec::with_capacity(argv.len());
    argv_read.extend_from_slice(argv);

    // Note 1 is returned by wgetopt to indicate a non-option argument.
    const NON_OPTION_ARGUMENT: char = 1 as char;
    const SET_CURSOR_SHORT: char = 2 as char;
    const RENAME_SHORT: char = 3 as char;

    // Note the leading '-' causes wgetopter to return arguments in order, instead of permuting
    // them. We need this behavior for compatibility with pre-builtin abbreviations where options
    // could be given literally, for example `abbr e emacs -nw`.
    const short_options: &wstr = L!("-:af:r:seqgUh");

    const longopts: &[woption] = &[
        wopt(L!("add"), woption_argument_t::no_argument, 'a'),
        wopt(L!("position"), woption_argument_t::required_argument, 'p'),
        wopt(L!("regex"), woption_argument_t::required_argument, 'r'),
        wopt(
            L!("set-cursor"),
            woption_argument_t::optional_argument,
            SET_CURSOR_SHORT,
        ),
        wopt(L!("function"), woption_argument_t::required_argument, 'f'),
        wopt(L!("rename"), woption_argument_t::no_argument, RENAME_SHORT),
        wopt(L!("erase"), woption_argument_t::no_argument, 'e'),
        wopt(L!("query"), woption_argument_t::no_argument, 'q'),
        wopt(L!("show"), woption_argument_t::no_argument, 's'),
        wopt(L!("list"), woption_argument_t::no_argument, 'l'),
        wopt(L!("global"), woption_argument_t::no_argument, 'g'),
        wopt(L!("universal"), woption_argument_t::no_argument, 'U'),
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    ];

    let mut opts = Options::default();
    let mut w = wgetopter_t::new(short_options, longopts, argv);

    while let Some(c) = w.wgetopt_long() {
        match c {
            NON_OPTION_ARGUMENT => {
                // If --add is specified (or implied by specifying no other commands), all
                // unrecognized options after the *second* non-option argument are considered part
                // of the abbreviation expansion itself, rather than options to the abbr command.
                // For example, `abbr e emacs -nw` works, because `-nw` occurs after the second
                // non-option, and --add is implied.
                if let Some(arg) = w.woptarg() {
                    opts.args.push(arg.to_owned())
                };
                if opts.args.len() >= 2
                    && !(opts.rename || opts.show || opts.list || opts.erase || opts.query)
                {
                    break;
                }
            }
            'a' => opts.add = true,
            'p' => {
                if opts.position.is_some() {
                    streams.err.append(&wgettext_fmt!(
                        "%ls: Cannot specify multiple positions\n",
                        CMD
                    ));
                    return STATUS_INVALID_ARGS;
                }
                if w.woptarg() == Some(L!("command")) {
                    opts.position = Some(Position::Command);
                } else if w.woptarg() == Some(L!("anywhere")) {
                    opts.position = Some(Position::Anywhere);
                } else {
                    streams.err.append(&wgettext_fmt!(
                        "%ls: Invalid position '%ls'\n",
                        CMD,
                        w.woptarg().unwrap_or_default()
                    ));
                    streams
                        .err
                        .append(L!("Position must be one of: command, anywhere.\n"));
                    return STATUS_INVALID_ARGS;
                }
            }
            'r' => {
                if opts.regex_pattern.is_some() {
                    streams.err.append(&wgettext_fmt!(
                        "%ls: Cannot specify multiple regex patterns\n",
                        CMD
                    ));
                    return STATUS_INVALID_ARGS;
                }
                opts.regex_pattern = w.woptarg().map(ToOwned::to_owned);
            }
            SET_CURSOR_SHORT => {
                if opts.set_cursor_marker.is_some() {
                    streams.err.append(&wgettext_fmt!(
                        "%ls: Cannot specify multiple set-cursor options\n",
                        CMD
                    ));
                    return STATUS_INVALID_ARGS;
                }
                // The default set-cursor indicator is '%'.
                let _ = opts
                    .set_cursor_marker
                    .insert(w.woptarg().unwrap_or(L!("%")).to_owned());
            }
            'f' => opts.function = w.woptarg().map(ToOwned::to_owned),
            RENAME_SHORT => opts.rename = true,
            'e' => opts.erase = true,
            'q' => opts.query = true,
            's' => opts.show = true,
            'l' => opts.list = true,
            // Kept for backwards compatibility but ignored.
            // This basically does nothing now.
            'g' => {}

            'U' => {
                // Kept and made ineffective, so we warn.
                streams.err.append(&wgettext_fmt!(
                    "%ls: Warning: Option '%ls' was removed and is now ignored",
                    w.cmd(),
                    argv_read[w.woptind - 1]
                ));
                builtin_print_error_trailer(parser, streams.err, w.cmd());
            }
            'h' => {
                builtin_print_help(parser, streams, w.cmd());
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    for arg in argv_read.drain(w.woptind..) {
        opts.args.push(arg);
    }

    if !opts.validate(streams) {
        return STATUS_INVALID_ARGS;
    }

    if opts.add {
        return abbr_add(&opts, streams);
    };
    if opts.show {
        return abbr_show(streams);
    };
    if opts.list {
        return abbr_list(&opts, streams);
    };
    if opts.rename {
        return abbr_rename(&opts, streams);
    };
    if opts.erase {
        return abbr_erase(&opts, parser);
    };
    if opts.query {
        return abbr_query(&opts);
    };

    // validate() should error or ensure at least one path is set.
    panic!("unreachable");
}
