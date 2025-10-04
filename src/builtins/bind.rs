//! Implementation of the bind builtin.

use super::prelude::*;
use crate::common::{
    escape, escape_string, str2wcstring, valid_var_name, EscapeFlags, EscapeStringStyle,
};
use crate::highlight::{colorize, highlight_shell};
use crate::input::{input_function_get_names, input_mappings, InputMappingSet, KeyNameStyle};
use crate::key::{
    self, char_to_symbol, function_key, parse_keys, Key, Modifiers, KEY_NAMES, MAX_FUNCTION_KEY,
};
use crate::nix::isatty;
use std::sync::MutexGuard;

const DEFAULT_BIND_MODE: &wstr = L!("default");

const BIND_INSERT: c_int = 0;
const BIND_ERASE: c_int = 1;
const BIND_KEY_NAMES: c_int = 2;
const BIND_FUNCTION_NAMES: c_int = 3;

struct Options {
    all: bool,
    bind_mode_given: bool,
    list_modes: bool,
    print_help: bool,
    silent: bool,
    have_user: bool,
    user: bool,
    have_preset: bool,
    preset: bool,
    mode: c_int,
    bind_mode: WString,
    sets_bind_mode: Option<WString>,
}

impl Options {
    fn new() -> Options {
        Options {
            all: false,
            bind_mode_given: false,
            list_modes: false,
            print_help: false,
            silent: false,
            have_user: false,
            user: false,
            have_preset: false,
            preset: false,
            mode: BIND_INSERT,
            bind_mode: DEFAULT_BIND_MODE.to_owned(),
            sets_bind_mode: None,
        }
    }
}

struct BuiltinBind {
    /// Note that BuiltinBind holds the singleton lock.
    /// It must not call out to anything which can execute fish shell code or attempt to acquire the
    /// lock again.
    input_mappings: MutexGuard<'static, InputMappingSet>,
    opts: Options,
}

impl BuiltinBind {
    fn new() -> BuiltinBind {
        BuiltinBind {
            input_mappings: input_mappings(),
            opts: Options::new(),
        }
    }

