use crate::{common::get_program_name, nix::isatty, threads::is_main_thread};
use fish_common::read_blocked;
use fish_thread::SingleThreadedOnceCell;
use libc::STDIN_FILENO;
use std::{
    panic::{UnwindSafe, set_hook, take_hook},
    sync::atomic::{AtomicBool, Ordering},
    time::Duration,
};

pub static AT_EXIT: SingleThreadedOnceCell<Box<dyn Fn() + Send + Sync>> =
    SingleThreadedOnceCell::new();

pub fn panic_handler(main: impl FnOnce() -> i32 + UnwindSafe) -> ! {
    // The isatty() check will stop us from hanging in most fish tests, but not those
    // running in a simulated terminal emulator environment (such as the tmux or pexpect
    // tests). The FISH_FAST_FAIL environment variable is set in the test driver to
    // prevent the test suite from hanging on panic.
    let cleanup = || {
        if is_main_thread() {
            if let Some(at_exit) = AT_EXIT.get() {
                (at_exit)();
            }
        }
    };
    if isatty(STDIN_FILENO) && std::env::var_os("FISH_FAST_FAIL").is_none() {
        let standard_hook = take_hook();
        set_hook(Box::new(move |panic_info| {
            standard_hook(panic_info);
            static PANICKING: AtomicBool = AtomicBool::new(false);
            if PANICKING
                .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
                .is_err()
            {
                return;
            }
            cleanup();
            eprintf!("%s crashed, please report a bug.", get_program_name());
            if !is_main_thread() {
                eprintf!("\n");
                std::thread::sleep(Duration::from_secs(1));
            } else {
                eprintf!(" Debug PID %d or press Enter to exit\n", unsafe {
                    libc::getpid()
                });
                let mut buf = [0_u8; 1];
                while let Ok(n) = read_blocked(STDIN_FILENO, &mut buf) {
                    if n == 0 || matches!(buf[0], b'q' | b'\n' | b'\r') {
                        eprintf!("\n");
                        break;
                    }
                }
            }
            std::process::abort();
        }));
    }
    let exit_status = main();
    cleanup();
    std::process::exit(exit_status)
}
