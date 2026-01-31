use super::prelude::*;
use crate::common::bytes2wcstring;
use crate::common::escape_string;
use crate::common::reformat_for_screen;
use crate::common::valid_func_name;
use crate::common::{EscapeFlags, EscapeStringStyle};
use crate::event::{self};
use crate::function;
use crate::highlight::highlight_and_colorize;
use crate::parse_util::apply_indents;
use crate::parse_util::compute_indents;
use crate::parser_keywords::parser_keywords_is_reserved;
use crate::termsize::termsize_last;

#[derive(Default)]
struct FunctionsCmdOpts<'args> {
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
    color: ColorEnabled,
    handlers_type: Option<&'args wstr>,
    description: Option<&'args wstr>,
}

const NO_METADATA_SHORT: char = 2 as char;

const SHORT_OPTIONS: &wstr = L!("Ht:Dacd:ehnqv");
#[rustfmt::skip]
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("erase"), ArgType::NoArgument, 'e'),
    wopt(L!("description"), ArgType::RequiredArgument, 'd'),
    wopt(L!("names"), ArgType::NoArgument, 'n'),
    wopt(L!("all"), ArgType::NoArgument, 'a'),
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("query"), ArgType::NoArgument, 'q'),
    wopt(L!("copy"), ArgType::NoArgument, 'c'),
    wopt(L!("details"), ArgType::NoArgument, 'D'),
    wopt(L!("no-details"), ArgType::NoArgument, NO_METADATA_SHORT),
    wopt(L!("verbose"), ArgType::NoArgument, 'v'),
    wopt(L!("handlers"), ArgType::NoArgument, 'H'),
    wopt(L!("handlers-type"), ArgType::RequiredArgument, 't'),
    wopt(L!("color"), ArgType::RequiredArgument, COLOR_OPTION_CHAR),
];

/// Parses options to builtin function, populating opts.
/// Returns an exit status.
fn parse_cmd_opts<'args>(
    opts: &mut FunctionsCmdOpts<'args>,
    optind: &mut usize,
    argv: &mut [&'args wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let cmd = L!("functions");
    let print_hints = false;
    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            'v' => opts.verbose = true,
            'e' => opts.erase = true,
            'D' => opts.report_metadata = true,
            NO_METADATA_SHORT => opts.no_metadata = true,
            'd' => {
                opts.description = Some(w.woptarg.unwrap());
            }
            'n' => opts.list = true,
            'a' => opts.show_hidden = true,
            'h' => opts.print_help = true,
            'q' => opts.query = true,
            'c' => opts.copy = true,
            'H' => opts.handlers = true,
            't' => {
                opts.handlers = true;
                opts.handlers_type = Some(w.woptarg.unwrap());
            }
            COLOR_OPTION_CHAR => {
                opts.color = ColorEnabled::parse_from_opt(streams, cmd, w.woptarg.unwrap())?;
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
            other => {
                panic!("Unexpected retval from WGetopter: {}", other);
            }
        }
    }

    *optind = w.wopt_index;
    Ok(SUCCESS)
}

