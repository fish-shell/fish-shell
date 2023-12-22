#![cfg_attr(feature = "benchmark", feature(test))]
#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(unstable_name_collisions)]
#![allow(clippy::bool_assert_comparison)]
#![allow(clippy::box_default)]
#![allow(clippy::collapsible_if)]
#![allow(clippy::comparison_chain)]
#![allow(clippy::derivable_impls)]
#![allow(clippy::field_reassign_with_default)]
#![allow(clippy::get_first)]
#![allow(clippy::if_same_then_else)]
#![allow(clippy::manual_is_ascii_check)]
#![allow(clippy::mut_from_ref)]
#![allow(clippy::needless_return)]
#![allow(clippy::option_map_unit_fn)]
#![allow(clippy::ptr_arg)]
#![allow(clippy::redundant_slicing)]
#![allow(clippy::too_many_arguments)]
#![allow(clippy::uninlined_format_args)]
#![allow(clippy::unnecessary_to_owned)]
#![allow(clippy::unnecessary_unwrap)]

pub const BUILD_VERSION: &str = match option_env!("FISH_BUILD_VERSION") {
    Some(v) => v,
    None => git_version::git_version!(args = ["--always", "--dirty=-dirty"], fallback = "unknown"),
};

#[macro_use]
mod common;

mod abbrs;
mod ast;
mod autoload;
mod builtins;
mod color;
#[allow(non_snake_case)]
mod compat;
mod complete;
mod curses;
mod editable_line;
mod env;
mod env_dispatch;
mod env_universal_common;
mod event;
mod exec;
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
#[allow(unused_imports)]
mod ffi;
mod ffi_init;
mod ffi_tests;
mod fish;
mod fish_indent;
mod flog;
mod fork_exec;
mod function;
mod future;
mod future_feature_flags;
mod global_safety;
mod highlight;
mod history;
mod input;
mod input_common;
mod input_ffi;
mod io;
mod job_group;
mod kill;
mod locale;
mod nix;
mod null_terminated_array;
mod operation_context;
mod output;
mod pager;
mod parse_constants;
mod parse_execution;
mod parse_tree;
mod parse_util;
mod parser;
mod parser_keywords;
mod path;
mod pointer;
mod print_help;
mod proc;
mod re;
mod reader;
mod reader_history_search;
mod redirection;
mod screen;
mod signal;
mod smoke;
mod termsize;
mod threads;
mod timer;
mod tinyexpr;
mod tokenizer;
mod topic_monitor;
mod trace;
mod universal_notifier;
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

#[cfg(any(test, feature = "fish-ffi-tests"))]
#[allow(unused_imports)] // Easy way to suppress warnings while we have two testing modes.
mod tests;
