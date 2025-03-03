//! A small utility to print information related to pressing keys. This is similar to using tools
//! like `xxd` and `od -tx1z` but provides more information such as the time delay between each
//! character. It also allows pressing and interpreting keys that are normally special such as
//! [ctrl-c] (interrupt the program) or [ctrl-d] (EOF to signal the program should exit).
//! And unlike those other tools this one disables ICRNL mode so it can distinguish between
//! carriage-return (\cM) and newline (\cJ).
//!
//! Type "exit" or "quit" to terminate the program.

use std::{ops::ControlFlow, os::unix::prelude::OsStrExt};

use libc::{STDIN_FILENO, TCSANOW, VEOF, VINTR};

#[allow(unused_imports)]
use crate::future::IsSomeAnd;
use crate::{
    builtins::shared::BUILTIN_ERR_UNKNOWN,
    common::{shell_modes, str2wcstring, PROGRAM_NAME},
    env::env_init,
    input::input_terminfo_get_name,
    input_common::{
        kitty_progressive_enhancements_query, terminal_protocol_hacks,
        terminal_protocols_enable_ifn, CharEvent, InputEventQueue, InputEventQueuer,
    },
    key::{char_to_symbol, Key},
    nix::isatty,
    panic::panic_handler,
    print_help::print_help,
    proc::set_interactive_session,
    reader::{check_exit_loop_maybe_warning, reader_init},
    signal::signal_set_handlers,
    threads,
    topic_monitor::topic_monitor_init,
    wchar::prelude::*,
    wgetopt::{wopt, ArgType, WGetopter, WOption},
};

use super::prelude::*;

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
fn should_exit(streams: &mut IoStreams, recent_keys: &mut Vec<Key>, key: Key) -> bool {
    recent_keys.push(key);

    for evt in [VINTR, VEOF] {
        let modes = shell_modes();
        let cc = Key::from_single_byte(modes.c_cc[evt]);

        if key == cc {
            if recent_keys.iter().rev().nth(1) == Some(&cc) {
                return true;
            }
            streams.err.append(wgettext_fmt!(
                "Press ctrl-%c again to exit\n",
                char::from(modes.c_cc[evt] + 0x60)
            ));
            return false;
        }
    }

    let Some(tail) = recent_keys
        .len()
        .checked_sub(4)
        .and_then(|start| recent_keys.get(start..))
    else {
        return false;
    };
    let tail = tail.iter().map(|c| c.codepoint);
    tail.clone().eq("exit".chars()) || tail.eq("quit".chars())
}

/// Return the name if the recent sequence of characters matches a known terminfo sequence.
fn sequence_name(recent_chars: &mut Vec<u8>, c: char) -> Option<WString> {
    if c >= '\u{80}' {
        // Terminfo sequences are always ASCII.
        recent_chars.clear();
        return None;
    }

    let c = c as u8;
    recent_chars.push(c);
    while recent_chars.len() > 8 {
        recent_chars.remove(0);
    }

    // The entire sequence needs to match the sequence, or else we would output substrings.
    input_terminfo_get_name(&str2wcstring(recent_chars))
}

