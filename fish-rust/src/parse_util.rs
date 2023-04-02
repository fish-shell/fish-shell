use crate::ast::{Node, NodeFfi, NodeVisitor};
use crate::ffi::indent_visitor_t;
use std::pin::Pin;

struct IndentVisitor<'a> {
    companion: Pin<&'a mut indent_visitor_t>,
}
impl<'a> NodeVisitor<'a> for IndentVisitor<'a> {
    // Default implementation is to just visit children.
    fn visit(&mut self, node: &'a dyn Node) {
        let ffi_node = NodeFfi::new(node);
        let dec = self
            .companion
            .as_mut()
            .visit((&ffi_node as *const NodeFfi<'_>).cast());
        node.accept(self, false);
        self.companion.as_mut().did_visit(dec);
    }
}

#[cxx::bridge]
#[allow(clippy::needless_lifetimes)] // false positive
mod parse_util_ffi {
    extern "C++" {
        include!("ast.h");
        include!("parse_util.h");
        type indent_visitor_t = crate::ffi::indent_visitor_t;
        type Ast = crate::ast::Ast;
        type NodeFfi<'a> = crate::ast::NodeFfi<'a>;
    }
    extern "Rust" {
        type IndentVisitor<'a>;
        unsafe fn new_indent_visitor(
            companion: Pin<&mut indent_visitor_t>,
        ) -> Box<IndentVisitor<'_>>;
        #[cxx_name = "visit"]
        unsafe fn visit_ffi<'a>(self: &mut IndentVisitor<'a>, node: &'a NodeFfi<'a>);
    }
}

fn new_indent_visitor(companion: Pin<&mut indent_visitor_t>) -> Box<IndentVisitor<'_>> {
    Box::new(IndentVisitor { companion })
}
impl<'a> IndentVisitor<'a> {
    fn visit_ffi(self: &mut IndentVisitor<'a>, node: &'a NodeFfi<'a>) {
        self.visit(node.as_node());
    }
}
