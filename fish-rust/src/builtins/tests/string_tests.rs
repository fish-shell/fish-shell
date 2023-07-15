use super::super::prelude::*;
use crate::common::escape;
use crate::ffi_tests::add_test;
use crate::io::{NullOutputStream, OutputStream, StringOutputStream};

add_test! {"test_string", || {
    use crate::parser::Parser;
    use crate::ffi;
    use crate::builtins::string::string;
    use crate::wchar_ffi::WCharFromFFI;
    use crate::common::{EscapeStringStyle, escape_string};
    use crate::wchar::wstr;
    use crate::wchar::L;
    use crate::builtins::shared::{STATUS_CMD_ERROR,STATUS_CMD_OK, STATUS_INVALID_ARGS};

    use crate::future_feature_flags::{scoped_test, FeatureFlag};

    // avoid 1.3k L!()'s
    macro_rules! test_cases {
        ([$($x:expr),*], $rc:expr, $out:expr) => { (vec![$(L!($x)),*], $rc, L!($out)) };
        [$($x:tt),* $(,)?] => { [$(test_cases!$x),*] };
    }

    // TODO: these should be individual tests, not all in one, port when we can run these with `cargo test`
    fn string_test(args: Vec<&wstr>, expected_rc: Option<i32>, expected_out: &wstr) {
        let mut args: Vec<WString> = args.into_iter().map(|s|s.to_owned()).collect();
        let parser: &Parser = Parser::principal_parser();
        let mut outs = StringOutputStream::new();
        let mut errs = NullOutputStream::new();
        let mut streams = IoStreams::new(&mut outs, &mut errs);
        streams.stdin_is_directly_redirected = false; // read from argv instead of stdin

        let rc = string(parser, &mut streams, args.as_mut_slice()).expect("string failed");

        assert_eq!(expected_rc.unwrap(), rc, "string builtin returned unexpected return code");

        let actual = escape(outs.contents());
        let expected = escape(expected_out);
        assert_eq!(expected, actual, "string builtin returned unexpected output");
    }

    let tests = test_cases![
        (["string", "escape"], STATUS_CMD_ERROR, ""),
        (["string", "escape", ""], STATUS_CMD_OK, "''\n"),
        (["string", "escape", "-n", ""], STATUS_CMD_OK, "\n"),
        (["string", "escape", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "escape", "\x07"], STATUS_CMD_OK, "\\cg\n"),
        (["string", "escape", "\"x\""], STATUS_CMD_OK, "'\"x\"'\n"),
        (["string", "escape", "hello world"], STATUS_CMD_OK, "'hello world'\n"),
        (["string", "escape", "-n", "hello world"], STATUS_CMD_OK, "hello\\ world\n"),
        (["string", "escape", "hello", "world"], STATUS_CMD_OK, "hello\nworld\n"),
        (["string", "escape", "-n", "~"], STATUS_CMD_OK, "\\~\n"),

        (["string", "join"], STATUS_INVALID_ARGS, ""),
        (["string", "join", ""], STATUS_CMD_ERROR, ""),
        (["string", "join", "", "", "", ""], STATUS_CMD_OK, "\n"),
        (["string", "join", "", "a", "b", "c"], STATUS_CMD_OK, "abc\n"),
        (["string", "join", ".", "fishshell", "com"], STATUS_CMD_OK, "fishshell.com\n"),
        (["string", "join", "/", "usr"], STATUS_CMD_ERROR, "usr\n"),
        (["string", "join", "/", "usr", "local", "bin"], STATUS_CMD_OK, "usr/local/bin\n"),
        (["string", "join", "...", "3", "2", "1"], STATUS_CMD_OK, "3...2...1\n"),
        (["string", "join", "-q"], STATUS_INVALID_ARGS, ""),
        (["string", "join", "-q", "."], STATUS_CMD_ERROR, ""),
        (["string", "join", "-q", ".", "."], STATUS_CMD_ERROR, ""),

        (["string", "length"], STATUS_CMD_ERROR, ""),
        (["string", "length", ""], STATUS_CMD_ERROR, "0\n"),
        (["string", "length", "", "", ""], STATUS_CMD_ERROR, "0\n0\n0\n"),
        (["string", "length", "a"], STATUS_CMD_OK, "1\n"),

        (["string", "length", "\u{2008A}"], STATUS_CMD_OK, "1\n"),
        (["string", "length", "um", "dois", "três"], STATUS_CMD_OK, "2\n4\n4\n"),
        (["string", "length", "um", "dois", "três"], STATUS_CMD_OK, "2\n4\n4\n"),
        (["string", "length", "-q"], STATUS_CMD_ERROR, ""),
        (["string", "length", "-q", ""], STATUS_CMD_ERROR, ""),
        (["string", "length", "-q", "a"], STATUS_CMD_OK, ""),

        (["string", "match"], STATUS_INVALID_ARGS, ""),
        (["string", "match", ""], STATUS_CMD_ERROR, ""),
        (["string", "match", "", ""], STATUS_CMD_OK, "\n"),
        (["string", "match", "?", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "match", "*", ""], STATUS_CMD_OK, "\n"),
        (["string", "match", "**", ""], STATUS_CMD_OK, "\n"),
        (["string", "match", "*", "xyzzy"], STATUS_CMD_OK, "xyzzy\n"),
        (["string", "match", "**", "plugh"], STATUS_CMD_OK, "plugh\n"),
        (["string", "match", "a*b", "axxb"], STATUS_CMD_OK, "axxb\n"),
        (["string", "match", "a??b", "axxb"], STATUS_CMD_OK, "axxb\n"),
        (["string", "match", "-i", "a??B", "axxb"], STATUS_CMD_OK, "axxb\n"),
        (["string", "match", "-i", "a??b", "Axxb"], STATUS_CMD_OK, "Axxb\n"),
        (["string", "match", "a*", "axxb"], STATUS_CMD_OK, "axxb\n"),
        (["string", "match", "*a", "xxa"], STATUS_CMD_OK, "xxa\n"),
        (["string", "match", "*a*", "axa"], STATUS_CMD_OK, "axa\n"),
        (["string", "match", "*a*", "xax"], STATUS_CMD_OK, "xax\n"),
        (["string", "match", "*a*", "bxa"], STATUS_CMD_OK, "bxa\n"),
        (["string", "match", "*a", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "match", "a*", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "match", "a*b*c", "axxbyyc"], STATUS_CMD_OK, "axxbyyc\n"),
        (["string", "match", "\\*", "*"], STATUS_CMD_OK, "*\n"),
        (["string", "match", "a*\\", "abc\\"], STATUS_CMD_OK, "abc\\\n"),
        (["string", "match", "a*\\?", "abc?"], STATUS_CMD_OK, "abc?\n"),

        (["string", "match", "?", ""], STATUS_CMD_ERROR, ""),
        (["string", "match", "?", "ab"], STATUS_CMD_ERROR, ""),
        (["string", "match", "??", "a"], STATUS_CMD_ERROR, ""),
        (["string", "match", "?a", "a"], STATUS_CMD_ERROR, ""),
        (["string", "match", "a?", "a"], STATUS_CMD_ERROR, ""),
        (["string", "match", "a??B", "axxb"], STATUS_CMD_ERROR, ""),
        (["string", "match", "a*b", "axxbc"], STATUS_CMD_ERROR, ""),
        (["string", "match", "*b", "bbba"], STATUS_CMD_ERROR, ""),
        (["string", "match", "0x[0-9a-fA-F][0-9a-fA-F]", "0xbad"], STATUS_CMD_ERROR, ""),

        (["string", "match", "-a", "*", "ab", "cde"], STATUS_CMD_OK, "ab\ncde\n"),
        (["string", "match", "*", "ab", "cde"], STATUS_CMD_OK, "ab\ncde\n"),
        (["string", "match", "-n", "*d*", "cde"], STATUS_CMD_OK, "1 3\n"),
        (["string", "match", "-n", "*x*", "cde"], STATUS_CMD_ERROR, ""),
        (["string", "match", "-q", "a*", "b", "c"], STATUS_CMD_ERROR, ""),
        (["string", "match", "-q", "a*", "b", "a"], STATUS_CMD_OK, ""),

        (["string", "match", "-r"], STATUS_INVALID_ARGS, ""),
        (["string", "match", "-r", ""], STATUS_CMD_ERROR, ""),
        (["string", "match", "-r", "", ""], STATUS_CMD_OK, "\n"),
        (["string", "match", "-r", ".", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "match", "-r", ".*", ""], STATUS_CMD_OK, "\n"),
        (["string", "match", "-r", "a*b", "b"], STATUS_CMD_OK, "b\n"),
        (["string", "match", "-r", "a*b", "aab"], STATUS_CMD_OK, "aab\n"),
        (["string", "match", "-r", "-i", "a*b", "Aab"], STATUS_CMD_OK, "Aab\n"),
        (["string", "match", "-r", "-a", "a[bc]", "abadac"], STATUS_CMD_OK, "ab\nac\n"),
        (["string", "match", "-r", "a", "xaxa", "axax"], STATUS_CMD_OK, "a\na\n"),
        (["string", "match", "-r", "-a", "a", "xaxa", "axax"], STATUS_CMD_OK, "a\na\na\na\n"),
        (["string", "match", "-r", "a[bc]", "abadac"], STATUS_CMD_OK, "ab\n"),
        (["string", "match", "-r", "-q", "a[bc]", "abadac"], STATUS_CMD_OK, ""),
        (["string", "match", "-r", "-q", "a[bc]", "ad"], STATUS_CMD_ERROR, ""),
        (["string", "match", "-r", "(a+)b(c)", "aabc"], STATUS_CMD_OK, "aabc\naa\nc\n"),
        (["string", "match", "-r", "-a", "(a)b(c)", "abcabc"], STATUS_CMD_OK, "abc\na\nc\nabc\na\nc\n"),
        (["string", "match", "-r", "(a)b(c)", "abcabc"], STATUS_CMD_OK, "abc\na\nc\n"),
        (["string", "match", "-r", "(a|(z))(bc)", "abc"], STATUS_CMD_OK, "abc\na\nbc\n"),
        (["string", "match", "-r", "-n", "a", "ada", "dad"], STATUS_CMD_OK, "1 1\n2 1\n"),
        (["string", "match", "-r", "-n", "-a", "a", "bacadae"], STATUS_CMD_OK, "2 1\n4 1\n6 1\n"),
        (["string", "match", "-r", "-n", "(a).*(b)", "a---b"], STATUS_CMD_OK, "1 5\n1 1\n5 1\n"),
        (["string", "match", "-r", "-n", "(a)(b)", "ab"], STATUS_CMD_OK, "1 2\n1 1\n2 1\n"),
        (["string", "match", "-r", "-n", "(a)(b)", "abab"], STATUS_CMD_OK, "1 2\n1 1\n2 1\n"),
        (["string", "match", "-r", "-n", "-a", "(a)(b)", "abab"], STATUS_CMD_OK, "1 2\n1 1\n2 1\n3 2\n3 1\n4 1\n"),
        (["string", "match", "-r", "*", ""], STATUS_INVALID_ARGS, ""),
        (["string", "match", "-r", "-a", "a*", "b"], STATUS_CMD_OK, "\n\n"),
        (["string", "match", "-r", "foo\\Kbar", "foobar"], STATUS_CMD_OK, "bar\n"),
        (["string", "match", "-r", "(foo)\\Kbar", "foobar"], STATUS_CMD_OK, "bar\nfoo\n"),
        (["string", "replace"], STATUS_INVALID_ARGS, ""),
        (["string", "replace", ""], STATUS_INVALID_ARGS, ""),
        (["string", "replace", "", ""], STATUS_CMD_ERROR, ""),
        (["string", "replace", "", "", ""], STATUS_CMD_ERROR, "\n"),
        (["string", "replace", "", "", " "], STATUS_CMD_ERROR, " \n"),
        (["string", "replace", "a", "b", ""], STATUS_CMD_ERROR, "\n"),
        (["string", "replace", "a", "b", "a"], STATUS_CMD_OK, "b\n"),
        (["string", "replace", "a", "b", "xax"], STATUS_CMD_OK, "xbx\n"),
        (["string", "replace", "a", "b", "xax", "axa"], STATUS_CMD_OK, "xbx\nbxa\n"),
        (["string", "replace", "bar", "x", "red barn"], STATUS_CMD_OK, "red xn\n"),
        (["string", "replace", "x", "bar", "red xn"], STATUS_CMD_OK, "red barn\n"),
        (["string", "replace", "--", "x", "-", "xyz"], STATUS_CMD_OK, "-yz\n"),
        (["string", "replace", "--", "y", "-", "xyz"], STATUS_CMD_OK, "x-z\n"),
        (["string", "replace", "--", "z", "-", "xyz"], STATUS_CMD_OK, "xy-\n"),
        (["string", "replace", "-i", "z", "X", "_Z_"], STATUS_CMD_OK, "_X_\n"),
        (["string", "replace", "-a", "a", "A", "aaa"], STATUS_CMD_OK, "AAA\n"),
        (["string", "replace", "-i", "a", "z", "AAA"], STATUS_CMD_OK, "zAA\n"),
        (["string", "replace", "-q", "x", ">x<", "x"], STATUS_CMD_OK, ""),
        (["string", "replace", "-a", "x", "", "xxx"], STATUS_CMD_OK, "\n"),
        (["string", "replace", "-a", "***", "_", "*****"], STATUS_CMD_OK, "_**\n"),
        (["string", "replace", "-a", "***", "***", "******"], STATUS_CMD_OK, "******\n"),
        (["string", "replace", "-a", "a", "b", "xax", "axa"], STATUS_CMD_OK, "xbx\nbxb\n"),

        (["string", "replace", "-r"], STATUS_INVALID_ARGS, ""),
        (["string", "replace", "-r", ""], STATUS_INVALID_ARGS, ""),
        (["string", "replace", "-r", "", ""], STATUS_CMD_ERROR, ""),
        (["string", "replace", "-r", "", "", ""], STATUS_CMD_OK, "\n"),  // pcre2 behavior
        (["string", "replace", "-r", "", "", " "], STATUS_CMD_OK, " \n"),  // pcre2 behavior
        (["string", "replace", "-r", "a", "b", ""], STATUS_CMD_ERROR, "\n"),
        (["string", "replace", "-r", "a", "b", "a"], STATUS_CMD_OK, "b\n"),
        (["string", "replace", "-r", ".", "x", "abc"], STATUS_CMD_OK, "xbc\n"),
        (["string", "replace", "-r", ".", "", "abc"], STATUS_CMD_OK, "bc\n"),
        (["string", "replace", "-r", "(\\w)(\\w)", "$2$1", "ab"], STATUS_CMD_OK, "ba\n"),
        (["string", "replace", "-r", "(\\w)", "$1$1", "ab"], STATUS_CMD_OK, "aab\n"),
        (["string", "replace", "-r", "-a", ".", "x", "abc"], STATUS_CMD_OK, "xxx\n"),
        (["string", "replace", "-r", "-a", "(\\w)", "$1$1", "ab"], STATUS_CMD_OK, "aabb\n"),
        (["string", "replace", "-r", "-a", ".", "", "abc"], STATUS_CMD_OK, "\n"),
        (["string", "replace", "-r", "a", "x", "bc", "cd", "de"], STATUS_CMD_ERROR, "bc\ncd\nde\n"),
        (["string", "replace", "-r", "a", "x", "aba", "caa"], STATUS_CMD_OK, "xba\ncxa\n"),
        (["string", "replace", "-r", "-a", "a", "x", "aba", "caa"], STATUS_CMD_OK, "xbx\ncxx\n"),
        (["string", "replace", "-r", "-i", "A", "b", "xax"], STATUS_CMD_OK, "xbx\n"),
        (["string", "replace", "-r", "-i", "[a-z]", ".", "1A2B"], STATUS_CMD_OK, "1.2B\n"),
        (["string", "replace", "-r", "A", "b", "xax"], STATUS_CMD_ERROR, "xax\n"),
        (["string", "replace", "-r", "a", "$1", "a"], STATUS_INVALID_ARGS, ""),
        (["string", "replace", "-r", "(a)", "$2", "a"], STATUS_INVALID_ARGS, ""),
        (["string", "replace", "-r", "*", ".", "a"], STATUS_INVALID_ARGS, ""),
        (["string", "replace", "-ra", "x", "\\c"], STATUS_CMD_ERROR, ""),
        (["string", "replace", "-r", "^(.)", "\t$1", "abc", "x"], STATUS_CMD_OK, "\tabc\n\tx\n"),

        (["string", "split"], STATUS_INVALID_ARGS, ""),
        (["string", "split", ":"], STATUS_CMD_ERROR, ""),
        (["string", "split", ".", "www.ch.ic.ac.uk"], STATUS_CMD_OK, "www\nch\nic\nac\nuk\n"),
        (["string", "split", "..", "...."], STATUS_CMD_OK, "\n\n\n"),
        (["string", "split", "-m", "x", "..", "...."], STATUS_INVALID_ARGS, ""),
        (["string", "split", "-m1", "..", "...."], STATUS_CMD_OK, "\n..\n"),
        (["string", "split", "-m0", "/", "/usr/local/bin/fish"], STATUS_CMD_ERROR, "/usr/local/bin/fish\n"),
        (["string", "split", "-m2", ":", "a:b:c:d", "e:f:g:h"], STATUS_CMD_OK, "a\nb\nc:d\ne\nf\ng:h\n"),
        (["string", "split", "-m1", "-r", "/", "/usr/local/bin/fish"], STATUS_CMD_OK, "/usr/local/bin\nfish\n"),
        (["string", "split", "-r", ".", "www.ch.ic.ac.uk"], STATUS_CMD_OK, "www\nch\nic\nac\nuk\n"),
        (["string", "split", "--", "--", "a--b---c----d"], STATUS_CMD_OK, "a\nb\n-c\n\nd\n"),
        (["string", "split", "-r", "..", "...."], STATUS_CMD_OK, "\n\n\n"),
        (["string", "split", "-r", "--", "--", "a--b---c----d"], STATUS_CMD_OK, "a\nb-\nc\n\nd\n"),
        (["string", "split", "", ""], STATUS_CMD_ERROR, "\n"),
        (["string", "split", "", "a"], STATUS_CMD_ERROR, "a\n"),
        (["string", "split", "", "ab"], STATUS_CMD_OK, "a\nb\n"),
        (["string", "split", "", "abc"], STATUS_CMD_OK, "a\nb\nc\n"),
        (["string", "split", "-m1", "", "abc"], STATUS_CMD_OK, "a\nbc\n"),
        (["string", "split", "-r", "", ""], STATUS_CMD_ERROR, "\n"),
        (["string", "split", "-r", "", "a"], STATUS_CMD_ERROR, "a\n"),
        (["string", "split", "-r", "", "ab"], STATUS_CMD_OK, "a\nb\n"),
        (["string", "split", "-r", "", "abc"], STATUS_CMD_OK, "a\nb\nc\n"),
        (["string", "split", "-r", "-m1", "", "abc"], STATUS_CMD_OK, "ab\nc\n"),
        (["string", "split", "-q"], STATUS_INVALID_ARGS, ""),
        (["string", "split", "-q", ":"], STATUS_CMD_ERROR, ""),
        (["string", "split", "-q", "x", "axbxc"], STATUS_CMD_OK, ""),

        (["string", "sub"], STATUS_CMD_ERROR, ""),
        (["string", "sub", "abcde"], STATUS_CMD_OK, "abcde\n"),
        (["string", "sub", "-l", "x", "abcde"], STATUS_INVALID_ARGS, ""),
        (["string", "sub", "-s", "x", "abcde"], STATUS_INVALID_ARGS, ""),
        (["string", "sub", "-l0", "abcde"], STATUS_CMD_OK, "\n"),
        (["string", "sub", "-l2", "abcde"], STATUS_CMD_OK, "ab\n"),
        (["string", "sub", "-l5", "abcde"], STATUS_CMD_OK, "abcde\n"),
        (["string", "sub", "-l6", "abcde"], STATUS_CMD_OK, "abcde\n"),
        (["string", "sub", "-l-1", "abcde"], STATUS_INVALID_ARGS, ""),
        (["string", "sub", "-s0", "abcde"], STATUS_INVALID_ARGS, ""),
        (["string", "sub", "-s1", "abcde"], STATUS_CMD_OK, "abcde\n"),
        (["string", "sub", "-s5", "abcde"], STATUS_CMD_OK, "e\n"),
        (["string", "sub", "-s6", "abcde"], STATUS_CMD_OK, "\n"),
        (["string", "sub", "-s-1", "abcde"], STATUS_CMD_OK, "e\n"),
        (["string", "sub", "-s-5", "abcde"], STATUS_CMD_OK, "abcde\n"),
        (["string", "sub", "-s-6", "abcde"], STATUS_CMD_OK, "abcde\n"),
        (["string", "sub", "-s1", "-l0", "abcde"], STATUS_CMD_OK, "\n"),
        (["string", "sub", "-s1", "-l1", "abcde"], STATUS_CMD_OK, "a\n"),
        (["string", "sub", "-s2", "-l2", "abcde"], STATUS_CMD_OK, "bc\n"),
        (["string", "sub", "-s-1", "-l1", "abcde"], STATUS_CMD_OK, "e\n"),
        (["string", "sub", "-s-1", "-l2", "abcde"], STATUS_CMD_OK, "e\n"),
        (["string", "sub", "-s-3", "-l2", "abcde"], STATUS_CMD_OK, "cd\n"),
        (["string", "sub", "-s-3", "-l4", "abcde"], STATUS_CMD_OK, "cde\n"),
        (["string", "sub", "-q"], STATUS_CMD_ERROR, ""),
        (["string", "sub", "-q", "abcde"], STATUS_CMD_OK, ""),

        (["string", "trim"], STATUS_CMD_ERROR, ""),
        (["string", "trim", ""], STATUS_CMD_ERROR, "\n"),
        (["string", "trim", " "], STATUS_CMD_OK, "\n"),
        (["string", "trim", "  \x0C\n\r\t"], STATUS_CMD_OK, "\n"),
        (["string", "trim", " a"], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "a "], STATUS_CMD_OK, "a\n"),
        (["string", "trim", " a "], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-l", " a"], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-l", "a "], STATUS_CMD_ERROR, "a \n"),
        (["string", "trim", "-l", " a "], STATUS_CMD_OK, "a \n"),
        (["string", "trim", "-r", " a"], STATUS_CMD_ERROR, " a\n"),
        (["string", "trim", "-r", "a "], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-r", " a "], STATUS_CMD_OK, " a\n"),
        (["string", "trim", "-c", ".", " a"], STATUS_CMD_ERROR, " a\n"),
        (["string", "trim", "-c", ".", "a "], STATUS_CMD_ERROR, "a \n"),
        (["string", "trim", "-c", ".", " a "], STATUS_CMD_ERROR, " a \n"),
        (["string", "trim", "-c", ".", ".a"], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-c", ".", "a."], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-c", ".", ".a."], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-c", "\\/", "/a\\"], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-c", "\\/", "a/"], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-c", "\\/", "\\a/"], STATUS_CMD_OK, "a\n"),
        (["string", "trim", "-c", "", ".a."], STATUS_CMD_ERROR, ".a.\n"),
    ];

    for (cmd, expected_status, expected_stdout) in tests {
        string_test(cmd, expected_status, expected_stdout);
    }

    let qmark_noglob_tests = test_cases![
        (["string", "match", "a*b?c", "axxb?c"], STATUS_CMD_OK, "axxb?c\n"),
        (["string", "match", "*?", "a"], STATUS_CMD_ERROR, ""),
        (["string", "match", "*?", "ab"], STATUS_CMD_ERROR, ""),
        (["string", "match", "?*", "a"], STATUS_CMD_ERROR, ""),
        (["string", "match", "?*", "ab"], STATUS_CMD_ERROR, ""),
        (["string", "match", "a*\\?", "abc?"], STATUS_CMD_ERROR, ""),
    ];

    scoped_test(FeatureFlag::qmark_noglob, true, || {
        for (cmd, expected_status, expected_stdout) in qmark_noglob_tests {
            string_test(cmd, expected_status, expected_stdout);
        }
    });

    let qmark_glob_tests = test_cases![
        (["string", "match", "a*b?c", "axxbyc"], STATUS_CMD_OK, "axxbyc\n"),
        (["string", "match", "*?", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "match", "*?", "ab"], STATUS_CMD_OK, "ab\n"),
        (["string", "match", "?*", "a"], STATUS_CMD_OK, "a\n"),
        (["string", "match", "?*", "ab"], STATUS_CMD_OK, "ab\n"),
        (["string", "match", "a*\\?", "abc?"], STATUS_CMD_OK, "abc?\n"),
    ];

    scoped_test(FeatureFlag::qmark_noglob, false, || {
        for (cmd, expected_status, expected_stdout) in qmark_glob_tests {
            string_test(cmd, expected_status, expected_stdout);
        }
    });

}}
