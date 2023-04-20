use crate::parser::Parser;
use crate::proc::JobGroupRef;
use crate::wchar::{wstr, WString};

pub fn exec_subshell_for_expand(
    cmd: &wstr,
    parser: &mut Parser,
    job_group: &JobGroupRef,
    outputs: &mut Vec<WString>,
) -> libc::c_int {
    todo!()
}
