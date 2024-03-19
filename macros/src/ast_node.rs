use proc_macro2::TokenStream;
use quote::{format_ident, quote, ToTokens};
use std::collections::HashMap;
use syn::{parse_quote, punctuated::Punctuated, Fields, Ident, ItemEnum, Token};

pub struct State<'a> {
    enums: &'a [ItemEnum],
    tree: HashMap<&'a Ident, Vec<&'a Ident>>,
}

impl<'a> State<'a> {
    pub fn new(enums: &'a [ItemEnum]) -> Self {
        Self {
            enums,
            tree: enums
                .iter()
                .map(|e| (&e.ident, e.variants.iter().map(|v| &v.ident).collect()))
                .collect(),
        }
    }

    pub fn generate_enum_definitions(&self, stream: &mut TokenStream) {
        for flavor in FLAVORS {
            for definition in self.enums {
                let new_def = flavor.transform(definition, |x| self.tree.contains_key(x));
                new_def.to_tokens(stream);
            }
        }
    }

    pub fn generate_transitive_from_impls(&self, stream: &mut TokenStream) {
        let root = &self.enums[0].ident;

        for category in &self.tree[root] {
            for node in &self.tree[category] {
                if let Some(leaves) = self.tree.get(node) {
                    // From<KeywordEnum> for NodeEnum
                    self.emit_from_impl([node, category, root], true, stream);

                    for leaf in leaves {
                        // From<KeywordTime> for LeafEnum
                        self.emit_from_impl([leaf, node, category], false, stream);
                        // From<KeywordTime> for NodeEnum
                        self.emit_from_impl([leaf, category, root], false, stream);
                    }
                } else {
                    // From<Argument> for NodeEnum>
                    self.emit_from_impl([node, category, root], false, stream);
                }
            }
        }
    }

    fn emit_from_impl(
        &self,
        [inner, middle, outer]: [&Ident; 3],
        inner_is_already_enum: bool,
        stream: &mut TokenStream,
    ) {
        for flavor in FLAVORS {
            let inner_ty = if inner_is_already_enum {
                flavor.suffix_and_lifetime(inner)
            } else {
                flavor.ampersand(inner)
            };
            let middle_ty = flavor.suffix(middle);
            let outer_ty = flavor.suffix_and_lifetime(outer);

            let lifetime = (flavor != Flavor::Val).then_some(quote!(<'n>));

            quote! {
                impl #lifetime From<#inner_ty> for #outer_ty {
                    fn from(value: #inner_ty) -> Self {
                        Self::from(#middle_ty::from(value))
                    }
                }
            }
            .to_tokens(stream)
        }
    }

    /// Hackish: An accept() method that takes NodeRef<'a> rather than &'a NodeRef<'_>.
    /// Visitors that actually use the lifetime, such as Traversal, need this behavior,
    /// because if you have an &'a NodeRef<'b>, then 'b is your actual borrow from the AST.
    pub fn generate_inherent_accept_method(&self, stream: &mut TokenStream) {
        for definition in self.enums {
            let ty = Flavor::Ref.suffix(&definition.ident);
            let variants = definition.variants.iter().map(|v| &v.ident);
            quote! {
                impl<'n> #ty<'n> {
                    pub fn accept(self, visitor: &mut impl NodeVisitor<'n>, reverse: bool) {
                        match self {
                            #(Self::#variants(x) => x.accept(visitor, reverse)),*
                        }
                    }
                }
            }
            .to_tokens(stream)
        }
    }
}

#[derive(Copy, Clone, PartialEq)]
enum Flavor {
    Val,
    Ref,
    Mut,
}

const FLAVORS: [Flavor; 3] = [Flavor::Val, Flavor::Ref, Flavor::Mut];

impl Flavor {
    fn transform(self, def: &ItemEnum, is_enum: impl Fn(&Ident) -> bool) -> ItemEnum {
        let mut def = def.clone();

        def.ident = self.suffix(&def.ident);
        if self != Self::Val {
            def.generics = parse_quote!(<'n>);
        }

        for variant in &mut def.variants {
            let name = &variant.ident;
            let field_type = if is_enum(name) {
                self.suffix_and_lifetime(name)
            } else {
                self.ampersand(name)
            };
            variant.fields = Fields::Unnamed(parse_quote! { (#field_type) });
        }

        // Hackish: We want to explicitly write the mut-specific #[enum_dispatch] attributes,
        // but they can't be applied to the Ref flavor. The Ref flavor also gets Copy+Clone.
        if self == Self::Ref {
            fn mentions_mut(attr: &syn::Attribute) -> bool {
                let parser = Punctuated::<Ident, Token![,]>::parse_separated_nonempty;
                let Ok(args) = attr.parse_args_with(parser) else {
                    return false;
                };
                args.iter().any(|arg| arg.to_string().contains("Mut"))
            }

            def.attrs.retain(|attr| !mentions_mut(attr));
            def.attrs.push(parse_quote!(#[derive(Copy, Clone)]))
        }

        def
    }

    fn suffix(self, name: &Ident) -> Ident {
        match self {
            Self::Val => format_ident!("{name}Enum"),
            Self::Ref => format_ident!("{name}Ref"),
            Self::Mut => format_ident!("{name}RefMut"),
        }
    }

    fn suffix_and_lifetime(self, name: &Ident) -> syn::Type {
        let name = self.suffix(name);
        match self {
            Self::Val => parse_quote!(#name),
            Self::Ref | Self::Mut => parse_quote!(#name<'n>),
        }
    }

    fn ampersand(self, name: &Ident) -> syn::Type {
        match self {
            Self::Val => parse_quote!(#name),
            Self::Ref => parse_quote!(&'n #name),
            Self::Mut => parse_quote!(&'n mut #name),
        }
    }
}
