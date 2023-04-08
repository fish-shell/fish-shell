#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(clippy::needless_return)]
#![allow(clippy::manual_is_ascii_check)]
#![allow(clippy::bool_assert_comparison)]
#![allow(clippy::uninlined_format_args)]
#![allow(clippy::derivable_impls)]

#[macro_use]
mod common;

mod abbrs;
mod builtins;
mod color;
mod compat;
mod env;
mod event;
mod expand;
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
mod global_safety;
mod job_group;
mod locale;
mod nix;
mod parse_constants;
mod path;
mod re;
mod redirection;
mod signal;
mod smoke;
mod termsize;
mod threads;
mod timer;
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
