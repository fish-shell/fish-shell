use super::*;
use libc::PATH_MAX;

macro_rules! test_cases_wdirname_wbasename {
    ($($name:ident: $test:expr),*  $(,)?) => {
        $(
            #[test]
            fn $name() {
                let (path, dir, base) = $test;
                let actual = wdirname(WString::from(path));
                assert_eq!(actual, WString::from(dir), "Wrong dirname for {:?}", path);
                let actual = wbasename(WString::from(path));
                assert_eq!(actual, WString::from(base), "Wrong basename for {:?}", path);
            }
        )*
    };
}

/// Helper to return a string whose length greatly exceeds PATH_MAX.
fn overlong_path() -> WString {
    let mut longpath = WString::with_capacity((PATH_MAX * 2 + 10) as usize);
    while longpath.len() < (PATH_MAX * 2) as usize {
        longpath.push_str("/overlong");
    }
    return longpath;
}

test_cases_wdirname_wbasename! {
    wdirname_wbasename_test_1: ("", ".", "."),
    wdirname_wbasename_test_2: ("foo//", ".", "foo"),
    wdirname_wbasename_test_3: ("foo//////", ".", "foo"),
    wdirname_wbasename_test_4: ("/////foo", "/", "foo"),
    wdirname_wbasename_test_5: ("/////foo", "/", "foo"),
    wdirname_wbasename_test_6: ("//foo/////bar", "//foo", "bar"),
    wdirname_wbasename_test_7: ("foo/////bar", "foo", "bar"),
    // Examples given in XPG4.2.
    wdirname_wbasename_test_8: ("/usr/lib", "/usr", "lib"),
    wdirname_wbasename_test_9: ("usr", ".", "usr"),
    wdirname_wbasename_test_10: ("/", "/", "/"),
    wdirname_wbasename_test_11: (".", ".", "."),
    wdirname_wbasename_test_12: ("..", ".", ".."),
}

// Ensures strings which greatly exceed PATH_MAX still work (#7837).
#[test]
fn test_overlong_wdirname_wbasename() {
    let path = overlong_path();
    let dir = {
        let mut longpath_dir = path.clone();
        let last_slash = longpath_dir.chars().rev().position(|c| c == '/').unwrap();
        longpath_dir.truncate(longpath_dir.len() - last_slash - 1);
        longpath_dir
    };
    let base = "overlong";

    let actual = wdirname(&path);
    assert_eq!(actual, dir, "Wrong dirname for {:?}", path);
    let actual = wbasename(&path);
    assert_eq!(actual, base, "Wrong basename for {:?}", path);
}
