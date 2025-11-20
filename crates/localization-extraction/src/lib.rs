extern crate proc_macro;
use proc_macro::TokenStream;
use std::fs::{File, OpenOptions};

/// Used for extraction of gettext msgids and Fluent message IDs.
pub fn localization_extract(
    token_stream: TokenStream,
    extraction_file_env_var_name: &str,
    extraction_func: impl FnOnce(String, File),
) -> TokenStream {
    if let Some(file_path) = std::env::var_os(extraction_file_env_var_name) {
        let pm2_token_stream = proc_macro2::TokenStream::from(token_stream.clone());
        let mut token_trees = pm2_token_stream.into_iter();
        let first_token = token_trees.next().unwrap_or_else(|| {
            panic!("localization_extract got empty token stream. Expected one token.")
        });
        if token_trees.next().is_some() {
            panic!(
                "Invalid number of tokens passed to localization_extract. Expected one token, but got more."
            )
        }
        let proc_macro2::TokenTree::Group(group) = first_token else {
            panic!("Expected group in localization_extract, but got: {first_token:?}");
        };
        let mut group_tokens = group.stream().into_iter();
        let first_group_token = group_tokens
            .next()
            .expect("localization_extract expected one group token but got none.");
        if group_tokens.next().is_some() {
            panic!(
                "Invalid number of tokens in group passed to localization_extract. Expected one token, but got more."
            )
        }
        if let proc_macro2::TokenTree::Literal(_) = first_group_token {
            let file = OpenOptions::new()
                .create(true)
                .append(true)
                .open(&file_path)
                .unwrap_or_else(|e| panic!("Could not open file {file_path:?}: {e}"));
            extraction_func(token_stream.to_string(), file);
        } else {
            panic!("Expected literal in localization_extract, but got: {first_group_token:?}");
        }
    }
    token_stream
}
