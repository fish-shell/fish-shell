use std::collections::HashMap;

use super::prelude::*;

use crate::env::{EnvMode, EnvStack};
use crate::exec::exec_subshell;
use crate::wutil::fish_iswalnum;

const VAR_NAME_PREFIX: &wstr = L!("_flag_");

localizable_consts!(
    BUILTIN_ERR_INVALID_OPT_SPEC
    "%s: Invalid option spec '%s' at char '%c'\n"
);

#[derive(Default)]
struct OptionSpec<'args> {
    short_flag: char,
    long_flag: &'args wstr,
    validation_command: &'args wstr,
    vals: Vec<WString>,
    short_flag_valid: bool,
    delete: bool,
    arg_type: ArgType,
    accumulate_args: bool,
    num_seen: isize,
}

impl OptionSpec<'_> {
    fn new(s: char) -> Self {
        Self {
            short_flag: s,
            short_flag_valid: true,
            arg_type: ArgType::NoArgument,
            accumulate_args: true,
            ..Default::default()
        }
    }
}

#[derive(Default, PartialEq)]
enum UnknownHandling {
    #[default]
    Error,
    Ignore,
    Move,
}

#[derive(Default)]
struct ArgParseCmdOpts<'args> {
    unknown_handling: UnknownHandling,
    unknown_arguments: ArgType,
    strict_long_opts: bool,
    print_help: bool,
    stop_nonopt: bool,
    min_args: usize,
    max_args: usize,
    implicit_int_flag: char,
    name: WString,
    raw_exclusive_flags: Vec<&'args wstr>,
    args: Vec<Cow<'args, wstr>>,
    args_opts: Vec<Cow<'args, wstr>>,
    options: HashMap<char, OptionSpec<'args>>,
    long_to_short_flag: HashMap<WString, char>,
    exclusive_flag_sets: Vec<Vec<char>>,
}

impl ArgParseCmdOpts<'_> {
    fn new() -> Self {
        Self {
            max_args: usize::MAX,
            unknown_arguments: ArgType::OptionalArgument,
            ..Default::default()
        }
    }
}

const SHORT_OPTIONS: &wstr = L!("+hn:siuU:x:SN:X:");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("stop-nonopt"), ArgType::NoArgument, 's'),
    wopt(L!("ignore-unknown"), ArgType::NoArgument, 'i'),
    wopt(L!("move-unknown"), ArgType::NoArgument, 'u'),
    wopt(L!("unknown-arguments"), ArgType::RequiredArgument, 'U'),
    wopt(L!("name"), ArgType::RequiredArgument, 'n'),
    wopt(L!("exclusive"), ArgType::RequiredArgument, 'x'),
    wopt(L!("strict-longopts"), ArgType::NoArgument, 'S'),
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("min-args"), ArgType::RequiredArgument, 'N'),
    wopt(L!("max-args"), ArgType::RequiredArgument, 'X'),
];

// Check if any pair of mutually exclusive options was seen. Note that since every option must have
// a short name we only need to check those.
fn check_for_mutually_exclusive_flags(
    opts: &ArgParseCmdOpts,
    streams: &mut IoStreams,
) -> BuiltinResult {
    for opt_spec in opts.options.values() {
        if opt_spec.num_seen == 0 {
            continue;
        }

        // We saw this option at least once. Check all the sets of mutually exclusive options to see
        // if this option appears in any of them.
        for xarg_set in &opts.exclusive_flag_sets {
            if xarg_set.contains(&opt_spec.short_flag) {
                // Okay, this option is in a mutually exclusive set of options. Check if any of the
                // other mutually exclusive options have been seen.
                for xflag in xarg_set {
                    let Some(xopt_spec) = opts.options.get(xflag) else {
                        continue;
                    };

                    // Ignore this flag in the list of mutually exclusive flags.
                    if xopt_spec.short_flag == opt_spec.short_flag {
                        continue;
                    }

                    // If it is a different flag check if it has been seen.
                    if xopt_spec.num_seen != 0 {
                        let mut flag1: WString = WString::new();
                        if opt_spec.short_flag_valid {
                            flag1.push(opt_spec.short_flag);
                        }
                        if !opt_spec.long_flag.is_empty() {
                            if opt_spec.short_flag_valid {
                                flag1.push('/');
                            }
                            flag1.push_utfstr(&opt_spec.long_flag);
                        }

                        let mut flag2: WString = WString::new();
                        if xopt_spec.short_flag_valid {
                            flag2.push(xopt_spec.short_flag);
                        }
                        if !xopt_spec.long_flag.is_empty() {
                            if xopt_spec.short_flag_valid {
                                flag2.push('/');
                            }
                            flag2.push_utfstr(&xopt_spec.long_flag);
                        }

                        // We want the flag order to be deterministic. Primarily to make unit
                        // testing easier.
                        if flag1 > flag2 {
                            std::mem::swap(&mut flag1, &mut flag2);
                        }
                        streams.err.append(wgettext_fmt!(
                            "%s: %s %s: options cannot be used together\n",
                            opts.name,
                            flag1,
                            flag2
                        ));
                        return Err(STATUS_CMD_ERROR);
                    }
                }
            }
        }
    }

    Ok(SUCCESS)
}

