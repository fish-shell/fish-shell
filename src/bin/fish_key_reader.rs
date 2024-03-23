//! A small utility to print information related to pressing keys. This is similar to using tools
//! like `xxd` and `od -tx1z` but provides more information such as the time delay between each
//! character. It also allows pressing and interpreting keys that are normally special such as
//! [ctrl-C] (interrupt the program) or [ctrl-D] (EOF to signal the program should exit).
//! And unlike those other tools this one disables ICRNL mode so it can distinguish between
//! carriage-return (\cM) and newline (\cJ).
//!
//! Type "exit" or "quit" to terminate the program.

use std::os::unix::prelude::OsStrExt;

use libc::{STDIN_FILENO, TCSANOW, VEOF, VINTR};

#[allow(unused_imports)]
use fish::future::IsSomeAnd;
use fish::{
    builtins::shared::BUILTIN_ERR_UNKNOWN,
    common::{shell_modes, str2wcstring, PROGRAM_NAME},
    env::env_init,
    eprintf, fprintf,
    input::input_terminfo_get_name,
    input_common::{parse_key, setup_terminal, CharEvent, InputEventQueue, InputEventQueuer},
    key::Chord,
    nix::isatty,
    print_help::print_help,
    printf,
    proc::set_interactive_session,
    reader::{
        check_exit_loop_maybe_warning, reader_init, reader_test_and_clear_interrupted,
        restore_term_mode,
    },
    signal::signal_set_handlers,
    threads,
    topic_monitor::topic_monitor_init,
    wchar::prelude::*,
    wgetopt::{wgetopter_t, wopt, woption, woption_argument_t},
};

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
fn should_exit(recent_chars: &mut Vec<Chord>, c: Chord) -> bool {
    recent_chars.push(c);

    for evt in [VINTR, VEOF] {
        let modes = shell_modes();
        let cc = modes.c_cc[evt];
        let cc = parse_key(cc).unwrap_or(Chord::from(char::from(cc)));

        if c == cc {
            if recent_chars.iter().rev().nth(1) == Some(&cc) {
                return true;
            }
            eprintf!(
                "Press [ctrl-%c] again to exit\n",
                char::from(modes.c_cc[evt] + 0x40)
            );
            return false;
        }
    }

    let Some(tail) = recent_chars
        .len()
        .checked_sub(4)
        .and_then(|start| recent_chars.get(start..))
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

fn output_bind_command(bind_chars: &mut Vec<(Chord, WString)>) {
    if !bind_chars.is_empty() {
        printf!("bind ");
        for (chord, _seq) in &*bind_chars {
            printf!("%s", chord);
        }
        printf!(" 'do something'\n");
        bind_chars.clear();
    }
}

fn output_matching_key_name(recent_chars: &mut Vec<u8>, c: char) -> bool {
    if let Some(name) = sequence_name(recent_chars, c) {
        printf!("bind -k %ls 'do something'\n", name);
        return true;
    }
    false
}

/// Process the characters we receive as the user presses keys.
fn process_input(continuous_mode: bool) {
    let mut first_char_seen = false;
    let mut queue = InputEventQueue::new(STDIN_FILENO);
    let mut bind_chars = vec![];
    let mut recent_chars1 = vec![];
    let mut recent_chars2 = vec![];
    eprintf!("Press a key:\n");

    while !check_exit_loop_maybe_warning(None) {
        let evt = if reader_test_and_clear_interrupted() != 0 {
            let vintr = shell_modes().c_cc[VINTR];
            Some(CharEvent::from_chord(
                parse_key(vintr).unwrap_or(Chord::from(char::from(vintr))),
            ))
        } else {
            queue.readch_timed_esc()
        };

        if evt.as_ref().is_none_or(|evt| !evt.is_char()) {
            output_bind_command(&mut bind_chars);
            if first_char_seen && !continuous_mode {
                return;
            }
            continue;
        }
        let evt = evt.unwrap();

        let chord = evt.get_chord().unwrap();
        let c = chord.codepoint;
        bind_chars.push((chord, evt.seq));
        if output_matching_key_name(&mut recent_chars1, c) {
            output_bind_command(&mut bind_chars);
        }

        if continuous_mode && should_exit(&mut recent_chars2, chord) {
            eprintf!("\nExiting at your request.\n");
            break;
        }

        first_char_seen = true;
    }
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
fn setup_and_process_keys(continuous_mode: bool) -> ! {
    set_interactive_session(true);
    topic_monitor_init();
    threads::init();
    env_init(None, true, false);
    reader_init();
    let raw_terminal = isatty(STDIN_FILENO).then(setup_terminal);

    signal_set_handlers(true);
    // We need to set the shell-modes for ICRNL,
    // in fish-proper this is done once a command is run.
    unsafe { libc::tcsetattr(STDIN_FILENO, TCSANOW, &*shell_modes()) };

    if continuous_mode {
        eprintf!("\n");
        eprintf!("To terminate this program type \"exit\" or \"quit\" in this window,\n");
        let modes = shell_modes();
        eprintf!(
            "or press [ctrl-%c] or [ctrl-%c] twice in a row.\n",
            char::from(modes.c_cc[VINTR] + 0x40),
            char::from(modes.c_cc[VEOF] + 0x40)
        );
        eprintf!("\n");
    }

    process_input(continuous_mode);
    restore_term_mode();
    drop(raw_terminal);
    std::process::exit(0);
}

fn parse_flags(continuous_mode: &mut bool) -> bool {
    let short_opts: &wstr = L!("+chvV");
    let long_opts: &[woption] = &[
        wopt(L!("continuous"), woption_argument_t::no_argument, 'c'),
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
        wopt(L!("version"), woption_argument_t::no_argument, 'v'),
        wopt(L!("verbose"), woption_argument_t::no_argument, 'V'),
    ];

    let args: Vec<WString> = std::env::args_os()
        .map(|osstr| str2wcstring(osstr.as_bytes()))
        .collect();
    let mut shim_args: Vec<&wstr> = args.iter().map(|s| s.as_ref()).collect();
    let mut w = wgetopter_t::new(short_opts, long_opts, &mut shim_args);
    while let Some(opt) = w.wgetopt_long() {
        match opt {
            'c' => {
                *continuous_mode = true;
            }
            'h' => {
                print_help("fish_key_reader");
                std::process::exit(0);
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
                std::process::exit(0);
            }
            'V' => {}
            '?' => {
                printf!(
                    "%s",
                    wgettext_fmt!(
                        BUILTIN_ERR_UNKNOWN,
                        "fish_key_reader",
                        w.argv[w.woptind - 1]
                    )
                );
                return false;
            }
            _ => panic!(),
        }
    }

    let argc = args.len() - w.woptind;
    if argc != 0 {
        eprintf!("Expected no arguments, got %d\n", argc);
        return false;
    }

    true
}

fn main() {
    PROGRAM_NAME.set(L!("fish_key_reader")).unwrap();
    let mut continuous_mode = false;

    if !parse_flags(&mut continuous_mode) {
        std::process::exit(1);
    }

    if unsafe { libc::isatty(STDIN_FILENO) } == 0 {
        eprintf!("Stdin must be attached to a tty.\n");
        std::process::exit(1);
    }

    setup_and_process_keys(continuous_mode);
}
