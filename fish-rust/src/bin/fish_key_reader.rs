//! A small utility to print information related to pressing keys. This is similar to using tools
//! like `xxd` and `od -tx1z` but provides more information such as the time delay between each
//! character. It also allows pressing and interpreting keys that are normally special such as
//! [ctrl-C] (interrupt the program) or [ctrl-D] (EOF to signal the program should exit).
//! And unlike those other tools this one disables ICRNL mode so it can distinguish between
//! carriage-return (\cM) and newline (\cJ).
//!
//! Type "exit" or "quit" to terminate the program.

use std::{
    os::unix::prelude::OsStrExt,
    time::{Duration, Instant},
};

use libc::{STDIN_FILENO, TCSANOW, VEOF, VINTR};

#[allow(unused_imports)]
use fish::future::IsSomeAnd;
use fish::{
    builtins::shared::BUILTIN_ERR_UNKNOWN,
    common::{scoped_push_replacer, shell_modes, str2wcstring, PROGRAM_NAME},
    env::env_init,
    eprintf,
    fallback::fish_wcwidth,
    fprintf,
    input::input_terminfo_get_name,
    input_common::{CharEvent, InputEventQueue, InputEventQueuer},
    parser::Parser,
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
fn should_exit(recent_chars: &mut Vec<u8>, c: char) -> bool {
    let c = if c < '\u{80}' { c as u8 } else { 0 };

    recent_chars.push(c);

    for evt in [VINTR, VEOF] {
        let modes = shell_modes();
        if c == modes.c_cc[evt] {
            if recent_chars.iter().rev().nth(1) == Some(&modes.c_cc[evt]) {
                return true;
            }
            eprintf!(
                "Press [ctrl-%c] again to exit\n",
                char::from(modes.c_cc[evt] + 0x40)
            );
            return false;
        }
    }

    recent_chars.ends_with(b"exit") || recent_chars.ends_with(b"quit")
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

/// Return true if the character must be escaped when used in the sequence of chars to be bound in
/// a `bind` command.
fn must_escape(c: char) -> bool {
    "[]()<>{}*\\?$#;&|'\"".contains(c)
}

fn ctrl_to_symbol(buf: &mut WString, c: char, bind_friendly: bool) {
    let ctrl_symbolic_names: [&wstr; 28] = {
        std::array::from_fn(|i| match i {
            8 => L!("\\b"),
            9 => L!("\\t"),
            10 => L!("\\n"),
            13 => L!("\\r"),
            27 => L!("\\e"),
            28 => L!("\\x1c"),
            _ => L!(""),
        })
    };

    let c = u8::try_from(c).unwrap();
    let cu = usize::from(c);

    if !ctrl_symbolic_names[cu].is_empty() {
        if bind_friendly {
            sprintf!(=> buf, "%s", ctrl_symbolic_names[cu]);
        } else {
            sprintf!(=> buf, "\\c%c  (or %ls)", char::from(c + 0x40), ctrl_symbolic_names[cu]);
        }
    } else {
        sprintf!(=> buf, "\\c%c", char::from(c + 0x40));
    }
}

fn space_to_symbol(buf: &mut WString, c: char, bind_friendly: bool) {
    if bind_friendly {
        sprintf!(=> buf, "\\x%X", u32::from(c));
    } else {
        sprintf!(=> buf, "\\x%X  (aka \"space\")", u32::from(c));
    }
}

fn del_to_symbol(buf: &mut WString, c: char, bind_friendly: bool) {
    if bind_friendly {
        sprintf!(=> buf, "\\x%X", u32::from(c));
    } else {
        sprintf!(=> buf, "\\x%X  (aka \"del\")", u32::from(c));
    }
}

fn ascii_printable_to_symbol(buf: &mut WString, c: char, bind_friendly: bool) {
    if bind_friendly && must_escape(c) {
        sprintf!(=> buf, "\\%c", c);
    } else {
        sprintf!(=> buf, "%c", c);
    }
}

/// Convert a wide-char to a symbol that can be used in our output.
fn char_to_symbol(c: char, bind_friendly: bool) -> WString {
    let mut buff = WString::new();
    let buf = &mut buff;
    if c == '\x1b' {
        // Escape - this is *technically* also \c[
        buf.push_str("\\e");
    } else if c < ' ' {
        // ASCII control character
        ctrl_to_symbol(buf, c, bind_friendly);
    } else if c == ' ' {
        // the "space" character
        space_to_symbol(buf, c, bind_friendly);
    } else if c == '\x7F' {
        // the "del" character
        del_to_symbol(buf, c, bind_friendly);
    } else if c < '\u{80}' {
        // ASCII characters that are not control characters
        ascii_printable_to_symbol(buf, c, bind_friendly);
    } else if fish_wcwidth(c) > 0 {
        sprintf!(=> buf, "%lc", c);
    } else if c <= '\u{FFFF}' {
        // BMP Unicode chararacter
        sprintf!(=> buf, "\\u%04X", u32::from(c));
    } else {
        sprintf!(=> buf, "\\U%06X", u32::from(c));
    }
    buff
}

fn add_char_to_bind_command(c: char, bind_chars: &mut Vec<char>) {
    bind_chars.push(c);
}

fn output_bind_command(bind_chars: &mut Vec<char>) {
    if !bind_chars.is_empty() {
        printf!("bind ");
        for &bind_char in &*bind_chars {
            printf!("%s", char_to_symbol(bind_char, true));
        }
        printf!(" 'do something'\n");
        bind_chars.clear();
    }
}

fn output_info_about_char(c: char) {
    eprintf!(
        "hex: %4X  char: %ls\n",
        u32::from(c),
        char_to_symbol(c, false)
    );
}

fn output_matching_key_name(recent_chars: &mut Vec<u8>, c: char) -> bool {
    if let Some(name) = sequence_name(recent_chars, c) {
        printf!("bind -k %ls 'do something'\n", name);
        return true;
    }
    false
}

fn output_elapsed_time(prev_timestamp: Instant, first_char_seen: bool, verbose: bool) -> Instant {
    // How much time has passed since the previous char was received in microseconds.
    let now = Instant::now();
    let delta = now - prev_timestamp;

    if verbose {
        if delta >= Duration::from_millis(200) && first_char_seen {
            eprintf!("\n");
        }
        if delta >= Duration::from_millis(1000) {
            eprintf!("              ");
        } else {
            eprintf!(
                "(%3lld.%03lld ms)  ",
                u64::try_from(delta.as_millis()).unwrap(),
                u64::try_from(delta.as_micros() % 1000).unwrap()
            );
        }
    }
    now
}

/// Process the characters we receive as the user presses keys.
fn process_input(continuous_mode: bool, verbose: bool) {
    let mut first_char_seen = false;
    let mut prev_timestamp = Instant::now()
        .checked_sub(Duration::from_millis(1000))
        .unwrap_or(Instant::now());
    let mut queue = InputEventQueue::new(STDIN_FILENO);
    let mut bind_chars = vec![];
    let mut recent_chars1 = vec![];
    let mut recent_chars2 = vec![];
    eprintf!("Press a key:\n");

    while !check_exit_loop_maybe_warning(None) {
        let evt = if reader_test_and_clear_interrupted() != 0 {
            Some(CharEvent::from_char(char::from(shell_modes().c_cc[VINTR])))
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

        let c = evt.get_char();
        prev_timestamp = output_elapsed_time(prev_timestamp, first_char_seen, verbose);
        // Hack for #3189. Do not suggest \c@ as the binding for nul, because a string containing
        // nul cannot be passed to builtin_bind since it uses C strings. We'll output the name of
        // this key (nul) elsewhere.
        if c != '\0' {
            add_char_to_bind_command(c, &mut bind_chars);
        }
        if verbose {
            output_info_about_char(c);
        }
        if output_matching_key_name(&mut recent_chars1, c) {
            output_bind_command(&mut bind_chars);
        }

        if continuous_mode && should_exit(&mut recent_chars2, c) {
            eprintf!("\nExiting at your request.\n");
            break;
        }

        first_char_seen = true;
    }
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
fn setup_and_process_keys(continuous_mode: bool, verbose: bool) -> ! {
    set_interactive_session(true);
    topic_monitor_init();
    threads::init();
    env_init(None, true, false);
    reader_init();

    let parser = Parser::principal_parser();
    let _interactive = scoped_push_replacer(
        |new_value| std::mem::replace(&mut parser.libdata_mut().pods.is_interactive, new_value),
        true,
    );

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

    process_input(continuous_mode, verbose);
    restore_term_mode();
    std::process::exit(0);
}

fn parse_flags(continuous_mode: &mut bool, verbose: &mut bool) -> bool {
    const short_opts: &wstr = L!("+chvV");
    const long_opts: &[woption] = &[
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
            }
            'V' => {
                *verbose = true;
            }
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
    let mut verbose = false;

    if !parse_flags(&mut continuous_mode, &mut verbose) {
        std::process::exit(1);
    }

    if unsafe { libc::isatty(STDIN_FILENO) } == 0 {
        eprintf!("Stdin must be attached to a tty.\n");
        std::process::exit(1);
    }

    setup_and_process_keys(continuous_mode, verbose);
}