// This should be called after all the option specs have been parsed. At that point we have enough
// information to parse the values associated with any `--exclusive` flags.
fn parse_exclusive_args(opts: &mut ArgParseCmdOpts, streams: &mut IoStreams) -> BuiltinResult {
    for raw_xflags in &opts.raw_exclusive_flags {
        let xflags: Vec<_> = raw_xflags.split(',').collect();
        if xflags.len() < 2 {
            streams.err.append(wgettext_fmt!(
                "%s: exclusive flag string '%s' is not valid\n",
                opts.name,
                raw_xflags
            ));
            return Err(STATUS_CMD_ERROR);
        }

        let exclusive_set: &mut Vec<char> = &mut vec![];
        for flag in xflags {
            if flag.char_count() == 1 && opts.options.contains_key(&flag.char_at(0)) {
                let short = flag.char_at(0);
                // It's a short flag.
                exclusive_set.push(short);
            } else if let Some(short_equiv) = opts.long_to_short_flag.get(flag) {
                // It's a long flag we store as its short flag equivalent.
                exclusive_set.push(*short_equiv);
            } else {
                streams.err.append(wgettext_fmt!(
                    "%s: exclusive flag '%s' is not valid\n",
                    opts.name,
                    flag
                ));
                return Err(STATUS_CMD_ERROR);
            }
        }

        // Store the set of exclusive flags for use when parsing the supplied set of arguments.
        opts.exclusive_flag_sets.push(exclusive_set.to_vec());
    }
    Ok(SUCCESS)
}

fn parse_flag_modifiers<'args>(
    opts: &ArgParseCmdOpts<'args>,
    opt_spec: &mut OptionSpec<'args>,
    option_spec: &wstr,
    opt_spec_str: &mut &'args wstr,
    streams: &mut IoStreams,
) -> bool {
    let mut s = *opt_spec_str;

    if opt_spec.short_flag == opts.implicit_int_flag
        && !s.is_empty()
        && s.char_at(0) != '!'
        && s.char_at(0) != '&'
    {
        streams.err.append(wgettext_fmt!(
            "%s: Implicit int short flag '%c' does not allow modifiers like '%c'\n",
            opts.name,
            opt_spec.short_flag,
            s.char_at(0)
        ));
        return false;
    }

    if s.char_at(0) == '=' {
        s = s.slice_from(1);
        (opt_spec.arg_type, opt_spec.accumulate_args) = match s.char_at(0) {
            '?' => {
                s = s.slice_from(1);
                (ArgType::OptionalArgument, false)
            }
            '+' => {
                s = s.slice_from(1);
                (ArgType::RequiredArgument, true)
            }
            '*' => {
                s = s.slice_from(1);
                (ArgType::OptionalArgument, true)
            }
            _ => (ArgType::RequiredArgument, false),
        };
    }

    if s.char_at(0) == '&' {
        opt_spec.delete = true;
        s = s.slice_from(1);
    }

    if s.char_at(0) == '!' {
        if opt_spec.arg_type == ArgType::NoArgument {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_INVALID_OPT_SPEC,
                opts.name,
                option_spec,
                s.char_at(0)
            ));
        }
        s = s.slice_from(1);
        opt_spec.validation_command = s;
        // Move cursor to the end so we don't expect a long flag.
        s = s.slice_from(s.char_count());
    } else if !s.is_empty() {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_INVALID_OPT_SPEC,
            opts.name,
            option_spec,
            s.char_at(0)
        ));
        return false;
    }

    // Make sure we have some validation for implicit int flags.
    if opt_spec.short_flag == opts.implicit_int_flag && opt_spec.validation_command.is_empty() {
        opt_spec.validation_command = L!("_validate_int");
    }

    if opts.options.contains_key(&opt_spec.short_flag) {
        streams.err.append(wgettext_fmt!(
            "%s: Short flag '%c' already defined\n",
            opts.name,
            opt_spec.short_flag
        ));
        return false;
    }

    *opt_spec_str = s;
    return true;
}

