use pcre2::utf32::Regex;

use crate::parse_constants::{
    ERROR_BAD_VAR_CHAR1, ERROR_BRACKETED_VARIABLE1, ERROR_BRACKETED_VARIABLE_QUOTED1,
    ERROR_NOT_ARGV_AT, ERROR_NOT_ARGV_COUNT, ERROR_NOT_ARGV_STAR, ERROR_NOT_PID, ERROR_NOT_STATUS,
    ERROR_NO_VAR_NAME,
};
use crate::parse_util::parse_util_detect_errors;
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wchar_ext::WExt;

#[test]
#[serial]
fn test_error_messages() {
    test_init();
    // Given a format string, returns a list of non-empty strings separated by format specifiers. The
    // format specifiers themselves are omitted.
    fn separate_by_format_specifiers(format: &wstr) -> Vec<&wstr> {
        let format_specifier_regex = Regex::new(L!(r"%l?[ds]").as_char_slice()).unwrap();
        let mut result = vec![];
        let mut offset = 0;
        for mtch in format_specifier_regex.find_iter(format.as_char_slice()) {
            let mtch = mtch.unwrap();
            result.push(&format[offset..mtch.start()]);
            offset = mtch.end();
        }
        result
    }

    // Given a format string 'format', return true if the string may have been produced by that format
    // string. We do this by splitting the format string around the format specifiers, and then ensuring
    // that each of the remaining chunks is found (in order) in the string.
    fn string_matches_format(s: &wstr, format: &wstr) -> bool {
        let components = separate_by_format_specifiers(format);
        let mut idx = 0;
        for component in components {
            let Some(relpos) = s[idx..].find(component) else {
                return false;
            };
            idx += relpos + component.len();
            assert!(idx <= s.len());
        }
        true
    }

    macro_rules! validate {
        ($src:expr, $error_text_format:expr) => {
            let mut errors = vec![];
            let res = parse_util_detect_errors(L!($src), Some(&mut errors), false);
            assert!(res.is_err());
            assert!(
                string_matches_format(&errors[0].text, L!($error_text_format)),
                "{}",
                $src
            );
        };
    }

    validate!("echo $^", ERROR_BAD_VAR_CHAR1);
    validate!("echo foo${a}bar", ERROR_BRACKETED_VARIABLE1);
    validate!("echo foo\"${a}\"bar", ERROR_BRACKETED_VARIABLE_QUOTED1);
    validate!("echo foo\"${\"bar", ERROR_BAD_VAR_CHAR1);
    validate!("echo $?", ERROR_NOT_STATUS);
    validate!("echo $$", ERROR_NOT_PID);
    validate!("echo $#", ERROR_NOT_ARGV_COUNT);
    validate!("echo $@", ERROR_NOT_ARGV_AT);
    validate!("echo $*", ERROR_NOT_ARGV_STAR);
    validate!("echo $", ERROR_NO_VAR_NAME);
    validate!("echo foo\"$\"bar", ERROR_NO_VAR_NAME);
    validate!("echo \"foo\"$\"bar\"", ERROR_NO_VAR_NAME);
    validate!("echo foo $ bar", ERROR_NO_VAR_NAME);
}
