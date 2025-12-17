//! Functions having to do with parser keywords, like testing if a function is a block command.

use crate::prelude::*;

struct ReservedWord {
    text: &'static wstr,
    is_reserved: bool,
    is_super_command: bool,
}

macro_rules! rw {
    ( ( $text:literal ) ) => {
        ReservedWord {
            text: L!($text),
            is_reserved: true,
            is_super_command: false,
        }
    };
    ( ( $text:literal, [subcommand] ) ) => {
        ReservedWord {
            text: L!($text),
            is_reserved: true,
            is_super_command: true,
        }
    };
    ( ( $text:literal, [subcommand], not reserved ) ) => {
        ReservedWord {
            text: L!($text),
            is_reserved: false,
            is_super_command: true,
        }
    };
}
macro_rules! reserved_words {
    ( $( $reserved_word:tt , ) * ) => {
        &[ $( rw!($reserved_word), )* ]
    }
}

// Don't forget to add any new reserved keywords to the documentation
const RESERVED_WORDS: &[ReservedWord] = reserved_words!(
    ("!", [subcommand], not reserved),
    ("["),
    ("_"),
    ("and", [subcommand]),
    ("argparse"),
    ("begin", [subcommand]),
    ("break"),
    ("builtin", [subcommand]),
    ("case"),
    ("command", [subcommand]),
    ("continue"),
    ("else", [subcommand]),
    ("end"),
    ("eval"),
    ("exec", [subcommand]),
    ("for"),
    ("function"),
    ("if", [subcommand]),
    ("not", [subcommand]),
    ("or", [subcommand]),
    ("read"),
    ("return"),
    ("set"),
    ("status"),
    ("string"),
    ("switch"),
    ("test"),
    ("time", [subcommand]),
    ("while", [subcommand]),
);

fn reserved_word(cmd: &wstr) -> Option<&'static ReservedWord> {
    RESERVED_WORDS
        .iter()
        .find(|reserved_word| reserved_word.text == cmd)
}

/// Tests if the specified command's parameters should be interpreted as another command.
pub fn parser_keywords_is_subcommand(cmd: &impl AsRef<wstr>) -> bool {
    reserved_word(cmd.as_ref()).is_some_and(|reserved_word| reserved_word.is_super_command)
}

/// Tests if the specified command is a reserved word, i.e. if it is the name of one of the builtin
/// functions that change the block or command scope, like 'for', 'end' or 'command' or 'exec'.
/// These functions may not be overloaded, so their names are reserved.
pub fn parser_keywords_is_reserved(word: &wstr) -> bool {
    reserved_word(word).is_some_and(|reserved_word| reserved_word.is_reserved)
}
