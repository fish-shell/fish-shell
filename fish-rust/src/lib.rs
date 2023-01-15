#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(clippy::needless_return)]

#[macro_use]
extern crate lazy_static;

mod fd_readable_set;
mod fds;
mod ffi;
mod ffi_init;
mod ffi_tests;
mod flog;
mod signal;
mod smoke;
mod topic_monitor;
mod wchar;
mod wchar_ext;
mod wchar_ffi;
mod wgetopt;
mod wutil;
