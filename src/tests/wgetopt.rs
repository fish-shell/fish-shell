use crate::wchar::prelude::*;
use crate::wcstringutil::join_strings;
use crate::wgetopt::{wopt, ArgType, WGetopter, WOption};

#[test]
fn test_wgetopt() {
    // Regression test for a crash.
    const short_options: &wstr = L!("-a");
    const long_options: &[WOption] = &[wopt(L!("add"), ArgType::NoArgument, 'a')];
    let mut argv = [
        L!("abbr"),
        L!("--add"),
        L!("emacsnw"),
        L!("emacs"),
        L!("-nw"),
    ];
    let mut w = WGetopter::new(short_options, long_options, &mut argv);
    let mut a_count = 0;
    let mut arguments = vec![];
    while let Some(opt) = w.next_opt() {
        match opt {
            'a' => {
                a_count += 1;
            }
            '\x01' => {
                // non-option argument
                arguments.push(w.woptarg.as_ref().unwrap().to_owned());
            }
            '?' => {
                // unrecognized option
                if let Some(arg) = w.argv.get(w.wopt_index - 1) {
                    arguments.push(arg.to_owned());
                }
            }
            _ => {
                panic!("unexpected option: {:?}", opt);
            }
        }
    }
    assert_eq!(a_count, 1);
    assert_eq!(arguments.len(), 3);
    assert_eq!(join_strings(&arguments, ' '), "emacsnw emacs -nw");
}

#[test]
fn test_wgetopt_proper_longopts() {
    // Test for issue #7341 (disable abbreviated long options, e.g. --forc shouldn't work as an alias to --force).
    const short_options: &wstr = L!("-f");
    const long_options: &[WOption] = &[wopt(L!("force"), ArgType::NoArgument, 'f')];
    let mut argv = [L!("rm"), L!("--forc"), L!("filename1"), L!("filename2")];
    let mut w = WGetopter::new(short_options, long_options, &mut argv);
    let mut f_count = 0;
    let mut unknown_count = 0;
    let mut arguments = vec![];
    while let Some(opt) = w.next_opt() {
        match opt {
            'f' => {
                f_count += 1;
            }
            '\x01' => {
                // non-option argument
                arguments.push(w.woptarg.as_ref().unwrap().to_owned());
            }
            '?' => {
                // unrecognized option
                unknown_count += 1;
                if let Some(arg) = w.argv.get(w.wopt_index - 1) {
                    arguments.push(arg.to_owned());
                }
            }
            _ => {
                panic!("unexpected option: {:?}", opt);
            }
        }
    }
    assert_eq!(f_count, 0);
    assert_eq!(unknown_count, 1);
    assert_eq!(arguments.len(), 3);
    assert_eq!(join_strings(&arguments, ' '), "--forc filename1 filename2");
}
