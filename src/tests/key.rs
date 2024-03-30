use crate::key::{self, ctrl, parse_keys, Key};
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
}
