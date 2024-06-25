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
use fish::future::IsSomeAnd;
use fish::{
    builtins::shared::BUILTIN_ERR_UNKNOWN,
    common::{shell_modes, str2wcstring, PROGRAM_NAME},
    env::env_init,
    eprintf, fprintf,
    input::input_terminfo_get_name,
    input_common::{terminal_protocols_enable_ifn, CharEvent, InputEventQueue, InputEventQueuer},
    key::{self, Key},
    panic::panic_handler,
    print_help::print_help,
    printf,
    proc::set_interactive_session,
    reader::{check_exit_loop_maybe_warning, reader_init},
    signal::signal_set_handlers,
    threads,
    topic_monitor::topic_monitor_init,
    wchar::prelude::*,
    wgetopt::{wopt, ArgType, WGetopter, WOption},
};

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
fn should_exit(recent_keys: &mut Vec<Key>, key: Key) -> bool {
    recent_keys.push(key);

    for evt in [VINTR, VEOF] {
        let modes = shell_modes();
        let cc = Key::from_single_byte(modes.c_cc[evt]);

        if key == cc {
            if recent_keys.iter().rev().nth(1) == Some(&cc) {
                return true;
            }
            eprintf!(
                "Press ctrl-%c again to exit\n",
                char::from(modes.c_cc[evt] + 0x60)
            );
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
fn process_input(continuous_mode: bool) -> i32 {
    let mut first_char_seen = false;
    let mut queue = InputEventQueue::new(STDIN_FILENO);
    let mut recent_chars1 = vec![];
    let mut recent_chars2 = vec![];
    eprintf!("Press a key:\n");

    terminal_protocols_enable_ifn();
    while (!first_char_seen || continuous_mode) && !check_exit_loop_maybe_warning(None) {
        let evt = queue.readch();

        let CharEvent::Key(kevt) = evt else {
            continue;
        };
        let c = kevt.key.codepoint;
        if c == key::Invalid {
            continue;
        }
        printf!("bind %s 'do something'\n", kevt.key);
        if let Some(name) = sequence_name(&mut recent_chars1, c) {
            printf!("bind -k %ls 'do something'\n", name);
        }

        if continuous_mode && should_exit(&mut recent_chars2, kevt.key) {
            eprintf!("\nExiting at your request.\n");
            break;
        }

        first_char_seen = true;
    }
    0
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
fn setup_and_process_keys(continuous_mode: bool) -> i32 {
    set_interactive_session(true);
    topic_monitor_init();
    threads::init();
    env_init(None, true, false);
    let _restore_term = reader_init();

    signal_set_handlers(true);
    // We need to set the shell-modes for ICRNL,
    // in fish-proper this is done once a command is run.
    unsafe { libc::tcsetattr(STDIN_FILENO, TCSANOW, &*shell_modes()) };

    if continuous_mode {
        eprintf!("\n");
        eprintf!("To terminate this program type \"exit\" or \"quit\" in this window,\n");
        let modes = shell_modes();
        eprintf!(
            "or press ctrl-%c or ctrl-%c twice in a row.\n",
            char::from(modes.c_cc[VINTR] + 0x60),
            char::from(modes.c_cc[VEOF] + 0x60)
        );
        eprintf!("\n");
    }

    process_input(continuous_mode)
}

fn parse_flags(continuous_mode: &mut bool) -> ControlFlow<i32> {
    let short_opts: &wstr = L!("+chvV");
    let long_opts: &[WOption] = &[
        wopt(L!("continuous"), ArgType::NoArgument, 'c'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("version"), ArgType::NoArgument, 'v'),
        wopt(L!("verbose"), ArgType::NoArgument, 'V'), // Removed
    ];

    let args: Vec<WString> = std::env::args_os()
        .map(|osstr| str2wcstring(osstr.as_bytes()))
        .collect();
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
                printf!(
                    "%s",
                    wgettext_fmt!(
                        "%ls, version %s\n",
                        PROGRAM_NAME.get().unwrap(),
                        fish::BUILD_VERSION
                    )
                );
                return ControlFlow::Break(0);
            }
            'V' => {}
            '?' => {
                printf!(
                    "%s",
                    wgettext_fmt!(
                        BUILTIN_ERR_UNKNOWN,
                        "fish_key_reader",
                        w.argv[w.wopt_index - 1]
                    )
                );
                return ControlFlow::Break(1);
            }
            _ => panic!(),
        }
    }

    let argc = args.len() - w.wopt_index;
    if argc != 0 {
        eprintf!("Expected no arguments, got %d\n", argc);
        return ControlFlow::Break(1);
    }

    ControlFlow::Continue(())
}

fn main() {
    PROGRAM_NAME.set(L!("fish_key_reader")).unwrap();
    panic_handler(throwing_main)
}

fn throwing_main() -> i32 {
    let mut continuous_mode = false;

    if let ControlFlow::Break(i) = parse_flags(&mut continuous_mode) {
        return i;
    }

    if unsafe { libc::isatty(STDIN_FILENO) } == 0 {
        eprintf!("Stdin must be attached to a tty.\n");
        return 1;
    }

    setup_and_process_keys(continuous_mode)
}
