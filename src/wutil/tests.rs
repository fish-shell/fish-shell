use crate::tests::prelude::*;
use crate::util::get_rng;
use crate::{fds::AutoCloseFd, fs::create_temporary_file};
use libc::{c_void, O_CREAT, O_RDWR, O_TRUNC, SEEK_SET};
use rand::Rng;
use std::ffi::CString;
use std::ptr;

use super::*;

#[test]
fn test_wdirname_wbasename() {
    // path, dir, base
    struct Test(&'static wstr, &'static wstr, &'static wstr);
    const testcases: &[Test] = &[
        Test(L!(""), L!("."), L!(".")),
        Test(L!("foo//"), L!("."), L!("foo")),
        Test(L!("foo//////"), L!("."), L!("foo")),
        Test(L!("/////foo"), L!("/"), L!("foo")),
        Test(L!("//foo/////bar"), L!("//foo"), L!("bar")),
        Test(L!("foo/////bar"), L!("foo"), L!("bar")),
        // Examples given in XPG4.2.
        Test(L!("/usr/lib"), L!("/usr"), L!("lib")),
        Test(L!("usr"), L!("."), L!("usr")),
        Test(L!("/"), L!("/"), L!("/")),
        Test(L!("."), L!("."), L!(".")),
        Test(L!(".."), L!("."), L!("..")),
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
    assert_eq!(wbasename(&longpath), L!("overlong"));
}

#[test]
#[serial]
fn test_wwrite_to_fd() {
    let _cleanup = test_init();
    let (_file, filename) = create_temporary_file(L!("/tmp/fish_test_wwrite.XXXXXX")).unwrap();
    let filename = CString::new(filename.to_string()).unwrap();
    let mut rng = get_rng();
    let sizes = [1, 2, 3, 5, 13, 23, 64, 128, 255, 4096, 4096 * 2];
    for &size in &sizes {
        let fd = AutoCloseFd::new(unsafe {
            libc::open(filename.as_ptr(), O_RDWR | O_TRUNC | O_CREAT, 0o666)
        });
        assert!(fd.is_valid());
        let mut input = WString::new();
        for _i in 0..size {
            input.push(rng.gen());
        }

        let amt = wwrite_to_fd(&input, fd.fd()).unwrap();
        let narrow = wcs2string(&input);
        assert_eq!(amt, narrow.len());

        assert!(unsafe { libc::lseek(fd.fd(), 0, SEEK_SET) } >= 0);

        let mut contents = vec![0u8; narrow.len()];
        let read_amt = unsafe {
            libc::read(
                fd.fd(),
                if size == 0 {
                    ptr::null_mut()
                } else {
                    (&mut contents[0]) as *mut u8 as *mut c_void
                },
                narrow.len(),
            )
        };
        assert!(usize::try_from(read_amt).unwrap() == narrow.len());
        assert_eq!(&contents, &narrow);
    }
    unsafe { libc::remove(filename.as_ptr()) };
}
