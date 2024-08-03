use crate::env::{EnvMode, Environment};
use crate::termsize::*;
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use std::sync::atomic::AtomicBool;
use std::sync::Mutex;

#[test]
#[serial]
fn test_termsize() {
    let _cleanup = test_init();
    let env_global = EnvMode::GLOBAL;
    let parser = TestParser::new();
    let vars = parser.vars();

    // Use a static variable so we can pretend we're the kernel exposing a terminal size.
    static STUBBY_TERMSIZE: Mutex<Option<Termsize>> = Mutex::new(None);
    fn stubby_termsize() -> Option<Termsize> {
        *STUBBY_TERMSIZE.lock().unwrap()
    }
    let ts = TermsizeContainer {
        data: Mutex::new(TermsizeData::defaults()),
        setting_env_vars: AtomicBool::new(false),
        tty_size_reader: stubby_termsize,
    };

    // Initially default value.
    assert_eq!(ts.last(), Termsize::defaults());

    // Haha we change the value, it doesn't even know.
    *STUBBY_TERMSIZE.lock().unwrap() = Some(Termsize {
        width: 42,
        height: 84,
    });
    assert_eq!(ts.last(), Termsize::defaults());

    // Ok let's tell it. But it still doesn't update right away.
    TermsizeContainer::handle_winch();
    assert_eq!(ts.last(), Termsize::defaults());

    // Ok now we tell it to update.
    ts.updating(&parser);
    assert_eq!(ts.last(), Termsize::new(42, 84));
    assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "42");
    assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "84");

    // Wow someone set COLUMNS and LINES to a weird value.
    // Now the tty's termsize doesn't matter.
    vars.set_one(L!("COLUMNS"), env_global, L!("75").to_owned());
    vars.set_one(L!("LINES"), env_global, L!("150").to_owned());
    ts.handle_columns_lines_var_change(parser.vars());
    assert_eq!(ts.last(), Termsize::new(75, 150));
    assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "75");
    assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "150");

    vars.set_one(L!("COLUMNS"), env_global, L!("33").to_owned());
    ts.handle_columns_lines_var_change(parser.vars());
    assert_eq!(ts.last(), Termsize::new(33, 150));

    // Oh it got SIGWINCH, now the tty matters again.
    TermsizeContainer::handle_winch();
    assert_eq!(ts.last(), Termsize::new(33, 150));
    assert_eq!(ts.updating(&parser), stubby_termsize().unwrap());
    assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "42");
    assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "84");

    // Test initialize().
    vars.set_one(L!("COLUMNS"), env_global, L!("83").to_owned());
    vars.set_one(L!("LINES"), env_global, L!("38").to_owned());
    ts.initialize(vars);
    assert_eq!(ts.last(), Termsize::new(83, 38));

    // initialize() even beats the tty reader until a sigwinch.
    let ts2 = TermsizeContainer {
        data: Mutex::new(TermsizeData::defaults()),
        setting_env_vars: AtomicBool::new(false),
        tty_size_reader: stubby_termsize,
    };
    ts.initialize(parser.vars());
    ts2.updating(&parser);
    assert_eq!(ts.last(), Termsize::new(83, 38));
    TermsizeContainer::handle_winch();
    assert_eq!(ts2.updating(&parser), stubby_termsize().unwrap());
}