/// Parse the text following the short flag letter.
fn parse_option_spec_sep<'args>(
    opts: &mut ArgParseCmdOpts<'args>,
    opt_spec: &mut OptionSpec<'args>,
    option_spec: &'args wstr,
    opt_spec_str: &mut &'args wstr,
    counter: &mut u32,
    streams: &mut IoStreams,
) -> bool {
    let mut s = *opt_spec_str;
    let mut i = 1usize;
    // C++ used -1 to check for # here, we instead adjust opt_spec_str to start one earlier
    if s.char_at(i - 1) == '#' {
        if s.char_at(i) != '-' {
            // Long-only!
            i -= 1;
            opt_spec.short_flag = char::from_u32(*counter).unwrap();
            *counter += 1;
        }
        if opts.implicit_int_flag != '\0' {
            streams.err.append(wgettext_fmt!(
                "%s: Implicit int flag '%c' already defined\n",
                opts.name,
                opts.implicit_int_flag
            ));
            return false;
        }
        opts.implicit_int_flag = opt_spec.short_flag;
        opt_spec.short_flag_valid = false;
        i += 1;
        *opt_spec_str = s.slice_from(i);
        return true;
    }

    match s.char_at(i) {
        '-' => {
            opt_spec.short_flag_valid = false;
            i += 1;
            if i == s.char_count() {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_INVALID_OPT_SPEC,
                    opts.name,
                    option_spec,
                    s.char_at(i - 1)
                ));
                return false;
            }
        }
        '/' => {
            i += 1; // the struct is initialized assuming short_flag_valid should be true
            if i == s.char_count() {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_INVALID_OPT_SPEC,
                    opts.name,
                    option_spec,
                    s.char_at(i - 1)
                ));
                return false;
            }
        }
        '#' => {
            if opts.implicit_int_flag != '\0' {
                streams.err.append(wgettext_fmt!(
                    "%s: Implicit int flag '%c' already defined\n",
                    opts.name,
                    opts.implicit_int_flag
                ));
                return false;
            }
            opts.implicit_int_flag = opt_spec.short_flag;
            opt_spec.arg_type = ArgType::RequiredArgument;
            opt_spec.accumulate_args = false;
            i += 1; // the struct is initialized assuming short_flag_valid should be true
        }
        '!' | '?' | '=' | '&' => {
            // Try to parse any other flag modifiers
            // parse_flag_modifiers assumes opt_spec_str starts where it should, not one earlier
            s = s.slice_from(i);
            i = 0;
            if !parse_flag_modifiers(opts, opt_spec, option_spec, &mut s, streams) {
                return false;
            }
        }
        _ => {
            // No short flag or separator, and no other modifiers, so this is a long only option.
            // Since getopt needs a wchar, we have a counter that we count up.
            opt_spec.short_flag_valid = false;
            if s.char_at(i - 1) != '/' {
                i -= 1
            }
            opt_spec.short_flag = char::from_u32(*counter).unwrap();
            *counter += 1;
        }
    }

    *opt_spec_str = s.slice_from(i);
    return true;
}

fn parse_option_spec<'args>(
    opts: &mut ArgParseCmdOpts<'args>,
    option_spec: &'args wstr,
    counter: &mut u32,
    streams: &mut IoStreams,
) -> bool {
    if option_spec.is_empty() {
        streams.err.append(wgettext_fmt!(
            "%s: An option spec must have at least a short or a long flag\n",
            opts.name
        ));
        return false;
    }

    let mut s = option_spec;
    if !fish_iswalnum(s.char_at(0)) && s.char_at(0) != '#' && !(s.char_at(0) == '/' && s.len() > 1)
    {
        streams.err.append(wgettext_fmt!(
            "%s: Short flag '%c' invalid, must be alphanum or '#'\n",
            opts.name,
            s.char_at(0)
        ));
        return false;
    }

    let mut opt_spec = OptionSpec::new(s.char_at(0));

    // Try parsing stuff after the short flag.
    if s.char_count() > 1
        && !parse_option_spec_sep(opts, &mut opt_spec, option_spec, &mut s, counter, streams)
    {
        return false;
    }

    // Collect any long flag name.
    if !s.is_empty() {
        let long_flag_char_count = s
            .chars()
            .take_while(|&c| c == '-' || c == '_' || fish_iswalnum(c))
            .count();

        if long_flag_char_count > 0 {
            opt_spec.long_flag = s.slice_to(long_flag_char_count);
            if opts.long_to_short_flag.contains_key(opt_spec.long_flag) {
                streams.err.append(wgettext_fmt!(
                    "%s: Long flag '%s' already defined\n",
                    opts.name,
                    opt_spec.long_flag
                ));
                return false;
            }
        }
        s = s.slice_from(long_flag_char_count);
    }

    if !parse_flag_modifiers(opts, &mut opt_spec, option_spec, &mut s, streams) {
        return false;
    }

    // Record our long flag if we have one.
    if !opt_spec.long_flag.is_empty() {
        let ins = opts
            .long_to_short_flag
            .insert(WString::from(opt_spec.long_flag), opt_spec.short_flag);
        assert!(ins.is_none(), "Should have inserted long flag");
    }

    // Record our option under its short flag.
    opts.options.insert(opt_spec.short_flag, opt_spec);

    return true;
}

