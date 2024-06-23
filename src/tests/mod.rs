mod abbrs;
mod common;
mod complete;
mod debounce;
mod editable_line;
mod encoding;
mod env;
mod env_universal_common;
mod expand;
mod fd_monitor;
mod highlight;
mod history;
mod input;
mod input_common;
mod key;
mod pager;
mod parse_util;
mod parser;
mod reader;
mod redirection;
mod screen;
mod std;
mod string_escape;
mod termsize;
mod threads;
mod tokenizer;
mod topic_monitor;
mod wgetopt;

pub mod prelude {
    use crate::common::ScopeGuarding;
    use crate::env::{env_init, misc_init};
    use crate::parser::{CancelBehavior, Parser};
    use crate::reader::reader_init;
    use crate::signal::signal_reset_handlers;
    pub use crate::tests::env::{PwdEnvironment, TestEnvironment};
    use crate::topic_monitor::topic_monitor_init;
    use crate::wutil::wgetcwd;
    use crate::{env::EnvStack, proc::proc_init};
    use once_cell::sync::OnceCell;
    use std::cell::RefCell;
    use std::env::set_current_dir;
    use std::ffi::CString;
    use std::rc::Rc;

    /// A wrapper around a Parser with some test helpers.
    pub struct TestParser {
        parser: Parser,
        pushed_dirs: RefCell<Vec<String>>,
    }

    impl TestParser {
        pub fn new() -> TestParser {
            TestParser {
                parser: Parser::new(Rc::new(EnvStack::new()), CancelBehavior::default()),
                pushed_dirs: RefCell::new(Vec::new()),
            }
        }

        /// Helper to chdir and then update $PWD.
        pub fn pushd(&self, path: &str) {
            let cwd = wgetcwd();
            self.pushed_dirs.borrow_mut().push(cwd.to_string());

            // We might need to create the directory. We don't care if this fails due to the directory
            // already being present.
            std::fs::create_dir_all(path).unwrap();

            std::env::set_current_dir(path).unwrap();
            self.parser.vars().set_pwd_from_getcwd();
        }

        pub fn popd(&self) {
            let old_cwd = self.pushed_dirs.borrow_mut().pop().unwrap();
            std::env::set_current_dir(old_cwd).unwrap();
            self.parser.vars().set_pwd_from_getcwd();
        }
    }

    impl std::ops::Deref for TestParser {
        type Target = Parser;
        fn deref(&self) -> &Self::Target {
            &self.parser
        }
    }

    pub fn test_init() -> impl ScopeGuarding<Target = ()> {
        static DONE: OnceCell<()> = OnceCell::new();
        DONE.get_or_init(|| {
            // If we are building with `cargo build` and have build w/ `cmake`, FISH_BUILD_DIR might
            // not yet exist.
            std::fs::create_dir_all(env!("FISH_BUILD_DIR")).unwrap();
            set_current_dir(env!("FISH_BUILD_DIR")).unwrap();
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

            // Set default signal handlers, so we can ctrl-C out of this.
            signal_reset_handlers();

            // Set PWD from getcwd - fixes #5599
            EnvStack::globals().set_pwd_from_getcwd();
        });
        reader_init()
    }

    pub use serial_test::serial;
}
