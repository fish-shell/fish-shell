use crate::ast::{self, Category, Node, NodeFfi, NodeVisitor, Type};
use crate::ffi::pretty_printer_t;
use crate::parse_constants::ParseTokenType;
use std::pin::Pin;

struct PrettyPrinter<'a> {
    companion: Pin<&'a mut pretty_printer_t>,
}
impl<'a> NodeVisitor<'a> for &mut PrettyPrinter<'a> {
    // Default implementation is to just visit children.
    fn visit(&mut self, node: &'a dyn Node) {
        let ffi_node = NodeFfi::new(node);
        // Leaf nodes we just visit their text.
        if node.as_keyword().is_some() {
            self.companion
                .as_mut()
                .emit_node_text((&ffi_node as *const NodeFfi<'_>).cast());
            return;
        }
        if let Some(token) = node.as_token() {
            if token.token_type() == ParseTokenType::end {
                self.companion
                    .as_mut()
                    .visit_semi_nl((&ffi_node as *const NodeFfi<'_>).cast());
                return;
            }
            self.companion
                .as_mut()
                .emit_node_text((&ffi_node as *const NodeFfi<'_>).cast());
            return;
        }
        match node.typ() {
            Type::argument | Type::variable_assignment => {
                self.companion
                    .as_mut()
                    .emit_node_text((&ffi_node as *const NodeFfi<'_>).cast());
            }
            Type::redirection => {
                self.companion.as_mut().visit_redirection(
                    (node.as_redirection().unwrap() as *const ast::Redirection).cast(),
                );
            }
            Type::maybe_newlines => {
                self.companion.as_mut().visit_maybe_newlines(
                    (node.as_maybe_newlines().unwrap() as *const ast::MaybeNewlines).cast(),
                );
            }
            Type::begin_header => {
                // 'begin' does not require a newline after it, but we insert one.
                node.accept(self, false);
                self.companion.as_mut().visit_begin_header();
            }
            _ => {
                // For branch and list nodes, default is to visit their children.
                if [Category::branch, Category::list].contains(&node.category()) {
                    node.accept(self, false);
                    return;
                }
                panic!("unexpected node type");
            }
        }
    }
}

#[cxx::bridge]
#[allow(clippy::needless_lifetimes)] // false positive
mod fish_indent_ffi {
    extern "C++" {
        include!("ast.h");
        include!("fish_indent_common.h");
        type pretty_printer_t = crate::ffi::pretty_printer_t;
        type Ast = crate::ast::Ast;
        type NodeFfi<'a> = crate::ast::NodeFfi<'a>;
    }
    extern "Rust" {
        type PrettyPrinter<'a>;
        unsafe fn new_pretty_printer(
            companion: Pin<&mut pretty_printer_t>,
        ) -> Box<PrettyPrinter<'_>>;
        #[cxx_name = "visit"]
        unsafe fn visit_ffi<'a>(self: &mut PrettyPrinter<'a>, node: &'a NodeFfi<'a>);
    }
}

fn new_pretty_printer(companion: Pin<&mut pretty_printer_t>) -> Box<PrettyPrinter<'_>> {
    Box::new(PrettyPrinter { companion })
}
impl<'a> PrettyPrinter<'a> {
    fn visit_ffi(mut self: &mut PrettyPrinter<'a>, node: &'a NodeFfi<'a>) {
        self.visit(node.as_node());
    }
}
