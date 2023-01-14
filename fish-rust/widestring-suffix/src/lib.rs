extern crate proc_macro as pm;

use proc_macro2::{Group, Literal, TokenStream, TokenTree};
use quote::quote_spanned;
use syn::{Lit, LitStr};

/// A proc macro which allows easy creation of nul-terminated wide strings.
/// It replaces strings with an L suffix like so:
///    "foo"L
/// with a call like so:
///     crate::wchar::L!("foo")
#[proc_macro_attribute]
pub fn widestrs(_attr: pm::TokenStream, input: pm::TokenStream) -> pm::TokenStream {
    let s = widen_stream(input.into());
    s.into()
}

fn widen_token_tree(tt: TokenTree) -> TokenStream {
    match tt {
        TokenTree::Group(group) => {
            let wide_stream = widen_stream(group.stream());
            TokenTree::Group(Group::new(group.delimiter(), wide_stream)).into()
        }
        TokenTree::Literal(lit) => widen_literal(lit),
        tt => tt.into(),
    }
}

fn widen_stream(input: TokenStream) -> TokenStream {
    input.into_iter().map(widen_token_tree).collect()
}

fn try_parse_literal(tt: TokenTree) -> Option<LitStr> {
    let ts: TokenStream = tt.into();
    match syn::parse2::<Lit>(ts) {
        Ok(Lit::Str(lit)) => Some(lit),
        _ => None,
    }
}

fn widen_literal(lit: Literal) -> TokenStream {
    let tt = TokenTree::Literal(lit);
    match try_parse_literal(tt.clone()) {
        Some(lit) if lit.suffix() == "L" => {
            let value = lit.value();
            let span = lit.span();
            quote_spanned!(span=> crate::wchar::L!(#value)).into()
        }
        _ => tt.into(),
    }
}
