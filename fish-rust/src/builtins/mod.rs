pub mod shared;

pub mod abbr;
pub mod argparse;
pub mod bg;
pub mod block;
pub mod builtin;
pub mod cd;
pub mod command;
pub mod contains;
pub mod count;
pub mod echo;
pub mod emit;
pub mod exit;
pub mod function;
pub mod functions;
pub mod math;
pub mod path;
pub mod printf;
pub mod pwd;
pub mod random;
pub mod realpath;
pub mod r#return;
pub mod set_color;
pub mod status;
pub mod string;
pub mod test;
pub mod r#type;
pub mod wait;

// Note these tests will NOT run with cfg(test).
mod tests;

mod prelude {
    pub use super::shared::*;
    pub use libc::c_int;

    pub(crate) use crate::{
        ffi::{self, parser_t, separation_type_t, Repin},
        wchar::prelude::*,
        wchar_ffi::{c_str, AsWstr, WCharFromFFI, WCharToFFI},
        wgetopt::{
            wgetopter_t, wopt, woption,
            woption_argument_t::{self, *},
            NONOPTION_CHAR_CODE,
        },
        wutil::{fish_wcstoi, fish_wcstol, fish_wcstoul},
    };
}
