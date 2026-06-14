extern crate proc_macro;
use fluent_ftl_tools::{
    HasEntries as _, format_resource, parse_str_as_syntax_resource, serialize_resource,
};
use proc_macro::TokenStream;
use std::{
    collections::HashSet,
    ffi::{OsStr, OsString},
    io::Write as _,
    path::PathBuf,
};
use syn::{
    Ident, LitStr, Token,
    parse::{Parse, ParseStream},
    parse_macro_input,
};

struct LocalizeDefinition {
    message_id: String,
    message_definition: String,
    variables: Vec<String>,
}

impl Parse for LocalizeDefinition {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let message_id = input.parse::<LitStr>()?.value();
        input.parse::<Token![=]>()?;
        let message_definition = input.parse::<LitStr>()?.value();
        if !input.is_empty() {
            input.parse::<Token![,]>()?;
        }

        fn parse_key(input: ParseStream) -> syn::Result<String> {
            let key = input.parse::<Ident>()?;
            Ok(format!("{key}"))
        }
        let variables = Vec::from_iter(input.parse_terminated(parse_key, Token![,])?);
        Ok(Self {
            message_id,
            message_definition,
            variables,
        })
    }
}

fn check(localize_definition: &LocalizeDefinition) -> String {
    let message = format!(
        "{} = {}",
        localize_definition.message_id, localize_definition.message_definition
    );
    let resource = parse_str_as_syntax_resource(&message)
        .unwrap_or_else(|err| panic!("Failed to parse Fluent message\n{message}\n\n{err}"));
    let resource_entries = &resource.body;
    assert_eq!(
        resource_entries.len(),
        1,
        "Expected exactly one Fluent entry specified via macro."
    );
    assert!(
        matches!(resource_entries[0], fluent_syntax::ast::Entry::Message(_)),
        "Expected definition of Fluent message, but got {:?}",
        resource_entries[0]
    );
    let formatted_resource = format_resource(resource.clone())
        .unwrap_or_else(|err| panic!("Resource is not formatted correctly:\n{err}"));
    let formatted_resource_string = serialize_resource(&formatted_resource);
    let formatted_resource_string_trimmed = formatted_resource_string.trim();
    assert!(
        message == formatted_resource_string_trimmed,
        "Message is not formatted correctly.\nActual:\n{message}\nExpected:\n{formatted_resource_string_trimmed}"
    );
    let mut expected_variables = HashSet::new();
    for variable in &localize_definition.variables {
        assert!(
            expected_variables.insert(variable.as_str()),
            "Variable {variable} is used as a key more than once."
        );
    }
    resource.check_if_expected_variables_match_message(&expected_variables, &localize_definition.message_id).unwrap_or_else(|err| {
        panic!("Variables used in macro key-value pairs do not match variables used in Fluent message:\n{err}");
    });
    message
}

fn extract(message: &str, dir_path: &OsStr) {
    let dir = PathBuf::from(dir_path);
    let (path, result) = fish_tempfile::create_file_with_retry(|| {
        dir.join(fish_tempfile::random_filename(OsString::new()))
    });
    let mut file = result.unwrap_or_else(|e| {
        panic!("Failed to create temporary file {path:?}:\n{e}");
    });
    file.write_all(message.as_bytes()).unwrap();
    file.write_all(b"\n").unwrap();
}

#[proc_macro]
pub fn fluent_extract(input: TokenStream) -> TokenStream {
    if let Some(dir_path) = std::env::var_os("FISH_FLUENT_EXTRACTION_DIR") {
        let cloned_input = input.clone();
        let localize_definition = parse_macro_input!(cloned_input as LocalizeDefinition);
        let message = check(&localize_definition);
        extract(&message, &dir_path);
    }
    TokenStream::new()
}
