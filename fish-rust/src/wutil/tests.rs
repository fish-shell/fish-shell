use super::*;

#[test]
#[widestrs]
fn test_wdirname_wbasename() {
    // path, dir, base
    struct Test(&'static wstr, &'static wstr, &'static wstr);
    const testcases: &[Test] = &[
        Test(""L, "."L, "."L),
        Test("foo//"L, "."L, "foo"L),
        Test("foo//////"L, "."L, "foo"L),
        Test("/////foo"L, "/"L, "foo"L),
        Test("//foo/////bar"L, "//foo"L, "bar"L),
        Test("foo/////bar"L, "foo"L, "bar"L),
        // Examples given in XPG4.2.
        Test("/usr/lib"L, "/usr"L, "lib"L),
        Test("usr"L, "."L, "usr"L),
        Test("/"L, "/"L, "/"L),
        Test("."L, "."L, "."L),
        Test(".."L, "."L, ".."L),
    ];

    for tc in testcases {
        let Test(path, tc_dir, tc_base) = *tc;
        let dir = wdirname(path);
        assert_eq!(
            dir, tc_dir,
            "\npath: {:?}, dir: {:?}, tc.dir: {:?}",
            path, dir, tc_dir
        );

        let base = wbasename(path);
        assert_eq!(
            base, tc_base,
            "\npath: {:?}, base: {:?}, tc.base: {:?}",
            path, base, tc_base
        );
    }

    // Ensure strings which greatly exceed PATH_MAX still work (#7837).
    const PATH_MAX: usize = libc::PATH_MAX as usize;
    let mut longpath = WString::new();
    longpath.reserve(PATH_MAX * 2 + 10);
    while longpath.char_count() <= PATH_MAX * 2 {
        longpath.push_str("/overlong");
    }
    let last_slash = longpath.chars().rposition(|c| c == '/').unwrap();
    let longpath_dir = &longpath[..last_slash];
    assert_eq!(wdirname(&longpath), longpath_dir);
    assert_eq!(wbasename(&longpath), "overlong"L);
}
