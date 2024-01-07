pub mod shared;

pub mod abbr;
pub mod argparse;
pub mod bg;
pub mod bind;
pub mod block;
pub mod builtin;
pub mod cd;
pub mod command;
pub mod commandline;
pub mod complete;
pub mod contains;
pub mod count;
pub mod disown;
pub mod echo;
pub mod emit;
pub mod eval;
pub mod exit;
pub mod fg;
pub mod function;
pub mod functions;
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
            wgetopter_t, wopt, woption,
            woption_argument_t::{self, *},
            NONOPTION_CHAR_CODE,
        },
        wutil::{fish_wcstoi, fish_wcstol, fish_wcstoul},
    };
}
