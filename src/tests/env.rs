use crate::env::{EnvMode, EnvStack, EnvVar, EnvVarFlags, Environment};
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wutil::wgetcwd;
use std::collections::HashMap;
use std::time::{SystemTime, UNIX_EPOCH};

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

/// Helper for test_timezone_env_vars().
fn return_timezone_hour(tstamp: SystemTime, timezone: &wstr) -> libc::c_int {
    let vars = EnvStack::globals().create_child(true /* dispatches_var_changes */);

    vars.set_one(L!("TZ"), EnvMode::EXPORT, timezone.to_owned());

    let _var = vars.get(L!("TZ"));

    let tstamp: libc::time_t = tstamp
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs()
        .try_into()
        .unwrap();
    let mut local_time: libc::tm = unsafe { std::mem::zeroed() };
    unsafe { libc::localtime_r(&tstamp, &mut local_time) };

    local_time.tm_hour
}

/// Verify that setting TZ calls tzset() in the current shell process.
fn test_timezone_env_vars() {
    // Confirm changing the timezone affects fish's idea of the local time.
    let tstamp = SystemTime::now();

    let first_tstamp = return_timezone_hour(tstamp, L!("UTC-1"));
    let second_tstamp = return_timezone_hour(tstamp, L!("UTC-2"));
    let delta = second_tstamp - first_tstamp;
    assert!(delta == 1 || delta == -23);
}

// Verify that setting special env vars have the expected effect on the current shell process.
#[test]
#[serial]
fn test_env_vars() {
    let _cleanup = test_init();
    test_timezone_env_vars();
    // TODO: Add tests for the locale and ncurses vars.

    let v1 = EnvVar::new(L!("abc").to_owned(), EnvVarFlags::EXPORT);
    let v2 = EnvVar::new_vec(vec![L!("abc").to_owned()], EnvVarFlags::EXPORT);
    let v3 = EnvVar::new_vec(vec![L!("abc").to_owned()], EnvVarFlags::empty());
    let v4 = EnvVar::new_vec(
        vec![L!("abc").to_owned(), L!("def").to_owned()],
        EnvVarFlags::EXPORT,
    );
    assert_eq!(v1, v2);
    assert_ne!(v1, v3);
    assert_ne!(v1, v4);
}

#[test]
#[serial]
fn test_env_snapshot() {
    let _cleanup = test_init();
    std::fs::create_dir_all("test/fish_env_snapshot_test/").unwrap();
    let parser = TestParser::new();
    let vars = parser.vars();
    parser.pushd("test/fish_env_snapshot_test/");
    vars.push(true);
    let before_pwd = vars.get(L!("PWD")).unwrap().as_string();
    vars.set_one(
        L!("test_env_snapshot_var"),
        EnvMode::default(),
        L!("before").to_owned(),
    );
    let snapshot = vars.snapshot();
    vars.set_one(L!("PWD"), EnvMode::default(), L!("/newdir").to_owned());
    vars.set_one(
        L!("test_env_snapshot_var"),
        EnvMode::default(),
        L!("after").to_owned(),
    );
    vars.set_one(
        L!("test_env_snapshot_var_2"),
        EnvMode::default(),
        L!("after").to_owned(),
    );

    // vars should be unaffected by the snapshot
    assert_eq!(vars.get(L!("PWD")).unwrap().as_string(), L!("/newdir"));
    assert_eq!(
        vars.get(L!("test_env_snapshot_var")).unwrap().as_string(),
        L!("after")
    );
    assert_eq!(
        vars.get(L!("test_env_snapshot_var_2")).unwrap().as_string(),
        L!("after")
    );

    // snapshot should have old values of vars
    assert_eq!(snapshot.get(L!("PWD")).unwrap().as_string(), before_pwd);
    assert_eq!(
        snapshot
            .get(L!("test_env_snapshot_var"))
            .unwrap()
            .as_string(),
        L!("before")
    );
    assert_eq!(snapshot.get(L!("test_env_snapshot_var_2")), None);

    // snapshots see global var changes except for perproc like PWD
    vars.set_one(
        L!("test_env_snapshot_var_3"),
        EnvMode::GLOBAL,
        L!("reallyglobal").to_owned(),
    );
    assert_eq!(
        vars.get(L!("test_env_snapshot_var_3")).unwrap().as_string(),
        L!("reallyglobal")
    );
    assert_eq!(
        snapshot
            .get(L!("test_env_snapshot_var_3"))
            .unwrap()
            .as_string(),
        L!("reallyglobal")
    );

    vars.pop();
    parser.popd();
}

// Can't push/pop from globals.
#[test]
#[should_panic]
fn test_no_global_push() {
    EnvStack::globals().push(true);
}

#[test]
#[should_panic]
fn test_no_global_pop() {
    EnvStack::globals().pop();
}
