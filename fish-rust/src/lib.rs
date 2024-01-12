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
pub mod common;

pub mod abbrs;
pub mod ast;
pub mod autoload;
pub mod builtins;
pub mod color;
pub mod complete;
pub mod curses;
pub mod editable_line;
pub mod env;
pub mod env_dispatch;
pub mod env_universal_common;
pub mod event;
pub mod exec;
pub mod expand;
pub mod fallback;
pub mod fd_monitor;
pub mod fd_readable_set;
pub mod fds;
pub mod flog;
pub mod fork_exec;
pub mod function;
pub mod future;
pub mod future_feature_flags;
pub mod global_safety;
pub mod highlight;
pub mod history;
pub mod input;
pub mod input_common;
pub mod io;
pub mod job_group;
pub mod kill;
#[allow(non_snake_case)]
pub mod libc;
pub mod locale;
pub mod nix;
pub mod null_terminated_array;
pub mod operation_context;
pub mod output;
pub mod pager;
pub mod parse_constants;
pub mod parse_execution;
pub mod parse_tree;
pub mod parse_util;
pub mod parser;
pub mod parser_keywords;
pub mod path;
pub mod pointer;
pub mod print_help;
pub mod proc;
pub mod re;
pub mod reader;
pub mod reader_history_search;
pub mod redirection;
pub mod screen;
pub mod signal;
pub mod termsize;
pub mod threads;
pub mod timer;
pub mod tinyexpr;
pub mod tokenizer;
pub mod topic_monitor;
pub mod trace;
pub mod universal_notifier;
pub mod util;
pub mod wait_handle;
pub mod wchar;
pub mod wchar_ext;
pub mod wcstringutil;
pub mod wgetopt;
pub mod widecharwidth;
pub mod wildcard;
pub mod wutil;

#[cfg(test)]
#[allow(unused_imports)] // Easy way to suppress warnings while we have two testing modes.
mod tests;
