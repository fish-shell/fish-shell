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

mod prelude {
    use crate::env::EnvStack;
    pub use crate::tests::env::{PwdEnvironment, TestEnvironment};
    use crate::wutil::wgetcwd;
    use once_cell::sync::Lazy;
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
}
