use crate::wchar::prelude::*;

mod abbrs;
#[cfg(test)]
mod common;
mod complete;
mod debounce;
#[cfg(test)]
mod editable_line;
#[cfg(test)]
mod encoding;
mod env;
mod env_universal_common;
mod expand;
mod fd_monitor;
mod highlight;
mod history;
mod pager;
mod parse_util;
mod parser;
#[cfg(test)]
mod reader;
#[cfg(test)]
mod redirection;
mod screen;
mod string_escape;
#[cfg(test)]
mod threads;
#[cfg(test)]
mod tokenizer;
mod topic_monitor;
mod wgetopt;

pub mod prelude {
    use crate::env::{env_init, misc_init};
    use crate::reader::reader_init;
    use crate::signal::signal_reset_handlers;
    pub use crate::tests::env::{PwdEnvironment, TestEnvironment};
    use crate::topic_monitor::topic_monitor_init;
    use crate::wutil::wgetcwd;
    use crate::{env::EnvStack, proc::proc_init};
    use once_cell::sync::Lazy;
    use once_cell::sync::OnceCell;
    use std::ffi::CString;
    use std::sync::Mutex;

    static PUSHED_DIRS: Lazy<Mutex<Vec<String>>> = Lazy::new(Mutex::default);
    /// Helper to chdir and then update $PWD.
    pub fn pushd(path: &str) {
        let cwd = wgetcwd();
        PUSHED_DIRS.lock().unwrap().push(cwd.to_string());

        // We might need to create the directory. We don't care if this fails due to the directory
        // already being present.
        std::fs::create_dir_all(path).unwrap();

        std::env::set_current_dir(path).unwrap();
        EnvStack::principal().set_pwd_from_getcwd();
    }

    pub fn popd() {
        let old_cwd = PUSHED_DIRS.lock().unwrap().pop().unwrap();
        std::env::set_current_dir(old_cwd).unwrap();
        EnvStack::principal().set_pwd_from_getcwd();
    }

    pub fn test_init() {
        static DONE: OnceCell<()> = OnceCell::new();
        DONE.get_or_init(|| {
            {
                let s = CString::new("").unwrap();
                unsafe {
                    libc::setlocale(libc::LC_ALL, s.as_ptr());
                }
            }
            topic_monitor_init();
            crate::threads::init();
            proc_init();
            env_init(None, true, false);
            misc_init();
            reader_init();

            // Set default signal handlers, so we can ctrl-C out of this.
            signal_reset_handlers();

            // Set PWD from getcwd - fixes #5599
            EnvStack::principal().set_pwd_from_getcwd();
        });
    }
}
