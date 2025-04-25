//! A small utility to print information related to pressing keys. This is similar to using tools
//! like `xxd` and `od -tx1z` but provides more information such as the time delay between each
//! character. It also allows pressing and interpreting keys that are normally special such as
//! [ctrl-c] (interrupt the program) or [ctrl-d] (EOF to signal the program should exit).
//! And unlike those other tools this one disables ICRNL mode so it can distinguish between
//! carriage-return (\cM) and newline (\cJ).
//!
//! Type "exit" or "quit" to terminate the program.

use std::{cell::RefCell, ops::ControlFlow, os::unix::prelude::OsStrExt, sync::atomic::Ordering};

use libc::{STDIN_FILENO, TCSANOW, VEOF, VINTR};
use once_cell::unsync::OnceCell;

#[allow(unused_imports)]
use crate::future::IsSomeAnd;
use crate::{
    builtins::shared::BUILTIN_ERR_UNKNOWN,
    common::{shell_modes, str2wcstring, PROGRAM_NAME},
    env::env_init,
    input_common::{
        terminal_protocol_hacks, terminal_protocols_enable_ifn, CharEvent, InputEventQueue,
        InputEventQueuer, KeyEvent, QueryResponseEvent, TerminalQuery,
    },
    key::{char_to_symbol, Key, Modifiers},
    nix::isatty,
    panic::panic_handler,
    print_help::print_help,
    proc::set_interactive_session,
    reader::{check_exit_loop_maybe_warning, initial_query, reader_init},
    signal::signal_set_handlers,
    terminal::{Capability, KITTY_KEYBOARD_SUPPORTED},
    threads,
    topic_monitor::topic_monitor_init,
    wchar::prelude::*,
    wgetopt::{wopt, ArgType, WGetopter, WOption},
};

use super::prelude::*;

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
fn should_exit(streams: &mut IoStreams, recent_keys: &mut Vec<KeyEvent>, key: KeyEvent) -> bool {
    recent_keys.push(key);

    for evt in [VINTR, VEOF] {
        let modes = shell_modes();
        let cc = Key::from_single_byte(modes.c_cc[evt]);

        if key == cc {
            if recent_keys
                .iter()
                .rev()
                .nth(1)
                .is_some_and(|&prev| prev == cc)
            {
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

/// Process the characters we receive as the user presses keys.
fn process_input(streams: &mut IoStreams, continuous_mode: bool, verbose: bool) -> BuiltinResult {
    let mut first_char_seen = false;
    let mut queue = InputEventQueue::new(STDIN_FILENO);
    let mut recent_chars = vec![];
    streams.err.appendln("Press a key:\n");

    while (!first_char_seen || continuous_mode) && !check_exit_loop_maybe_warning(None) {
        terminal_protocols_enable_ifn();

        let kevt = match queue.readch() {
            CharEvent::Key(kevt) => kevt,
            CharEvent::Readline(_) | CharEvent::Command(_) | CharEvent::Implicit(_) => continue,
            CharEvent::QueryResponse(QueryResponseEvent::PrimaryDeviceAttribute) => {
                if KITTY_KEYBOARD_SUPPORTED.load(Ordering::Relaxed) == Capability::Unknown as _ {
                    KITTY_KEYBOARD_SUPPORTED
                        .store(Capability::NotSupported as _, Ordering::Release);
                }
                continue;
            }
            CharEvent::QueryResponse(_) => continue,
        };
        if verbose {
            streams.out.append(L!("# decoded from: "));
            for (i, byte) in kevt.seq.chars().enumerate() {
                streams.out.append(char_to_symbol(byte, i == 0));
            }
            streams.out.append(L!("\n"));
        }
        let mut print_bind_example = |key: &Key, recommended: bool| {
            streams.out.append(sprintf!(
                "bind %s 'do something'%s\n",
                key,
                if recommended {
                    " # recommended notation"
                } else {
                    ""
                }
            ));
        };
        let have_shifted_key = kevt.key.shifted_codepoint != '\0';
        // If we have shift + some other modifier, the lowercase version is the canonical one.
        let prefer_explicit_shift = kevt.key.modifiers.shift
            && kevt.key.modifiers != Modifiers::SHIFT
            && kevt
                .key
                .shifted_codepoint
                .to_lowercase()
                .eq(Some(kevt.key.codepoint).into_iter());
        if have_shifted_key {
            let mut shifted_key = kevt.key.key;
            shifted_key.modifiers.shift = false;
            shifted_key.codepoint = kevt.key.shifted_codepoint;
            print_bind_example(&shifted_key, !prefer_explicit_shift);
        }
        print_bind_example(&kevt.key, have_shifted_key && prefer_explicit_shift);

        if continuous_mode && should_exit(streams, &mut recent_chars, kevt.key) {
            streams.err.appendln("\nExiting at your request.");
            break;
        }

        first_char_seen = true;
    }
    Ok(SUCCESS)
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
fn setup_and_process_keys(
    streams: &mut IoStreams,
    continuous_mode: bool,
    verbose: bool,
) -> BuiltinResult {
    signal_set_handlers(true);
    // We need to set the shell-modes for ICRNL,
    // in fish-proper this is done once a command is run.
    unsafe { libc::tcsetattr(0, TCSANOW, &*shell_modes()) };
    terminal_protocol_hacks();
    let blocking_query: OnceCell<RefCell<Option<TerminalQuery>>> = OnceCell::new();
    initial_query(&blocking_query, streams.out, None);

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
) -> ControlFlow<BuiltinResult> {
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
                return ControlFlow::Break(Ok(SUCCESS));
            }
            'v' => {
                streams.out.appendln(wgettext_fmt!(
                    "%ls, version %s",
                    PROGRAM_NAME.get().unwrap(),
                    crate::BUILD_VERSION
                ));
                return ControlFlow::Break(Ok(SUCCESS));
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
                return ControlFlow::Break(Err(STATUS_CMD_ERROR));
            }
            _ => panic!(),
        }
    }

    let argc = args.len() - w.wopt_index;
    if argc != 0 {
        streams
            .err
            .appendln(wgettext_fmt!("Expected no arguments, got %d", argc));
        return ControlFlow::Break(Err(STATUS_CMD_ERROR));
    }

    ControlFlow::Continue(())
}

pub fn fish_key_reader(
    _parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
) -> BuiltinResult {
    let mut continuous_mode = false;
    let mut verbose = false;

    let args = args.iter_mut().map(|x| x.to_owned()).collect();
    if let ControlFlow::Break(s) = parse_flags(streams, args, &mut continuous_mode, &mut verbose) {
        return s;
    }

    if streams.stdin_fd < 0 || unsafe { libc::isatty(streams.stdin_fd) } == 0 {
        streams.err.appendln("Stdin must be attached to a tty.");
        return Err(STATUS_CMD_ERROR);
    }

    setup_and_process_keys(streams, continuous_mode, verbose)
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
    if let ControlFlow::Break(s) =
        parse_flags(&mut streams, args, &mut continuous_mode, &mut verbose)
    {
        return s.builtin_status_code();
    }

    if !isatty(libc::STDIN_FILENO) {
        streams
            .err
            .appendln(wgettext!("Stdin must be attached to a tty."));
        return 1;
    }

    setup_and_process_keys(&mut streams, continuous_mode, verbose).builtin_status_code()
}
