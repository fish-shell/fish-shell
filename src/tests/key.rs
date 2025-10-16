use crate::key::{self, Key, ctrl, function_key, parse_keys};
use crate::wchar::prelude::*;

#[test]
fn test_parse_key() {
    assert_eq!(
        parse_keys(L!("escape")),
        Ok(vec![Key::from_raw(key::Escape)])
    );
    assert_eq!(parse_keys(L!("\x1b")), Ok(vec![Key::from_raw(key::Escape)]));
    assert_eq!(parse_keys(L!("ctrl-a")), Ok(vec![ctrl('a')]));
    assert_eq!(parse_keys(L!("\x01")), Ok(vec![ctrl('a')]));
    assert!(parse_keys(L!("f0")).is_err());
    assert_eq!(
        parse_keys(L!("f1")),
        Ok(vec![Key::from_raw(function_key(1))])
    );
    assert!(parse_keys(L!("F1")).is_err());
}
