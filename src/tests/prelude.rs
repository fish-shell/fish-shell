use crate::common::{BUILD_DIR, ScopeGuard, ScopeGuarding};
use crate::env::env_init;
use crate::env::{EnvMode, EnvVar, EnvVarFlags, Environment};
use crate::locale::set_libc_locales;
use crate::parser::{CancelBehavior, Parser};
use crate::reader::{reader_deinit, reader_init};
use crate::signal::signal_reset_handlers;
use crate::topic_monitor::topic_monitor_init;
use crate::wchar::prelude::*;
use crate::wutil::wgetcwd;
use crate::{env::EnvStack, proc::proc_init};
use once_cell::sync::OnceCell;
use std::cell::RefCell;
use std::collections::HashMap;
use std::env::set_current_dir;
use std::path::PathBuf;

pub use serial_test::serial;

pub fn test_init() -> impl ScopeGuarding<Target = ()> {
    static DONE: OnceCell<()> = OnceCell::new();
    DONE.get_or_init(|| {
        // If we are building with `cargo build` and have build w/ `cmake`, this might not
        // yet exist.
        let mut test_dir = PathBuf::from(BUILD_DIR);
        test_dir.push("fish-test");
        std::fs::create_dir_all(&test_dir).unwrap();
        set_current_dir(&test_dir).unwrap();
        // Safety: all tests that access locale should go through the enclosing function.
        unsafe {
            set_libc_locales(/*log_ok=*/ false)
        };
        topic_monitor_init();
        crate::threads::init();
        proc_init();
        env_init(None, true, false);

        // Set default signal handlers, so we can ctrl-C out of this.
        signal_reset_handlers();

        // Set PWD from getcwd - fixes #5599
        EnvStack::globals().set_pwd_from_getcwd();
    });
    reader_init(false);
    ScopeGuard::new((), |()| {
        reader_deinit(false);
    })
}

/// An environment built around an std::map.
#[derive(Clone, Default)]
pub struct TestEnvironment {
    pub vars: HashMap<WString, WString>,
}
impl TestEnvironment {
    #[allow(dead_code)]
    pub fn new() -> Self {
        Self::default()
    }
}
impl Environment for TestEnvironment {
    fn getf(&self, name: &wstr, _mode: EnvMode) -> Option<EnvVar> {
        self.vars
            .get(name)
            .map(|value| EnvVar::new(value.clone(), EnvVarFlags::default()))
    }
    fn get_names(&self, _flags: EnvMode) -> Vec<WString> {
        self.vars.keys().cloned().collect()
    }
}

/// A test environment that knows about PWD.
#[derive(Default)]
pub struct PwdEnvironment {
    pub parent: TestEnvironment,
}
impl PwdEnvironment {
    #[allow(dead_code)]
    pub fn new() -> Self {
        Self::default()
    }
}
impl Environment for PwdEnvironment {
    fn getf(&self, name: &wstr, mode: EnvMode) -> Option<EnvVar> {
        if name == "PWD" {
            return Some(EnvVar::new(wgetcwd(), EnvVarFlags::default()));
        }
        self.parent.getf(name, mode)
    }

    fn get_names(&self, flags: EnvMode) -> Vec<WString> {
        let mut res = self.parent.get_names(flags);
        if !res.iter().any(|n| n == "PWD") {
            res.push(L!("PWD").to_owned());
        }
        res
    }
}

/// A wrapper around a Parser with some test helpers.
pub struct TestParser {
    parser: Parser,
    pushed_dirs: RefCell<Vec<String>>,
}

impl TestParser {
    pub fn new() -> TestParser {
        TestParser {
            parser: Parser::new(EnvStack::new(), CancelBehavior::default()),
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
