use crate::common::{str2wcstring, wcs2string};
use crate::wchar::prelude::*;

/// Verify correct behavior with embedded nulls.
#[test]
fn test_convert_nulls() {
    let input = L!("AAA\0BBB");
    let out_str = wcs2string(input);
    assert_eq!(
        input.chars().collect::<Vec<_>>(),
        std::str::from_utf8(&out_str)
            .unwrap()
            .chars()
            .collect::<Vec<_>>()
    );

    let out_wstr = str2wcstring(&out_str);
    assert_eq!(input, &out_wstr);
}