/// Process the characters we receive as the user presses keys.
fn process_input(streams: &mut IoStreams, continuous_mode: bool, verbose: bool) -> i32 {
    let mut first_char_seen = false;
    let mut queue = InputEventQueue::new(STDIN_FILENO);
    let mut recent_chars1 = vec![];
    let mut recent_chars2 = vec![];
    streams.err.appendln("Press a key:\n");

    while (!first_char_seen || continuous_mode) && !check_exit_loop_maybe_warning(None) {
        terminal_protocols_enable_ifn();
        let evt = queue.readch();

        let CharEvent::Key(kevt) = evt else {
            continue;
        };
        let c = kevt.key.codepoint;
        if verbose {
            streams.out.append(L!("# decoded from: "));
            for byte in kevt.seq.chars() {
                streams.out.append(char_to_symbol(byte));
            }
            streams.out.append(L!("\n"));
        }
        streams
            .out
            .append(sprintf!("bind %s 'do something'\n", kevt.key));
        if let Some(name) = sequence_name(&mut recent_chars1, c) {
            streams
                .out
                .append(sprintf!("bind -k %ls 'do something'\n", name));
        }

        if continuous_mode && should_exit(streams, &mut recent_chars2, kevt.key) {
            streams.err.appendln("\nExiting at your request.");
            break;
        }

        first_char_seen = true;
    }
    0
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
fn setup_and_process_keys(streams: &mut IoStreams, continuous_mode: bool, verbose: bool) -> i32 {
    signal_set_handlers(true);
    // We need to set the shell-modes for ICRNL,
    // in fish-proper this is done once a command is run.
    unsafe { libc::tcsetattr(0, TCSANOW, &*shell_modes()) };
    terminal_protocol_hacks();
    streams
        .out
        .append(str2wcstring(kitty_progressive_enhancements_query()));

    if continuous_mode {
        streams.err.append(L!("\n"));
        streams
            .err
            .appendln("To terminate this program type \"exit\" or \"quit\" in this window,");
        let modes = shell_modes();
        streams.err.appendln(wgettext_fmt!(
            "or press ctrl-%c or ctrl-%c twice in a row.",
            char::from(modes.c_cc[VINTR] + 0x60),
            char::from(modes.c_cc[VEOF] + 0x60)
        ));
        streams.err.appendln(L!("\n"));
    }

    process_input(streams, continuous_mode, verbose)
}

fn parse_flags(
    streams: &mut IoStreams,
    args: Vec<WString>,
    continuous_mode: &mut bool,
    verbose: &mut bool,
) -> ControlFlow<i32> {
    let short_opts: &wstr = L!("+chvV");
    let long_opts: &[WOption] = &[
        wopt(L!("continuous"), ArgType::NoArgument, 'c'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("version"), ArgType::NoArgument, 'v'),
        wopt(L!("verbose"), ArgType::NoArgument, 'V'),
    ];

    let mut shim_args: Vec<&wstr> = args.iter().map(|s| s.as_ref()).collect();
    let mut w = WGetopter::new(short_opts, long_opts, &mut shim_args);
    while let Some(opt) = w.next_opt() {
        match opt {
            'c' => {
                *continuous_mode = true;
            }
            'h' => {
                print_help("fish_key_reader");
                return ControlFlow::Break(0);
            }
            'v' => {
                streams.out.appendln(wgettext_fmt!(
                    "%ls, version %s",
                    PROGRAM_NAME.get().unwrap(),
                    crate::BUILD_VERSION
                ));
                return ControlFlow::Break(0);
            }
            'V' => {
                *verbose = true;
            }
            '?' => {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_UNKNOWN,
                    "fish_key_reader",
                    w.argv[w.wopt_index - 1]
                ));
                return ControlFlow::Break(1);
            }
            _ => panic!(),
        }
    }

    let argc = args.len() - w.wopt_index;
    if argc != 0 {
        streams
            .err
            .appendln(wgettext_fmt!("Expected no arguments, got %d", argc));
        return ControlFlow::Break(1);
    }

    ControlFlow::Continue(())
}

pub fn fish_key_reader(
    _parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
) -> Option<c_int> {
    let mut continuous_mode = false;
    let mut verbose = false;

    let args = args.iter_mut().map(|x| x.to_owned()).collect();
    if let ControlFlow::Break(i) = parse_flags(streams, args, &mut continuous_mode, &mut verbose) {
        return Some(i);
    }

    if streams.stdin_fd < 0 || unsafe { libc::isatty(streams.stdin_fd) } == 0 {
        streams.err.appendln("Stdin must be attached to a tty.");
        return Some(1);
    }

    Some(setup_and_process_keys(streams, continuous_mode, verbose))
}

pub fn main() {
    PROGRAM_NAME.set(L!("fish_key_reader")).unwrap();
    panic_handler(throwing_main)
}

fn throwing_main() -> i32 {
    use crate::io::FdOutputStream;
    use crate::io::IoChain;
    use crate::io::OutputStream::Fd;
    use libc::{STDERR_FILENO, STDOUT_FILENO};

    set_interactive_session(true);
    topic_monitor_init();
    threads::init();
    env_init(None, true, false);
    reader_init(false);

    let mut out = Fd(FdOutputStream::new(STDOUT_FILENO));
    let mut err = Fd(FdOutputStream::new(STDERR_FILENO));
    let io_chain = IoChain::new();
    let mut streams = IoStreams::new(&mut out, &mut err, &io_chain);

    let mut continuous_mode = false;
    let mut verbose = false;

    let args: Vec<WString> = std::env::args_os()
        .map(|osstr| str2wcstring(osstr.as_bytes()))
        .collect();
    if let ControlFlow::Break(i) =
        parse_flags(&mut streams, args, &mut continuous_mode, &mut verbose)
    {
        return i;
    }

    if !isatty(libc::STDIN_FILENO) {
        streams
            .err
            .appendln(wgettext!("Stdin must be attached to a tty."));
        return 1;
    }

    setup_and_process_keys(&mut streams, continuous_mode, verbose)
}
