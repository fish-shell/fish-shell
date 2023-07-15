use super::prelude::*;
use crate::builtins::shared::builtin_exists;
use crate::common::str2wcstring;
use crate::function;
use crate::highlight::{colorize, highlight_shell};
use crate::parser::lineno_or_minus_1;

use crate::path::{path_get_path, path_get_paths};

#[derive(Default)]
struct type_cmd_opts_t {
    all: bool,
    short_output: bool,
    no_functions: bool,
    get_type: bool,
    path: bool,
    force_path: bool,
    print_help: bool,
    query: bool,
}

pub fn r#type(parser: &Parser, streams: &mut IoStreams<'_>, argv: &mut [WString]) -> Option<c_int> {
    let argc = argv.len();
    let print_hints = false;
    let mut opts: type_cmd_opts_t = Default::default();

    const shortopts: &wstr = L!(":hasftpPq");
    const longopts: &[woption] = &[
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
        wopt(L!("all"), woption_argument_t::no_argument, 'a'),
        wopt(L!("short"), woption_argument_t::no_argument, 's'),
        wopt(L!("no-functions"), woption_argument_t::no_argument, 'f'),
        wopt(L!("type"), woption_argument_t::no_argument, 't'),
        wopt(L!("path"), woption_argument_t::no_argument, 'p'),
        wopt(L!("force-path"), woption_argument_t::no_argument, 'P'),
        wopt(L!("query"), woption_argument_t::no_argument, 'q'),
        wopt(L!("quiet"), woption_argument_t::no_argument, 'q'),
    ];

    let mut w = wgetopter_t::new(shortopts, longopts, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'a' => opts.all = true,
            's' => opts.short_output = true,
            'f' => opts.no_functions = true,
            't' => opts.get_type = true,
            'p' => opts.path = true,
            'P' => opts.force_path = true,
            'q' => opts.query = true,
            'h' => {
                builtin_print_help(parser, streams, w.cmd());
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    if opts.query as i64 + opts.path as i64 + opts.get_type as i64 + opts.force_path as i64 > 1 {
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_COMBO, w.cmd()));
        return STATUS_INVALID_ARGS;
    }

    let mut res = false;

    let optind = w.woptind;
    for arg in argv.iter().take(argc).skip(optind) {
        let mut found = 0;
        if !opts.force_path && !opts.no_functions {
            if let Some(props) = function::get_props_autoload(arg, parser) {
                found += 1;
                res = true;
                // Early out - query means *any of the args exists*.
                if opts.query {
                    return STATUS_CMD_OK;
                }
                if !opts.get_type {
                    let path = props.definition_file().unwrap_or(L!(""));
                    let mut comment = WString::new();

                    if path.is_empty() {
                        comment.push_utfstr(&wgettext_fmt!("Defined interactively"));
                    } else if path == L!("-") {
                        comment.push_utfstr(&wgettext_fmt!("Defined via `source`"));
                    } else {
                        let lineno: i32 = props.definition_lineno();
                        comment.push_utfstr(&wgettext_fmt!(
                            "Defined in %ls @ line %d",
                            path,
                            lineno
                        ));
                    }

                    if props.is_copy() {
                        let path = props.copy_definition_file().unwrap_or(L!(""));
                        if path.is_empty() {
                            comment.push_utfstr(&wgettext_fmt!(", copied interactively"));
                        } else if path == L!("-") {
                            comment.push_utfstr(&wgettext_fmt!(", copied via `source`"));
                        } else {
                            let lineno: i32 = props.copy_definition_lineno();
                            comment.push_utfstr(&wgettext_fmt!(
                                ", copied in %ls @ line %d",
                                path,
                                lineno_or_minus_1(lineno)
                            ));
                        }
                    }
                    if opts.path {
                        if let Some(orig_path) = props.copy_definition_file() {
                            streams.out.appendln(orig_path);
                        } else {
                            streams.out.appendln(path);
                        }
                    } else if !opts.short_output {
                        streams.out.append(&wgettext_fmt!("%ls is a function", arg));
                        streams.out.appendln(wgettext!(" with definition"));
                        let mut def = WString::new();
                        def.push_utfstr(&sprintf!(
                            "# %ls\n%ls",
                            comment,
                            props.annotated_definition(arg)
                        ));

                        if streams.out_is_terminal() {
                            let mut color = vec![];
                            highlight_shell(
                                &def,
                                &mut color,
                                &parser.context(),
                                /*io_ok=*/ false,
                                /*cursor=*/ None,
                            );
                            let col = str2wcstring(&colorize(&def, &color, parser.vars()));
                            streams.out.append(&col);
                        } else {
                            streams.out.append(&def);
                        }
                    } else {
                        streams.out.append(&wgettext_fmt!("%ls is a function", arg));
                        streams.out.append(&wgettext_fmt!(" (%ls)\n", comment));
                    }
                } else if opts.get_type {
                    streams.out.appendln(L!("function"));
                }
                if !opts.all {
                    continue;
                }
            }
        }

        if !opts.force_path && builtin_exists(arg) {
            found += 1;
            res = true;
            if opts.query {
                return STATUS_CMD_OK;
            }
            if !opts.get_type {
                streams
                    .out
                    .append(&wgettext_fmt!("%ls is a builtin\n", arg));
            } else if opts.get_type {
                streams.out.append(wgettext!("builtin\n"));
            }
            if !opts.all {
                continue;
            }
        }

        let paths = if opts.all {
            path_get_paths(arg, &*parser.vars())
        } else {
            match path_get_path(arg, &*parser.vars()) {
                Some(p) => vec![p],
                None => vec![],
            }
        };

        for path in paths.iter() {
            found += 1;
            res = true;
            if opts.query {
                return STATUS_CMD_OK;
            }
            if !opts.get_type {
                if opts.path || opts.force_path {
                    streams.out.appendln(path);
                } else {
                    streams
                        .out
                        .append(&wgettext_fmt!("%ls is %ls\n", arg, path));
                }
            } else if opts.get_type {
                streams.out.appendln(L!("file"));
                break;
            }
            if !opts.all {
                // We need to *break* out of this loop
                // and continue on to the next argument,
                // otherwise we would print every other path
                // for a given argument.
                break;
            }
        }

        if found == 0 && !opts.query && !opts.path {
            streams.err.append(&wgettext_fmt!(
                "%ls: Could not find '%ls'\n",
                L!("type"),
                arg
            ));
        }
    }

    if res {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}
