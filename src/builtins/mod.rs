pub mod shared;

pub mod abbr;
pub mod argparse;
pub mod bg;
pub mod bind;
pub mod block;
pub mod r#break;
pub mod breakpoint;
pub mod builtin;
pub mod cd;
pub mod command;
pub mod commandline;
pub mod complete;
pub mod contains;
pub mod r#continue;
pub mod count;
pub mod disown;
pub mod echo;
pub mod emit;
pub mod eval;
pub mod exit;
pub mod r#false;
pub mod fg;
pub mod fish_indent;
pub mod fish_key_reader;
pub mod function;
pub mod functions;
pub mod r#gettext;
pub mod history;
pub mod jobs;
pub mod math;
pub mod path;
pub mod printf;
pub mod pwd;
pub mod random;
pub mod read;
pub mod realpath;
pub mod r#return;
pub mod set;
pub mod set_color;
pub mod source;
pub mod status;
pub mod string;
pub mod test;
pub mod r#true;
pub mod r#type;
pub mod ulimit;
pub mod wait;

#[cfg(test)]
mod tests;

mod prelude {
    pub use super::shared::*;
    pub use libc::c_int;
    pub use std::borrow::Cow;

    #[allow(unused_imports)]
    pub(crate) use crate::{
        flog::{FLOG, FLOGF},
        io::{IoStreams, SeparationType},
        parser::Parser,
        wchar::prelude::*,
        wgetopt::{
            wopt,
            ArgType::{self, *},
            WGetopter, WOption, NON_OPTION_CHAR,
        },
        wutil::{fish_wcstoi, fish_wcstol, fish_wcstoul},
    };
}