fn collect_option_specs<'args>(
    opts: &mut ArgParseCmdOpts<'args>,
    optind: &mut usize,
    argc: usize,
    args: &[&'args wstr],
    streams: &mut IoStreams,
) -> BuiltinResult {
    let Some(&cmd) = args.get(0) else {
        return Err(STATUS_INVALID_ARGS);
    };

    // A counter to give short chars to long-only options because getopt needs that.
    // Luckily we have wgetopt so we can use wchars - this is one of the private use areas so we
    // have 6400 options available.
    let mut counter = 0xE000u32;

    loop {
        if *optind == argc {
            streams
                .err
                .append(wgettext_fmt!("%s: Missing -- separator\n", cmd));
            return Err(STATUS_INVALID_ARGS);
        }

        if "--" == args[*optind] {
            *optind += 1;
            break;
        }

        if !parse_option_spec(opts, args[*optind], &mut counter, streams) {
            return Err(STATUS_CMD_ERROR);
        }

        *optind += 1;
    }

    // Check for counter overreach once at the end because this is very unlikely to ever be reached.
    let counter_max = 0xF8FFu32;

    if counter > counter_max {
        streams
            .err
            .append(wgettext_fmt!("%s: Too many long-only options\n", cmd));
        return Err(STATUS_INVALID_ARGS);
    }

    return Ok(SUCCESS);
}

