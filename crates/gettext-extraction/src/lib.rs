extern crate proc_macro;
use fish_localization_extraction::localization_extract;
use proc_macro::TokenStream;
use std::{fs::File, io::Write as _};

fn unescape_multiline_rust_string(s: String) -> String {
    if !s.contains('\n') {
        return s;
    }
    let mut unescaped = String::new();
    enum State {
        Ground,
        Escaped,
        ContinuationLineLeadingWhitespace,
    }
    use State::*;
    let mut state = Ground;
    for c in s.chars() {
        match state {
            Ground => match c {
                '\\' => state = Escaped,
                _ => {
                    unescaped.push(c);
                }
            },
            Escaped => match c {
                '\\' => {
                    unescaped.push('\\');
                    state = Ground;
                }
                '\n' => state = ContinuationLineLeadingWhitespace,
                _ => panic!("Unsupported escape sequence '\\{c}' in message string '{s}'"),
            },
            ContinuationLineLeadingWhitespace => match c {
                ' ' | '\t' => (),
                _ => {
                    unescaped.push(c);
                    state = Ground;
                }
            },
        }
    }
    unescaped
}

// Each entry is written to a fresh file to avoid race conditions arising when there are multiple
// unsynchronized writers to the same file.
fn write_po_entry_to_file(message: String, mut file: File) {
    let message_string = unescape_multiline_rust_string(message);
    assert!(
        !message_string.contains('\n'),
        "Gettext strings may not contain unescaped newlines. Unescaped newline found in '{message_string}'"
    );
    let msgid_without_quotes = &message_string[1..(message_string.len() - 1)];
    // We don't want leading or trailing whitespace in our messages.
    let trimmed_msgid = msgid_without_quotes.trim();
    assert_eq!(msgid_without_quotes, trimmed_msgid);
    assert!(!trimmed_msgid.starts_with("\\n"));
    assert!(!trimmed_msgid.ends_with("\\n"));
    assert!(!trimmed_msgid.starts_with("\\t"));
    assert!(!trimmed_msgid.ends_with("\\t"));
    // Crude check for format strings. This might result in false positives.
    let format_string_annotation = if message_string.contains('%') {
        "#, c-format\n"
    } else {
        ""
    };
    let po_entry = format!("{format_string_annotation}msgid {message_string}\nmsgstr \"\"\n\n");
    file.write_all(po_entry.as_bytes()).unwrap();
}

/// The `message` is passed through unmodified.
/// If `FISH_GETTEXT_EXTRACTION_DIR` is defined in the environment,
/// the message ID is written into a new file in this directory,
/// so that it can then be used for generating gettext PO files.
/// The `message` must be a string literal.
///
/// # Panics
///
/// This macro panics if the `FISH_GETTEXT_EXTRACTION_DIR` variable is set and `message` has an
/// unexpected format.
/// Note that for example `concat!(...)` cannot be passed to this macro, because expansion works
/// outside in, meaning this macro would still see the `concat!` macro invocation, instead of a
/// string literal.
#[proc_macro]
pub fn gettext_extract(message: TokenStream) -> TokenStream {
    localization_extract(
        message,
        "FISH_GETTEXT_EXTRACTION_DIR",
        write_po_entry_to_file,
    )
}
