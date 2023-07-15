use std::collections::HashMap;

use super::prelude::*;

use crate::env::{EnvMode, EnvStack};
use crate::exec::exec_subshell;
use crate::wcstringutil::split_string;
use crate::wutil::fish_iswalnum;

const VAR_NAME_PREFIX: &wstr = L!("_flag_");

const BUILTIN_ERR_INVALID_OPT_SPEC: &str = "%ls: Invalid option spec '%ls' at char '%lc'\n";

#[derive(PartialEq)]
enum ArgCardinality {
    Optional = -1isize,
    None = 0,
    Once = 1,
    AtLeastOnce = 2,
}

impl Default for ArgCardinality {
    fn default() -> Self {
        Self::None
    }
}

#[derive(Default)]
struct OptionSpec {
    short_flag: char,
    long_flag: WString,
    validation_command: WString,
    vals: Vec<WString>,
    short_flag_valid: bool,
    num_allowed: ArgCardinality,
    num_seen: isize,
}

impl OptionSpec {
    fn new(s: char) -> Self {
        Self {
            short_flag: s,
            short_flag_valid: true,
            ..Default::default()
        }
    }
}

#[derive(Default)]
struct ArgParseCmdOpts {
    ignore_unknown: bool,
    print_help: bool,
    stop_nonopt: bool,
    min_args: usize,
    max_args: usize,
    implicit_int_flag: char,
    name: WString,
    raw_exclusive_flags: Vec<WString>,
    args: Vec<WString>,
    options: HashMap<char, OptionSpec>,
    long_to_short_flag: HashMap<WString, char>,
    exclusive_flag_sets: Vec<Vec<char>>,
}

impl ArgParseCmdOpts {
    fn new() -> Self {
        Self {
            max_args: usize::MAX,
            ..Default::default()
        }
    }
}

const SHORT_OPTIONS: &wstr = L!("+:hn:six:N:X:");
const LONG_OPTIONS: &[woption] = &[
    wopt(L!("stop-nonopt"), woption_argument_t::no_argument, 's'),
    wopt(L!("ignore-unknown"), woption_argument_t::no_argument, 'i'),
    wopt(L!("name"), woption_argument_t::required_argument, 'n'),
    wopt(L!("exclusive"), woption_argument_t::required_argument, 'x'),
    wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    wopt(L!("min-args"), woption_argument_t::required_argument, 'N'),
    wopt(L!("max-args"), woption_argument_t::required_argument, 'X'),
];

// Check if any pair of mutually exclusive options was seen. Note that since every option must have
// a short name we only need to check those.
fn check_for_mutually_exclusive_flags(
    opts: &ArgParseCmdOpts,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
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
                        streams.err.append(&wgettext_fmt!(
                            "%ls: %ls %ls: options cannot be used together\n",
                            opts.name,
                            flag1,
                            flag2
                        ));
                        return STATUS_CMD_ERROR;
                    }
                }
            }
        }
    }

    return STATUS_CMD_OK;
}

// This should be called after all the option specs have been parsed. At that point we have enough
// information to parse the values associated with any `--exclusive` flags.
fn parse_exclusive_args(opts: &mut ArgParseCmdOpts, streams: &mut IoStreams<'_>) -> Option<c_int> {
    for raw_xflags in &opts.raw_exclusive_flags {
        let xflags = split_string(raw_xflags, ',');
        if xflags.len() < 2 {
            streams.err.append(&wgettext_fmt!(
                "%ls: exclusive flag string '%ls' is not valid\n",
                opts.name,
                raw_xflags
            ));
            return STATUS_CMD_ERROR;
        }

        let exclusive_set: &mut Vec<char> = &mut vec![];
        for flag in &xflags {
            if flag.char_count() == 1 && opts.options.contains_key(&flag.char_at(0)) {
                let short = flag.char_at(0);
                // It's a short flag.
                exclusive_set.push(short);
            } else if let Some(short_equiv) = opts.long_to_short_flag.get(flag) {
                // It's a long flag we store as its short flag equivalent.
                exclusive_set.push(*short_equiv);
            } else {
                streams.err.append(&wgettext_fmt!(
                    "%ls: exclusive flag '%ls' is not valid\n",
                    opts.name,
                    flag
                ));
                return STATUS_CMD_ERROR;
            }
        }

        // Store the set of exclusive flags for use when parsing the supplied set of arguments.
        opts.exclusive_flag_sets.push(exclusive_set.to_vec());
    }
    return STATUS_CMD_OK;
}