fn parse_cmd_opts<'args>(
    opts: &mut ArgParseCmdOpts<'args>,
    optind: &mut usize,
    argc: usize,
    args: &mut [&'args wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let Some(&cmd) = args.get(0) else {
        return Err(STATUS_INVALID_ARGS);
    };

    let mut args_read = Vec::with_capacity(args.len());
    args_read.extend_from_slice(args);

    let mut seen_unknown_arguments = false;
    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, args);
    while let Some(c) = w.next_opt() {
        match c {
            'n' => opts.name = w.woptarg.unwrap().to_owned(),
            's' => opts.stop_nonopt = true,
            'i' | 'u' => {
                if opts.unknown_handling != UnknownHandling::Error {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_COMBO2_EXCLUSIVE,
                        cmd,
                        "--ignore-unknown",
                        "--move-unknown"
                    ));
                    return Err(STATUS_INVALID_ARGS);
                };
                opts.unknown_handling = if c == 'i' {
                    UnknownHandling::Ignore
                } else {
                    UnknownHandling::Move
                }
            }
            'U' => {
                seen_unknown_arguments = true;
                let kind = w.woptarg.unwrap();
                opts.unknown_arguments = if kind == L!("optional") {
                    ArgType::OptionalArgument
                } else if kind == L!("required") {
                    ArgType::RequiredArgument
                } else if kind == L!("none") {
                    ArgType::NoArgument
                } else {
                    streams.err.append(wgettext_fmt!(
                        "%s: Invalid --unknown-arguments value '%s'\n",
                        cmd,
                        kind
                    ));
                    return Err(STATUS_INVALID_ARGS);
                }
            }
            'S' => opts.strict_long_opts = true,
            // Just save the raw string here. Later, when we have all the short and long flag
            // definitions we'll parse these strings into a more useful data structure.
            'x' => opts.raw_exclusive_flags.push(w.woptarg.unwrap()),
            'h' => opts.print_help = true,
            'N' => {
                opts.min_args = {
                    let x = fish_wcstol(w.woptarg.unwrap()).unwrap_or(-1);
                    if x < 0 {
                        streams.err.append(wgettext_fmt!(
                            "%s: Invalid --min-args value '%s'\n",
                            cmd,
                            w.woptarg.unwrap()
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    x.try_into().unwrap()
                }
            }
            'X' => {
                opts.max_args = {
                    let x = fish_wcstol(w.woptarg.unwrap()).unwrap_or(-1);
                    if x < 0 {
                        streams.err.append(wgettext_fmt!(
                            "%s: Invalid --max-args value '%s'\n",
                            cmd,
                            w.woptarg.unwrap()
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    x.try_into().unwrap()
                }
            }
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    cmd,
                    args[w.wopt_index - 1],
                    /* print_hints */ false,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(
                    parser,
                    streams,
                    cmd,
                    args[w.wopt_index - 1],
                    /* print_hints */ false,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from next_opt"),
        }
    }

    // Imply --unknown-arguments implies --move-unknown, unless --ignore-unknown was given
    if seen_unknown_arguments && opts.unknown_handling == UnknownHandling::Error {
        opts.unknown_handling = UnknownHandling::Move;
    }

    if opts.print_help {
        return Ok(SUCCESS);
    }

    if "--" == args_read[w.wopt_index - 1] {
        w.wopt_index -= 1;
    }

    if argc == w.wopt_index {
        // The user didn't specify any option specs.
        streams
            .err
            .append(wgettext_fmt!("%s: Missing -- separator\n", cmd));
        return Err(STATUS_INVALID_ARGS);
    }

    if opts.name.is_empty() {
        // If no name has been given, we default to the function name.
        // If any error happens, the backtrace will show which argparse it was.
        opts.name = parser
            .get_function_name(1)
            .unwrap_or_else(|| L!("argparse").to_owned());
    }

    *optind = w.wopt_index;
    return collect_option_specs(opts, optind, argc, args, streams);
}

fn populate_option_strings<'args>(
    opts: &ArgParseCmdOpts<'args>,
    short_options: &mut WString,
    long_options: &mut Vec<WOption<'args>>,
) {
    for opt_spec in opts.options.values() {
        if opt_spec.short_flag_valid {
            short_options.push(opt_spec.short_flag);
        }

        if opt_spec.short_flag_valid {
            match opt_spec.arg_type {
                ArgType::OptionalArgument => short_options.push_str("::"),
                ArgType::RequiredArgument => short_options.push_str(":"),
                ArgType::NoArgument => {}
            };
        }

        if !opt_spec.long_flag.is_empty() {
            long_options.push(wopt(
                opt_spec.long_flag,
                opt_spec.arg_type,
                opt_spec.short_flag,
            ));
        }
    }
}

fn validate_arg<'opts>(
    parser: &Parser,
    opts_name: &wstr,
    opt_spec: &mut OptionSpec<'opts>,
    is_long_flag: bool,
    woptarg: &'opts wstr,
    streams: &mut IoStreams,
) -> BuiltinResult {
    // Obviously if there is no arg validation command we assume the arg is okay.
    if opt_spec.validation_command.is_empty() {
        return Ok(SUCCESS);
    }

    let vars = parser.vars();
    vars.push(true /* new_scope */);

    let env_mode = EnvMode::LOCAL | EnvMode::EXPORT;
    vars.set_one(L!("_argparse_cmd"), env_mode, opts_name.to_owned());
    let flag_name = WString::from(VAR_NAME_PREFIX) + "name";
    if is_long_flag {
        vars.set_one(&flag_name, env_mode, opt_spec.long_flag.to_owned());
    } else {
        vars.set_one(
            &flag_name,
            env_mode,
            WString::from_chars(vec![opt_spec.short_flag]),
        );
    }
    vars.set_one(
        &(WString::from(VAR_NAME_PREFIX) + "value"),
        env_mode,
        woptarg.to_owned(),
    );

    let mut cmd_output = Vec::new();

    let retval = exec_subshell(
        opt_spec.validation_command,
        parser,
        Some(&mut cmd_output),
        false,
    );

    for output in cmd_output {
        streams.err.append(output);
        streams.err.append_char('\n');
    }
    vars.pop();
    retval.map(|()| SUCCESS)
}

/// Return whether the option 'opt' is an implicit integer option.
fn is_implicit_int(opts: &ArgParseCmdOpts, val: &wstr) -> bool {
    if opts.implicit_int_flag == '\0' {
        // There is no implicit integer option.
        return false;
    }

    // We succeed if this argument can be parsed as an integer.
    fish_wcstol(val).is_ok()
}

// Store this value under the implicit int option.
fn validate_and_store_implicit_int<'args>(
    parser: &Parser,
    opts: &mut ArgParseCmdOpts<'args>,
    val: &'args wstr,
    w: &mut WGetopter,
    is_long_flag: bool,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let opt_spec = opts.options.get_mut(&opts.implicit_int_flag).unwrap();
    if opt_spec.delete {
        // No need to call delete_flag, as w.argv_opts.last will be of form -<number>
        w.argv_opts.pop().unwrap();
    }
    validate_arg(parser, &opts.name, opt_spec, is_long_flag, val, streams)?;

    // It's a valid integer so store it and return success.
    opt_spec.vals.clear();
    opt_spec.vals.push(val.into());
    opt_spec.num_seen += 1;
    w.remaining_text = L!("");

    Ok(SUCCESS)
}

/// Delete the most recently matched flag, is_long_flag should be true if the flag was given with a
/// long flag name (even if it was given with a single - and not two). The returned value is the
/// deleted flag and its value (unless the value was given as a seperate argument)
fn delete_flag<'args>(w: &mut WGetopter<'_, 'args, '_>, is_long_flag: bool) -> Cow<'args, wstr> {
    // Does the option have a value that was a seperate argument to the option name itself? (e.g.
    // w.argv_opts ends in --<long_flag> <value> or -<other-short-options>...<short_flag> <value>)
    let separate_value = w
        .woptarg
        .zip(w.argv_opts.last())
        .is_some_and(|(value, last)| value == *last);
    if separate_value {
        // Remove the value
        w.argv_opts.pop();
        // There can't be any short options between the flag and its value
        assert!(w.remaining_text.is_empty());
    }

    // Remove the last option (we may have to put part of it back if it had other short options in
    // it)
    let opt_arg = w.argv_opts.pop().unwrap();
    assert!(opt_arg.starts_with("-"));

    if is_long_flag {
        // opt_arg will be of form --<long-flag>, except there may only be one -, and <long-flag>
        // may be abbreviated
        assert!(w.remaining_text.is_empty());
        // There will be no short options, so we're done
        return opt_arg;
    }

    // Otherwise, it's a short option so opt_arg will be of form:
    //    -<previous-short-ops>...<short-flag><more-short-ops>...
    //    -<previous-short-ops>...<short-flag><value>, or
    //    -<previous-short-ops>...<short-flag>
    let more_opts = w.remaining_text;
    let value = if separate_value {
        L!("")
    } else {
        w.woptarg.unwrap_or_default()
    };
    assert!(more_opts.is_empty() || value.is_empty()); // Both can't be present
    assert!(opt_arg.ends_with(more_opts));
    assert!(opt_arg.ends_with(value));

    // The length of <previous-short-ops>... (the -2 is for the '-' and <short-flag>)
    let previous_opts = opt_arg.len() - (more_opts.len() + value.len()) - 2;

    if previous_opts == 0 && more_opts.is_empty() {
        // opt_arg will be of form -<short-flag> or -<short-flag><value>
        // (i.e. there are no other options that we need to add back to w.argv_opts)
        return opt_arg;
    }

    // Set opt_arg_with to be opt_arg minus the <previous-short-ops>... (i.e. -<short-flag><value>)
    // (the +1 is to skip over the leading '-', and the +2 make it a one character slice)
    let opt_arg_with = L!("-").to_owned() + &opt_arg[previous_opts + 1..previous_opts + 2] + value;

    // Set opt_arg_without to to be opt_arg minus <short-flag><value> (i.e.
    // -<previous-short-opts>...<more-short-opts>). (the +1 is to skip over the leading '-')
    let opt_arg_without = opt_arg[..previous_opts + 1].to_owned() + more_opts;
    assert!(opt_arg.len() > 1); // There should be at least one short opt

    // Put the version without <short-flag> back, and return the version with it
    w.argv_opts.push(opt_arg_without.into());
    Cow::Owned(opt_arg_with)
}

