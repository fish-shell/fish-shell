//! Functions having to do with parser keywords, like testing if a function is a block command.

use crate::wchar::wstr;
use widestring_suffix::widestrs;

#[widestrs]
const SKIP_KEYWORDS: &[&wstr] = &["else"L, "begin"L];
#[widestrs]
const SUBCOMMAND_KEYWORDS: &[&wstr] = &[
    "command"L, "builtin"L, "while"L, "exec"L, "if"L, "and"L, "or"L, "not"L, "time"L, "begin"L,
];
#[widestrs]
const BLOCK_KEYWORDS: &[&wstr] = &["for"L, "while"L, "if"L, "function"L, "switch"L, "begin"L];

// Don't forget to add any new reserved keywords to the documentation
#[widestrs]
const RESERVED_KEYWORDS: &[&wstr] = &[
    "end"L,
    "case"L,
    "else"L,
    "return"L,
    "continue"L,
    "break"L,
    "argparse"L,
    "read"L,
    "string"L,
    "set"L,
    "status"L,
    "test"L,
    "["L,
    "_"L,
    "eval"L,
];

// The lists above are purposely implemented separately from the logic below, so that future
// maintainers may assume the contents of the list based off their names, and not off what the
// functions below require them to contain.

/// Tests if the specified commands parameters should be interpreted as another command, which will
/// be true if the command is either 'command', 'exec', 'if', 'while', or 'builtin'.  This does not
/// handle "else if" which is more complicated.
pub fn parser_keywords_is_subcommand(cmd: &wstr) -> bool {
    SUBCOMMAND_KEYWORDS.contains(&cmd) || SKIP_KEYWORDS.contains(&cmd)
}

/// Tests if the specified command is a reserved word, i.e. if it is the name of one of the builtin
/// functions that change the block or command scope, like 'for', 'end' or 'command' or 'exec'.
/// These functions may not be overloaded, so their names are reserved.
pub fn parser_keywords_is_reserved(word: &wstr) -> bool {
    SUBCOMMAND_KEYWORDS.contains(&word)
        || SKIP_KEYWORDS.contains(&word)
        || BLOCK_KEYWORDS.contains(&word)
        || RESERVED_KEYWORDS.contains(&word)
}