    /// List a single key binding.
    /// Returns false if no binding with that sequence and mode exists.
    fn list_one(
        &self,
        seq: &[Key],
        bind_mode: &wstr,
        user: bool,
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> bool {
        let mut ecmds: &[_] = &[];
        let mut sets_mode = None;
        let mut key_name_style = KeyNameStyle::Plain;
        let mut out = WString::new();
        if !self.input_mappings.get(
            seq,
            bind_mode,
            &mut ecmds,
            user,
            &mut sets_mode,
            &mut key_name_style,
        ) {
            return false;
        }

        out.push_str("bind");

        // Append the mode flags if applicable.
        if !user {
            out.push_str(" --preset");
        }
        if bind_mode != DEFAULT_BIND_MODE {
            out.push_str(" -M ");
            out.push_utfstr(&escape(bind_mode));
        }

        if let Some(sets_mode) = sets_mode {
            if sets_mode != bind_mode {
                out.push_str(" -m ");
                out.push_utfstr(&escape(sets_mode));
            }
        }

        out.push(' ');
        match key_name_style {
            KeyNameStyle::Plain => {
                // Append the name.
                for (i, key) in seq.iter().enumerate() {
                    if i != 0 {
                        out.push(key::KEY_SEPARATOR);
                    }
                    out.push_utfstr(&WString::from(*key));
                }
                if seq.is_empty() {
                    out.push_str("''");
                }
            }
            KeyNameStyle::RawEscapeSequence => {
                for (i, key) in seq.iter().enumerate() {
                    if key.modifiers == Modifiers::ALT {
                        out.push_utfstr(&char_to_symbol('\x1b', i == 0));
                        out.push_utfstr(&char_to_symbol(
                            if key.codepoint == key::Escape {
                                '\x1b'
                            } else {
                                key.codepoint
                            },
                            false,
                        ));
                    } else {
                        assert!(key.modifiers.is_none());
                        out.push_utfstr(&char_to_symbol(key.codepoint, i == 0));
                    }
                }
            }
        }

        // Now show the list of commands.
        for ecmd in ecmds {
            out.push(' ');
            out.push_utfstr(&escape(ecmd));
        }
        out.push('\n');

        if !streams.out_is_redirected && isatty(libc::STDOUT_FILENO) {
            let mut colors = Vec::new();
            highlight_shell(&out, &mut colors, &parser.context(), false, None);
            let colored = colorize(&out, &colors, parser.vars());
            streams.out.append(str2wcstring(&colored));
        } else {
            streams.out.append(out);
        }

        true
    }

    // Overload with both kinds of bindings.
    // Returns false only if neither exists.
    fn list_one_user_andor_preset(
        &self,
        seq: &[Key],
        bind_mode: &wstr,
        user: bool,
        preset: bool,
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> bool {
        let mut retval = false;
        if preset {
            retval |= self.list_one(seq, bind_mode, false, parser, streams);
        }
        if user {
            retval |= self.list_one(seq, bind_mode, true, parser, streams);
        }
        retval
    }

    /// List all current key bindings.
    fn list(&self, bind_mode: Option<&wstr>, user: bool, parser: &Parser, streams: &mut IoStreams) {
        let lst = self.input_mappings.get_names(user);
        for binding in lst {
            if bind_mode.is_some_and(|m| m != binding.mode) {
                continue;
            }

            self.list_one(&binding.seq, &binding.mode, user, parser, streams);
        }
    }

    /// Print all named keys to the string buffer used for standard output.
    fn key_names(&self, streams: &mut IoStreams) {
        let function_keys: Vec<WString> = (1..=MAX_FUNCTION_KEY)
            .map(|i| WString::from(Key::from_raw(function_key(i))))
            .collect();
        let mut keys: Vec<&wstr> = KEY_NAMES
            .iter()
            .map(|(_encoding, name)| *name)
            .chain(function_keys.iter().map(|s| s.as_utfstr()))
            .collect();
        keys.sort_unstable();
        for name in keys {
            streams.out.appendln(name);
        }
    }

    /// Print all the special key binding functions to string buffer used for standard output.
    fn function_names(&self, streams: &mut IoStreams) {
        let names = input_function_get_names();
        for name in names {
            streams.out.appendln(name);
        }
    }

    /// Add specified key binding.
    fn add(
        &mut self,
        seq: &wstr,
        cmds: &[&wstr],
        mode: WString,
        sets_mode: Option<WString>,
        user: bool,
        streams: &mut IoStreams,
    ) -> bool {
        let cmds = cmds.iter().map(|&s| s.to_owned()).collect();
        let is_raw_escape_sequence = seq.len() > 2 && seq.char_at(0) == '\x1b';
        let Some(key_seq) = self.compute_seq(streams, seq) else {
            return true;
        };
        let key_name_style = if is_raw_escape_sequence {
            KeyNameStyle::RawEscapeSequence
        } else {
            KeyNameStyle::Plain
        };
        self.input_mappings
            .add(key_seq, key_name_style, cmds, mode, sets_mode, user);
        false
    }

    fn compute_seq(&self, streams: &mut IoStreams, seq: &wstr) -> Option<Vec<Key>> {
        match parse_keys(seq) {
            Ok(keys) => Some(keys),
            Err(err) => {
                streams.err.append(sprintf!("bind: %s\n", err));
                None
            }
        }
    }

    /// Erase specified key bindings
    ///
    /// @param  seq
    ///    an array of all key bindings to erase
    /// @param  all
    ///    if specified, _all_ key bindings will be erased
    ///
    fn erase(&mut self, seq: &[&wstr], all: bool, user: bool, streams: &mut IoStreams) -> bool {
        let mode = if self.opts.bind_mode_given {
            Some(self.opts.bind_mode.as_utfstr())
        } else {
            None
        };

        if all {
            self.input_mappings.clear(mode, user);
            return false;
        }

        let mode = mode.unwrap_or(DEFAULT_BIND_MODE);

        for s in seq {
            let Some(s) = self.compute_seq(streams, s) else {
                return true;
            };
            self.input_mappings.erase(&s, mode, user);
        }
        false
    }

    fn insert(
        &mut self,
        optind: usize,
        argv: &[&wstr],
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> bool {
        let argc = argv.len();
        let cmd = argv[0];
        let arg_count = argc - optind;
        if arg_count < 2 {
            // If we get both or neither preset/user, we list both.
            if !self.opts.have_preset && !self.opts.have_user {
                self.opts.preset = true;
                self.opts.user = true;
            }
        } else {
            // Inserting both on the other hand makes no sense.
            if self.opts.have_preset && self.opts.have_user {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_COMBO2_EXCLUSIVE,
                    cmd,
                    "--preset",
                    "--user"
                ));
                return true;
            }
        }

        if arg_count == 0 {
            // We don't overload this with user and def because we want them to be grouped.
            // First the presets, then the users (because of scrolling).
            let bind_mode = if self.opts.bind_mode_given {
                Some(self.opts.bind_mode.as_utfstr())
            } else {
                None
            };
            if self.opts.preset {
                self.list(bind_mode, false, parser, streams);
            }
            if self.opts.user {
                self.list(bind_mode, true, parser, streams);
            }
        } else if arg_count == 1 {
            let Some(seq) = self.compute_seq(streams, argv[optind]) else {
                return true;
            };

            if !self.list_one_user_andor_preset(
                &seq,
                &self.opts.bind_mode,
                self.opts.user,
                self.opts.preset,
                parser,
                streams,
            ) {
                let eseq = escape_string(
                    argv[optind],
                    EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES),
                );
                if !self.opts.silent {
                    if seq.len() == 1 {
                        streams.err.append(wgettext_fmt!(
                            "%s: No binding found for key '%s'\n",
                            cmd,
                            seq[0]
                        ));
                    } else {
                        streams.err.append(wgettext_fmt!(
                            "%s: No binding found for key sequence '%s'\n",
                            cmd,
                            eseq
                        ));
                    }
                }
                return true;
            }
        } else {
            // Actually insert!
            let seq = argv[optind];
            if self.add(
                seq,
                &argv[optind + 1..],
                self.opts.bind_mode.to_owned(),
                self.opts.sets_bind_mode.to_owned(),
                self.opts.user,
                streams,
            ) {
                return true;
            }
        }

        false
    }

