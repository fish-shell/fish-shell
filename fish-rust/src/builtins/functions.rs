use super::prelude::*;
use crate::common::escape_string;
use crate::common::reformat_for_screen;
use crate::common::str2wcstring;
use crate::common::valid_func_name;
use crate::common::{EscapeFlags, EscapeStringStyle};
use crate::event::{self};
use crate::function;
use crate::highlight::colorize;
use crate::highlight::highlight_shell;
use crate::parser::Parser;
use crate::parser_keywords::parser_keywords_is_reserved;
use crate::termsize::termsize_last;

struct FunctionsCmdOpts {
    print_help: bool,
    erase: bool,
    list: bool,
    show_hidden: bool,
    query: bool,
    copy: bool,
    report_metadata: bool,
    no_metadata: bool,
    verbose: bool,
    handlers: bool,
    handlers_type: Option<WString>,
    description: Option<WString>,
}

impl Default for FunctionsCmdOpts {
    fn default() -> Self {
        Self {
            print_help: false,
            erase: false,
            list: false,
            show_hidden: false,
            query: false,
            copy: false,
            report_metadata: false,
            no_metadata: false,
            verbose: false,
            handlers: false,
            handlers_type: None,
            description: None,
        }
    }
}

const NO_METADATA_SHORT: char = 2 as char;

const SHORT_OPTIONS: &wstr = L!(":Ht:Dacd:ehnqv");
#[rustfmt::skip]
const LONG_OPTIONS: &[woption] = &[
    wopt(L!("erase"), woption_argument_t::no_argument, 'e'),
    wopt(L!("description"), woption_argument_t::required_argument, 'd'),
    wopt(L!("names"), woption_argument_t::no_argument, 'n'),
    wopt(L!("all"), woption_argument_t::no_argument, 'a'),
    wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    wopt(L!("query"), woption_argument_t::no_argument, 'q'),
    wopt(L!("copy"), woption_argument_t::no_argument, 'c'),
    wopt(L!("details"), woption_argument_t::no_argument, 'D'),
    wopt(L!("no-details"), woption_argument_t::no_argument, NO_METADATA_SHORT),
    wopt(L!("verbose"), woption_argument_t::no_argument, 'v'),
    wopt(L!("handlers"), woption_argument_t::no_argument, 'H'),
    wopt(L!("handlers-type"), woption_argument_t::required_argument, 't'),
];

/// Parses options to builtin function, populating opts.
/// Returns an exit status.
fn parse_cmd_opts<'args>(
    opts: &mut FunctionsCmdOpts,
    optind: &mut usize,
    argv: &'args mut [WString],
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let cmd = L!("functions");
    let print_hints = false;
    let mut w = wgetopter_t::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(opt) = w.wgetopt_long() {
        match opt {
            'v' => opts.verbose = true,
            'e' => opts.erase = true,
            'D' => opts.report_metadata = true,
            NO_METADATA_SHORT => opts.no_metadata = true,
            'd' => {
                opts.description = Some(w.woptarg().unwrap().to_owned());
            }
            'n' => opts.list = true,
            'a' => opts.show_hidden = true,
            'h' => opts.print_help = true,
            'q' => opts.query = true,
            'c' => opts.copy = true,
            'H' => opts.handlers = true,
            't' => {
                opts.handlers = true;
                opts.handlers_type = Some(w.woptarg().unwrap().to_owned());
            }
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    cmd,
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, &w.argv()[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            other => {
                panic!("Unexpected retval from wgetopt_long: {}", other);
            }
        }
    }

    *optind = w.woptind;
    STATUS_CMD_OK
}

