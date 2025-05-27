extern crate proc_macro;
use proc_macro::TokenStream;
use std::{fs::OpenOptions, io::Write};

fn append_line_to_file(message: &TokenStream, file_name: &str) {
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(file_name)
        .unwrap();
    let mut message_string = message.to_string();
    message_string.push('\n');
    file.write_all(message_string.as_bytes()).unwrap();
}

/// The `message` is passed through unmodified.
/// If `GETTEXT_EXTRACTION_DIR` is defined in the environment,
/// this directory is used to write the message,
/// so that it can then be used for generating gettext PO files.
/// The `message` can be a string literal, in which case it is appended to the `literals` file
/// in the directory. Literals are written enclosed in double quotes and with a trailing newline.
/// Alternatively, the name of a constant can be passed as the `message`.
/// In this case, the macro cannot obtain the corresponding string literal.
/// (at least not without using unstable features/external commands)
/// The names of such constants are written to the `consts` file.
/// This file can then be used as a starting point for extracting the constant strings from the
/// source files with external tooling.
///
/// # Panics
///
/// This macro panics if the `GETTEXT_EXTRACTION_DIR` variable is set and `message` has an
/// unexpected format.
/// Note that for example `concat!(...)` cannot be passed to this macro, because expansion works
/// outside in, meaning this macro would still see the `concat!` macro invocation, instead of a
/// string literal.
#[proc_macro]
pub fn gettext_extract(message: TokenStream) -> TokenStream {
    if let Ok(dir) = std::env::var("GETTEXT_EXTRACTION_DIR") {
        let pm2_message = proc_macro2::TokenStream::from(message.clone());
        let token_trees = pm2_message.into_iter().collect::<Vec<_>>();
        if token_trees.len() != 1 {
            panic!("Invalid number of tokens passed to gettext_extract. Expected one token, got: {token_trees:?}")
        }
        match &token_trees[0] {
            proc_macro2::TokenTree::Group(group) => {
                let group_tokens = group.stream().into_iter().collect::<Vec<_>>();
                if group_tokens.len() != 1 {
                    panic!("Invalid number of tokens in group passed to gettext_extract. Expected one token, got: {group_tokens:?}")
                }
                match &group_tokens[0] {
                    proc_macro2::TokenTree::Literal(_) => {
                        append_line_to_file(&message, &format!("{dir}/literals"));
                    }
                    proc_macro2::TokenTree::Ident(_) => {
                        append_line_to_file(&message, &format!("{dir}/consts"));
                    }
                    other => {
                        panic!(
                            "Expected literal or constant in gettext_extract, but got: {other:?}"
                        );
                    }
                }
            }
            other => {
                panic!("Expected group in gettext_extract, but got: {other:?}");
            }
        }
    }
    message
}
