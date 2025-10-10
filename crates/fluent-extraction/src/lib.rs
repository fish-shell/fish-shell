extern crate proc_macro;
use fish_localization_extraction::localization_extract;
use proc_macro::TokenStream;
use std::{fs::File, io::Write as _};

fn write_fluent_id_to_file(fluent_id: String, mut file: File) {
    let stripped_fluent_id = fluent_id
        .strip_prefix('"')
        .unwrap_or_else(|| {
            panic!("Fluent ID token stream does not start with double quote: {fluent_id}")
        })
        .strip_suffix('"')
        .unwrap_or_else(|| {
            panic!("Fluent ID token stream does not end with double quote: {fluent_id}")
        });
    let id_bytes = stripped_fluent_id.as_bytes();
    // Ensure that IDs match the spec
    // Identifier          ::= [a-zA-Z] [a-zA-Z0-9_-]*
    // https://github.com/projectfluent/fluent/blob/10a1bc60bee843c14a30216fa4cebdc559bf2076/spec/fluent.ebnf#L85
    match id_bytes.iter().next() {
        Some(b) => {
            assert!(
                b.is_ascii_alphabetic(),
                "Fluent message ID must start with an alphabetic ASCII character [a-zA-Z]\nAffected ID: {fluent_id}"
            );
        }
        None => {
            panic!("Empty Fluent ID.");
        }
    }
    if !id_bytes
        .iter()
        .all(|&b| b.is_ascii_alphanumeric() || b == b'-' || b == b'_')
    {
        panic!(
            "Fluent ID contains invalid characters. Allowed characters: [a-zA-Z_-]\nAffected ID: {fluent_id}"
        )
    }
    file.write_all(id_bytes).unwrap();
    file.write_all(b"\n").unwrap();
}

/// The `fluent_id` is passed through unmodified.
/// If `FISH_FLUENT_ID_DIR` is defined in the environment,
/// the Fluent ID is written into a new file in this directory,
/// so that it can be used to check whether all IDs have associated translations in the
/// default/fallback language, and that no ID are specified in FTL files which are not used by the
/// code.
/// The `fluent_id` must be a string literal.
///
/// # Panics
///
/// This macro panics if the `FISH_FLUENT_ID_DIR` variable is set and `fluent_id` has an
/// unexpected format.
/// Note that for example `concat!(...)` cannot be passed to this macro, because expansion works
/// outside in, meaning this macro would still see the `concat!` macro invocation, instead of a
/// string literal.
#[proc_macro]
pub fn fluent_extract(fluent_id: TokenStream) -> TokenStream {
    localization_extract(fluent_id, "FISH_FLUENT_ID_DIR", write_fluent_id_to_file)
}