fn parse_flag_modifiers(
    opts: &ArgParseCmdOpts,
    opt_spec: &mut OptionSpec,
    option_spec: &wstr,
    opt_spec_str: &mut &wstr,
    streams: &mut IoStreams<'_>,
) -> bool {
    let mut s = *opt_spec_str;

    if opt_spec.short_flag == opts.implicit_int_flag && !s.is_empty() && s.char_at(0) != '!' {
        streams.err.append(&wgettext_fmt!(
            "%ls: Implicit int short flag '%lc' does not allow modifiers like '%lc'\n",
            opts.name,
            opt_spec.short_flag,
            s.char_at(0)
        ));
        return false;
    }

    if s.char_at(0) == '=' {
        s = s.slice_from(1);
        opt_spec.num_allowed = match s.char_at(0) {
            '?' => ArgCardinality::Optional,
            '+' => ArgCardinality::AtLeastOnce,
            _ => ArgCardinality::Once,
        };
        if opt_spec.num_allowed != ArgCardinality::Once {
            s = s.slice_from(1);
        }
    }

    if s.char_at(0) == '!' {
        s = s.slice_from(1);
        opt_spec.validation_command = s.to_owned();
        // Move cursor to the end so we don't expect a long flag.
        s = s.slice_from(s.char_count());
    } else if !s.is_empty() {
        streams.err.append(&wgettext_fmt!(
            BUILTIN_ERR_INVALID_OPT_SPEC,
            opts.name,
            option_spec,
            s.char_at(0)
        ));
        return false;
    }

    // Make sure we have some validation for implicit int flags.
    if opt_spec.short_flag == opts.implicit_int_flag && opt_spec.validation_command.is_empty() {
        opt_spec.validation_command = L!("_validate_int").to_owned();
    }

    if opts.options.contains_key(&opt_spec.short_flag) {
        streams.err.append(&wgettext_fmt!(
            "%ls: Short flag '%lc' already defined\n",
            opts.name,
            opt_spec.short_flag
        ));
        return false;
    }

    *opt_spec_str = s;
    return true;
}

