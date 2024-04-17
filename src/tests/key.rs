use crate::key::{self, ctrl, function_key, parse_keys, Key};
use crate::wchar::prelude::*;

#[test]
fn test_parse_key() {
    assert_eq!(
        parse_keys(L!("escape")),
        Ok(vec![Key::from_raw(key::Escape)])
    );
    assert_eq!(parse_keys(L!("\x1b")), Ok(vec![Key::from_raw(key::Escape)]));
    assert_eq!(parse_keys(L!("ctrl-a")), Ok(vec![ctrl('a')]));
    assert_eq!(parse_keys(L!("c-a")), Ok(vec![ctrl('a')]));
    assert_eq!(parse_keys(L!("\x01")), Ok(vec![ctrl('a')]));
    assert!(parse_keys(L!("F0")).is_err());
    assert_eq!(
        parse_keys(L!("F1")),
        Ok(vec![Key::from_raw(function_key(1))])
    );
}
