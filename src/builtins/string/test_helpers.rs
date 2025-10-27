use super::string;
use crate::builtins::shared::BuiltinResultExt;
use crate::io::IoChain;
use crate::io::{IoStreams, OutputStream, StringOutputStream};
use crate::tests::prelude::*;
use crate::wchar::prelude::*;

#[macro_export]
macro_rules! validate {
    ( [$($argv:expr),*], $expected_rc:expr, $expected_out:expr ) => {
        {
            use $crate::common::escape;
            use $crate::wchar::prelude::*;
            use $crate::builtins::string::test_helpers::string_test;
            let (actual_out, actual_rc) = string_test(vec![$(L!($argv)),*]);
            assert_eq!(escape(L!($expected_out)), escape(&actual_out));
            assert_eq!($expected_rc, actual_rc);
        }
    };
}

pub fn string_test(mut args: Vec<&wstr>) -> (WString, libc::c_int) {
    let parser = TestParser::new();
    let mut outs = OutputStream::String(StringOutputStream::new());
    let mut errs = OutputStream::Null;
    let io_chain = IoChain::new();
    let mut streams = IoStreams::new(&mut outs, &mut errs, &io_chain);
    streams.stdin_is_directly_redirected = false; // read from argv instead of stdin

    let rc = string(&parser, &mut streams, args.as_mut_slice());

    (outs.contents().to_owned(), rc.builtin_status_code())
}
