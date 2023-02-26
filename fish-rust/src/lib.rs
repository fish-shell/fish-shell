#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(clippy::needless_return)]
#![allow(clippy::manual_is_ascii_check)]

mod common;
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
mod flog;
mod future_feature_flags;
mod job_group;
mod nix;
mod parse_constants;
mod redirection;
mod signal;
mod smoke;
mod threads;
mod timer;
mod tokenizer;
mod topic_monitor;
mod util;
mod wchar;
mod wchar_ext;
mod wchar_ffi;
mod wgetopt;
mod wutil;

mod abbrs;
mod builtins;
mod env;
mod input_common;
mod re;
