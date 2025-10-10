extern crate proc_macro;
use proc_macro::TokenStream;
use std::{ffi::OsString, fs::OpenOptions, io::Write};

fn append_fluent_id_to_file(fluent_id: &TokenStream, file_path: &OsString) {
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(file_path)
        .unwrap_or_else(|e| panic!("Could not open file {file_path:?}: {e}"));
    let fluent_id = fluent_id.to_string();
    if fluent_id.starts_with('-') {
        panic!(
            "Fluent ID may not start with a '-'. A leading '-' marks a term within an FTL file.\nAffected ID: {fluent_id}"
        );
    }
    if fluent_id.contains('\n') {
        panic!("Fluent IDs may not contain newlines. Newline found in '{fluent_id}'")
    }
    if fluent_id.contains('.') {
        panic!(
            "Fluent IDs may not contain periods. Period found in '{fluent_id}'.\nFluent attributes are unsupported."
        )
    }
    if !fluent_id.starts_with('"') {
        panic!("Fluent ID token stream does not start with double quote: {fluent_id}");
    }
    if !fluent_id.ends_with('"') {
        panic!("Fluent ID token stream does not end with double quote: {fluent_id}");
    }
    if fluent_id.len() < 3 {
        panic!("Fluent ID is empty: {fluent_id}");
    }
    let id_bytes = &fluent_id.as_bytes()[1..(fluent_id.len() - 1)];
    file.write_all(id_bytes).unwrap();
    file.write_all(b"\n").unwrap();
}

/// The `fluent_id` is passed through unmodified.
/// If `FISH_FLUENT_EXTRACTION_FILE` is defined in the environment,
/// the Fluent ID is appended to the file at this path,
/// so that it can be used to check whether all IDs have associated translations in the
/// default/fallback language, and that no ID are specified in FTL files which are not used by the
/// code.
/// The `fluent_id` must be a string literal.
///
/// # Panics
///
/// This macro panics if the `FISH_FLUENT_EXTRACTION_FILE` variable is set and `fluent_id` has an
/// unexpected format.
/// Note that for example `concat!(...)` cannot be passed to this macro, because expansion works
/// outside in, meaning this macro would still see the `concat!` macro invocation, instead of a
/// string literal.
#[proc_macro]
pub fn fluent_extract(fluent_id: TokenStream) -> TokenStream {
    if let Some(file_path) = std::env::var_os("FISH_FLUENT_EXTRACTION_FILE") {
        let pm2_fluent_id = proc_macro2::TokenStream::from(fluent_id.clone());
        let mut token_trees = pm2_fluent_id.into_iter();
        let first_token = token_trees
            .next()
            .expect("fluent_extract got empty token stream. Expected one token.");
        if token_trees.next().is_some() {
            panic!(
                "Invalid number of tokens passed to fluent_extract. Expected one token, but got more."
            )
        }
        if let proc_macro2::TokenTree::Group(group) = first_token {
            let mut group_tokens = group.stream().into_iter();
            let first_group_token = group_tokens
                .next()
                .expect("fluent_extract expected one group token but got none.");
            if group_tokens.next().is_some() {
                panic!(
                    "Invalid number of tokens in group passed to fluent_extract. Expected one token, but got more."
                )
            }
            if let proc_macro2::TokenTree::Literal(_) = first_group_token {
                append_fluent_id_to_file(&fluent_id, &file_path);
            } else {
                panic!("Expected literal in fluent_extract, but got: {first_group_token:?}");
            }
        } else {
            panic!("Expected group in fluent_extract, but got: {first_token:?}");
        }
    }
    fluent_id
}