/// Parse the text following the short flag letter.
fn parse_option_spec_sep(
    opts: &mut ArgParseCmdOpts,
    opt_spec: &mut OptionSpec,
    option_spec: &wstr,
    opt_spec_str: &mut &wstr,
    counter: &mut u32,
    streams: &mut IoStreams<'_>,
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
            streams.err.append(&wgettext_fmt!(
                "%ls: Implicit int flag '%lc' already defined\n",
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
                streams.err.append(&wgettext_fmt!(
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
                streams.err.append(&wgettext_fmt!(
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
                streams.err.append(&wgettext_fmt!(
                    "%ls: Implicit int flag '%lc' already defined\n",
                    opts.name,
                    opts.implicit_int_flag
                ));
                return false;
            }
            opts.implicit_int_flag = opt_spec.short_flag;
            opt_spec.num_allowed = ArgCardinality::Once;
            i += 1; // the struct is initialized assuming short_flag_valid should be true
        }
        '!' | '?' | '=' => {
            // Try to parse any other flag modifiers
            // parse_flag_modifiers assumes opt_spec_str starts where it should, not one earlier
            s = s.slice_from(i);
            i = 0;
            if !parse_flag_modifiers(opts, opt_spec, option_spec, &mut s, streams) {
                return false;
            }
        }
        _ => {
            // No short flag separator and no other modifiers, so this is a long only option.
            // Since getopt needs a wchar, we have a counter that we count up.
            opt_spec.short_flag_valid = false;
            i -= 1;
            opt_spec.short_flag = char::from_u32(*counter).unwrap();
            *counter += 1;
        }
    }

    *opt_spec_str = s.slice_from(i);
    return true;
}

fn parse_option_spec(
    opts: &mut ArgParseCmdOpts,
    option_spec: &wstr,
    counter: &mut u32,
    streams: &mut IoStreams<'_>,
) -> bool {
    if option_spec.is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%ls: An option spec must have at least a short or a long flag\n",
            opts.name
        ));
        return false;
    }

    let mut s = option_spec;
    if !fish_iswalnum(s.char_at(0)) && s.char_at(0) != '#' {
        streams.err.append(&wgettext_fmt!(
            "%ls: Short flag '%lc' invalid, must be alphanum or '#'\n",
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
            opt_spec.long_flag = s.slice_to(long_flag_char_count).to_owned();
            if opts.long_to_short_flag.contains_key(&opt_spec.long_flag) {
                streams.err.append(&wgettext_fmt!(
                    "%ls: Long flag '%ls' already defined\n",
                    opts.name,
                    &opt_spec.long_flag
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
            .insert(opt_spec.long_flag.clone(), opt_spec.short_flag);
        assert!(ins.is_none(), "Should have inserted long flag");
    }

    // Record our option under its short flag.
    opts.options.insert(opt_spec.short_flag, opt_spec);

    return true;
}

fn collect_option_specs<'opts, 'args>(
    w: &mut wgetopter_t<'opts, 'args>,
    opts: &mut ArgParseCmdOpts,
    optind: &mut usize,
    argc: usize,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let cmd: &wstr = &w.argv()[0];

    // A counter to give short chars to long-only options because getopt needs that.
    // Luckily we have wgetopt so we can use wchars - this is one of the private use areas so we
    // have 6400 options available.
    let mut counter = 0xE000u32;

    loop {
        if *optind == argc {
            streams
                .err
                .append(&wgettext_fmt!("%ls: Missing -- separator\n", cmd));
            return STATUS_INVALID_ARGS;
        }

        if "--" == w.argv()[*optind] {
            *optind += 1;
            break;
        }

        if !parse_option_spec(opts, &w.argv()[*optind], &mut counter, streams) {
            return STATUS_CMD_ERROR;
        }

        *optind += 1;
    }

    // Check for counter overreach once at the end because this is very unlikely to ever be reached.
    let counter_max = 0xF8FFu32;

    if counter > counter_max {
        streams
            .err
            .append(&wgettext_fmt!("%ls: Too many long-only options\n", cmd));
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

fn parse_cmd_opts<'opts, 'args>(
    w: &mut wgetopter_t<'opts, 'args>,
    opts: &mut ArgParseCmdOpts,
    optind: &mut usize,
    argc: usize,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let mut args_read = Vec::with_capacity(w.argv().len());
    args_read.extend_from_slice(w.argv());

    while let Some(c) = w.wgetopt_long() {
        match c {
            'n' => opts.name = w.woptarg().unwrap().to_owned(),
            's' => opts.stop_nonopt = true,
            'i' => opts.ignore_unknown = true,
            // Just save the raw string here. Later, when we have all the short and long flag
            // definitions we'll parse these strings into a more useful data structure.
            'x' => opts
                .raw_exclusive_flags
                .push(w.woptarg().unwrap().to_owned()),
            'h' => opts.print_help = true,
            'N' => {
                opts.min_args = {
                    let x = fish_wcstol(w.woptarg().unwrap()).unwrap_or(-1);
                    if x < 0 {
                        streams.err.append(&wgettext_fmt!(
                            "%ls: Invalid --min-args value '%ls'\n",
                            w.cmd(),
                            w.woptarg().unwrap()
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    x.try_into().unwrap()
                }
            }
            'X' => {
                opts.max_args = {
                    let x = fish_wcstol(w.woptarg().unwrap()).unwrap_or(-1);
                    if x < 0 {
                        streams.err.append(&wgettext_fmt!(
                            "%ls: Invalid --max-args value '%ls'\n",
                            w.cmd(),
                            w.woptarg().unwrap()
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    x.try_into().unwrap()
                }
            }
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    /* print_hints */ false,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from wgetopt_long"),
        }
    }

    if opts.print_help {
        return STATUS_CMD_OK;
    }

    if "--" == args_read[w.woptind - 1] {
        w.woptind -= 1;
    }

    if argc == w.woptind {
        // The user didn't specify any option specs.
        streams
            .err
            .append(&wgettext_fmt!("%ls: Missing -- separator\n", w.cmd()));
        return STATUS_INVALID_ARGS;
    }

    if opts.name.is_empty() {
        // If no name has been given, we default to the function name.
        // If any error happens, the backtrace will show which argparse it was.
        opts.name = parser
            .get_function_name(1)
            .unwrap_or_else(|| L!("argparse").to_owned());
    }

    *optind = w.woptind;
    return collect_option_specs(w, opts, optind, argc, streams);
}

fn populate_option_strings<'opts>(
    opts: &ArgParseCmdOpts,
    long_option_names: &'opts [WString],
    short_options: &mut WString,
    long_options: &mut Vec<woption<'opts>>,
) {
    let mut i = 0;
    for opt_spec in opts.options.values() {
        if opt_spec.short_flag_valid {
            short_options.push(opt_spec.short_flag);
        }

        let arg_type = match opt_spec.num_allowed {
            ArgCardinality::Optional => {
                if opt_spec.short_flag_valid {
                    short_options.push_str("::");
                }
                woption_argument_t::optional_argument
            }
            ArgCardinality::Once | ArgCardinality::AtLeastOnce => {
                if opt_spec.short_flag_valid {
                    short_options.push_str(":");
                }
                woption_argument_t::required_argument
            }
            ArgCardinality::None => woption_argument_t::no_argument,
        };

        let name = &long_option_names[i];
        i += 1;
        assert!(name == &opt_spec.long_flag);
        if !name.is_empty() {
            long_options.push(wopt(&name, arg_type, opt_spec.short_flag));
        }
    }
}

fn validate_arg<'opts>(
    parser: &Parser,
    opts_name: &wstr,
    opt_spec: &mut OptionSpec,
    is_long_flag: bool,
    woptarg: &wstr,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    // Obviously if there is no arg validation command we assume the arg is okay.
    if opt_spec.validation_command.is_empty() {
        return STATUS_CMD_OK;
    }

    let vars = parser.vars();
    vars.push(true /* new_scope */);

    let env_mode = EnvMode::LOCAL | EnvMode::EXPORT;
    vars.set_one(L!("_argparse_cmd"), env_mode, opts_name.to_owned());
    let flag_name = WString::from(VAR_NAME_PREFIX) + "name";
    if is_long_flag {
        vars.set_one(&flag_name, env_mode, opt_spec.long_flag.clone());
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
        &opt_spec.validation_command,
        parser,
        Some(&mut cmd_output),
        false,
    );

    for output in cmd_output {
        streams.err.append(&output);
        streams.err.append_char('\n');
    }
    vars.pop();
    Some(retval)
}

/// \return whether the option 'opt' is an implicit integer option.
fn is_implicit_int(opts: &ArgParseCmdOpts, val: &wstr) -> bool {
    if opts.implicit_int_flag == '\0' {
        // There is no implicit integer option.
        return false;
    }

    // We succeed if this argument can be parsed as an integer.
    fish_wcstol(val).is_ok()
}

// Store this value under the implicit int option.
fn validate_and_store_implicit_int(
    parser: &Parser,
    opts: &mut ArgParseCmdOpts,
    val: WString,
    w: &mut wgetopter_t,
    is_long_flag: bool,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let opt_spec = opts.options.get_mut(&opts.implicit_int_flag).unwrap();
    let retval = validate_arg(parser, &opts.name, opt_spec, is_long_flag, &val, streams);

    if retval != STATUS_CMD_OK {
        return retval;
    }

    // It's a valid integer so store it and return success.
    opt_spec.vals.clear();
    opt_spec.vals.push(val);
    opt_spec.num_seen += 1;
    w.nextchar_slice = 0..0;

    return STATUS_CMD_OK;
}

fn handle_flag(
    parser: &Parser,
    opts: &mut ArgParseCmdOpts,
    opt: char,
    is_long_flag: bool,
    woptarg: Option<WString>,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let opt_spec = opts.options.get_mut(&opt).unwrap();

    opt_spec.num_seen += 1;
    if opt_spec.num_allowed == ArgCardinality::None {
        // It's a boolean flag. Save the flag we saw since it might be useful to know if the
        // short or long flag was given.
        assert!(woptarg.is_none());
        let s = if is_long_flag {
            WString::from("--") + &opt_spec.long_flag[..]
        } else {
            WString::from_chars(['-', opt_spec.short_flag])
        };
        opt_spec.vals.push(s);
        return STATUS_CMD_OK;
    }

    if let Some(woptarg) = woptarg.as_ref() {
        let retval = validate_arg(parser, &opts.name, opt_spec, is_long_flag, woptarg, streams);
        if retval != STATUS_CMD_OK {
            return retval;
        }
    }

    match opt_spec.num_allowed {
        ArgCardinality::Optional | ArgCardinality::Once => {
            // We're depending on `wgetopt_long()` to report that a mandatory value is missing if
            // `opt_spec->num_allowed == 1` and thus return ':' so that we don't take this branch if
            // the mandatory arg is missing.
            opt_spec.vals.clear();
            if let Some(arg) = woptarg {
                opt_spec.vals.push(arg.into());
            }
        }
        _ => {
            opt_spec.vals.push(woptarg.unwrap().into());
        }
    }

    return STATUS_CMD_OK;
}

fn argparse_parse_flags<'opts, 'args>(
    w: &mut wgetopter_t<'opts, 'args>,
    parser: &Parser,
    opts: &mut ArgParseCmdOpts,
    argc: usize,
    optind: &mut usize,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let mut args_read = Vec::with_capacity(w.argv().len());
    args_read.extend_from_slice(w.argv());

    let mut long_idx: usize = usize::MAX;
    while let Some(opt) = w.wgetopt_long_idx(&mut long_idx) {
        let retval = match opt {
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    &opts.name,
                    &args_read[w.woptind - 1],
                    false,
                );
                STATUS_INVALID_ARGS
            }
            '?' => {
                // It's not a recognized flag. See if it's an implicit int flag.
                let arg_contents = args_read[w.woptind - 1].slice_from(1);

                if is_implicit_int(opts, arg_contents) {
                    validate_and_store_implicit_int(
                        parser,
                        opts,
                        arg_contents.to_owned(),
                        w,
                        long_idx != usize::MAX,
                        streams,
                    )
                } else if !opts.ignore_unknown {
                    streams.err.append(&wgettext_fmt!(
                        BUILTIN_ERR_UNKNOWN,
                        opts.name,
                        args_read[w.woptind - 1]
                    ));
                    STATUS_INVALID_ARGS
                } else {
                    // Any unrecognized option is put back if ignore_unknown is used.
                    // This allows reusing the same argv in multiple argparse calls,
                    // or just ignoring the error (e.g. in completions).
                    opts.args.push(args_read[w.woptind - 1].to_owned());
                    // Work around weirdness with wgetopt, which crashes if we `continue` here.
                    if w.woptind == argc {
                        break;
                    }
                    // Explain to wgetopt that we want to skip to the next arg,
                    // because we can't handle this opt group.
                    w.nextchar_slice = 0..0;
                    STATUS_CMD_OK
                }
            }
            NONOPTION_CHAR_CODE => {
                // A non-option argument.
                // We use `-` as the first option-string-char to disable GNU getopt's reordering,
                // otherwise we'd get ignored options first and normal arguments later.
                // E.g. `argparse -i -- -t tango -w` needs to keep `-t tango -w` in $argv, not `-t -w
                // tango`.
                opts.args.push(args_read[w.woptind - 1].to_owned());
                continue;
            }
            // It's a recognized flag.
            _ => handle_flag(
                parser,
                opts,
                opt,
                long_idx != usize::MAX,
                w.woptarg().map(|arg| arg.to_owned()),
                streams,
            ),
        };
        if retval != STATUS_CMD_OK {
            return retval;
        }
        long_idx = usize::MAX;
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

// This function mimics the `wgetopt_long()` usage found elsewhere in our other builtin commands.
// It's different in that the short and long option structures are constructed dynamically based on
// arguments provided to the `argparse` command.
fn argparse_parse_args<'opts>(
    opts: &mut ArgParseCmdOpts,
    args: &mut [WString],
    argc: usize,
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    if argc <= 1 {
        return STATUS_CMD_OK;
    }

    // "+" means stop at nonopt, "-" means give nonoptions the option character code `1`, and don't
    // reorder.
    let mut short_options = WString::from(if opts.stop_nonopt { L!("+:") } else { L!("-:") });
    let long_option_names: Vec<WString> = opts
        .options
        .values()
        .map(|opt| opt.long_flag.clone())
        .collect();
    let mut long_options = vec![];
    populate_option_strings(
        opts,
        &long_option_names,
        &mut short_options,
        &mut long_options,
    );

    let mut w = wgetopter_t::new(&short_options, &long_options, args);
    let mut optind = 0usize;
    let retval = argparse_parse_flags(&mut w, parser, opts, argc, &mut optind, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let retval = check_for_mutually_exclusive_flags(opts, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    // TODO
    // for arg in &w.argv()[optind..] {
    //     opts.args.push(arg.to_owned());
    // }

    return STATUS_CMD_OK;
}

fn check_min_max_args_constraints(
    opts: &ArgParseCmdOpts,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let cmd = &opts.name;

    if opts.args.len() < opts.min_args {
        streams.err.append(&wgettext_fmt!(
            BUILTIN_ERR_MIN_ARG_COUNT1,
            cmd,
            opts.min_args,
            opts.args.len()
        ));
        return STATUS_CMD_ERROR;
    }

    if opts.max_args != usize::MAX && opts.args.len() > opts.max_args {
        streams.err.append(&wgettext_fmt!(
            BUILTIN_ERR_MAX_ARG_COUNT1,
            cmd,
            opts.max_args,
            opts.args.len()
        ));
        return STATUS_CMD_ERROR;
    }

    return STATUS_CMD_OK;
}

/// Put the result of parsing the supplied args into the caller environment as local vars.
fn set_argparse_result_vars(vars: &EnvStack, opts: &ArgParseCmdOpts) {
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

    let args = opts.args.iter().map(|s| s.to_owned()).collect();
    vars.set(L!("argv"), EnvMode::LOCAL, args);
}

/// The argparse builtin. This is explicitly not compatible with the BSD or GNU version of this
/// command. That's because fish doesn't have the weird quoting problems of POSIX shells. So we
/// don't need to support flags like `--unquoted`. Similarly we don't want to support introducing
/// long options with a single dash so we don't support the `--alternative` flag. That `getopt` is
/// an external command also means its output has to be in a form that can be eval'd. Because our
/// version is a builtin it can directly set variables local to the current scope (e.g., a
/// function). It doesn't need to write anything to stdout that then needs to be eval'd.
pub fn argparse(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    let argc = args.len();

    let mut opts = ArgParseCmdOpts::new();
    let mut optind = 0usize;
    let mut w = wgetopter_t::new(SHORT_OPTIONS, LONG_OPTIONS, args);
    let retval = parse_cmd_opts(&mut w, &mut opts, &mut optind, argc, parser, streams);
    if retval != STATUS_CMD_OK {
        // This is an error in argparse usage, so we append the error trailer with a stack trace.
        // The other errors are an error in using *the command* that is using argparse,
        // so our help doesn't apply.
        builtin_print_error_trailer(parser, streams.err, w.cmd());
        return retval;
    }

    if opts.print_help {
        builtin_print_help(parser, streams, w.cmd());
        return STATUS_CMD_OK;
    }

    let retval = parse_exclusive_args(&mut opts, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    // wgetopt expects the first argument to be the command, and skips it.
    // if optind was 0 we'd already have returned.
    assert!(optind > 0, "Optind is 0?");
    let retval = argparse_parse_args(
        &mut opts,
        &mut w.argv_mut()[optind - 1..],
        argc - optind + 1,
        parser,
        streams,
    );
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let retval = check_min_max_args_constraints(&opts, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    set_argparse_result_vars(&parser.vars(), &opts);

    return retval;
}
