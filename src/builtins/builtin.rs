use super::prelude::*;

#[derive(Default)]
struct builtin_cmd_opts_t {
    query: bool,
    list_names: bool,
}

pub fn r#builtin(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let cmd = argv[0];
    let argc = argv.len();
    let print_hints = false;
    let mut opts: builtin_cmd_opts_t = Default::default();

    const shortopts: &wstr = L!("hnq");
    const longopts: &[WOption] = &[
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("names"), ArgType::NoArgument, 'n'),
        wopt(L!("query"), ArgType::NoArgument, 'q'),
    ];

    let mut w = WGetopter::new(shortopts, longopts, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'q' => opts.query = true,
            'n' => opts.list_names = true,
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
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    if opts.query && opts.list_names {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_COMBO2,
            cmd,
            wgettext!("--query and --names are mutually exclusive")
        ));
        return Err(STATUS_INVALID_ARGS);
    }

    // If we don't have either, we print our help.
    // This is also what e.g. command and time,
    // the other decorator/builtins do.
    if !opts.query && !opts.list_names {
        builtin_print_help(parser, streams, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if opts.query {
        let optind = w.wopt_index;
        for arg in argv.iter().take(argc).skip(optind) {
            if builtin_exists(arg) {
                return Ok(SUCCESS);
            }
        }
        return Err(STATUS_CMD_ERROR);
    }

    if opts.list_names {
        // List is guaranteed to be sorted by name.
        let names = builtin_get_names();
        for name in names {
            streams.out.appendln(name);
        }
    }

    Ok(SUCCESS)
}
