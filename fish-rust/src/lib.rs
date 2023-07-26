#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(clippy::needless_return)]
#![allow(clippy::manual_is_ascii_check)]
#![allow(clippy::bool_assert_comparison)]
#![allow(clippy::uninlined_format_args)]
#![allow(clippy::derivable_impls)]
#![allow(clippy::option_map_unit_fn)]
#![allow(clippy::ptr_arg)]
#![allow(clippy::field_reassign_with_default)]

pub const BUILD_VERSION: &str = match option_env!("FISH_BUILD_VERSION") {
    Some(v) => v,
    None => git_version::git_version!(args = ["--always", "--dirty=-dirty"], fallback = "unknown"),
};

#[macro_use]
mod common;

mod abbrs;
mod ast;
mod builtins;
mod color;
mod compat;
mod complete;
mod curses;
mod env;
mod env_dispatch;
mod event;
mod expand;
mod fallback;
mod fd_monitor;
mod fd_readable_set;
mod fds;
#[allow(rustdoc::broken_intra_doc_links)]
#[allow(clippy::module_inception)]
#[allow(clippy::new_ret_no_self)]
#[allow(clippy::wrong_self_convention)]
#[allow(clippy::needless_lifetimes)]
mod ffi;
mod ffi_init;
mod ffi_tests;
mod fish;
mod fish_indent;
mod flog;
mod function;
mod future_feature_flags;
mod global_safety;
mod highlight;
mod history;
mod io;
mod job_group;
mod kill;
mod locale;
mod nix;
mod null_terminated_array;
mod operation_context;
mod output;
mod parse_constants;
mod parse_tree;
mod parse_util;
mod parser_keywords;
mod path;
mod print_help;
mod re;
mod reader;
mod redirection;
mod signal;
mod smoke;
mod spawn;
mod termsize;
mod threads;
mod timer;
mod tinyexpr;
mod tokenizer;
mod topic_monitor;
mod trace;
mod util;
mod wait_handle;
mod wchar;
mod wchar_ext;
mod wchar_ffi;
mod wcstringutil;
mod wgetopt;
mod widecharwidth;
mod wildcard;
mod wutil;

// Don't use `#[cfg(test)]` here to make sure ffi tests are built and tested
mod tests;