    /// List all current bind modes.
    fn list_modes(&mut self, streams: &mut IoStreams) {
        // List all known modes, even if they are only in preset bindings.
        let lst = self.input_mappings.get_names(true);
        let preset_lst = self.input_mappings.get_names(false);

        // Extract the bind modes, uniqueize, and sort.
        let mut modes: Vec<WString> = lst.into_iter().chain(preset_lst).map(|m| m.mode).collect();
        modes.sort_unstable();
        modes.dedup();

        for mode in modes {
            streams.out.appendln(mode);
        }
    }
}

fn parse_cmd_opts(
    opts: &mut Options,
    optind: &mut usize,
    argv: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let cmd = argv[0];
    let short_options = L!("aehkKfM:Lm:s");
    const long_options: &[WOption] = &[
        wopt(L!("all"), NoArgument, 'a'),
        wopt(L!("erase"), NoArgument, 'e'),
        wopt(L!("function-names"), NoArgument, 'f'),
        wopt(L!("help"), NoArgument, 'h'),
        wopt(L!("key"), NoArgument, 'k'),
        wopt(L!("key-names"), NoArgument, 'K'),
        wopt(L!("list-modes"), NoArgument, 'L'),
        wopt(L!("mode"), RequiredArgument, 'M'),
        wopt(L!("preset"), NoArgument, 'p'),
        wopt(L!("sets-mode"), RequiredArgument, 'm'),
        wopt(L!("silent"), NoArgument, 's'),
        wopt(L!("user"), NoArgument, 'u'),
    ];

    let mut w = WGetopter::new(short_options, long_options, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'a' => opts.all = true,
            'e' => opts.mode = BIND_ERASE,
            'f' => opts.mode = BIND_FUNCTION_NAMES,
            'h' => opts.print_help = true,
            'k' => {
                streams.err.append(wgettext_fmt!(
                    "%s: the -k/--key syntax is no longer supported. See `bind --help` and `bind --key-names`\n",
                    cmd,
                ));
                return Err(STATUS_INVALID_ARGS);
            }
            'K' => opts.mode = BIND_KEY_NAMES,
            'L' => {
                opts.list_modes = true;
                return Ok(SUCCESS);
            }
            'M' => {
                if !valid_var_name(w.woptarg.unwrap()) {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_BIND_MODE,
                        cmd,
                        w.woptarg.unwrap()
                    ));
                    return Err(STATUS_INVALID_ARGS);
                }
                opts.bind_mode = w.woptarg.unwrap().to_owned();
                opts.bind_mode_given = true;
            }
            'm' => {
                let new_mode = w.woptarg.unwrap();
                if !valid_var_name(new_mode) {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_BIND_MODE, cmd, new_mode));
                    return Err(STATUS_INVALID_ARGS);
                }
                opts.sets_bind_mode = Some(new_mode.to_owned());
            }
            'p' => {
                opts.have_preset = true;
                opts.preset = true;
            }
            's' => opts.silent = true,
            'u' => {
                opts.have_user = true;
                opts.user = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from WGetopter")
            }
        }
    }
    *optind = w.wopt_index;
    return Ok(SUCCESS);
}

impl BuiltinBind {
    /// The bind builtin, used for setting character sequences.
    pub fn bind(
        &mut self,
        parser: &Parser,
        streams: &mut IoStreams,
        argv: &mut [&wstr],
    ) -> BuiltinResult {
        let cmd = argv[0];
        let mut optind = 0;
        parse_cmd_opts(&mut self.opts, &mut optind, argv, parser, streams)?;

        if self.opts.list_modes {
            self.list_modes(streams);
            return Ok(SUCCESS);
        }

        if self.opts.print_help {
            builtin_print_help(parser, streams, cmd);
            return Ok(SUCCESS);
        }

        // Default to user mode
        if !self.opts.have_preset && !self.opts.have_user {
            self.opts.user = true;
        }

        match self.opts.mode {
            BIND_ERASE => {
                // If we get both, we erase both.
                if self.opts.user {
                    if self.erase(
                        &argv[optind..],
                        self.opts.all,
                        true, /* user */
                        streams,
                    ) {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                if self.opts.preset {
                    if self.erase(
                        &argv[optind..],
                        self.opts.all,
                        false, /* user */
                        streams,
                    ) {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
            }
            BIND_INSERT => {
                if self.insert(optind, argv, parser, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            BIND_KEY_NAMES => self.key_names(streams),
            BIND_FUNCTION_NAMES => self.function_names(streams),
            _ => {
                streams
                    .err
                    .append(wgettext_fmt!("%s: Invalid state\n", cmd));
                return Err(STATUS_CMD_ERROR);
            }
        }
        Ok(SUCCESS)
    }
}

pub fn bind(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    BuiltinBind::new().bind(parser, streams, args)
}
