extern crate proc_macro;
use proc_macro::TokenStream;
use std::{ffi::OsString, fs::OpenOptions, io::Write};

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
                    state = Ground
                }
                '\n' => state = ContinuationLineLeadingWhitespace,
                _ => panic!("Unsupported escape sequence '\\{c}' in message string '{s}'"),
            },
            ContinuationLineLeadingWhitespace => match c {
                ' ' | '\t' => (),
                _ => {
                    unescaped.push(c);
                    state = Ground
                }
            },
        }
    }
    unescaped
}

fn append_po_entry_to_file(message: &TokenStream, file_name: &OsString) {
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(file_name)
        .unwrap_or_else(|e| panic!("Could not open file {file_name:?}: {e}"));
    let message_string = unescape_multiline_rust_string(message.to_string());
    if message_string.contains('\n') {
        panic!(
            "Gettext strings may not contain unescaped newlines. Unescaped newline found in '{message_string}'"
        )
    }
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
/// If `FISH_GETTEXT_EXTRACTION_FILE` is defined in the environment,
/// this file is used to write the message,
/// so that it can then be used for generating gettext PO files.
/// The `message` must be a string literal.
///
/// # Panics
///
/// This macro panics if the `FISH_GETTEXT_EXTRACTION_FILE` variable is set and `message` has an
/// unexpected format.
/// Note that for example `concat!(...)` cannot be passed to this macro, because expansion works
/// outside in, meaning this macro would still see the `concat!` macro invocation, instead of a
/// string literal.
#[proc_macro]
pub fn gettext_extract(message: TokenStream) -> TokenStream {
    if let Some(file_path) = std::env::var_os("FISH_GETTEXT_EXTRACTION_FILE") {
        let pm2_message = proc_macro2::TokenStream::from(message.clone());
        let mut token_trees = pm2_message.into_iter();
        let first_token = token_trees
            .next()
            .expect("gettext_extract got empty token stream. Expected one token.");
        if token_trees.next().is_some() {
            panic!(
                "Invalid number of tokens passed to gettext_extract. Expected one token, but got more."
            )
        }
        let proc_macro2::TokenTree::Group(group) = first_token else {
            panic!("Expected group in gettext_extract, but got: {first_token:?}");
        };
        let mut group_tokens = group.stream().into_iter();
        let first_group_token = group_tokens
            .next()
            .expect("gettext_extract expected one group token but got none.");
        if group_tokens.next().is_some() {
            panic!(
                "Invalid number of tokens in group passed to gettext_extract. Expected one token, but got more."
            )
        }
        if let proc_macro2::TokenTree::Literal(_) = first_group_token {
            append_po_entry_to_file(&message, &file_path);
        } else {
            panic!("Expected literal in gettext_extract, but got: {first_group_token:?}");
        }
    }
    message
}
