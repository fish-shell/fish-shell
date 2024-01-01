/// Bridged functions concerned with initialization.
use crate::locale;

#[cxx::bridge]
mod ffi2 {
    extern "Rust" {
        fn rust_init();
        fn rust_set_flog_file_fd(fd: i32);
        fn rust_invalidate_numeric_locale();
    }
}

/// Entry point for Rust-specific initialization.
fn rust_init() {
    crate::topic_monitor::topic_monitor_init();
    crate::threads::init();
}

/// FFI bridge for setting FLOG file descriptor.
fn rust_set_flog_file_fd(fd: i32) {
    crate::flog::set_flog_file_fd(fd as libc::c_int);
}

/// FFI bridge to invalidate cached locale bits.
fn rust_invalidate_numeric_locale() {
    locale::invalidate_numeric_locale();
}
