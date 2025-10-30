use super::prelude::*;
use crate::common::bytes2wcstring;
use crate::function;
use crate::highlight::{colorize, highlight_shell};

use crate::parse_util::{apply_indents, parse_util_compute_indents};
use crate::path::{path_get_path, path_get_paths};

#[derive(Default)]
struct type_cmd_opts_t {
    all: bool,
    short_output: bool,
    no_functions: bool,
    get_type: bool,
    path: bool,
    force_path: bool,
    query: bool,
}

pub fn r#type(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let cmd = argv[0];
    let argc = argv.len();
    let print_hints = false;
    let mut opts: type_cmd_opts_t = Default::default();

    let shortopts: &wstr = L!("hasftpPq");
    let longopts: &[WOption] = &[
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("all"), ArgType::NoArgument, 'a'),
        wopt(L!("short"), ArgType::NoArgument, 's'),
        wopt(L!("no-functions"), ArgType::NoArgument, 'f'),
        wopt(L!("type"), ArgType::NoArgument, 't'),
        wopt(L!("path"), ArgType::NoArgument, 'p'),
        wopt(L!("force-path"), ArgType::NoArgument, 'P'),
        wopt(L!("query"), ArgType::NoArgument, 'q'),
        wopt(L!("quiet"), ArgType::NoArgument, 'q'),
    ];

    let mut w = WGetopter::new(shortopts, longopts, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'a' => opts.all = true,
            's' => opts.short_output = true,
            'f' => opts.no_functions = true,
            't' => opts.get_type = true,
            'p' => opts.path = true,
            'P' => opts.force_path = true,
            'q' => opts.query = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(SUCCESS);
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(
                    parser,
                    streams,
                    cmd,
                    argv[w.wopt_index - 1],
                    print_hints,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    if opts.query as i64 + opts.path as i64 + opts.get_type as i64 + opts.force_path as i64 > 1 {
        streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        return Err(STATUS_INVALID_ARGS);
    }

    let mut res = false;

    let optind = w.wopt_index;
    for arg in argv.iter().take(argc).skip(optind) {
        let mut found = 0;
        if !opts.force_path && !opts.no_functions {
            if let Some(props) = function::get_props_autoload(arg, parser) {
                found += 1;
                res = true;
                // Early out - query means *any of the args exists*.
                if opts.query {
                    return Ok(SUCCESS);
                }
                if !opts.get_type {
                    let path = props.definition_file().unwrap_or(L!(""));
                    let mut comment = WString::new();

                    if path.is_empty() {
                        comment.push_utfstr(&wgettext!("Defined interactively"));
                    } else if path == "-" {
                        comment.push_utfstr(&wgettext!("Defined via `source`"));
                    } else {
                        let lineno: i32 = props.definition_lineno();
                        comment.push_utfstr(&wgettext_fmt!(
                            "Defined in %s @ line %d",
                            path,
                            lineno
                        ));
                    }

                    if props.is_copy() {
                        let path = props.copy_definition_file().unwrap_or(L!(""));
                        if path.is_empty() {
                            comment.push_utfstr(&wgettext!(", copied interactively"));
                        } else if path == "-" {
                            comment.push_utfstr(&wgettext!(", copied via `source`"));
                        } else {
                            let lineno = props.copy_definition_lineno();
                            comment.push_utfstr(&wgettext_fmt!(
                                ", copied in %s @ line %d",
                                path,
                                lineno
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
                        streams.out.append(&wgettext_fmt!("%s is a function", arg));
                        streams.out.appendln(wgettext!(" with definition"));
                        let mut def = WString::new();
                        def.push_utfstr(&sprintf!(
                            "# %s\n%s",
                            comment,
                            props.annotated_definition(arg)
                        ));
                        if props.definition_file().is_none() {
                            def = apply_indents(&def, &parse_util_compute_indents(&def));
                        }

                        if streams.out_is_terminal() {
                            let mut color = vec![];
                            highlight_shell(
                                &def,
                                &mut color,
                                &parser.context(),
                                /*io_ok=*/ false,
                                /*cursor=*/ None,
                            );
                            let col = bytes2wcstring(&colorize(&def, &color, parser.vars()));
                            streams.out.append(&col);
                        } else {
                            streams.out.append(&def);
                        }
                    } else {
                        streams.out.append(&wgettext_fmt!("%s is a function", arg));
                        streams.out.append(&wgettext_fmt!(" (%s)\n", comment));
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
                return Ok(SUCCESS);
            }
            if !opts.get_type {
                streams.out.append(&wgettext_fmt!("%s is a builtin\n", arg));
            } else if opts.get_type {
                streams.out.append(L!("builtin\n"));
            }
            if !opts.all {
                continue;
            }
        }

        let paths = if opts.all {
            path_get_paths(arg, parser.vars())
        } else {
            match path_get_path(arg, parser.vars()) {
                Some(p) => vec![p],
                None => vec![],
            }
        };

        for path in paths.iter() {
            found += 1;
            res = true;
            if opts.query {
                return Ok(SUCCESS);
            }
            if !opts.get_type {
                if opts.path || opts.force_path {
                    streams.out.appendln(path);
                } else {
                    streams.out.append(&wgettext_fmt!("%s is %s\n", arg, path));
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
            streams
                .err
                .append(&wgettext_fmt!("%s: Could not find '%s'\n", L!("type"), arg));
        }
    }

    if res {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}