fn handle_flag<'args>(
    parser: &Parser,
    opts: &mut ArgParseCmdOpts<'args>,
    opt: char,
    is_long_flag: bool,
    w: &mut WGetopter<'_, 'args, '_>,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let opt_spec = opts.options.get_mut(&opt).unwrap();
    if opt_spec.delete {
        delete_flag(w, is_long_flag);
    }

    opt_spec.num_seen += 1;
    if opt_spec.arg_type == ArgType::NoArgument {
        // It's a boolean flag. Save the flag we saw since it might be useful to know if the
        // short or long flag was given.
        assert!(opt_spec.accumulate_args);
        assert!(w.woptarg.is_none());
        let s = if is_long_flag {
            WString::from("--") + opt_spec.long_flag
        } else {
            WString::from_chars(['-', opt_spec.short_flag])
        };
        opt_spec.vals.push(s);
        return Ok(SUCCESS);
    }

    if let Some(woptarg) = w.woptarg {
        validate_arg(parser, &opts.name, opt_spec, is_long_flag, woptarg, streams)?;
    }

    if opt_spec.accumulate_args {
        if let Some(arg) = w.woptarg {
            opt_spec.vals.push(arg.into());
        } else {
            opt_spec.vals.push(WString::new());
        }
    } else {
        // We're depending on `next_opt()` to report that a mandatory value is missing if
        // `opt_spec->arg_type == ArgType::RequiredArgument` and thus return ':' so that we don't
        // take this branch if the mandatory arg is missing.
        opt_spec.vals.clear();
        if let Some(arg) = w.woptarg {
            opt_spec.vals.push(arg.into());
        }
    }

    Ok(SUCCESS)
}

