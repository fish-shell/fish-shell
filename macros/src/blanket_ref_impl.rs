use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::spanned::Spanned;
use syn::{parse_quote, parse_quote_spanned};
use syn::{Block, Ident, ImplItem, ImplItemFn, ItemTrait, Pat, Signature, TraitItem, Visibility};

pub fn emit_trait_impl(trait_def: &ItemTrait, mutable: bool, stream: &mut TokenStream) {
    let name = &trait_def.ident;
    let impl_items = make_impl_body(&trait_def.items, mutable);
    let implementor = if mutable { quote!(&mut T) } else { quote!(&T) };
    quote! {
        impl<T: #name> #name for #implementor {
            #(#impl_items)*
        }
    }
    .to_tokens(stream);
}

/// Input:
/// ```
/// type Foo;
/// fn bar(&self, baz: Qux) -> Quux;
/// ```
///
/// Output:
/// ```
/// type Foo = T::Foo;
/// fn bar(&self, baz: Qux) -> Quux { T::bar(self, baz) }
/// ```
fn make_impl_body(trait_items: &[TraitItem], mutable: bool) -> Vec<ImplItem> {
    let mut impl_items = vec![];

    for item in trait_items {
        let trait_fn = match item {
            TraitItem::Fn(f) => f,
            TraitItem::Type(associated_type) => {
                let name = &associated_type.ident;
                impl_items.push(parse_quote! {
                    type #name = T::#name;
                });
                continue;
            }
            _ => continue,
        };

        let block = make_fn_body(&trait_fn.sig, mutable);

        // Copy the signature from the trait's definition.
        // Discard attributes and the default body.
        let impl_fn = ImplItemFn {
            attrs: vec![],
            vis: Visibility::Inherited,
            defaultness: None,
            sig: trait_fn.sig.clone(),
            block,
        };
        impl_items.push(impl_fn.into());
    }

    impl_items
}

/// Input:
/// ```
/// fn bar(&self, baz: Qux) -> Quux
/// ```
///
/// Output:
/// ```
/// { T::bar(self, baz) }
/// ```
fn make_fn_body(sig: &Signature, mutable: bool) -> Block {
    let mut args = vec![];
    for param in &sig.inputs {
        let arg = match param {
            syn::FnArg::Receiver(r) if r.mutability.is_some() && !mutable => {
                // Hackish: This trait method needs &mut self,
                // but we're currently generating the &T flavor of blanket impl,
                // so we're unable to call the original method.
                //
                // Just emit a dummy body, because this method isn't expected
                // to be called for this implementor.
                //
                // TODO: Split up the traits that need this (Leaf, Token, Keyword)
                // so that this case can become a hard error.
                return parse_quote!({ unimplemented!() });
            }

            syn::FnArg::Receiver(r) => Ident::from(r.self_token),
            syn::FnArg::Typed(pat) => {
                let Pat::Ident(pat) = &*pat.pat else {
                    // Reject any pattern other than just an identifier
                    return parse_quote_spanned! {pat.span()=>
                        {
                            compile_error!("expected a simple identifier");
                            loop {}
                        }
                    };
                };
                pat.ident.clone()
            }
        };
        args.push(arg)
    }

    let name = &sig.ident;
    parse_quote!({ T::#name(#(#args),*) })
}
