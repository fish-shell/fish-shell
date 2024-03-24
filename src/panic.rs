use std::panic::{catch_unwind, UnwindSafe};

use libc::STDIN_FILENO;

use crate::{
    common::{read_blocked, PROGRAM_NAME},
    nix::isatty,
    threads::asan_maybe_exit,
};

pub fn panic_handler(main: impl FnOnce() -> i32 + UnwindSafe) -> ! {
    let exit_status = panic_handler_impl(main);
    asan_maybe_exit(exit_status);
    std::process::exit(exit_status)
}

fn panic_handler_impl(main: impl FnOnce() -> i32 + UnwindSafe) -> i32 {
    if !isatty(STDIN_FILENO) {
        return main();
    }
    if let Ok(status) = catch_unwind(main) {
        return status;
    }
    printf!(
        "%s with PID %d crashed, please report a bug. Press Enter to exit",
        PROGRAM_NAME.get().unwrap(),
        unsafe { libc::getpid() }
    );
    let mut buf = [0_u8; 1];
    loop {
        let Ok(n) = read_blocked(STDIN_FILENO, &mut buf) else {
            break;
        };
        if n == 0 || matches!(buf[0], b'q' | b'\n' | b'\r') {
            printf!("\n");
            break;
        }
    }
    110
}