fn argparse_parse_flags<'args>(
    parser: &Parser,
    opts: &mut ArgParseCmdOpts<'args>,
    argc: usize,
    args: &mut [&'args wstr],
    optind: &mut usize,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let mut args_read = Vec::with_capacity(args.len());
    args_read.extend_from_slice(args);

    // "+" means stop at nonopt, "-" means give nonoptions the option character code `1`, and don't
    // reorder.
    let mut short_options = WString::from(if opts.stop_nonopt { L!("+") } else { L!("-") });
    let mut long_options = vec![];
    populate_option_strings(opts, &mut short_options, &mut long_options);

    let mut w = WGetopter::new(&short_options, &long_options, args);
    w.strict_long_opts = opts.strict_long_opts;
    while let Some((opt, longopt_idx)) = w.next_opt_indexed() {
        let is_long_flag = longopt_idx.is_some();
        match opt {
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    &opts.name,
                    args_read[w.wopt_index - 1],
                    false,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(
                    parser,
                    streams,
                    &opts.name,
                    args_read[w.wopt_index - 1],
                    false,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                // It's not a recognized flag. See if it's an implicit int flag.
                let arg_contents = &args_read[w.wopt_index - 1].slice_from(1);

                if is_implicit_int(opts, arg_contents) {
                    validate_and_store_implicit_int(
                        parser,
                        opts,
                        arg_contents,
                        &mut w,
                        is_long_flag,
                        streams,
                    )?;
                } else if opts.unknown_handling == UnknownHandling::Error {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_UNKNOWN,
                        opts.name,
                        args_read[w.wopt_index - 1]
                    ));
                    return Err(STATUS_INVALID_ARGS);
                } else {
                    // The option is unknown, so there's no long opt index it could have used
                    assert!(!is_long_flag);
                    // arg_contents already skipped over the first '-'
                    let is_long_flag = arg_contents.starts_with("-");

                    if is_long_flag {
                        // For some reason, wgetopt parses unknown long options as if they where a
                        // short '-' flag, followed by the long flag name, interpreted either as the
                        // value for '-' or as remaining short options, so this fixes that
                        let rest = if let Some(value) = w.woptarg {
                            assert!(w.remaining_text.is_empty());
                            value
                        } else {
                            w.remaining_text
                        };
                        // The arguments was of form --<long-flag>=<value>, so extract out <value>
                        if let Some(i) = rest.find_char('=') {
                            w.woptarg = Some(&rest[i + 1..]);
                        }
                        // Ensure w.remaining_text is not misinterpreted as the value of the flag,
                        // or as remaining short options
                        w.remaining_text = L!("");
                    }

                    // Any unrecognized option is put back if ignore_unknown is used.
                    // This allows reusing the same argv in multiple argparse calls,
                    // or just ignoring the error (e.g. in completions).

                    // First we consume any remaining text as if it was the option's argument
                    if opts.unknown_arguments != ArgType::NoArgument && !w.remaining_text.is_empty()
                    {
                        assert!(w.woptarg.is_none()); // Both don't make sense
                        w.woptarg = Some(w.remaining_text);
                        // Explain to wgetopt that we want to skip to the next arg,
                        // because we can't handle this opt group.
                        w.remaining_text = L!("");
                    }

                    // If unknown options require arguments, but there weren't any worked out above,
                    // take the next element of w.argv as the argument
                    let separate_value = if opts.unknown_arguments == ArgType::RequiredArgument
                        && w.woptarg.is_none()
                    {
                        if w.wopt_index < w.argv.len() {
                            w.wopt_index += 1; // Tell wgetop to skip over the options value
                            Some(w.argv[w.wopt_index - 1])
                        } else {
                            // the option is at the end of argv, so it has no argument
                            streams.err.append(wgettext_fmt!(
                                BUILTIN_ERR_MISSING,
                                opts.name,
                                args_read[w.wopt_index - 1]
                            ));
                            return Err(STATUS_INVALID_ARGS);
                        }
                    } else {
                        None
                    };

                    // If the option uses the long flag syntax with a value (i.e. --<flag>=<value>)
                    if opts.unknown_arguments == ArgType::NoArgument
                        && is_long_flag
                        && arg_contents.contains('=')
                    {
                        streams.err.append(wgettext_fmt!(
                            BUILTIN_ERR_UNEXP_ARG,
                            opts.name,
                            args_read[w.wopt_index - 1]
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }

                    if opts.unknown_handling == UnknownHandling::Ignore {
                        // Now by calling delete_flag we ensure that if the unknown flag is
                        // precceded by known flags, the known flags are kept in $argv_opts, and not
                        // added to $argv. (any argument to the option is also returned in
                        // unknown_flag, and removed from opts.argv_opts) Now by calling delete_flag
                        // we ensure that if the unknown flag is precceded by known flags, the known
                        // flags are kept in $argv_opts, and not added to $argv. (any argument to
                        // the option is also returned in unknown_flag, and removed from
                        // opts.argv_opts)
                        let unknown_flag = delete_flag(&mut w, is_long_flag);
                        opts.args.push(unknown_flag);
                        // If the value of the argument was the next element of the input arguments,
                        // record it to be stored in the new $argv value
                        if let Some(value) = separate_value {
                            opts.args.push(Cow::Borrowed(value));
                        }
                    } else {
                        assert!(opts.unknown_handling == UnknownHandling::Move);
                        // w.argv_opts will already contain the option and its value, unless the
                        // value was given as a seperate argument
                        if let Some(value) = separate_value {
                            w.argv_opts.push(Cow::Borrowed(value));
                        }
                    }

                    // Make wgetopt keep reading this argument for more options
                    if !w.remaining_text.is_empty() {
                        w.wopt_index -= 1;
                    }

                    // Work around weirdness with wgetopt, which crashes if we `continue` here.
                    if w.wopt_index == argc {
                        break;
                    }
                }
            }
            NON_OPTION_CHAR => {
                // A non-option argument.
                // We use `-` as the first option-string-char to disable GNU getopt's reordering,
                // otherwise we'd get ignored options first and normal arguments later.
                // E.g. `argparse -i -- -t tango -w` needs to keep `-t tango -w` in $argv, not `-t -w
                // tango`.
                opts.args.push(Cow::Borrowed(args_read[w.wopt_index - 1]));
                continue;
            }
            // It's a recognized flag.
            _ => {
                handle_flag(parser, opts, opt, is_long_flag, &mut w, streams)?;
            }
        }
    }
    opts.args_opts = w.argv_opts;

    *optind = w.wopt_index;
    return Ok(SUCCESS);
}

