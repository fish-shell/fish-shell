use std::{
    panic::{set_hook, take_hook, PanicInfo, UnwindSafe},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    time::Duration,
};

use libc::{SIGINT, SIG_DFL, STDIN_FILENO};
use once_cell::sync::OnceCell;
use sentry::integrations::panic::PanicIntegration;

use crate::{
    common::{read_blocked, PROGRAM_NAME},
    nix::isatty,
    threads::{asan_maybe_exit, is_main_thread},
};

pub static AT_EXIT: OnceCell<Box<dyn Fn(bool) + Send + Sync>> = OnceCell::new();

pub fn panic_handler(main: impl FnOnce() -> i32 + UnwindSafe) -> ! {
    // The isatty() check will stop us from hanging in most fish tests, but not those
    // running in a simulated terminal emulator environment (such as the tmux or pexpect
    // tests). The FISH_FAST_FAIL environment variable is set in the test driver to
    // prevent the test suite from hanging on panic.
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
            if let Some(at_exit) = AT_EXIT.get() {
                (at_exit)(false);
            }
            eprintf!(
                "%s crashed, please report a bug.",
                PROGRAM_NAME.get().unwrap(),
            );
            if !is_main_thread() {
                eprintf!("\n");
                std::thread::sleep(Duration::from_secs(1));
            } else {
                eprintf!(" Debug PID %d or press Enter to exit\n", unsafe {
                    libc::getpid()
                });

                {
                    let mut act: libc::sigaction = unsafe { std::mem::zeroed() };
                    act.sa_sigaction = SIG_DFL;
                    unsafe { libc::sigemptyset(&mut act.sa_mask) };
                    act.sa_flags = 0;
                    unsafe { libc::sigaction(SIGINT, &act, std::ptr::null_mut()) };
                }

                if read_line("Type 'report' to send a bug report: ") == b"report" {
                    report_crash(panic_info);
                } else {
                    eprintf!("Not sending crash report\n");
                }
            }
            std::process::abort();
        }));
    }
    let exit_status = main();
    if let Some(at_exit) = AT_EXIT.get() {
        (at_exit)(false);
    }
    asan_maybe_exit(exit_status);
    std::process::exit(exit_status)
}

fn read_line(prompt: &str) -> Vec<u8> {
    eprintf!("%s", prompt);
    let mut line = vec![];
    let mut c = [0_u8; 1];
    while let Ok(n) = read_blocked(STDIN_FILENO, &mut c) {
        let c = c[0];
        if n == 0 || matches!(c, b'\n' | b'\r') {
            break;
        }
        line.push(c);
    }
    line
}

fn report_crash(panic_info: &PanicInfo) {
    let email = read_line("optional email address or username: ");
    let message = read_line("optional other info such as steps to reproduce: ");

    let _sentry = sentry::init(("https://cef1ac6352bd78fa3dfe5e3e51e3358c@o4508437566586880.ingest.us.sentry.io/4508437569142784", sentry::ClientOptions {
          release: sentry::release_name!(),
          default_integrations: false,
          integrations: vec![
                Arc::new(sentry::integrations::backtrace::AttachStacktraceIntegration),
                Arc::new(sentry::integrations::debug_images::DebugImagesIntegration::default()),
                Arc::new(sentry::integrations::contexts::ContextIntegration::default()),
                Arc::new(sentry::integrations::backtrace::ProcessStacktraceIntegration),
            ],
          ..Default::default()
        }));
    let mut event = PanicIntegration::new().event_from_panic_info(panic_info);
    let event_id = event.event_id;
    event.user = Some(sentry::User {
        email: Some(String::from_utf8_lossy(&email).to_string()),
        ..Default::default()
    });
    event.message = Some(String::from_utf8_lossy(&message).to_string());
    eprintf!("Sending crash report\n");
    sentry::Hub::with_active(|hub| {
        hub.capture_event(event);
        if let Some(client) = hub.client() {
            client.flush(None);
        }
    });
    eprintf!("Sent crash report with ID %s\n", event_id.to_string());
}
