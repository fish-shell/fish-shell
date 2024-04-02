use std::panic::{set_hook, take_hook, UnwindSafe};

use libc::STDIN_FILENO;

use crate::{
    common::{read_blocked, PROGRAM_NAME},
    nix::isatty,
    threads::asan_maybe_exit,
};

pub fn panic_handler(main: impl FnOnce() -> i32 + UnwindSafe) -> ! {
    if isatty(STDIN_FILENO) {
        let standard_hook = take_hook();
        set_hook(Box::new(move |panic_info| {
            standard_hook(panic_info);
            eprintf!(
                "%s crashed, please report a bug. Debug PID %d or press Enter to exit",
                PROGRAM_NAME.get().unwrap(),
                unsafe { libc::getpid() }
            );
            let mut buf = [0_u8; 1];
            loop {
                let Ok(n) = read_blocked(STDIN_FILENO, &mut buf) else {
                    break;
                };
                if n == 0 || matches!(buf[0], b'q' | b'\n' | b'\r') {
                    eprintf!("\n");
                    break;
                }
            }
        }));
    }
    let exit_status = main();
    asan_maybe_exit(exit_status);
    std::process::exit(exit_status)
}