pub fn functions(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    let mut opts = FunctionsCmdOpts::default();
    let mut optind = 0;
    let retval = parse_cmd_opts(&mut opts, &mut optind, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }
    let cmd = &args[0];
    // Shadow our args with the positionals
    let args = &args[optind..];

    if opts.print_help {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_CMD_OK;
    }

    let describe = opts.description.is_some();
    if [describe, opts.erase, opts.list, opts.query, opts.copy]
        .into_iter()
        .filter(|b| *b)
        .count()
        > 1
    {
        streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if opts.report_metadata && opts.no_metadata {
        streams.err.append(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if opts.erase {
        for arg in args {
            function::remove(arg);
        }
        // Historical - this never failed?
        return STATUS_CMD_OK;
    }

    if let Some(desc) = opts.description {
        if args.len() != 1 {
            streams.err.append(&wgettext_fmt!(
                "%ls: Expected exactly one function name\n",
                cmd
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
        let current_func = &args[0];

        if !function::exists(current_func, parser) {
            streams.err.append(&wgettext_fmt!(
                "%ls: Function '%ls' does not exist\n",
                cmd,
                current_func
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        function::set_desc(&current_func, desc.into(), parser);
        return STATUS_CMD_OK;
    }

    if opts.report_metadata {
        if args.len() != 1 {
            streams.err.append(&wgettext_fmt!(
                BUILTIN_ERR_ARG_COUNT2,
                cmd,
                // This error is
                // functions: --details: expected 1 arguments; got 2
                // The "--details" was "argv[optind - 1]" in the C++
                // which would just give the last option.
                // This is broken because you could do `functions --details --verbose foo bar`, and it would error about "--verbose".
                "--details",
                1,
                args.len()
            ));
            return STATUS_INVALID_ARGS;
        }
        let props = function::get_props_autoload(&args[0], parser);
        let def_file = if let Some(p) = props.as_ref() {
            if let Some(cpf) = &p.copy_definition_file {
                cpf.as_ref().to_owned()
            } else if let Some(df) = &p.definition_file {
                df.as_ref().to_owned()
            } else {
                L!("stdin").to_owned()
            }
        } else {
            L!("n/a").to_owned()
        };
        streams.out.appendln_owned(def_file);

        if opts.verbose {
            let copy_place = match props.as_ref() {
                Some(p) if p.copy_definition_file.is_some() => {
                    if let Some(df) = &p.definition_file {
                        df.as_ref().to_owned()
                    } else {
                        L!("stdin").to_owned()
                    }
                }
                Some(p) if p.is_autoload.load() => L!("autoloaded").to_owned(),
                Some(p) if !p.is_autoload.load() => L!("not-autoloaded").to_owned(),
                _ => L!("n/a").to_owned(),
            };
            streams.out.appendln_owned(copy_place);
            let line = if let Some(p) = props.as_ref() {
                p.definition_lineno()
            } else {
                0
            };
            streams.out.appendln_owned(line.to_wstring());

            let shadow = match props.as_ref() {
                Some(p) if p.shadow_scope => L!("scope-shadowing").to_owned(),
                Some(p) if !p.shadow_scope => L!("no-scope-shadowing").to_owned(),
                _ => L!("n/a").to_owned(),
            };
            streams.out.appendln_owned(shadow);

            let desc = match props.as_ref() {
                Some(p) if !p.description.is_empty() => escape_string(
                    &p.description,
                    EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
                ),
                Some(p) if p.description.is_empty() => L!("").to_owned(),
                _ => L!("n/a").to_owned(),
            };
            streams.out.appendln_owned(desc);
        }
        // Historical - this never failed?
        return STATUS_CMD_OK;
    }

    if opts.handlers {
        // Empty handlers-type is the same as "all types".
        if !opts
            .handlers_type
            .as_ref()
            .map(|s| s.is_empty())
            .unwrap_or(true)
            && !event::EVENT_FILTER_NAMES.contains(&opts.handlers_type.as_ref().unwrap().as_ref())
        {
            streams.err.append(&wgettext_fmt!(
                "%ls: Expected generic | variable | signal | exit | job-id for --handlers-type\n",
                cmd
            ));
            return STATUS_INVALID_ARGS;
        }
        event::print(streams, &opts.handlers_type.unwrap_or(L!("").into()));
        return STATUS_CMD_OK;
    }

    if opts.query && args.is_empty() {
        return STATUS_CMD_ERROR;
    }

    if opts.list || args.is_empty() {
        let mut names = function::get_names(opts.show_hidden);
        names.sort();
        if streams.out_is_terminal() {
            let mut buff = WString::new();
            let mut first: bool = true;
            for name in names {
                if !first {
                    buff.push_utfstr(L!(", "));
                }
                buff.push_utfstr(&name);
                first = false;
            }
            streams
                .out
                .append(&reformat_for_screen(&buff, &termsize_last()));
        } else {
            for name in names {
                streams.out.appendln_owned(name);
            }
        }
        return STATUS_CMD_OK;
    }

    if opts.copy {
        if args.len() != 2 {
            streams.err.append(&wgettext_fmt!(
                "%ls: Expected exactly two names (current function name, and new function name)\n",
                cmd
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
        let current_func = &args[0];
        let new_func = &args[1];

        if !function::exists(current_func, parser) {
            streams.err.append(&wgettext_fmt!(
                "%ls: Function '%ls' does not exist\n",
                cmd,
                current_func
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        if !valid_func_name(new_func) || parser_keywords_is_reserved(new_func) {
            streams.err.append(&wgettext_fmt!(
                "%ls: Illegal function name '%ls'\n",
                cmd,
                new_func
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        if function::exists(new_func, parser) {
            streams.err.append(&wgettext_fmt!(
                "%ls: Function '%ls' already exists. Cannot create copy '%ls'\n",
                cmd,
                new_func,
                current_func
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }
        if function::copy(current_func, new_func.into(), parser) {
            return STATUS_CMD_OK;
        }
        return STATUS_CMD_ERROR;
    }

    let mut res: c_int = STATUS_CMD_OK.unwrap();

    let mut first = true;
    for arg in args.iter() {
        let Some(props) = function::get_props_autoload(arg, parser) else {
            res += 1;
            first = false;
            continue;
        };
        if opts.query {
            continue;
        }
        if !first {
            streams.out.append(L!("\n"));
        };

        let mut comment = WString::new();
        if !opts.no_metadata {
            // TODO: This is duplicated in type.
            // Extract this into a helper.
            match props.definition_file() {
                Some(path) if path == "-" => {
                    comment.push_utfstr(&wgettext!("Defined via `source`"))
                }
                Some(path) => {
                    comment.push_utfstr(&wgettext_fmt!(
                        "Defined in %ls @ line %d",
                        path,
                        props.definition_lineno()
                    ));
                }
                None => comment.push_utfstr(&wgettext_fmt!("Defined interactively")),
            }

            if props.is_copy() {
                match props.copy_definition_file() {
                    Some(path) if path == "-" => {
                        comment.push_utfstr(&wgettext_fmt!(", copied via `source`"))
                    }
                    Some(path) => {
                        comment.push_utfstr(&wgettext_fmt!(
                            ", copied in %ls @ line %d",
                            path,
                            props.copy_definition_lineno()
                        ));
                    }
                    None => comment.push_utfstr(&wgettext_fmt!(", copied interactively")),
                }
            }
        }

        let mut def = WString::new();

        if !comment.is_empty() {
            def.push_utfstr(&sprintf!(
                "# %ls\n%ls",
                comment,
                props.annotated_definition(arg)
            ));
        } else {
            def = props.annotated_definition(arg);
        }

        if streams.out_is_terminal() {
            let mut colors = vec![];
            highlight_shell(&def, &mut colors, &parser.context(), false, None);
            streams
                .out
                .append(&str2wcstring(&colorize(&def, &mut colors, parser.vars())));
        } else {
            streams.out.append(&def);
        }
        first = false;
    }

    return Some(res);
}
