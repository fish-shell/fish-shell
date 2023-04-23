use crate::ast;
use crate::io::IoStreams;
use crate::parse_tree::ParsedSourceRef;
use crate::parser::Parser;
use crate::wchar::WString;
use libc::c_int;

pub fn function(
    parser: &mut Parser,
    streams: &IoStreams,
    c_args: &[WString],
    source: &ParsedSourceRef,
    func_node: &ast::BlockStatement,
) -> c_int {
    todo!()
}