pub fn functions(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let Some(&cmd) = args.first() else {
        return Err(STATUS_INVALID_ARGS);
    };

    localizable_consts! {
        FUNCTION_DOES_NOT_EXIST
        "%s: Function '%s' does not exist"
    }

    let mut opts = FunctionsCmdOpts::default();
    let mut optind = 0;

    parse_cmd_opts(&mut opts, &mut optind, args, parser, streams)?;

    // Shadow our args with the positionals
    let args = &args[optind..];

    if opts.print_help {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Ok(SUCCESS);
    }

    let describe = opts.description.is_some();
    if [describe, opts.erase, opts.list, opts.query, opts.copy]
        .into_iter()
        .filter(|b| *b)
        .count()
        > 1
    {
        streams.err.appendln(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if opts.report_metadata && opts.no_metadata {
        streams.err.appendln(&wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if opts.erase {
        for arg in args {
            function::remove(arg);
        }
        // Historical - this never failed?
        return Ok(SUCCESS);
    }

    if let Some(desc) = opts.description {
        if args.len() != 1 {
            streams.err.appendln(&wgettext_fmt!(
                "%s: Expected exactly one function name",
                cmd
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }
        let current_func = args[0];

        if !function::exists(current_func, parser) {
            streams
                .err
                .appendln(&wgettext_fmt!(FUNCTION_DOES_NOT_EXIST, cmd, current_func));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_CMD_ERROR);
        }

        function::set_desc(current_func, desc.into(), parser);
        return Ok(SUCCESS);
    }

    if opts.report_metadata {
        if args.len() != 1 {
            streams.err.appendln(&wgettext_fmt!(
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
            return Err(STATUS_INVALID_ARGS);
        }
        let props = function::get_props_autoload(args[0], parser);
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
        streams.out.appendln(&def_file);

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
            streams.out.appendln(&copy_place);
            let line = if let Some(p) = props.as_ref() {
                p.definition_lineno()
            } else {
                0
            };
            streams.out.appendln(&line.to_wstring());

            let shadow = match props.as_ref() {
                Some(p) if p.shadow_scope => L!("scope-shadowing").to_owned(),
                Some(p) if !p.shadow_scope => L!("no-scope-shadowing").to_owned(),
                _ => L!("n/a").to_owned(),
            };
            streams.out.appendln(&shadow);

            let desc = match props.as_ref() {
                Some(p) => {
                    let localized_description = p.description.localize();
                    if localized_description.is_empty() {
                        L!("").to_owned()
                    } else {
                        escape_string(
                            localized_description,
                            EscapeStringStyle::Script(
                                EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED,
                            ),
                        )
                    }
                }
                _ => L!("n/a").to_owned(),
            };
            streams.out.appendln(&desc);
        }
        // Historical - this never failed?
        return Ok(SUCCESS);
    }

    if opts.handlers {
        // Empty handlers-type is the same as "all types".
        if !opts.handlers_type.unwrap_or(L!("")).is_empty()
            && !event::EVENT_FILTER_NAMES.contains(&opts.handlers_type.unwrap())
        {
            streams.err.appendln(&wgettext_fmt!(
                "%s: Expected %s for %s",
                cmd,
                "generic | variable | signal | exit | job-id",
                "--handlers-type",
            ));
            return Err(STATUS_INVALID_ARGS);
        }
        event::print(streams, opts.handlers_type.unwrap_or(L!("")));
        return Ok(SUCCESS);
    }

    if opts.query && args.is_empty() {
        return Err(STATUS_CMD_ERROR);
    }

    if opts.list || args.is_empty() {
        let mut names = function::get_names(opts.show_hidden, parser.vars());
        names.sort();
        if opts.color.enabled(streams) {
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
                streams.out.appendln(&name);
            }
        }
        return Ok(SUCCESS);
    }

    if opts.copy {
        if args.len() != 2 {
            streams.err.appendln(&wgettext_fmt!(
                "%s: Expected exactly two names (current function name, and new function name)",
                cmd
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }
        let current_func = args[0];
        let new_func = args[1];

        if !function::exists(current_func, parser) {
            streams
                .err
                .appendln(&wgettext_fmt!(FUNCTION_DOES_NOT_EXIST, cmd, current_func));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_CMD_ERROR);
        }

        if !valid_func_name(new_func) || parser_keywords_is_reserved(new_func) {
            streams.err.appendln(&wgettext_fmt!(
                "%s: Illegal function name '%s'",
                cmd,
                new_func
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        if function::exists(new_func, parser) {
            streams.err.appendln(&wgettext_fmt!(
                "%s: Function '%s' already exists. Cannot create copy of '%s'",
                cmd,
                new_func,
                current_func
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_CMD_ERROR);
        }
        if function::copy(current_func, new_func.into(), parser) {
            return Ok(SUCCESS);
        }
        return Err(STATUS_CMD_ERROR);
    }

    let mut res: c_int = 0;

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
        }

        let mut comment = WString::new();
        if !opts.no_metadata {
            // TODO: This is duplicated in type.
            // Extract this into a helper.
            match props.definition_file() {
                Some(path) if path == "-" => {
                    comment.push_utfstr(&wgettext!("Defined via `source`"));
                }
                Some(path) => {
                    comment.push_utfstr(&wgettext_fmt!(
                        "Defined in %s @ line %d",
                        path,
                        props.definition_lineno()
                    ));
                }
                None => comment.push_utfstr(&wgettext!("Defined interactively")),
            }

            if props.is_copy() {
                match props.copy_definition_file() {
                    Some(path) if path == "-" => {
                        comment.push_utfstr(&wgettext!(", copied via `source`"));
                    }
                    Some(path) => {
                        comment.push_utfstr(&wgettext_fmt!(
                            ", copied in %s @ line %d",
                            path,
                            props.copy_definition_lineno()
                        ));
                    }
                    None => comment.push_utfstr(&wgettext!(", copied interactively")),
                }
            }
        }

        let mut def = WString::new();

        if !comment.is_empty() {
            def.push_utfstr(&sprintf!(
                "# %s\n%s",
                comment,
                props.annotated_definition(arg)
            ));
        } else {
            def = props.annotated_definition(arg);
        }

        if props.definition_file().is_none() {
            def = apply_indents(&def, &compute_indents(&def));
        }

        if opts.color.enabled(streams) {
            streams.out.append(&bytes2wcstring(&highlight_and_colorize(
                &def,
                &parser.context(),
                parser.vars(),
            )));
        } else {
            streams.out.append(&def);
        }
        first = false;
    }

    BuiltinResult::from_dynamic(res)
}