// This function mimics the `next_opt()` usage found elsewhere in our other builtin commands.
// It's different in that the short and long option structures are constructed dynamically based on
// arguments provided to the `argparse` command.
fn argparse_parse_args<'args>(
    opts: &mut ArgParseCmdOpts<'args>,
    args: &mut [&'args wstr],
    argc: usize,
    parser: &Parser,
    streams: &mut IoStreams,
) -> BuiltinResult {
    if argc <= 1 {
        return Ok(SUCCESS);
    }

    let mut optind = 0usize;
    argparse_parse_flags(parser, opts, argc, args, &mut optind, streams)?;

    check_for_mutually_exclusive_flags(opts, streams)?;

    opts.args
        .extend(args[optind..].iter().map(|&s| Cow::Borrowed(s)));

    Ok(SUCCESS)
}

fn check_min_max_args_constraints(
    opts: &ArgParseCmdOpts,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let cmd = &opts.name;

    if opts.args.len() < opts.min_args {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_MIN_ARG_COUNT1,
            cmd,
            opts.min_args,
            opts.args.len()
        ));
        return Err(STATUS_CMD_ERROR);
    }

    if opts.max_args != usize::MAX && opts.args.len() > opts.max_args {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_MAX_ARG_COUNT1,
            cmd,
            opts.max_args,
            opts.args.len()
        ));
        return Err(STATUS_CMD_ERROR);
    }

    Ok(SUCCESS)
}

/// Put the result of parsing the supplied args into the caller environment as local vars.
fn set_argparse_result_vars(vars: &EnvStack, opts: ArgParseCmdOpts) {
    for opt_spec in opts.options.values() {
        if opt_spec.num_seen == 0 {
            continue;
        }

        if opt_spec.short_flag_valid {
            let mut var_name = WString::from(VAR_NAME_PREFIX);
            var_name.push(opt_spec.short_flag);
            vars.set(&var_name, EnvMode::LOCAL, opt_spec.vals.clone());
        }

        if !opt_spec.long_flag.is_empty() {
            // We do a simple replacement of all non alphanum chars rather than calling
            // escape_string(long_flag, 0, STRING_STYLE_VAR).
            let long_flag = opt_spec
                .long_flag
                .chars()
                .map(|c| if fish_iswalnum(c) { c } else { '_' });
            let var_name_long: WString = VAR_NAME_PREFIX.chars().chain(long_flag).collect();
            vars.set(&var_name_long, EnvMode::LOCAL, opt_spec.vals.clone());
        }
    }

    let args = opts.args.into_iter().map(|s| s.into_owned()).collect();
    vars.set(L!("argv"), EnvMode::LOCAL, args);
    let args_opts = opts.args_opts.into_iter().map(|s| s.into_owned()).collect();
    vars.set(L!("argv_opts"), EnvMode::LOCAL, args_opts);
}

/// The argparse builtin. This is explicitly not compatible with the BSD or GNU version of this
/// command. That's because fish doesn't have the weird quoting problems of POSIX shells. So we
/// don't need to support flags like `--unquoted`. Similarly we don't want to support introducing
/// long options with a single dash so we don't support the `--alternative` flag. That `getopt` is
/// an external command also means its output has to be in a form that can be eval'd. Because our
/// version is a builtin it can directly set variables local to the current scope (e.g., a
/// function). It doesn't need to write anything to stdout that then needs to be eval'd.
pub fn argparse(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let Some(&cmd) = args.get(0) else {
        return Err(STATUS_INVALID_ARGS);
    };

    let argc = args.len();

    let mut opts = ArgParseCmdOpts::new();
    let mut optind = 0usize;
    let retval = parse_cmd_opts(&mut opts, &mut optind, argc, args, parser, streams);
    if retval.is_err() {
        // This is an error in argparse usage, so we append the error trailer with a stack trace.
        // The other errors are an error in using *the command* that is using argparse,
        // so our help doesn't apply.
        builtin_print_error_trailer(parser, streams.err, cmd);
        return retval;
    }

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    parse_exclusive_args(&mut opts, streams)?;

    // wgetopt expects the first argument to be the command, and skips it.
    // if optind was 0 we'd already have returned.
    assert!(optind > 0, "Optind is 0?");
    argparse_parse_args(
        &mut opts,
        &mut args[optind - 1..],
        argc - optind + 1,
        parser,
        streams,
    )?;

    check_min_max_args_constraints(&opts, streams)?;

    set_argparse_result_vars(parser.vars(), opts);

    Ok(SUCCESS)
}
