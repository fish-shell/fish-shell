use std::panic::{catch_unwind, UnwindSafe};

use libc::STDIN_FILENO;

use crate::{
    common::{read_blocked, PROGRAM_NAME},
    nix::isatty,
};

pub fn panic_handler(main: impl FnOnce() + UnwindSafe) -> ! {
    if !isatty(STDIN_FILENO) {
        main();
        unreachable!();
    }
    if catch_unwind(main).is_ok() {
        unreachable!();
    }
    printf!(
        "%s with PID %d crashed, please report a bug. Press Enter to exit",
        PROGRAM_NAME.get().unwrap(),
        unsafe { libc::getpid() }
    );
    let mut buf = [0_u8; 1];
    loop {
        let Ok(n) = read_blocked(STDIN_FILENO, &mut buf) else {
            std::process::exit(110);
        };
        if n == 0 || matches!(buf[0], b'q' | b'\n' | b'\r') {
            printf!("\n");
            std::process::exit(110);
        }
    }
}
