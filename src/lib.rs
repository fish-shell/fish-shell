#![cfg_attr(feature = "benchmark", feature(test))]
#![allow(non_camel_case_types)]

pub const BUILD_VERSION: &str = env!("FISH_BUILD_VERSION");

#[macro_use]
pub mod common;

#[macro_use]
pub mod wutil;

pub mod abbrs;
pub mod ast;
pub mod autoload;
pub mod builtins;
pub mod color;
pub mod complete;
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
pub mod fs;
pub mod function;
pub mod future_feature_flags;
pub mod global_safety;
pub mod highlight;
pub mod history;
pub mod input;
pub mod input_common;
pub mod io;
pub mod job_group;
pub mod key;
pub mod kill;
pub mod locale;
pub mod nix;
pub mod null_terminated_array;
pub mod operation_context;
pub mod pager;
pub mod panic;
pub mod parse_constants;
pub mod parse_execution;
pub mod parse_tree;
pub mod parse_util;
pub mod parser;
pub mod parser_keywords;
pub mod path;
pub mod print_help;
pub mod proc;
pub mod re;
pub mod reader;
pub mod reader_history_search;
pub mod redirection;
pub mod screen;
pub mod signal;
pub mod stdx;
pub mod terminal;
pub mod termsize;
pub mod text_face;
pub mod threads;
pub mod timer;
pub mod tinyexpr;
pub mod tokenizer;
pub mod topic_monitor;
pub mod trace;
pub mod tty_handoff;
pub mod universal_notifier;
pub mod util;
pub mod wait_handle;
pub mod wchar;
pub mod wchar_ext;
pub mod wcstringutil;
pub mod wgetopt;
pub mod widecharwidth;
pub mod wildcard;

#[cfg(feature = "gettext-extract")]
pub extern crate fish_gettext_extraction;

#[cfg(test)]
mod tests;
