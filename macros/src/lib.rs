use proc_macro2::TokenStream;
use syn::{parse::Parse, ItemTrait};

mod ast_node;
mod blanket_ref_impl;

fn parse_several<T: Parse>(input: syn::parse::ParseStream<'_>) -> Result<Vec<T>, syn::Error> {
    let mut items = Vec::new();
    while !input.is_empty() {
        items.push(input.parse()?);
    }
    Ok(items)
}

#[proc_macro]
pub fn define_node(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let mut stream = TokenStream::new();

    let enum_defs = syn::parse_macro_input!(input with parse_several);
    let state = ast_node::State::new(&enum_defs);
    state.generate_enum_definitions(&mut stream);
    state.generate_transitive_from_impls(&mut stream);
    state.generate_inherent_accept_method(&mut stream);

    stream.into()
}

/// Given a trait definition such as:
/// ```
/// trait Node {
///     type ParseError;
///     const NODE_TYPE: NodeType;
///     fn source_range(&self) -> SourceRange;
/// }
/// ```
/// Generate an implementation of the form:
/// ```
/// impl<T: Node> Node for &T {
///     type ParseError = T::ParseError;
///     const NODE_TYPE: NodeType = T::NODE_TYPE;
///     fn source_range(&self) -> SourceRange {
///         T::source_range(self)
///     }
/// }
/// ```
/// A similar `impl` will be generated for `&mut T`.
#[proc_macro_attribute]
pub fn blanket_ref(
    _attr: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let mut output = TokenStream::from(input.clone());
    let trait_def = syn::parse_macro_input!(input as ItemTrait);
    blanket_ref_impl::emit_trait_impl(&trait_def, false, &mut output);
    blanket_ref_impl::emit_trait_impl(&trait_def, true, &mut output);
    output.into()
}

/// Like [`macro@blanket_ref`], but only generates an `impl` for `&mut T`.
#[proc_macro_attribute]
pub fn blanket_mut(
    _attr: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let mut output = TokenStream::from(input.clone());
    let trait_def = syn::parse_macro_input!(input as ItemTrait);
    blanket_ref_impl::emit_trait_impl(&trait_def, true, &mut output);
    output.into()
}
